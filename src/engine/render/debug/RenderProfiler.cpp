#include "engine/render/debug/RenderProfiler.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <vector>

namespace NoMoreDay::render::debug {
namespace {

constexpr size_t ToIndex(RenderPassId passId) {
  return static_cast<size_t>(passId);
}

constexpr uint32_t kGLTimeElapsed = 0x88BF;
constexpr uint32_t kGLQueryResult = 0x8866;
constexpr uint32_t kGLQueryResultAvailable = 0x8867;

float ComputeMean(const std::vector<float> &values) {
  if (values.empty()) {
    return 0.0f;
  }
  float sum = 0.0f;
  for (float value : values) {
    sum += value;
  }
  return sum / static_cast<float>(values.size());
}

float ComputeP95(std::vector<float> values) {
  if (values.empty()) {
    return 0.0f;
  }
  std::sort(values.begin(), values.end());
  const size_t idx = static_cast<size_t>(
      std::clamp(static_cast<int>(values.size() * 95 / 100), 0,
                 static_cast<int>(values.size() - 1)));
  return values[idx];
}

} // namespace

RenderProfiler::RenderProfiler() {
  m_gpuTimingAvailable = false;
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    m_gpuApi.genQueries = reinterpret_cast<GpuTimerQueryApi::GenQueriesFn>(
        glfwGetProcAddress("glGenQueries"));
    m_gpuApi.beginQuery = reinterpret_cast<GpuTimerQueryApi::BeginQueryFn>(
        glfwGetProcAddress("glBeginQuery"));
    m_gpuApi.endQuery = reinterpret_cast<GpuTimerQueryApi::EndQueryFn>(
        glfwGetProcAddress("glEndQuery"));
    m_gpuApi.getQueryObjectUi64v =
        reinterpret_cast<GpuTimerQueryApi::GetQueryObjectUi64vFn>(
            glfwGetProcAddress("glGetQueryObjectui64v"));
    m_gpuApi.deleteQueries = reinterpret_cast<GpuTimerQueryApi::DeleteQueriesFn>(
        glfwGetProcAddress("glDeleteQueries"));
    m_gpuTimingAvailable = m_gpuApi.IsAvailable();
  }

  if (m_gpuTimingAvailable) {
    LOG_INFO("RenderProfiler: GPU timer query path enabled (non-blocking poll)");
  } else if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    LOG_WARN("RenderProfiler: GPU timer query unavailable, fallback to CPU-only mode");
  }
}

RenderProfiler::~RenderProfiler() {
  if (!m_gpuTimingAvailable || !m_gpuApi.IsAvailable()) {
    return;
  }
  for (PassState &state : m_passStates) {
    if (state.gpuQueryId != 0) {
      uint32_t queryId = state.gpuQueryId;
      m_gpuApi.deleteQueries(1, &queryId);
      state.gpuQueryId = 0;
    }
  }
}

void RenderProfiler::BeginFrame() { m_frameActive = true; }

void RenderProfiler::EndFrame() { m_frameActive = false; }

void RenderProfiler::BeginPass(RenderPassId passId) {
  if (!m_frameActive) {
    return;
  }

  PassState &state = m_passStates[ToIndex(passId)];
  state.cpuStart = Clock::now();
  state.cpuRunning = true;

  state.gpuRunning = false;
  if (!m_gpuTimingAvailable || !m_gpuApi.IsAvailable()) {
    return;
  }

  if (state.gpuQueryId == 0) {
    uint32_t queryId = 0;
    m_gpuApi.genQueries(1, &queryId);
    state.gpuQueryId = queryId;
    if (state.gpuQueryId == 0) {
      m_gpuTimingAvailable = false;
      LOG_WARN("RenderProfiler: query allocation failed, fallback to CPU-only mode");
      return;
    }
  }

  m_gpuApi.beginQuery(kGLTimeElapsed, state.gpuQueryId);
  state.gpuRunning = true;
}

void RenderProfiler::EndPass(RenderPassId passId) {
  if (!m_frameActive) {
    return;
  }

  PassState &state = m_passStates[ToIndex(passId)];
  if (!state.cpuRunning) {
    return;
  }

  const auto cpuEnd = Clock::now();
  const float cpuMs =
      std::chrono::duration<float, std::milli>(cpuEnd - state.cpuStart).count();
  state.cpuRunning = false;

  float gpuMs = 0.0f;
  if (m_gpuTimingAvailable && m_gpuApi.IsAvailable() && state.gpuRunning) {
    m_gpuApi.endQuery(kGLTimeElapsed);
    state.gpuRunning = false;

    // Do not block the render thread waiting for GPU completion.
    uint64_t queryAvailable = 0;
    m_gpuApi.getQueryObjectUi64v(state.gpuQueryId, kGLQueryResultAvailable,
                                 &queryAvailable);
    if (queryAvailable != 0) {
      uint64_t elapsedNs = 0;
      m_gpuApi.getQueryObjectUi64v(state.gpuQueryId, kGLQueryResult, &elapsedNs);
      gpuMs = static_cast<float>(elapsedNs) * 1.0e-6f;
    }
  }

  PassTimingSample &sample = state.samples[static_cast<size_t>(state.writeIndex)];
  sample.cpuMs = cpuMs;
  sample.gpuMs = gpuMs;
  state.writeIndex = (state.writeIndex + 1) % kWindowSize;
  if (state.sampleCount < kWindowSize) {
    ++state.sampleCount;
  }
}

