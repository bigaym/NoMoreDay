#include "engine/render/lighting/GlobalHeightField.hpp"

#include "engine/render/GPUUtils.hpp"
#include "game/components/Common.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/ShadowCasterComponent.hpp"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::render::lighting {
namespace {

constexpr uint32_t kMaxChunkUploadsPerFrame = 64u;

[[nodiscard]] inline uint16_t ToHeightU16(const float value) {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  return static_cast<uint16_t>(std::round(clamped * 65535.0f));
}

[[nodiscard]] inline float ToHeightNorm(const uint16_t value) {
  return static_cast<float>(value) / 65535.0f;
}

[[nodiscard]] float EstimateMaskBlue(SpriteComponent &sprite,
                                     std::unordered_map<uint32_t, float> &cache) {
  if (sprite.texture.id == 0u) {
    return 0.0f;
  }

  const uint32_t id = sprite.texture.id;
  const auto cached = cache.find(id);
  if (cached != cache.end()) {
    return cached->second;
  }

  Image image = LoadImageFromTexture(sprite.texture);
  if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
    cache[id] = 0.0f;
    return 0.0f;
  }

  const int centerX = std::clamp(image.width / 2, 0, image.width - 1);
  const int centerY = std::clamp(image.height / 2, 0, image.height - 1);
  const Color center = GetImageColor(image, centerX, centerY);
  UnloadImage(image);

  const float blue = static_cast<float>(center.b) / 255.0f;
  cache[id] = blue;
  return blue;
}

} // namespace

std::size_t GlobalHeightField::ChunkCoordHash::operator()(
    const ChunkCoord &coord) const noexcept {
  const std::size_t hx = std::hash<int32_t>{}(coord.x);
  const std::size_t hy = std::hash<int32_t>{}(coord.y);
  return hx ^ (hy + 0x9e3779b9 + (hx << 6U) + (hx >> 2U));
}

bool GlobalHeightField::Initialize(const Config &config) {
  Shutdown();

  m_config = config;
  m_config.textureWidth = std::max(1, m_config.textureWidth);
  m_config.textureHeight = std::max(1, m_config.textureHeight);
  m_config.chunkSize = std::max(1, m_config.chunkSize);
  m_config.worldWidth = std::max(1.0f, m_config.worldWidth);
  m_config.worldHeight = std::max(1.0f, m_config.worldHeight);

  const size_t texelCount = static_cast<size_t>(m_config.textureWidth) *
                            static_cast<size_t>(m_config.textureHeight);
  m_baseLayer.assign(texelCount, 0u);
  m_dynamicLayer.assign(texelCount, 0u);
  m_compositedLayer.assign(texelCount, 0u);

  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  const int chunkGridY =
      (m_config.textureHeight + (m_config.chunkSize - 1)) / m_config.chunkSize;
  m_dirtyChunks.assign(static_cast<size_t>(chunkGridX * chunkGridY), 1u);
  m_dynamicChunkMarks.assign(m_dirtyChunks.size(), 0u);
  m_prevDynamicChunks.clear();
  m_currDynamicChunks.clear();
  m_maskBlueCache.clear();
  m_lastStats = {};

  m_initialized = EnsureTexture();
  m_pendingFullRebuild = true;
  return m_initialized;
}

void GlobalHeightField::Shutdown() {
  if (m_texture.id != 0u) {
    UnloadTexture(m_texture);
    m_texture = {};
  }
  m_baseLayer.clear();
  m_dynamicLayer.clear();
  m_compositedLayer.clear();
  m_dirtyChunks.clear();
  m_dynamicChunkMarks.clear();
  m_prevDynamicChunks.clear();
  m_currDynamicChunks.clear();
  std::fill(m_dynamicChunkMarks.begin(), m_dynamicChunkMarks.end(), 0u);
  m_maskBlueCache.clear();
  m_uploadScratch.clear();
  m_lastStats = {};
  m_initialized = false;
  m_pendingFullRebuild = true;
}

