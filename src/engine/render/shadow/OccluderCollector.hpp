#pragma once

#include "engine/render/GPUData.hpp"
#include "engine/render/RenderConstants.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render::shadow {

struct ShadowChunkCoord {
  int32_t x = 0;
  int32_t y = 0;

  [[nodiscard]] bool operator==(const ShadowChunkCoord &rhs) const noexcept {
    return x == rhs.x && y == rhs.y;
  }
};

struct ShadowChunkCoordHash {
  [[nodiscard]] std::size_t operator()(
      const ShadowChunkCoord &coord) const noexcept;
};

struct OccluderEntry {
  uint32_t occluderId = 0;
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 0.0f;
  float occluderHeight = 0.0f;
  uint32_t shapeIndex = 0;
  uint32_t dynamicFlag = 0;
};

struct OccluderCollectorConfig {
  float chunkSize = RenderConstants::Shadow::kShadowChunkSize;
  float cameraNeighborhoodRadius =
      RenderConstants::Shadow::kCameraNeighborhoodRadius;
  uint32_t maxShadowCasters = RenderConstants::Shadow::kMaxShadowCasters;
  uint32_t maxCachedStaticChunks = 256;
};

struct ShadowUploadBatch {
  uint32_t offset = 0;
  uint32_t count = 0;
};

struct OccluderCollectResult {
  uint32_t staticCasterCount = 0;
  uint32_t dynamicCasterCount = 0;
  uint32_t totalCasterCount = 0;
  uint32_t evictedChunkCount = 0;
  bool truncated = false;
};

class OccluderCollector final {
public:
  explicit OccluderCollector(OccluderCollectorConfig config = {});

  void BeginFrame(uint32_t frameIndex) noexcept;
  [[nodiscard]] bool UpsertOccluder(const OccluderEntry &entry);
  [[nodiscard]] bool RemoveOccluder(uint32_t occluderId);
  [[nodiscard]] OccluderCollectResult CollectVisible(
      float cameraWorldX, float cameraWorldY);

  [[nodiscard]] std::vector<ShadowUploadBatch>
  BuildUploadBatches(uint32_t batchSize) const;

  [[nodiscard]] const std::vector<components::GPUShadowCaster> &
  GetStagingCasters() const noexcept {
    return m_stagingCasters;
  }

  [[nodiscard]] uint32_t GetCachedChunkCount() const noexcept;
  [[nodiscard]] bool HasChunkCached(const ShadowChunkCoord &coord) const;
  [[nodiscard]] uint64_t
  GetChunkUploadCount(const ShadowChunkCoord &coord) const noexcept;

  [[nodiscard]] static ShadowChunkCoord
  WorldToChunk(float worldX, float worldY, float chunkSize) noexcept;

private:
  struct CachedChunk {
    std::vector<components::GPUShadowCaster> staticCasters;
    uint32_t lastTouchedFrame = 0;
  };

  [[nodiscard]] components::GPUShadowCaster
  ToGPUShadowCaster(const OccluderEntry &entry) const noexcept;
  void RemoveFromStaticChunkIndex(
      uint32_t occluderId, const ShadowChunkCoord &chunk);
  void AddToStaticChunkIndex(
      uint32_t occluderId, const ShadowChunkCoord &chunk);
  [[nodiscard]] uint32_t AppendWithLimit(
      const components::GPUShadowCaster &caster, uint32_t limit,
      bool &truncated);
  [[nodiscard]] uint32_t AppendRangeWithLimit(
      const std::vector<components::GPUShadowCaster> &casters, uint32_t limit,
      bool &truncated);
  [[nodiscard]] uint32_t EvictColdChunksIfNeeded() noexcept;
  [[nodiscard]] std::optional<ShadowChunkCoord>
  SelectEvictionCandidate() const noexcept;

  OccluderCollectorConfig m_config;
  uint32_t m_frameIndex = 0;

  std::unordered_map<uint32_t, OccluderEntry> m_occluders;
  std::unordered_map<ShadowChunkCoord, std::vector<uint32_t>, ShadowChunkCoordHash>
      m_staticChunkIndex;
  std::unordered_map<ShadowChunkCoord, CachedChunk, ShadowChunkCoordHash>
      m_cachedStaticChunks;
  std::unordered_map<ShadowChunkCoord, uint64_t, ShadowChunkCoordHash>
      m_chunkUploadCounters;
  std::vector<components::GPUShadowCaster> m_stagingCasters;
};

} // namespace NoMoreDay::render::shadow
