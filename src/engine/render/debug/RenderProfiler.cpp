#include "engine/render/debug/RenderProfiler.hpp"

#include "core/logging/Logger.hpp"

#include <algorithm>
#include <vector>

namespace NoMoreDay::render::debug {
namespace {

constexpr size_t ToIndex(RenderPassId passId) {
  return static_cast<size_t>(passId);
}

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
  LOG_WARN("RenderProfiler: GPU timer query path disabled (S1a CPU-only mode; "
           "GPU timings reported as Unavailable until S1b)");
}

RenderProfiler::~RenderProfiler() = default;

void RenderProfiler::BeginFrame() { m_frameActive = true; }

void RenderProfiler::EndFrame() { m_frameActive = false; }

void RenderProfiler::BeginPass(RenderPassId passId) {
  if (!m_frameActive) {
    return;
  }

  PassState &state = m_passStates[ToIndex(passId)];
  state.cpuStart = Clock::now();
  state.cpuRunning = true;
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

  PassTimingSample &sample = state.samples[static_cast<size_t>(state.writeIndex)];
  sample.cpuMs = cpuMs;
  sample.gpuMs = 0.0f;
  state.writeIndex = (state.writeIndex + 1) % kWindowSize;
  if (state.sampleCount < kWindowSize) {
    ++state.sampleCount;
  }
}

void RenderProfiler::BeginCpuPass(const char *passName) {
  if (passName == nullptr) {
    return;
  }
  const auto passId = FromPassName(passName);
  if (!passId.has_value()) {
    return;
  }
  m_activeCpuPass = *passId;
  BeginPass(*passId);
}

void RenderProfiler::EndCpuPass() {
  if (!m_activeCpuPass.has_value()) {
    return;
  }
  EndPass(*m_activeCpuPass);
  m_activeCpuPass = std::nullopt;
}

PassTimingStats RenderProfiler::GetStats(RenderPassId passId) const {
  const PassState &state = m_passStates[ToIndex(passId)];
  PassTimingStats stats = {};
  stats.budgetMs = GetBudgetMs(passId);
  stats.gpuState = QueryState::Unavailable;
  if (state.sampleCount <= 0) {
    return stats;
  }

  std::vector<float> cpuValues;
  cpuValues.reserve(static_cast<size_t>(state.sampleCount));
  for (int i = 0; i < state.sampleCount; ++i) {
    cpuValues.push_back(state.samples[static_cast<size_t>(i)].cpuMs);
  }

  stats.cpuMeanMs = ComputeMean(cpuValues);
  stats.cpuP95Ms = ComputeP95(cpuValues);
  return stats;
}

void RenderProfiler::UpdateStats() {
  for (size_t i = 0; i < static_cast<size_t>(RenderPassId::Count); ++i) {
    m_cachedStats[i] = GetStats(static_cast<RenderPassId>(i));
  }
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