PassTimingStats RenderProfiler::GetStats(RenderPassId passId) const {
  const PassState &state = m_passStates[ToIndex(passId)];
  PassTimingStats stats = {};
  stats.budgetMs = GetBudgetMs(passId);
  if (state.sampleCount <= 0) {
    return stats;
  }

  std::vector<float> cpuValues;
  std::vector<float> gpuValues;
  cpuValues.reserve(static_cast<size_t>(state.sampleCount));
  gpuValues.reserve(static_cast<size_t>(state.sampleCount));
  for (int i = 0; i < state.sampleCount; ++i) {
    cpuValues.push_back(state.samples[static_cast<size_t>(i)].cpuMs);
    if (state.samples[static_cast<size_t>(i)].gpuMs > 0.0f) {
      gpuValues.push_back(state.samples[static_cast<size_t>(i)].gpuMs);
    }
  }

  stats.cpuMeanMs = ComputeMean(cpuValues);
  stats.cpuP95Ms = ComputeP95(cpuValues);
  stats.gpuMeanMs = ComputeMean(gpuValues);
  stats.gpuP95Ms = ComputeP95(gpuValues);
  return stats;
}

std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)>
RenderProfiler::GetAllStats() const {
  std::array<PassTimingStats, static_cast<size_t>(RenderPassId::Count)> stats = {};
  for (size_t i = 0; i < stats.size(); ++i) {
    stats[i] = GetStats(static_cast<RenderPassId>(i));
  }
  return stats;
}

const char *RenderProfiler::ToString(RenderPassId passId) {
  switch (passId) {
  case RenderPassId::Scene:
    return "Scene";
  case RenderPassId::Lighting:
    return "Lighting";
  case RenderPassId::HeightShadow:
    return "HeightShadow";
  case RenderPassId::OccluderExtract:
    return "OccluderExtract";
  case RenderPassId::JFA:
    return "JFA";
  case RenderPassId::RadianceCascades:
    return "RadianceCascades";
  case RenderPassId::GIComposite:
    return "GIComposite";
  case RenderPassId::FluidSimulation:
    return "FluidSimulation";
  case RenderPassId::Volumetric:
    return "Volumetric";
  case RenderPassId::VFX:
    return "VFX";
  case RenderPassId::GPUText:
    return "GPUText";
  case RenderPassId::GPULoot:
    return "GPULoot";
  case RenderPassId::UIWorld:
    return "UIWorld";
  case RenderPassId::PostProcess:
    return "PostProcess";
  case RenderPassId::Distortion:
    return "Distortion";
  case RenderPassId::Composite:
    return "Composite";
  case RenderPassId::Count:
    break;
  }
  return "Unknown";
}

std::optional<RenderPassId> RenderProfiler::FromPassName(std::string_view passName) {
  if (passName == "ScenePass") {
    return RenderPassId::Scene;
  }
  if (passName == "LightingPass") {
    return RenderPassId::Lighting;
  }
  if (passName == "HeightShadowPass") {
    return RenderPassId::HeightShadow;
  }
  if (passName == "OccluderExtractPass") {
    return RenderPassId::OccluderExtract;
  }
  if (passName == "JFAPass") {
    return RenderPassId::JFA;
  }
  if (passName == "RadianceCascadesPass") {
    return RenderPassId::RadianceCascades;
  }
  if (passName == "GICompositePass") {
    return RenderPassId::GIComposite;
  }
  if (passName == "FluidSimulationPass") {
    return RenderPassId::FluidSimulation;
  }
  if (passName == "VolumetricLightPass") {
    return RenderPassId::Volumetric;
  }
  if (passName == "VFXPass") {
    return RenderPassId::VFX;
  }
  if (passName == "GPUTextPass") {
    return RenderPassId::GPUText;
  }
  if (passName == "GPULootPass") {
    return RenderPassId::GPULoot;
  }
  if (passName == "UIWorldPass") {
    return RenderPassId::UIWorld;
  }
  if (passName == "PostProcessPass") {
    return RenderPassId::PostProcess;
  }
  if (passName == "DistortionPass") {
    return RenderPassId::Distortion;
  }
  if (passName == "CompositePass") {
    return RenderPassId::Composite;
  }
  return std::nullopt;
}

float RenderProfiler::GetBudgetMs(RenderPassId passId) {
  switch (passId) {
  case RenderPassId::Scene:
    return 1.20f;
  case RenderPassId::Lighting:
    return 0.80f;
  case RenderPassId::HeightShadow:
    return 0.90f;
  case RenderPassId::OccluderExtract:
    return 0.10f;
  case RenderPassId::JFA:
    return 0.40f;
  case RenderPassId::RadianceCascades:
    return 1.20f;
  case RenderPassId::GIComposite:
    return 0.05f;
  case RenderPassId::FluidSimulation:
    return 0.30f;
  case RenderPassId::Volumetric:
    return 0.80f;
  case RenderPassId::VFX:
    return 1.00f;
  case RenderPassId::GPUText:
    return 0.15f;
  case RenderPassId::GPULoot:
    return 0.20f;
  case RenderPassId::UIWorld:
    return 0.60f;
  case RenderPassId::PostProcess:
    return 0.50f;
  case RenderPassId::Distortion:
    return 0.30f;
  case RenderPassId::Composite:
    return 0.25f;
  case RenderPassId::Count:
    break;
  }
  return 0.0f;
}

} // namespace NoMoreDay::render::debug
