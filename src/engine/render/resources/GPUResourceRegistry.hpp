#pragma once

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/graph/RenderResourceDescriptor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render::resources {

struct GPUResourceRecord {
  uint32_t handle = 0;
  graph::ResourceKind kind = graph::ResourceKind::Texture2D;
  graph::RenderOwnerTag ownerTag = graph::RenderOwnerTag::Unknown;
  size_t sizeBytes = 0;
  std::string name;
  uint64_t creationFrame = 0;
};

struct GPUResourceStats {
  size_t currentTotalBytes = 0;
  size_t peakTotalBytes = 0;
  size_t activeCount = 0;
  size_t totalCreatedCount = 0;
  size_t totalDestroyedCount = 0;

  std::unordered_map<uint8_t, size_t> bytesByKind;
  std::unordered_map<uint8_t, size_t> bytesByOwner;
};

// S4 (M0-C R5.2): point-in-time quiescence sample of the resource registry.
// Fields: resource object count, byte count, lifecycle counters, reference
// state and timestamps (registry frame index + monotonic wall clock ms since
// the first snapshot of the current epoch).
struct GPUResourceSnapshot {
  uint64_t frameIndex = 0;
  uint64_t wallClockMs = 0;
  size_t activeResourceCount = 0;
  size_t currentTotalBytes = 0;
  size_t peakTotalBytes = 0;
  size_t totalCreatedCount = 0;
  size_t totalDestroyedCount = 0;
  size_t liveReferenceCount = 0;
  size_t pendingReferenceCount = 0;
};

class GPUResourceRegistry {
public:
  static GPUResourceRegistry &Get();

  void RegisterResource(uint32_t handle, graph::ResourceKind kind,
                        graph::RenderOwnerTag ownerTag, size_t sizeBytes,
                        std::string_view name);
  void UnregisterResource(uint32_t handle, graph::ResourceKind kind);
  void UpdateResourceSize(uint32_t handle, graph::ResourceKind kind, size_t newSizeBytes);
  void AdvanceFrame();
  void Reset();

  GPUResourceStats GetStats() const;
  std::vector<GPUResourceRecord> GetActiveResources() const;
  std::vector<GPUResourceRecord> DetectLeakCandidates(uint64_t ageInFramesThreshold = 1000) const;
  std::string GenerateReportJson() const;

  GPUResourceSnapshot TakeSnapshot();
  uint64_t GetFrameIndex() const;

private:
  GPUResourceRegistry() = default;
  ~GPUResourceRegistry() = default;

  mutable std::mutex m_mutex;
  std::unordered_map<uint64_t, GPUResourceRecord> m_records;
  GPUResourceStats m_stats;
  uint64_t m_currentFrame = 0;
  std::chrono::steady_clock::time_point m_snapshotEpoch{};
  bool m_snapshotEpochSet = false;

  static uint64_t MakeKey(uint32_t handle, graph::ResourceKind kind) {
    return (static_cast<uint64_t>(kind) << 32u) | static_cast<uint64_t>(handle);
  }
};

} // namespace NoMoreDay::render::resources
