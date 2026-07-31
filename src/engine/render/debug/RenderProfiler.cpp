#include "engine/render/debug/RenderProfiler.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/graph/RenderGraph.hpp"

#include <algorithm>

namespace NoMoreDay::render::debug {
namespace {

constexpr size_t ToIndex(RenderPassId passId) {
  return static_cast<size_t>(passId);
}

float ComputeMean(const float *values, size_t count) {
  if (count == 0) {
    return 0.0f;
  }
  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    sum += values[i];
  }
  return sum / static_cast<float>(count);
}

float ComputeP95(float *values, size_t count) {
  if (count == 0) {
    return 0.0f;
  }
  std::sort(values, values + count);
  const size_t idx = std::clamp(static_cast<size_t>(count) * 95u / 100u,
                                static_cast<size_t>(0), count - 1);
  return values[idx];
}

// Fill `out` with the first `count` samples in window order.
void CopySampleWindow(const std::array<float, RenderProfiler::kWindowSize> &samples,
                      int count, float *out) {
  for (int i = 0; i < count; ++i) {
    out[i] = samples[static_cast<size_t>(i)];
  }
}

// Fill `out` with the CPU timings of the first `count` samples.
void CopyCpuSampleWindow(
    const std::array<PassTimingSample, RenderProfiler::kWindowSize> &samples,
    int count, float *out) {
  for (int i = 0; i < count; ++i) {
    out[i] = samples[static_cast<size_t>(i)].cpuMs;
  }
}

} // namespace

RenderProfiler::RenderProfiler() {
  std::map<uint32_t, RenderPassId> idToPass;
  for (size_t i = 0; i < static_cast<size_t>(RenderPassId::Count); ++i) {
    const auto passId = static_cast<RenderPassId>(i);
    const uint32_t stableId = graph::StablePassId(
        graph::CanonicalizePassName(FullPassName(passId)));
    m_stablePassIdByIndex[i] = stableId;
    auto [it, inserted] = idToPass.emplace(stableId, passId);
    if (!inserted) {
      // Alias risk (gate-side absent pass / derived-ID collision): record it
      // and disable backfill for the colliding pass.
      LOG_ERROR("RenderProfiler: stablePassId collision between {} and {} "
                "(id={:#010x}); GPU backfill disabled for {}",
                ToString(it->second), ToString(passId), stableId,
                ToString(passId));
      m_stablePassIdByIndex[i] = 0;
    }
  }
  m_passByStableId = std::move(idToPass);

  if (GPUTimerQueryRing::Get().IsGpuTimerSupported()) {
    LOG_INFO("RenderProfiler: GPU timer ring present; four-state GPU telemetry "
             "(Pending/Valid/Unavailable/CpuFallback) with delayed backfill active");
  } else {
    LOG_WARN("RenderProfiler: no GL timer queries available; GPU timings reported "
             "as CpuFallback and CPU-focused aggregation is used");
  }
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
  const size_t index = ToIndex(passId);
  const PassState &state = m_passStates[index];
  PassTimingStats stats = {};
  stats.budgetMs = GetBudgetMs(passId);

  if (state.sampleCount > 0) {
    std::array<float, kWindowSize> cpuScratch = {};
    CopyCpuSampleWindow(state.samples, state.sampleCount, cpuScratch.data());
    stats.cpuMeanMs = ComputeMean(cpuScratch.data(),
                                  static_cast<size_t>(state.sampleCount));
    stats.cpuP95Ms = ComputeP95(cpuScratch.data(),
                                static_cast<size_t>(state.sampleCount));
  }

  const GpuTrack &gpu = m_gpuTracks[index];
  stats.gpuState = gpu.state;
  if (gpu.sampleCount > 0 &&
      (gpu.state == QueryState::Valid || gpu.state == QueryState::Pending)) {
    // Valid participates in GPU mean/P95; Pending carries the last-frame value.
    std::array<float, kWindowSize> gpuScratch = {};
    CopySampleWindow(gpu.samples, gpu.sampleCount, gpuScratch.data());
    stats.gpuMeanMs = ComputeMean(gpuScratch.data(),
                                  static_cast<size_t>(gpu.sampleCount));
    stats.gpuP95Ms = ComputeP95(gpuScratch.data(),
                                static_cast<size_t>(gpu.sampleCount));
    stats.frameIndex = gpu.lastAcceptedFrameIndex;
  } else {
    // Unavailable/CpuFallback do not participate in GPU aggregation.
    stats.gpuMeanMs = 0.0f;
    stats.gpuP95Ms = 0.0f;
    stats.frameIndex = 0;
  }
  return stats;
}

