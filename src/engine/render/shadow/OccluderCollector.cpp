#include "engine/render/shadow/OccluderCollector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NoMoreDay::render::shadow {

namespace {

int32_t FloorToInt(const float value) noexcept {
  return static_cast<int32_t>(std::floor(value));
}

bool IsDeterministicallyLess(const ShadowChunkCoord &lhs,
                             const ShadowChunkCoord &rhs) noexcept {
  if (lhs.y != rhs.y) {
    return lhs.y < rhs.y;
  }
  return lhs.x < rhs.x;
}

} // namespace

std::size_t
ShadowChunkCoordHash::operator()(const ShadowChunkCoord &coord) const noexcept {
  const std::size_t hx = std::hash<int32_t>{}(coord.x);
  const std::size_t hy = std::hash<int32_t>{}(coord.y);
  return hx ^ (hy + 0x9e3779b9 + (hx << 6U) + (hx >> 2U));
}

OccluderCollector::OccluderCollector(OccluderCollectorConfig config)
    : m_config(std::move(config)) {
  if (m_config.chunkSize <= 0.0f) {
    m_config.chunkSize = RenderConstants::Shadow::kShadowChunkSize;
  }
  if (m_config.cameraNeighborhoodRadius <= 0.0f) {
    m_config.cameraNeighborhoodRadius =
        RenderConstants::Shadow::kCameraNeighborhoodRadius;
  }
  if (m_config.maxShadowCasters == 0) {
    m_config.maxShadowCasters = RenderConstants::Shadow::kMaxShadowCasters;
  }
  if (m_config.maxCachedStaticChunks == 0) {
    m_config.maxCachedStaticChunks = 1;
  }
}

void OccluderCollector::BeginFrame(const uint32_t frameIndex) noexcept {
  m_frameIndex = frameIndex;
}

bool OccluderCollector::UpsertOccluder(const OccluderEntry &entry) {
  if (entry.occluderId == 0u) {
    return false;
  }

  const auto existingIt = m_occluders.find(entry.occluderId);
  if (existingIt != m_occluders.end()) {
    const OccluderEntry &old = existingIt->second;
    if (old.dynamicFlag == 0u) {
      const ShadowChunkCoord oldChunk =
          WorldToChunk(old.posX, old.posY, m_config.chunkSize);
      RemoveFromStaticChunkIndex(entry.occluderId, oldChunk);
      m_cachedStaticChunks.erase(oldChunk);
    }
    existingIt->second = entry;
  } else {
    m_occluders.emplace(entry.occluderId, entry);
  }

  if (entry.dynamicFlag == 0u) {
    const ShadowChunkCoord chunk =
        WorldToChunk(entry.posX, entry.posY, m_config.chunkSize);
    AddToStaticChunkIndex(entry.occluderId, chunk);
    m_cachedStaticChunks.erase(chunk);
  }

  return true;
}

bool OccluderCollector::RemoveOccluder(const uint32_t occluderId) {
  const auto it = m_occluders.find(occluderId);
  if (it == m_occluders.end()) {
    return false;
  }

  const OccluderEntry removed = it->second;
  m_occluders.erase(it);
  if (removed.dynamicFlag == 0u) {
    const ShadowChunkCoord chunk =
        WorldToChunk(removed.posX, removed.posY, m_config.chunkSize);
    RemoveFromStaticChunkIndex(occluderId, chunk);
    m_cachedStaticChunks.erase(chunk);
  }
  return true;
}

OccluderCollectResult
OccluderCollector::CollectVisible(const float cameraWorldX,
                                  const float cameraWorldY) {
  OccluderCollectResult result{};
  m_stagingCasters.clear();
  m_stagingCasters.reserve(m_config.maxShadowCasters);

  const ShadowChunkCoord cameraChunk =
      WorldToChunk(cameraWorldX, cameraWorldY, m_config.chunkSize);
  const int32_t chunkRadius = std::max(
      0, FloorToInt(std::ceil(m_config.cameraNeighborhoodRadius / m_config.chunkSize)));

  bool truncated = false;

  for (int32_t cy = cameraChunk.y - chunkRadius; cy <= cameraChunk.y + chunkRadius;
       ++cy) {
    for (int32_t cx = cameraChunk.x - chunkRadius; cx <= cameraChunk.x + chunkRadius;
         ++cx) {
      const ShadowChunkCoord coord{cx, cy};
      auto cacheIt = m_cachedStaticChunks.find(coord);
      if (cacheIt == m_cachedStaticChunks.end()) {
        const auto staticIt = m_staticChunkIndex.find(coord);
        if (staticIt == m_staticChunkIndex.end() || staticIt->second.empty()) {
          continue;
        }

        CachedChunk chunk;
        chunk.staticCasters.reserve(staticIt->second.size());
        for (const uint32_t occluderId : staticIt->second) {
          if (const auto occluderIt = m_occluders.find(occluderId);
              occluderIt != m_occluders.end() &&
              occluderIt->second.dynamicFlag == 0u) {
            chunk.staticCasters.push_back(ToGPUShadowCaster(occluderIt->second));
          }
        }
        if (chunk.staticCasters.empty()) {
          continue;
        }

        chunk.lastTouchedFrame = m_frameIndex;
        m_chunkUploadCounters[coord] += 1u;
        cacheIt = m_cachedStaticChunks.emplace(coord, std::move(chunk)).first;
      } else {
        cacheIt->second.lastTouchedFrame = m_frameIndex;
      }

      result.staticCasterCount += AppendRangeWithLimit(
          cacheIt->second.staticCasters, m_config.maxShadowCasters, truncated);
    }
  }

  const float radiusSq =
      m_config.cameraNeighborhoodRadius * m_config.cameraNeighborhoodRadius;
  for (const auto &[occluderId, entry] : m_occluders) {
    (void)occluderId;
    if (entry.dynamicFlag == 0u) {
      continue;
    }
    const float dx = entry.posX - cameraWorldX;
    const float dy = entry.posY - cameraWorldY;
    if ((dx * dx) + (dy * dy) > radiusSq) {
      continue;
    }
    result.dynamicCasterCount +=
        AppendWithLimit(ToGPUShadowCaster(entry), m_config.maxShadowCasters, truncated);
  }

  result.truncated = truncated;
  result.totalCasterCount = static_cast<uint32_t>(m_stagingCasters.size());
  result.evictedChunkCount = EvictColdChunksIfNeeded();
  return result;
}

