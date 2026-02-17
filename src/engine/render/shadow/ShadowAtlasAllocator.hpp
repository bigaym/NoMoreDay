#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render::shadow {

struct ShadowTileRequest {
  uint32_t lightId = 0;
  float priorityScore = 0.0f;
};

struct ShadowTileAllocation {
  bool success = false;
  bool reusedExisting = false;
  bool evicted = false;
  uint32_t tileIndex = 0;
  uint32_t evictedLightId = 0;
};

class ShadowAtlasAllocator final {
public:
  explicit ShadowAtlasAllocator(
      uint32_t tileCount,
      uint32_t evictionHysteresisFrames = 0);

  void BeginFrame(uint32_t frameIndex) noexcept;
  [[nodiscard]] ShadowTileAllocation
  AcquireTile(const ShadowTileRequest &request);
  [[nodiscard]] bool ReleaseTile(uint32_t lightId) noexcept;
  void Clear() noexcept;

  [[nodiscard]] uint32_t GetTileCount() const noexcept { return m_tileCount; }
  [[nodiscard]] uint32_t GetAllocatedTileCount() const noexcept;

private:
  struct TileRecord {
    bool occupied = false;
    uint32_t lightId = 0;
    float priorityScore = 0.0f;
    uint32_t lastUsedFrame = 0;
  };

  [[nodiscard]] std::optional<uint32_t> FindFreeTile() const noexcept;
  [[nodiscard]] std::optional<uint32_t>
  SelectEvictionCandidate() const noexcept;
  void AssignTile(uint32_t tileIndex, const ShadowTileRequest &request) noexcept;
  void ResetPendingEvictionCountersExcept(uint32_t tileIndex) noexcept;

  uint32_t m_tileCount = 0;
  uint32_t m_currentFrame = 0;
  uint32_t m_evictionHysteresisFrames = 0;
  std::vector<TileRecord> m_tiles;
  std::vector<uint32_t> m_pendingEvictionCounters;
  std::unordered_map<uint32_t, uint32_t> m_lightToTile;
};

} // namespace NoMoreDay::render::shadow
