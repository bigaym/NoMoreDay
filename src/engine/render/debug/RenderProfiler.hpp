#pragma once

#include "engine/render/debug/GPUTimerQueryRing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

namespace NoMoreDay::render::debug {

enum class RenderPassId : uint8_t {
  Scene = 0,
  Lighting = 1,
  HeightShadow = 2,
  OccluderExtract = 3,
  JFA = 4,
  RadianceCascades = 5,
  GIComposite = 6,
  FluidSimulation = 7,
  Volumetric = 8,
  VFX = 9,
  GPUText = 10,
  GPULoot = 11,
  UIWorld = 12,
  PostProcess = 13,
  Distortion = 14,
  Composite = 15,
  Count
};

struct PassTimingSample {
  float cpuMs = 0.0f;
  float gpuMs = 0.0f;
};

struct PassTimingStats {
  float cpuMeanMs = 0.0f;
  float cpuP95Ms = 0.0f;
  float gpuMeanMs = 0.0f;
  float gpuP95Ms = 0.0f;
  float budgetMs = 0.0f;
  // Four-state GPU telemetry (S1b): Pending (query issued, not ready yet ->
  // carry last-frame value), Valid (ready result accepted), Unavailable
  // (mapping failure / no sample / overage), CpuFallback (no GPU timers).
  QueryState gpuState = QueryState::Unavailable;
  // Source frame index of the reported GPU data (0 = none backfilled yet).
  uint64_t frameIndex = 0;
};

class RenderProfiler {
public:
  static constexpr int kWindowSize = 120;

  RenderProfiler();
  ~RenderProfiler();

  void BeginFrame();
  void EndFrame();
  void BeginPass(RenderPassId passId);
  void EndPass(RenderPassId passId);
  void BeginCpuPass(const char *passName);
  void EndCpuPass();

  [[nodiscard]] PassTimingStats GetStats(RenderPassId passId) const;
  [[nodiscard]] const std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)> &
  GetAllStats() const {
    return m_cachedStats;
  }
  // S1b: single Poll call point of the render path. Polls GPUTimerQueryRing,
  // backfills each pass's GPU result under the frame-acceptance rule, then
  // advances the four-state model. Must be called at the end of
  // RenderSystem::render (after graph execute, before UpdateStats) and BEFORE
  // any DRS/adaptive-policy read.
  void FlushRingToProfiler();
  void UpdateStats();
  // Returns the backfilled four-state stats for a stable pass ID. Passes the
  // profiler does not track (mapping failure) return default stats marked
  // Unavailable.
  [[nodiscard]] PassTimingStats GetPassResult(uint32_t stablePassId) const;
  [[nodiscard]] bool IsGpuTimingAvailable() const;

  static const char *ToString(RenderPassId passId);
  static std::optional<RenderPassId> FromPassName(std::string_view passName);
  static float GetBudgetMs(RenderPassId passId);

private:
  using Clock = std::chrono::high_resolution_clock;

  static constexpr uint32_t kPendingOverageFrames = 6;

  struct PassState {
    std::array<PassTimingSample, kWindowSize> samples = {};
    int sampleCount = 0;
    int writeIndex = 0;
    Clock::time_point cpuStart = {};
    bool cpuRunning = false;
  };

  // Per-pass GPU backfill state (S1b). Samples hold accepted ready results;
  // mean/P95 are aggregated over the window only while the state participates
  // in GPU aggregation (Valid, or Pending carrying the last-frame value).
  struct GpuTrack {
    std::array<float, kWindowSize> samples = {};
    int sampleCount = 0;
    int writeIndex = 0;
    QueryState state = QueryState::Pending;
    uint64_t lastAcceptedFrameIndex = 0;
  };

  static const char *FullPassName(RenderPassId passId);

  std::array<PassState, static_cast<size_t>(RenderPassId::Count)> m_passStates = {};
  std::array<GpuTrack, static_cast<size_t>(RenderPassId::Count)> m_gpuTracks = {};
  std::array<uint32_t, static_cast<size_t>(RenderPassId::Count)> m_stablePassIdByIndex = {};
  // Reverse lookup for GetPassResult(stablePassId); built once in the ctor.
  std::map<uint32_t, RenderPassId> m_passByStableId;
  std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)> m_cachedStats = {};
  std::optional<RenderPassId> m_activeCpuPass = std::nullopt;
  bool m_frameActive = false;
};

void DrawProfilerHud(const RenderProfiler &profiler, float x, float y);

} // namespace NoMoreDay::render::debug