std::vector<ShadowUploadBatch>
OccluderCollector::BuildUploadBatches(uint32_t batchSize) const {
  if (batchSize == 0u) {
    batchSize = 1u;
  }
  std::vector<ShadowUploadBatch> batches;
  batches.reserve((m_stagingCasters.size() + batchSize - 1u) / batchSize);

  const uint32_t total = static_cast<uint32_t>(m_stagingCasters.size());
  for (uint32_t offset = 0; offset < total; offset += batchSize) {
    const uint32_t count = std::min(batchSize, total - offset);
    batches.push_back({.offset = offset, .count = count});
  }
  return batches;
}

uint32_t OccluderCollector::GetCachedChunkCount() const noexcept {
  return static_cast<uint32_t>(m_cachedStaticChunks.size());
}

bool OccluderCollector::HasChunkCached(const ShadowChunkCoord &coord) const {
  return m_cachedStaticChunks.find(coord) != m_cachedStaticChunks.end();
}

uint64_t OccluderCollector::GetChunkUploadCount(
    const ShadowChunkCoord &coord) const noexcept {
  if (const auto it = m_chunkUploadCounters.find(coord);
      it != m_chunkUploadCounters.end()) {
    return it->second;
  }
  return 0;
}

ShadowChunkCoord OccluderCollector::WorldToChunk(const float worldX,
                                                 const float worldY,
                                                 const float chunkSize) noexcept {
  if (chunkSize <= 0.0f) {
    return {};
  }
  return {
      .x = FloorToInt(worldX / chunkSize),
      .y = FloorToInt(worldY / chunkSize),
  };
}

components::GPUShadowCaster
OccluderCollector::ToGPUShadowCaster(const OccluderEntry &entry) const noexcept {
  return {
      .posX = entry.posX,
      .posY = entry.posY,
      .radius = entry.radius,
      .occluderHeight = entry.occluderHeight,
      .shapeIndex = entry.shapeIndex,
      .dynamicFlag = entry.dynamicFlag,
      .reserved0 = 0u,
      .reserved1 = 0u,
  };
}

void OccluderCollector::RemoveFromStaticChunkIndex(
    const uint32_t occluderId, const ShadowChunkCoord &chunk) {
  const auto it = m_staticChunkIndex.find(chunk);
  if (it == m_staticChunkIndex.end()) {
    return;
  }
  auto &ids = it->second;
  ids.erase(std::remove(ids.begin(), ids.end(), occluderId), ids.end());
  if (ids.empty()) {
    m_staticChunkIndex.erase(it);
  }
}

void OccluderCollector::AddToStaticChunkIndex(
    const uint32_t occluderId, const ShadowChunkCoord &chunk) {
  auto &ids = m_staticChunkIndex[chunk];
  if (std::find(ids.begin(), ids.end(), occluderId) == ids.end()) {
    ids.push_back(occluderId);
  }
}

uint32_t OccluderCollector::AppendWithLimit(
    const components::GPUShadowCaster &caster, const uint32_t limit,
    bool &truncated) {
  if (m_stagingCasters.size() >= limit) {
    truncated = true;
    return 0u;
  }
  m_stagingCasters.push_back(caster);
  return 1u;
}

uint32_t OccluderCollector::AppendRangeWithLimit(
    const std::vector<components::GPUShadowCaster> &casters, const uint32_t limit,
    bool &truncated) {
  uint32_t appended = 0u;
  for (const auto &caster : casters) {
    appended += AppendWithLimit(caster, limit, truncated);
  }
  return appended;
}

uint32_t OccluderCollector::EvictColdChunksIfNeeded() noexcept {
  uint32_t evicted = 0u;
  while (m_cachedStaticChunks.size() > m_config.maxCachedStaticChunks) {
    const auto candidate = SelectEvictionCandidate();
    if (!candidate.has_value()) {
      break;
    }
    m_cachedStaticChunks.erase(*candidate);
    ++evicted;
  }
  return evicted;
}

std::optional<ShadowChunkCoord>
OccluderCollector::SelectEvictionCandidate() const noexcept {
  std::optional<ShadowChunkCoord> candidate;
  uint32_t oldestFrame = std::numeric_limits<uint32_t>::max();

  for (const auto &[coord, chunk] : m_cachedStaticChunks) {
    if (!candidate.has_value() || chunk.lastTouchedFrame < oldestFrame ||
        (chunk.lastTouchedFrame == oldestFrame &&
         IsDeterministicallyLess(coord, *candidate))) {
      candidate = coord;
      oldestFrame = chunk.lastTouchedFrame;
    }
  }
  return candidate;
}

} // namespace NoMoreDay::render::shadow