void GlobalHeightField::Update(entt::registry &registry) {
  if (!m_initialized && !EnsureTexture()) {
    return;
  }

  m_lastStats = {};
  if (m_pendingFullRebuild) {
    BuildTerrainAndStatic(registry);
    m_pendingFullRebuild = false;
    m_lastStats.didFullRebuild = true;
  }

  ClearDynamicLayerForPreviousChunks();
  BuildDynamicLayer(registry);
  ComposeDirtyChunks();
  UploadDirtyChunks();
}

bool GlobalHeightField::EnsureTexture() {
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  if (m_texture.id != 0u) {
    return true;
  }

  Image image = GenImageColor(m_config.textureWidth, m_config.textureHeight, BLACK);
  if (image.data == nullptr) {
    return false;
  }
  m_texture = LoadTextureFromImage(image);
  UnloadImage(image);
  return m_texture.id != 0u;
}

void GlobalHeightField::BuildTerrainAndStatic(entt::registry &registry) {
  std::fill(m_baseLayer.begin(), m_baseLayer.end(), 0u);
  std::fill(m_dynamicLayer.begin(), m_dynamicLayer.end(), 0u);

  auto tileView = registry.view<MapTileComponent>();
  for (const entt::entity entity : tileView) {
    const auto &tile = tileView.get<MapTileComponent>(entity);
    const float height = (tile.tileType == Tile::Type::WALL)
                             ? m_config.terrainWallHeight
                             : m_config.terrainFloorHeight;
    StampTileRectMax(m_baseLayer, tile.gridX, tile.gridY, height);
  }

  auto staticShadowView = registry.view<Position, NoMoreDay::ShadowCasterComponent>();
  for (const entt::entity entity : staticShadowView) {
    const auto [pos, caster] =
        staticShadowView.get<Position, NoMoreDay::ShadowCasterComponent>(entity);
    if (caster.dynamicFlag != 0u) {
      continue;
    }
    const float radius = 20.0f;
    StampDiscMax(m_baseLayer, pos.x, pos.y, radius,
                 std::clamp(caster.occluderHeight, 0.0f, 1.0f), false);
  }

  auto staticColliderView = registry.view<Position, ColliderComponent>();
  for (const entt::entity entity : staticColliderView) {
    const auto [pos, collider] = staticColliderView.get<Position, ColliderComponent>(entity);
    if (collider.type != ColliderType::Static) {
      continue;
    }
    const float radius = std::max(collider.width, collider.height) * 0.5f;
    StampDiscMax(m_baseLayer, pos.x, pos.y, std::max(2.0f, radius), 0.75f, false);
  }

  std::fill(m_dirtyChunks.begin(), m_dirtyChunks.end(), 1u);
  m_prevDynamicChunks.clear();
}

void GlobalHeightField::ClearDynamicLayerForPreviousChunks() {
  m_lastStats.dynamicChunkCount = static_cast<uint32_t>(m_prevDynamicChunks.size());
  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  for (const uint32_t flat : m_prevDynamicChunks) {
    const int cx = static_cast<int>(flat % static_cast<uint32_t>(chunkGridX));
    const int cy = static_cast<int>(flat / static_cast<uint32_t>(chunkGridX));
    const ChunkCoord coord{cx, cy};
    const int startX = cx * m_config.chunkSize;
    const int startY = cy * m_config.chunkSize;
    const int endX = std::min(startX + m_config.chunkSize, m_config.textureWidth);
    const int endY = std::min(startY + m_config.chunkSize, m_config.textureHeight);
    for (int y = startY; y < endY; ++y) {
      const size_t rowOffset = static_cast<size_t>(y) *
                               static_cast<size_t>(m_config.textureWidth);
      std::fill(m_dynamicLayer.begin() + static_cast<std::ptrdiff_t>(rowOffset + startX),
                m_dynamicLayer.begin() + static_cast<std::ptrdiff_t>(rowOffset + endX),
                0u);
    }
    MarkChunkDirty(coord);
  }
  m_currDynamicChunks.clear();
}

