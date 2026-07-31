#pragma once

#include "engine/render/debug/GPUTimerQueryRing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
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
  // S1a transitional state: profiler no longer owns GL timer queries, so the
  // GPU mean/P95 fields are always marked Unavailable until S1b backfills them.
  QueryState gpuState = QueryState::Unavailable;
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
  void UpdateStats();
  [[nodiscard]] bool IsGpuTimingAvailable() const { return false; }

  static const char *ToString(RenderPassId passId);
  static std::optional<RenderPassId> FromPassName(std::string_view passName);
  static float GetBudgetMs(RenderPassId passId);

private:
  using Clock = std::chrono::high_resolution_clock;

  struct PassState {
    std::array<PassTimingSample, kWindowSize> samples = {};
    int sampleCount = 0;
    int writeIndex = 0;
    Clock::time_point cpuStart = {};
    bool cpuRunning = false;
  };

  std::array<PassState, static_cast<size_t>(RenderPassId::Count)> m_passStates = {};
  std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)> m_cachedStats = {};
  std::optional<RenderPassId> m_activeCpuPass = std::nullopt;
  bool m_frameActive = false;
};

void DrawProfilerHud(const RenderProfiler &profiler, float x, float y);

} // namespace NoMoreDay::render::debug
