#include "game/application/render/HeightFieldAdapter.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/MapComponent.hpp"
#include "game/foundation/components/ShadowCasterComponent.hpp"
#include "game/systems/world/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {
namespace {

// Historical GlobalHeightField::Config defaults preserved verbatim for
// byte-for-byte equivalent rasterization.
constexpr float kTerrainWallHeight = 0.85f;
constexpr float kTerrainFloorHeight = 0.10f;
constexpr float kStaticCasterRadius = 20.0f;
constexpr float kStaticColliderHeight = 0.75f;
constexpr float kDynamicCasterRadius = 18.0f;
constexpr float kSpriteBlueThreshold = 0.02f;

// Cache of texture.id -> estimated mask blue (moved verbatim from
// GlobalHeightField::EstimateMaskBlue). thread_local: multiple render threads
// never share raylib texture reads; the Game adapter is the sole owner.
[[nodiscard]] float EstimateMaskBlue(const SpriteComponent &sprite,
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

HeightFieldProjection HeightFieldAdapter::BuildStamps(entt::registry &registry) {
  HeightFieldProjection projection;
  using Stamp = render::lighting::GlobalHeightField::HeightStamp;

  projection.worldWidth = static_cast<float>(Constants::World::WORLD_WIDTH);
  projection.worldHeight = static_cast<float>(Constants::World::WORLD_HEIGHT);
  projection.tileWorldSize = Constants::World::GRID_TILE_SIZE;

  // Terrain tiles -> full tile-rect stamps (static/base layer).
  auto tileView = registry.view<const MapTileComponent>();
  projection.stamps.reserve(static_cast<size_t>(tileView.size()));
  for (const entt::entity entity : tileView) {
    const auto &tile = tileView.get<MapTileComponent>(entity);
    Stamp stamp = {};
    stamp.kind = Stamp::Kind::Tile;
    stamp.tileX = tile.gridX;
    stamp.tileY = tile.gridY;
    stamp.height = (tile.tileType == Tile::Type::WALL) ? kTerrainWallHeight
                                                       : kTerrainFloorHeight;
    stamp.dynamic = false;
    projection.stamps.push_back(stamp);
  }

  // Static casters -> disc stamps on the base layer.
  auto staticShadowView =
      registry.view<const Position, const NoMoreDay::ShadowCasterComponent>();
  for (const entt::entity entity : staticShadowView) {
    const auto &[pos, caster] =
        staticShadowView.get<const Position, const NoMoreDay::ShadowCasterComponent>(
            entity);
    if (caster.dynamicFlag != 0u) {
      continue;
    }
    Stamp stamp = {};
    stamp.kind = Stamp::Kind::Disc;
    stamp.worldX = pos.x;
    stamp.worldY = pos.y;
    stamp.worldRadius = kStaticCasterRadius;
    stamp.height = std::clamp(caster.occluderHeight, 0.0f, 1.0f);
    stamp.dynamic = false;
    projection.stamps.push_back(stamp);
  }

  // Static colliders -> disc stamps on the base layer.
  auto staticColliderView = registry.view<const Position, const ColliderComponent>();
  for (const entt::entity entity : staticColliderView) {
    const auto &[pos, collider] =
        staticColliderView.get<const Position, const ColliderComponent>(entity);
    if (collider.type != ColliderType::Static) {
      continue;
    }
    const float radius = std::max(collider.width, collider.height) * 0.5f;
    Stamp stamp = {};
    stamp.kind = Stamp::Kind::Disc;
    stamp.worldX = pos.x;
    stamp.worldY = pos.y;
    stamp.worldRadius = std::max(2.0f, radius);
    stamp.height = kStaticColliderHeight;
    stamp.dynamic = false;
    projection.stamps.push_back(stamp);
  }

  // Dynamic casters -> disc stamps on the dynamic layer.
  auto dynamicShadowView =
      registry.view<const Position, const NoMoreDay::ShadowCasterComponent>();
  for (const entt::entity entity : dynamicShadowView) {
    const auto &[pos, caster] =
        dynamicShadowView.get<const Position, const NoMoreDay::ShadowCasterComponent>(
            entity);
    if (caster.dynamicFlag == 0u) {
      continue;
    }
    Stamp stamp = {};
    stamp.kind = Stamp::Kind::Disc;
    stamp.worldX = pos.x;
    stamp.worldY = pos.y;
    stamp.worldRadius = kDynamicCasterRadius;
    stamp.height = std::clamp(caster.occluderHeight, 0.0f, 1.0f);
    stamp.dynamic = true;
    projection.stamps.push_back(stamp);
  }

  // Blue-masked sprites -> disc stamps on the dynamic layer.
  static thread_local std::unordered_map<uint32_t, float> s_maskBlueCache;
  auto spriteView = registry.view<const Position, const SpriteComponent>();
  for (const entt::entity entity : spriteView) {
    const auto &[pos, sprite] =
        spriteView.get<const Position, const SpriteComponent>(entity);
    const float blue = EstimateMaskBlue(sprite, s_maskBlueCache);
    if (blue <= kSpriteBlueThreshold) {
      continue;
    }
    const float radius = std::max(6.0f, 8.0f * std::max(0.25f, sprite.scale));
    Stamp stamp = {};
    stamp.kind = Stamp::Kind::Disc;
    stamp.worldX = pos.x;
    stamp.worldY = pos.y;
    stamp.worldRadius = radius;
    stamp.height = blue;
    stamp.dynamic = true;
    projection.stamps.push_back(stamp);
  }

  return projection;
}

} // namespace NoMoreDay