void GlobalHeightField::BuildDynamicLayer(entt::registry &registry) {
  auto dynamicShadowView = registry.view<Position, NoMoreDay::ShadowCasterComponent>();
  for (const entt::entity entity : dynamicShadowView) {
    const auto [pos, caster] =
        dynamicShadowView.get<Position, NoMoreDay::ShadowCasterComponent>(entity);
    if (caster.dynamicFlag == 0u) {
      continue;
    }
    StampDiscMax(m_dynamicLayer, pos.x, pos.y, 18.0f,
                 std::clamp(caster.occluderHeight, 0.0f, 1.0f), true);
  }

  auto spriteView = registry.view<Position, SpriteComponent>();
  for (const entt::entity entity : spriteView) {
    auto [pos, sprite] = spriteView.get<Position, SpriteComponent>(entity);
    const float blue = EstimateMaskBlue(sprite, m_maskBlueCache);
    if (blue <= 0.02f) {
      continue;
    }
    const float radius = std::max(6.0f, 8.0f * std::max(0.25f, sprite.scale));
    StampDiscMax(m_dynamicLayer, pos.x, pos.y, radius, blue, true);
  }

  m_prevDynamicChunks = m_currDynamicChunks;
}

void GlobalHeightField::ComposeDirtyChunks() {
  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  const int chunkGridY =
      (m_config.textureHeight + (m_config.chunkSize - 1)) / m_config.chunkSize;
  uint32_t dirtyCount = 0u;
  for (int cy = 0; cy < chunkGridY; ++cy) {
    for (int cx = 0; cx < chunkGridX; ++cx) {
      const uint32_t flat =
          static_cast<uint32_t>(cy * chunkGridX + cx);
      if (flat >= m_dirtyChunks.size() || m_dirtyChunks[flat] == 0u) {
        continue;
      }
      ++dirtyCount;
      const int startX = cx * m_config.chunkSize;
      const int startY = cy * m_config.chunkSize;
      const int endX = std::min(startX + m_config.chunkSize, m_config.textureWidth);
      const int endY = std::min(startY + m_config.chunkSize, m_config.textureHeight);
      for (int y = startY; y < endY; ++y) {
        const size_t rowOffset = static_cast<size_t>(y) *
                                 static_cast<size_t>(m_config.textureWidth);
        for (int x = startX; x < endX; ++x) {
          const size_t idx = rowOffset + static_cast<size_t>(x);
          m_compositedLayer[idx] = std::max(m_baseLayer[idx], m_dynamicLayer[idx]);
        }
      }
    }
  }
  m_lastStats.dirtyChunkCount = dirtyCount;
}

void GlobalHeightField::UploadDirtyChunks() {
  if (m_texture.id == 0u || !NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return;
  }

  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  const int chunkGridY =
      (m_config.textureHeight + (m_config.chunkSize - 1)) / m_config.chunkSize;

  uint32_t uploaded = 0u;
  for (int cy = 0; cy < chunkGridY; ++cy) {
    for (int cx = 0; cx < chunkGridX; ++cx) {
      const uint32_t flat =
          static_cast<uint32_t>(cy * chunkGridX + cx);
      if (flat >= m_dirtyChunks.size() || m_dirtyChunks[flat] == 0u) {
        continue;
      }
      if (uploaded >= kMaxChunkUploadsPerFrame) {
        break;
      }
      UploadChunkRect({cx, cy});
      m_dirtyChunks[flat] = 0u;
      ++uploaded;
    }
    if (uploaded >= kMaxChunkUploadsPerFrame) {
      break;
    }
  }
  m_lastStats.uploadedChunkCount = uploaded;
}

