#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace NoMoreDay::render::debug {

enum class SlotState : uint8_t {
  Free = 0,
  Pending,
  Ready,
  Discarded,
};

constexpr const char *ToSlotStateName(SlotState state) {
  switch (state) {
  case SlotState::Free: return "Free";
  case SlotState::Pending: return "Pending";
  case SlotState::Ready: return "Ready";
  case SlotState::Discarded: return "Discarded";
  default: return "Unknown";
  }
}

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
  static constexpr uint32_t kFramePassId = 0xFFFFFFFFu;

  static GPUTimerQueryRing &Get();

  void Initialize();
  void Shutdown();

  void BeginFrame();
  void EndFrame();

  void BeginPass(uint32_t passId);
  void EndPass(uint32_t passId);

  GPUTimerResult GetPassResult(uint32_t passId) const;
  GPUTimerResult GetFrameResult() const { return m_latestFrameResult; }
  double GetValidFrameP95Ms() const;
  size_t GetValidFrameSampleCount() const { return m_frameHistoryCount; }
  bool IsGpuTimeValid(uint32_t passId) const;
  double GetValidGpuTimeMs(uint32_t passId) const;

  void PollReadyQueries();

  // True when all GL timer query entry points were resolved at Initialize().
  // Drives the CpuFallback path in the four-state model (no GPU timers).
  bool IsGpuTimerSupported() const;

  // Test hooks: allow tests to drive/observe the internal frame counter.
  void DebugSetFrameIndex(uint64_t frameIndex);
  uint64_t DebugGetFrameIndex() const { return m_frameIndex; }
  // Test hooks (S1b): inject a per-pass ready result as if PollReadyQueries had
  // observed a ready GL query, and force the timer-capability flag. Shutdown()
  // clears both overrides.
  void DebugInjectPassResult(uint32_t passId, const GPUTimerResult &result);
  void DebugSetGpuTimerSupported(bool supported);
  // Test hooks (P0-6): observe / manipulate slot state.
  SlotState DebugGetSlotState(size_t ringIndex, uint32_t passId) const;
  void DebugSetSlotState(size_t ringIndex, uint32_t passId, SlotState state);

private:
  GPUTimerQueryRing() = default;
  ~GPUTimerQueryRing() = default;

  struct QuerySlot {
    uint32_t queryBegin = 0;
    uint32_t queryEnd = 0;
    uint32_t passId = 0;
    uint64_t frameIndex = 0;
    SlotState state = SlotState::Free;
    bool active = false;
    bool touchedThisFrame = false;
    bool resultReady = false;
    bool resultValid = false;
    double cpuStartTimeMs = 0.0;
    double cpuDurationMs = 0.0;
    double gpuDurationMs = 0.0;
  };

  struct FrameRingSlot {
    uint64_t frameIndex = 0;
    std::map<uint32_t, QuerySlot> slots;
    bool isComplete = false;
    bool aggregatePublished = false;
  };

  FrameRingSlot m_ring[kRingDepth];
  size_t m_currentRingIndex = 0;
  uint64_t m_frameIndex = 0;
  std::map<uint32_t, GPUTimerResult> m_latestValidResults;
  GPUTimerResult m_latestFrameResult = {};
  std::array<GPUTimerResult, 120> m_frameHistory = {};
  size_t m_frameHistoryCount = 0;
  size_t m_frameHistoryWriteIndex = 0;
  bool m_gpuTimerOverrideActive = false;
  bool m_gpuTimerOverrideValue = false;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render::debug
