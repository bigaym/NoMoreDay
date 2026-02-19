#pragma once

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
  Volumetric = 3,
  VFX = 4,
  GPUText = 5,
  GPULoot = 6,
  UIWorld = 7,
  PostProcess = 8,
  Distortion = 9,
  Composite = 10,
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

  [[nodiscard]] PassTimingStats GetStats(RenderPassId passId) const;
  [[nodiscard]] std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)>
  GetAllStats() const;
  [[nodiscard]] bool IsGpuTimingAvailable() const { return m_gpuTimingAvailable; }

  static const char *ToString(RenderPassId passId);
  static std::optional<RenderPassId> FromPassName(std::string_view passName);
  static float GetBudgetMs(RenderPassId passId);

private:
  using Clock = std::chrono::high_resolution_clock;
  struct GpuTimerQueryApi {
    using GenQueriesFn = void (*)(int, uint32_t *);
    using BeginQueryFn = void (*)(uint32_t, uint32_t);
    using EndQueryFn = void (*)(uint32_t);
    using GetQueryObjectUi64vFn = void (*)(uint32_t, uint32_t, uint64_t *);
    using DeleteQueriesFn = void (*)(int, const uint32_t *);

    GenQueriesFn genQueries = nullptr;
    BeginQueryFn beginQuery = nullptr;
    EndQueryFn endQuery = nullptr;
    GetQueryObjectUi64vFn getQueryObjectUi64v = nullptr;
    DeleteQueriesFn deleteQueries = nullptr;

    [[nodiscard]] bool IsAvailable() const {
      return genQueries != nullptr && beginQuery != nullptr &&
             endQuery != nullptr && getQueryObjectUi64v != nullptr &&
             deleteQueries != nullptr;
    }
  };

  struct PassState {
    std::array<PassTimingSample, kWindowSize> samples = {};
    int sampleCount = 0;
    int writeIndex = 0;
    Clock::time_point cpuStart = {};
    bool cpuRunning = false;
    uint32_t gpuQueryId = 0;
    bool gpuRunning = false;
  };

  std::array<PassState, static_cast<size_t>(RenderPassId::Count)> m_passStates = {};
  GpuTimerQueryApi m_gpuApi = {};
  bool m_gpuTimingAvailable = false;
  bool m_frameActive = false;
};

void DrawProfilerHud(const RenderProfiler &profiler, float x, float y);

} // namespace NoMoreDay::render::debug