bool GlobalHeightField::WorldToTexel(float worldX, float worldY, int &tx,
                                     int &ty) const {
  const float nx = (worldX - m_config.worldOriginX) / m_config.worldWidth;
  const float ny = (worldY - m_config.worldOriginY) / m_config.worldHeight;
  if (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f) {
    return false;
  }

  tx = std::clamp(static_cast<int>(std::floor(nx * static_cast<float>(m_config.textureWidth))),
                  0, m_config.textureWidth - 1);
  ty = std::clamp(static_cast<int>(std::floor(ny * static_cast<float>(m_config.textureHeight))),
                  0, m_config.textureHeight - 1);
  return true;
}

GlobalHeightField::ChunkCoord GlobalHeightField::TexelToChunk(const int tx,
                                                              const int ty) const {
  return {tx / m_config.chunkSize, ty / m_config.chunkSize};
}

uint32_t GlobalHeightField::FlattenChunk(const ChunkCoord &coord) const {
  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  return static_cast<uint32_t>(coord.y * chunkGridX + coord.x);
}

void GlobalHeightField::MarkChunkDirty(const ChunkCoord &coord) {
  const int chunkGridX =
      (m_config.textureWidth + (m_config.chunkSize - 1)) / m_config.chunkSize;
  const int chunkGridY =
      (m_config.textureHeight + (m_config.chunkSize - 1)) / m_config.chunkSize;
  if (coord.x < 0 || coord.y < 0 || coord.x >= chunkGridX || coord.y >= chunkGridY) {
    return;
  }
  const uint32_t flat = FlattenChunk(coord);
  if (flat < m_dirtyChunks.size()) {
    m_dirtyChunks[flat] = 1u;
  }
}

void GlobalHeightField::StampDiscMax(std::vector<uint16_t> &layer, const float worldX,
                                     const float worldY, const float worldRadius,
                                     const float normalizedHeight,
                                     const bool trackDynamicChunk) {
  int centerX = 0;
  int centerY = 0;
  if (!WorldToTexel(worldX, worldY, centerX, centerY)) {
    return;
  }

  const float texelRadiusX =
      (worldRadius / m_config.worldWidth) * static_cast<float>(m_config.textureWidth);
  const float texelRadiusY =
      (worldRadius / m_config.worldHeight) * static_cast<float>(m_config.textureHeight);
  const int rx = std::max(1, static_cast<int>(std::ceil(texelRadiusX)));
  const int ry = std::max(1, static_cast<int>(std::ceil(texelRadiusY)));
  const int minX = std::max(0, centerX - rx);
  const int maxX = std::min(m_config.textureWidth - 1, centerX + rx);
  const int minY = std::max(0, centerY - ry);
  const int maxY = std::min(m_config.textureHeight - 1, centerY + ry);

  const uint16_t u16 = ToHeightU16(normalizedHeight);
  const float invRx = 1.0f / std::max(1.0f, static_cast<float>(rx));
  const float invRy = 1.0f / std::max(1.0f, static_cast<float>(ry));
  for (int y = minY; y <= maxY; ++y) {
    const float dy = static_cast<float>(y - centerY) * invRy;
    const size_t rowOffset = static_cast<size_t>(y) *
                             static_cast<size_t>(m_config.textureWidth);
    for (int x = minX; x <= maxX; ++x) {
      const float dx = static_cast<float>(x - centerX) * invRx;
      if ((dx * dx) + (dy * dy) > 1.0f) {
        continue;
      }
      const size_t idx = rowOffset + static_cast<size_t>(x);
      layer[idx] = std::max(layer[idx], u16);
      const ChunkCoord chunk = TexelToChunk(x, y);
      MarkChunkDirty(chunk);
      if (trackDynamicChunk) {
        const uint32_t flat = FlattenChunk(chunk);
        if (flat < m_dynamicChunkMarks.size() && m_dynamicChunkMarks[flat] == 0u) {
          m_dynamicChunkMarks[flat] = 1u;
          m_currDynamicChunks.push_back(flat);
        }
      }
    }
  }
}