void RenderProfiler::FlushRingToProfiler() {
  auto &ring = GPUTimerQueryRing::Get();
  ring.PollReadyQueries();

  if (!ring.IsGpuTimerSupported()) {
    for (GpuTrack &track : m_gpuTracks) {
      track.state = QueryState::CpuFallback;
    }
    return;
  }

  const uint64_t currentFrameIndex = ring.DebugGetFrameIndex();
  for (size_t i = 0; i < static_cast<size_t>(RenderPassId::Count); ++i) {
    const uint32_t stableId = m_stablePassIdByIndex[i];
    if (stableId == 0) {
      continue; // alias-collision marked pass: backfill disabled
    }
    GpuTrack &track = m_gpuTracks[i];
    const GPUTimerResult res = ring.GetPassResult(stableId);

    if (res.state == QueryState::Valid &&
        res.frameIndex > track.lastAcceptedFrameIndex) {
      // Frame-acceptance rule: strictly increasing frameIndex. New-frame ready
      // results overwrite/carry forward; each source result is backfilled only
      // once (a repeated same-frame result is never re-accepted).
      float &sample = track.samples[static_cast<size_t>(track.writeIndex)];
      sample = static_cast<float>(res.gpuTimeMs);
      track.writeIndex = (track.writeIndex + 1) % kWindowSize;
      if (track.sampleCount < kWindowSize) {
        ++track.sampleCount;
      }
      track.lastAcceptedFrameIndex = res.frameIndex;
      track.state = QueryState::Valid;
      continue;
    }

    // No newly accepted result this flush: the query is not ready yet
    // (Pending), or a duplicate/stale ready result was rejected. Transition
    // Pending -> Unavailable on overage; otherwise carry the last-frame value
    // as Pending while a newer frame's query is in flight.
    const uint64_t age = currentFrameIndex >= track.lastAcceptedFrameIndex
                             ? currentFrameIndex - track.lastAcceptedFrameIndex
                             : 0;
    if (age >= kPendingOverageFrames) {
      track.state = QueryState::Unavailable;
    } else if (currentFrameIndex > track.lastAcceptedFrameIndex) {
      track.state = QueryState::Pending;
    }
    // else: same frame, no newer query in flight -> keep current state.
  }
}

void RenderProfiler::UpdateStats() {
  for (size_t i = 0; i < static_cast<size_t>(RenderPassId::Count); ++i) {
    m_cachedStats[i] = GetStats(static_cast<RenderPassId>(i));
  }
}

PassTimingStats RenderProfiler::GetPassResult(uint32_t stablePassId) const {
  const auto it = m_passByStableId.find(stablePassId);
  if (it == m_passByStableId.end()) {
    // Mapping failure -> Unavailable (no tracked pass for this stable ID).
    return {};
  }
  return m_cachedStats[ToIndex(it->second)];
}

bool RenderProfiler::IsGpuTimingAvailable() const {
  return GPUTimerQueryRing::Get().IsGpuTimerSupported();
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

const char *RenderProfiler::FullPassName(RenderPassId passId) {
  switch (passId) {
  case RenderPassId::Scene:
    return "ScenePass";
  case RenderPassId::Lighting:
    return "LightingPass";
  case RenderPassId::HeightShadow:
    return "HeightShadowPass";
  case RenderPassId::OccluderExtract:
    return "OccluderExtractPass";
  case RenderPassId::JFA:
    return "JFAPass";
  case RenderPassId::RadianceCascades:
    return "RadianceCascadesPass";
  case RenderPassId::GIComposite:
    return "GICompositePass";
  case RenderPassId::FluidSimulation:
    return "FluidSimulationPass";
  case RenderPassId::Volumetric:
    return "VolumetricLightPass";
  case RenderPassId::VFX:
    return "VFXPass";
  case RenderPassId::GPUText:
    return "GPUTextPass";
  case RenderPassId::GPULoot:
    return "GPULootPass";
  case RenderPassId::UIWorld:
    return "UIWorldPass";
  case RenderPassId::PostProcess:
    return "PostProcessPass";
  case RenderPassId::Distortion:
    return "DistortionPass";
  case RenderPassId::Composite:
    return "CompositePass";
  case RenderPassId::Count:
    break;
  }
  return "UnknownPass";
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
