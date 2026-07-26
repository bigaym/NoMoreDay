#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace NoMoreDay::render::debug {

enum class QueryState : uint8_t {
  Pending = 0,
  Valid,
  Unavailable,
  CpuFallback,
};

constexpr const char *ToQueryStateName(QueryState state) {
  switch (state) {
  case QueryState::Pending: return "Pending";
  case QueryState::Valid: return "Valid";
  case QueryState::Unavailable: return "Unavailable";
  case QueryState::CpuFallback: return "CpuFallback";
  default: return "Unknown";
  }
}

struct GPUTimerResult {
  double gpuTimeMs = 0.0;
  double cpuTimeMs = 0.0;
  QueryState state = QueryState::Pending;
  uint64_t frameIndex = 0;
};

class GPUTimerQueryRing {
public:
  static constexpr size_t kRingDepth = 3;

  static GPUTimerQueryRing &Get();

  void Initialize();
  void Shutdown();

  void BeginFrame();
  void EndFrame();

  void BeginPass(uint32_t passId);
  void EndPass(uint32_t passId);

  GPUTimerResult GetPassResult(uint32_t passId) const;
  bool IsGpuTimeValid(uint32_t passId) const;
  double GetValidGpuTimeMs(uint32_t passId) const;

  void PollReadyQueries();

private:
  GPUTimerQueryRing() = default;
  ~GPUTimerQueryRing() = default;

  struct QuerySlot {
    uint32_t queryBegin = 0;
    uint32_t queryEnd = 0;
    uint32_t passId = 0;
    uint64_t frameIndex = 0;
    bool active = false;
    double cpuStartTimeMs = 0.0;
    double cpuDurationMs = 0.0;
  };

  struct FrameRingSlot {
    uint64_t frameIndex = 0;
    std::map<uint32_t, QuerySlot> slots;
    bool isComplete = false;
  };

  FrameRingSlot m_ring[kRingDepth];
  size_t m_currentRingIndex = 0;
  uint64_t m_frameIndex = 0;
  std::map<uint32_t, GPUTimerResult> m_latestValidResults;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::debug
