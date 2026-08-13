#include "engine/render/debug/RenderProfiler.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/graph/RenderGraph.hpp"

#include <algorithm>

namespace NoMoreDay::render::debug {
namespace {

constexpr size_t ToIndex(RenderPassId passId) {
  return static_cast<size_t>(passId);
}

constexpr std::array<float, static_cast<size_t>(graph::RenderPassType::Count)>
    kPassBudgetMs = {
        1.20f, // Scene
        0.80f, // Lighting
        0.90f, // HeightShadow
        0.10f, // OccluderExtract
        0.40f, // JFA
        1.20f, // RadianceCascades
        0.05f, // GIComposite
        0.30f, // FluidSimulation
        0.80f, // Volumetric
        1.00f, // VFX
        0.15f, // GPUText
        0.20f, // GPULoot
        0.60f, // UIWorld
        0.50f, // PostProcess
        0.30f, // Distortion
        0.25f, // Composite
        0.15f, // LightCulling (V3 contract kBudgetLightCulling_Normal)
        0.10f, // ShadowPrepare (lightweight compute, aligned with OccluderExtract)
        0.40f, // ShadowBuild (V3 contract kBudgetShadow_Normal)
        0.30f, // ShadowResolve (screen-space resolve, aligned with Distortion)
    };

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
        graph::CanonicalizePassName(graph::kRenderPassNames[ToIndex(passId)].full));
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
  const size_t index = ToIndex(passId);
  if (index < graph::kRenderPassNames.size()) {
    return graph::kRenderPassNames[index].display.data();
  }
  return "Unknown";
}

std::optional<RenderPassId> RenderProfiler::FromPassName(std::string_view passName) {
  for (size_t i = 0; i < graph::kRenderPassNames.size(); ++i) {
    if (graph::kRenderPassNames[i].full == passName) {
      return static_cast<RenderPassId>(i);
    }
  }
  return std::nullopt;
}

float RenderProfiler::GetBudgetMs(RenderPassId passId) {
  const size_t index = ToIndex(passId);
  return index < kPassBudgetMs.size() ? kPassBudgetMs[index] : 0.0f;
}

} // namespace NoMoreDay::render::debug
