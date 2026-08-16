// S1b focused tests: four-state GPU timer data model and delayed backfill.
// These drive GPUTimerQueryRing through its test hooks (no GL context needed)
// and assert the RenderProfiler backfill/aggregation contract.

#include "doctest.h"

#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/graph/RenderGraph.hpp"

#include <cstdint>

namespace {
using NoMoreDay::render::debug::GPUTimerQueryRing;
using NoMoreDay::render::debug::GPUTimerResult;
using NoMoreDay::render::debug::QueryState;
using NoMoreDay::render::debug::RenderPassId;
using NoMoreDay::render::debug::RenderProfiler;

uint32_t S1bFourStateSceneId() {
  return NoMoreDay::render::graph::StablePassId(
      NoMoreDay::render::graph::CanonicalizePassName("ScenePass"));
}

GPUTimerResult S1bFourStateMakeResult(double gpuMs, uint64_t frameIndex,
                                      QueryState state) {
  GPUTimerResult result = {};
  result.gpuTimeMs = gpuMs;
  result.cpuTimeMs = gpuMs / 10.0;
  result.state = state;
  result.frameIndex = frameIndex;
  return result;
}

void S1bFourStateResetRing() {
  GPUTimerQueryRing::Get().Shutdown();
  GPUTimerQueryRing::Get().Initialize();
  GPUTimerQueryRing::Get().DebugSetGpuTimerSupported(true);
}
} // namespace

// Pending -> Valid: a ready result with a strictly increasing frameIndex is
// accepted exactly once and reported with its source frame.
TEST_CASE("[Unit] RenderProfiler four-state - Pending to Valid acceptance") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(1);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 1, QueryState::Valid));

  RenderProfiler profiler;
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();

  const auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Valid);
  CHECK(stats.gpuMeanMs == doctest::Approx(5.0f));
  CHECK(stats.gpuP95Ms == doctest::Approx(5.0f));
  CHECK(stats.frameIndex == 1);

  // GetPassResult(stablePassId) resolves the tracked pass to the same stats.
  const auto viaId = profiler.GetPassResult(S1bFourStateSceneId());
  CHECK(viaId.gpuState == QueryState::Valid);
  CHECK(viaId.gpuMeanMs == doctest::Approx(5.0f));

  GPUTimerQueryRing::Get().Shutdown();
}

// Pending carries the last-frame value (source frame marked); a delayed ready
// result is backfilled exactly once (single backfill, no double-counting).
TEST_CASE("[Unit] RenderProfiler four-state - delayed ready single backfill") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(3);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 1, QueryState::Valid));

  RenderProfiler profiler;
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  CHECK(profiler.GetStats(RenderPassId::Scene).gpuState == QueryState::Valid);

  // Frame 4 starts but frame 3's query is not ready yet -> Pending carry of
  // frame 1's value, source frame still marked as 1.
  GPUTimerQueryRing::Get().DebugSetFrameIndex(4);
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Pending);
  CHECK(stats.gpuMeanMs == doctest::Approx(5.0f));
  CHECK(stats.frameIndex == 1);

  // Delayed ready result for frame 2 arrives -> accepted once.
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(6.0, 2, QueryState::Valid));
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Valid);
  CHECK(stats.gpuMeanMs == doctest::Approx(5.5f)); // (5 + 6) / 2
  CHECK(stats.frameIndex == 2);

  // Same source result reported again must NOT be re-accepted (single
  // backfill): mean stays (5 + 6) / 2, not (5+6+6)/3.
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Pending); // frame 3 still in flight
  CHECK(stats.gpuMeanMs == doctest::Approx(5.5f));
  CHECK(stats.frameIndex == 2);

  GPUTimerQueryRing::Get().Shutdown();
}

// Old/late frames are rejected: a ready result with frameIndex below the last
// accepted frame never enters the sample window.
TEST_CASE("[Unit] RenderProfiler four-state - stale frame rejected") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(5);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 4, QueryState::Valid));

  RenderProfiler profiler;
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  CHECK(profiler.GetStats(RenderPassId::Scene).gpuState == QueryState::Valid);

  // A late result for an older frame (frame 3 < lastAccepted 4) must not
  // overwrite the newer frame's data.
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(9.0, 3, QueryState::Valid));
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  const auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuMeanMs == doctest::Approx(5.0f)); // 9.0 rejected
  CHECK(stats.frameIndex == 4);

  GPUTimerQueryRing::Get().Shutdown();
}

// Pending -> Unavailable on overage; a later ready result recovers to Valid.
TEST_CASE("[Unit] RenderProfiler four-state - overage Unavailable and recovery") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(8);

  RenderProfiler profiler;
  // No ready result ever arrives for Scene: after the pending overage
  // threshold the pass becomes Unavailable and drops out of GPU aggregation.
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Unavailable);
  CHECK(stats.gpuMeanMs == 0.0f);
  CHECK(stats.gpuP95Ms == 0.0f);

  // Later ready result (strictly increasing frame) recovers to Valid.
  GPUTimerQueryRing::Get().DebugSetFrameIndex(9);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 9, QueryState::Valid));
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Valid);
  CHECK(stats.gpuMeanMs == doctest::Approx(5.0f));
  CHECK(stats.frameIndex == 9);

  GPUTimerQueryRing::Get().Shutdown();
}