void GlobalHeightField::StampTileRectMax(std::vector<uint16_t> &layer, const int tileX,
                                         const int tileY, const float normalizedHeight) {
  const float tileSize = std::max(1.0f, Constants::World::GRID_TILE_SIZE);
  const float worldX0 = static_cast<float>(tileX) * tileSize;
  const float worldY0 = static_cast<float>(tileY) * tileSize;
  const float worldX1 = worldX0 + tileSize;
  const float worldY1 = worldY0 + tileSize;

  int tx0 = 0;
  int ty0 = 0;
  int tx1 = 0;
  int ty1 = 0;
  if (!WorldToTexel(worldX0, worldY0, tx0, ty0)) {
    return;
  }
  if (!WorldToTexel(worldX1, worldY1, tx1, ty1)) {
    tx1 = tx0;
    ty1 = ty0;
  }

  const int minX = std::max(0, std::min(tx0, tx1));
  const int maxX = std::min(m_config.textureWidth - 1, std::max(tx0, tx1));
  const int minY = std::max(0, std::min(ty0, ty1));
  const int maxY = std::min(m_config.textureHeight - 1, std::max(ty0, ty1));
  const uint16_t u16 = ToHeightU16(normalizedHeight);
  for (int y = minY; y <= maxY; ++y) {
    const size_t rowOffset = static_cast<size_t>(y) *
                             static_cast<size_t>(m_config.textureWidth);
    for (int x = minX; x <= maxX; ++x) {
      layer[rowOffset + static_cast<size_t>(x)] =
          std::max(layer[rowOffset + static_cast<size_t>(x)], u16);
      MarkChunkDirty(TexelToChunk(x, y));
    }
  }
}

void GlobalHeightField::UploadChunkRect(const ChunkCoord &coord) {
  const int startX = coord.x * m_config.chunkSize;
  const int startY = coord.y * m_config.chunkSize;
  const int width = std::min(m_config.chunkSize, m_config.textureWidth - startX);
  const int height = std::min(m_config.chunkSize, m_config.textureHeight - startY);
  if (width <= 0 || height <= 0) {
    return;
  }

  const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
  m_uploadScratch.resize(pixelCount);
  for (int row = 0; row < height; ++row) {
    const size_t srcOffset =
        static_cast<size_t>(startY + row) * static_cast<size_t>(m_config.textureWidth) +
        static_cast<size_t>(startX);
    const size_t dstOffset = static_cast<size_t>(row) * static_cast<size_t>(width);
    for (int col = 0; col < width; ++col) {
      const uint8_t v = static_cast<uint8_t>(
          std::round(ToHeightNorm(m_compositedLayer[srcOffset + static_cast<size_t>(col)]) *
                     255.0f));
      m_uploadScratch[dstOffset + static_cast<size_t>(col)] = {v, v, v, 255};
    }
  }

  const Rectangle rect = {static_cast<float>(startX), static_cast<float>(startY),
                          static_cast<float>(width), static_cast<float>(height)};
  UpdateTextureRec(m_texture, rect, m_uploadScratch.data());
}

float GlobalHeightField::SampleNormalizedHeight(const float worldX,
                                                const float worldY) const {
  int tx = 0;
  int ty = 0;
  if (!WorldToTexel(worldX, worldY, tx, ty)) {
    return 0.0f;
  }
  const size_t idx = static_cast<size_t>(ty) * static_cast<size_t>(m_config.textureWidth) +
                     static_cast<size_t>(tx);
  if (idx >= m_compositedLayer.size()) {
    return 0.0f;
  }
  return ToHeightNorm(m_compositedLayer[idx]);
}

} // namespace NoMoreDay::render::lighting
