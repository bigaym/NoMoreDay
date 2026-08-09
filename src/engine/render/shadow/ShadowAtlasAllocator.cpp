#include "engine/render/shadow/ShadowAtlasAllocator.hpp"

#include <algorithm>

namespace NoMoreDay::render::shadow {

ShadowAtlasAllocator::ShadowAtlasAllocator(
    const uint32_t tileCount,
    const uint32_t evictionHysteresisFrames)
    : m_tileCount(tileCount),
      m_evictionHysteresisFrames(evictionHysteresisFrames),
      m_tiles(tileCount),
      m_pendingEvictionCounters(tileCount, 0u) {}

void ShadowAtlasAllocator::BeginFrame(const uint32_t frameIndex) noexcept {
  m_currentFrame = frameIndex;
}

ShadowTileAllocation
ShadowAtlasAllocator::AcquireTile(const ShadowTileRequest &request) {
  if (m_tileCount == 0 || request.lightId == 0u) {
    return {};
  }

  if (const auto existing = m_lightToTile.find(request.lightId);
      existing != m_lightToTile.end()) {
    AssignTile(existing->second, request);
    return {
        .success = true,
        .reusedExisting = true,
        .evicted = false,
        .tileIndex = existing->second,
        .evictedLightId = 0u,
    };
  }

  if (const auto freeTile = FindFreeTile(); freeTile.has_value()) {
    AssignTile(*freeTile, request);
    return {
        .success = true,
        .reusedExisting = false,
        .evicted = false,
        .tileIndex = *freeTile,
        .evictedLightId = 0u,
    };
  }

  const std::optional<uint32_t> candidate = SelectEvictionCandidate();
  if (!candidate.has_value()) {
    return {};
  }

  const uint32_t tileIndex = *candidate;
  const TileRecord &victim = m_tiles[tileIndex];
  if (!victim.occupied || request.priorityScore < victim.priorityScore) {
    return {};
  }

  ResetPendingEvictionCountersExcept(tileIndex);
  const bool staleVictim = m_currentFrame - victim.lastUsedFrame >
                           kStaleTileEvictionFrames;
  if (!staleVictim) {
    uint32_t &counter = m_pendingEvictionCounters[tileIndex];
    ++counter;
    if (counter <= m_evictionHysteresisFrames) {
      return {};
    }
    counter = 0u;
  }
  const uint32_t evictedLightId = victim.lightId;
  (void)m_lightToTile.erase(evictedLightId);
  AssignTile(tileIndex, request);
  return {
      .success = true,
      .reusedExisting = false,
      .evicted = true,
      .tileIndex = tileIndex,
      .evictedLightId = evictedLightId,
  };
}

bool ShadowAtlasAllocator::ReleaseTile(const uint32_t lightId) noexcept {
  if (const auto it = m_lightToTile.find(lightId); it != m_lightToTile.end()) {
    const uint32_t tileIndex = it->second;
    if (tileIndex < m_tiles.size()) {
      m_tiles[tileIndex] = {};
      m_pendingEvictionCounters[tileIndex] = 0u;
    }
    m_lightToTile.erase(it);
    return true;
  }
  return false;
}

void ShadowAtlasAllocator::SweepStaleTiles(
    const uint32_t currentFrame,
    const uint32_t retentionFrames) noexcept {
  if (retentionFrames == 0u) {
    return;
  }
  for (uint32_t tileIndex = 0; tileIndex < m_tiles.size(); ++tileIndex) {
    const TileRecord &record = m_tiles[tileIndex];
    if (!record.occupied) {
      continue;
    }
    if (currentFrame - record.lastUsedFrame >= retentionFrames) {
      (void)m_lightToTile.erase(record.lightId);
      m_tiles[tileIndex] = {};
      m_pendingEvictionCounters[tileIndex] = 0u;
    }
  }
}

void ShadowAtlasAllocator::Clear() noexcept {
  m_lightToTile.clear();
  std::fill(m_tiles.begin(), m_tiles.end(), TileRecord{});
  std::fill(m_pendingEvictionCounters.begin(), m_pendingEvictionCounters.end(),
            0u);
}

uint32_t ShadowAtlasAllocator::GetAllocatedTileCount() const noexcept {
  return static_cast<uint32_t>(m_lightToTile.size());
}

std::optional<uint32_t> ShadowAtlasAllocator::FindFreeTile() const noexcept {
  for (uint32_t tileIndex = 0; tileIndex < m_tiles.size(); ++tileIndex) {
    if (!m_tiles[tileIndex].occupied) {
      return tileIndex;
    }
  }
  return std::nullopt;
}

std::optional<uint32_t> ShadowAtlasAllocator::SelectEvictionCandidate() const noexcept {
  std::optional<uint32_t> best;

  for (uint32_t tileIndex = 0; tileIndex < m_tiles.size(); ++tileIndex) {
    const TileRecord &record = m_tiles[tileIndex];
    if (!record.occupied) {
      continue;
    }
    if (!best.has_value()) {
      best = tileIndex;
      continue;
    }

    const TileRecord &bestRecord = m_tiles[*best];
    if (record.priorityScore < bestRecord.priorityScore ||
        (record.priorityScore == bestRecord.priorityScore &&
         record.lastUsedFrame < bestRecord.lastUsedFrame) ||
        (record.priorityScore == bestRecord.priorityScore &&
         record.lastUsedFrame == bestRecord.lastUsedFrame &&
         record.lightId < bestRecord.lightId)) {
      best = tileIndex;
    }
  }

  return best;
}

void ShadowAtlasAllocator::AssignTile(
    const uint32_t tileIndex,
    const ShadowTileRequest &request) noexcept {
  TileRecord &record = m_tiles[tileIndex];
  record.occupied = true;
  record.lightId = request.lightId;
  record.priorityScore = request.priorityScore;
  record.lastUsedFrame = m_currentFrame;
  m_lightToTile[request.lightId] = tileIndex;
}

void ShadowAtlasAllocator::ResetPendingEvictionCountersExcept(
    const uint32_t tileIndex) noexcept {
  for (uint32_t i = 0; i < m_pendingEvictionCounters.size(); ++i) {
    if (i == tileIndex) {
      continue;
    }
    m_pendingEvictionCounters[i] = 0u;
  }
}

} // namespace NoMoreDay::render::shadow