// No GPU timers -> every pass falls back to CpuFallback and CPU aggregation;
// capability is reported unavailable and injected results are ignored.
TEST_CASE("[Unit] RenderProfiler four-state - no GPU CpuFallback") {
  GPUTimerQueryRing::Get().Shutdown();
  GPUTimerQueryRing::Get().Initialize();
  GPUTimerQueryRing::Get().DebugSetGpuTimerSupported(false);
  GPUTimerQueryRing::Get().DebugSetFrameIndex(1);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 1, QueryState::Valid));

  RenderProfiler profiler;
  CHECK_FALSE(profiler.IsGpuTimingAvailable());
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();

  const auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::CpuFallback);
  CHECK(stats.gpuMeanMs == 0.0f);
  CHECK(stats.gpuP95Ms == 0.0f);
  CHECK(stats.frameIndex == 0);

  GPUTimerQueryRing::Get().Shutdown();
}

// Mapping failure (untracked stable pass ID) reports Unavailable; tracked IDs
// resolve to the backfilled cached stats.
TEST_CASE("[Unit] RenderProfiler four-state - mapping failure Unavailable") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(1);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(5.0, 1, QueryState::Valid));

  RenderProfiler profiler;
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();

  // Arbitrary untracked ID.
  const auto unknown = profiler.GetPassResult(0xDEADBEEFu);
  CHECK(unknown.gpuState == QueryState::Unavailable);
  CHECK(unknown.gpuMeanMs == 0.0f);

  // Every render pass is tracked now (pass identity is an enum). A derived
  // stable ID for a real pass therefore resolves to its own cached stats and
  // must NOT alias the Scene data injected above.
  const uint32_t gateOnlyId = NoMoreDay::render::graph::StablePassId(
      NoMoreDay::render::graph::CanonicalizePassName("ShadowResolvePass"));
  const auto gateOnly = profiler.GetPassResult(gateOnlyId);
  CHECK(gateOnly.gpuMeanMs == 0.0f);
  CHECK(gateOnly.gpuState ==
        profiler.GetStats(RenderPassId::ShadowResolve).gpuState);

  // Tracked ID resolves to the backfilled stats (Valid).
  const auto tracked = profiler.GetPassResult(S1bFourStateSceneId());
  CHECK(tracked.gpuState == QueryState::Valid);
  CHECK(tracked.gpuMeanMs == doctest::Approx(5.0f));

  GPUTimerQueryRing::Get().Shutdown();
}

// DRS/HUD decision inputs: the documented consumer rule (Valid with a positive
// GPU mean -> GPU cost, otherwise CPU) is fed by each of the four states.
TEST_CASE("[Unit] RenderProfiler four-state - DRS/HUD decision inputs") {
  S1bFourStateResetRing();
  GPUTimerQueryRing::Get().DebugSetFrameIndex(1);
  GPUTimerQueryRing::Get().DebugInjectPassResult(
      S1bFourStateSceneId(), S1bFourStateMakeResult(8.0, 1, QueryState::Valid));

  RenderProfiler profiler;
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();

  const auto pickGpuCost = [](const auto &stats) {
    return (stats.gpuState == QueryState::Valid && stats.gpuMeanMs > 0.0f)
               ? stats.gpuMeanMs
               : stats.cpuMeanMs;
  };

  // Valid -> GPU cost picked.
  auto stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(pickGpuCost(stats) == doctest::Approx(stats.gpuMeanMs));

  // Pending (frame in flight) -> CPU cost picked, GPU value carried.
  GPUTimerQueryRing::Get().DebugSetFrameIndex(2);
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Pending);
  CHECK(pickGpuCost(stats) == doctest::Approx(stats.cpuMeanMs));
  CHECK(stats.gpuMeanMs == doctest::Approx(8.0f)); // carried value still visible

  // Unavailable (no samples) -> CPU cost picked, no GPU data.
  GPUTimerQueryRing::Get().DebugSetFrameIndex(8);
  profiler.FlushRingToProfiler();
  profiler.UpdateStats();
  stats = profiler.GetStats(RenderPassId::Scene);
  CHECK(stats.gpuState == QueryState::Unavailable);
  CHECK(stats.gpuMeanMs == 0.0f);
  CHECK(pickGpuCost(stats) == doctest::Approx(stats.cpuMeanMs));

  GPUTimerQueryRing::Get().Shutdown();
}

// P0-6: SlotState string conversion and debug state observation
TEST_CASE("[Unit] GPUTimerQueryRing slot state machine - SlotState transitions") {
  using NoMoreDay::render::debug::SlotState;
  using NoMoreDay::render::debug::ToSlotStateName;
  CHECK(std::string(ToSlotStateName(SlotState::Free)) == "Free");
  CHECK(std::string(ToSlotStateName(SlotState::Pending)) == "Pending");
  CHECK(std::string(ToSlotStateName(SlotState::Ready)) == "Ready");
  CHECK(std::string(ToSlotStateName(SlotState::Discarded)) == "Discarded");

  GPUTimerQueryRing::Get().Shutdown();
  GPUTimerQueryRing::Get().Initialize();

  const uint32_t testPassId = 0x1234u;
  CHECK(GPUTimerQueryRing::Get().DebugGetSlotState(0, testPassId) == SlotState::Free);

  GPUTimerQueryRing::Get().DebugSetSlotState(0, testPassId, SlotState::Discarded);
  CHECK(GPUTimerQueryRing::Get().DebugGetSlotState(0, testPassId) == SlotState::Discarded);

  GPUTimerQueryRing::Get().DebugSetSlotState(0, testPassId, SlotState::Ready);
  CHECK(GPUTimerQueryRing::Get().DebugGetSlotState(0, testPassId) == SlotState::Ready);

  GPUTimerQueryRing::Get().Shutdown();
  CHECK(GPUTimerQueryRing::Get().DebugGetSlotState(0, testPassId) == SlotState::Free);
}

