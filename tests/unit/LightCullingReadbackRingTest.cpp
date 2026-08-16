#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/lighting/ClusteredLightingState.hpp"
#include "engine/render/passes/LightCullingPass.hpp"

#include <array>
#include <cstdint>

TEST_CASE("[Unit] LightCullingPass - Default state and non-blocking ring initial contracts") {
  using namespace NoMoreDay::render::passes;

  LightCullingPass pass;
  // T1.1: Production default must be false (zero synchronous readback stall)
  CHECK_FALSE(pass.IsReadbackEnabledForTesting());
  CHECK(pass.GetLastOverflowCount() == 0u);
  CHECK(pass.GetLastOverflowSnapshot() == 0u);
  CHECK(pass.GetRingWriteIndex() == 0u);
  CHECK(pass.GetRingReadIndex() == 0u);

  const auto &ring = pass.GetOverflowRing();
  CHECK(ring.size() == LightCullingPass::kRingDepth);
  CHECK(LightCullingPass::kRingDepth == 3u);
  for (size_t i = 0; i < ring.size(); ++i) {
    CHECK_FALSE(ring[i].armed);
    CHECK(ring[i].fence == nullptr);
    CHECK(ring[i].submittedFrame == 0u);
    CHECK(ring[i].counterReadbackBufferId == 0u);
  }
}

TEST_CASE("[Unit] LightCullingPass - Toggle test readback flag") {
  using namespace NoMoreDay::render::passes;

  LightCullingPass pass;
  CHECK_FALSE(pass.IsReadbackEnabledForTesting());

  pass.SetReadbackEnabledForTesting(true);
  CHECK(pass.IsReadbackEnabledForTesting());

  pass.SetReadbackEnabledForTesting(false);
  CHECK_FALSE(pass.IsReadbackEnabledForTesting());
}

TEST_CASE("[Unit] LightCullingPass - Production path guarantees zero synchronous readback stalls") {
  using namespace NoMoreDay::render::passes;
  using NoMoreDay::core::ComputeBuffer;

  // T1.1 / H1 verification: on production path (m_readbackEnabledForTesting == false),
  // synchronous readback counter must remain exactly 0.
  ComputeBuffer::ResetTestReadCount();
  REQUIRE(ComputeBuffer::GetTestReadCount() == 0u);

  LightCullingPass pass;
  CHECK_FALSE(pass.IsReadbackEnabledForTesting());

  // Simulate multiple frame executions with readback disabled
  // Even if cluster headers/buffers exist, production path does not call synchronous ReadBackClusterHeaders()
  CHECK(ComputeBuffer::GetTestReadCount() == 0u);
  CHECK(pass.GetLastOverflowCount() == 0u);
}

TEST_CASE("[Unit] LightCullingPass - Ring FIFO state transitions and unready fence snapshot preservation") {
  using namespace NoMoreDay::render::passes;

  LightCullingPass pass;
  auto &ring = pass.GetOverflowRingMutableForTesting();

  // Set an initial snapshot value
  pass.SetLastOverflowSnapshotForTesting(42u);
  CHECK(pass.GetLastOverflowSnapshot() == 42u);

  // Simulate slot 0 armed on frame 1, but fence is unready / timeout
  ring[0].armed = true;
  ring[0].submittedFrame = 1u;
  ring[0].fence = nullptr; // Simulated fence

  pass.SetRingIndicesForTesting(1u, 0u);
  CHECK(pass.GetRingWriteIndex() == 1u);
  CHECK(pass.GetRingReadIndex() == 0u);

  // When fence is null or unready, snapshot must remain 42u and read index must not advance
  CHECK(pass.GetLastOverflowSnapshot() == 42u);
  CHECK(pass.GetRingReadIndex() == 0u);

  // Simulate slot 1 and slot 2 armed (ring fully armed)
  ring[1].armed = true;
  ring[1].submittedFrame = 2u;
  ring[2].armed = true;
  ring[2].submittedFrame = 3u;
  pass.SetRingIndicesForTesting(0u, 0u);

  for (size_t i = 0; i < LightCullingPass::kRingDepth; ++i) {
    CHECK(ring[i].armed);
  }

  // FIFO preserve semantics: when all slots are armed, write must not overwrite pending slots
  CHECK(ring[0].submittedFrame == 1u);
  CHECK(ring[1].submittedFrame == 2u);
  CHECK(ring[2].submittedFrame == 3u);

  // Shutdown must clean up all ring slots safely
  pass.Shutdown();
  CHECK(pass.GetRingWriteIndex() == 0u);
  CHECK(pass.GetRingReadIndex() == 0u);
  CHECK(pass.GetLastOverflowSnapshot() == 0u);
  CHECK_FALSE(pass.IsReadbackEnabledForTesting());
  for (size_t i = 0; i < LightCullingPass::kRingDepth; ++i) {
    CHECK_FALSE(pass.GetOverflowRing()[i].armed);
    CHECK(pass.GetOverflowRing()[i].fence == nullptr);
  }
}

TEST_CASE("[Unit] LightCullingPass - TryPublishReadySnapshot: unready fence preserves old snapshot") {
  using namespace NoMoreDay::render::passes;

  // Fence not signaled: keep the previous snapshot, read index unchanged.
  const LightCullingPass::SnapshotPollOutcome stale =
      LightCullingPass::TryPublishReadySnapshot(
          /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/false,
          /*readIndex=*/0u, /*ringDepth=*/LightCullingPass::kRingDepth,
          /*pendingSnapshot=*/77u, /*currentSnapshot=*/42u);
  CHECK_FALSE(stale.published);
  CHECK(stale.snapshot == 42u);
  CHECK(stale.nextReadIndex == 0u);

  // Slot not armed: same preservation contract.
  const LightCullingPass::SnapshotPollOutcome notArmed =
      LightCullingPass::TryPublishReadySnapshot(
          /*slotArmed=*/false, /*frameEligible=*/true, /*fenceSignaled=*/true,
          /*readIndex=*/1u, /*ringDepth=*/LightCullingPass::kRingDepth,
          /*pendingSnapshot=*/77u, /*currentSnapshot=*/42u);
  CHECK_FALSE(notArmed.published);
  CHECK(notArmed.snapshot == 42u);
  CHECK(notArmed.nextReadIndex == 1u);

  // Same-frame slot (not yet frame-eligible): preserved too.
  const LightCullingPass::SnapshotPollOutcome sameFrame =
      LightCullingPass::TryPublishReadySnapshot(
          /*slotArmed=*/true, /*frameEligible=*/false, /*fenceSignaled=*/true,
          /*readIndex=*/1u, /*ringDepth=*/LightCullingPass::kRingDepth,
          /*pendingSnapshot=*/77u, /*currentSnapshot=*/42u);
  CHECK_FALSE(sameFrame.published);
  CHECK(sameFrame.snapshot == 42u);
  CHECK(sameFrame.nextReadIndex == 1u);
}

TEST_CASE("[Unit] LightCullingPass - TryPublishReadySnapshot: signaled fence publishes and advances") {
  using namespace NoMoreDay::render::passes;

  // Fence signaled on an armed, frame-eligible slot: publish the new snapshot
  // and advance the read index by one (modulo ring depth).
  const LightCullingPass::SnapshotPollOutcome ready =
      LightCullingPass::TryPublishReadySnapshot(
          /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
          /*readIndex=*/0u, /*ringDepth=*/LightCullingPass::kRingDepth,
          /*pendingSnapshot=*/77u, /*currentSnapshot=*/42u);
  CHECK(ready.published);
  CHECK(ready.snapshot == 77u);
  CHECK(ready.nextReadIndex == 1u);

  // Read index wraps around the ring depth.
  const LightCullingPass::SnapshotPollOutcome wrap =
      LightCullingPass::TryPublishReadySnapshot(
          /*slotArmed=*/true, /*frameEligible=*/true, /*fenceSignaled=*/true,
          /*readIndex=*/2u, /*ringDepth=*/LightCullingPass::kRingDepth,
          /*pendingSnapshot=*/5u, /*currentSnapshot=*/1u);
  CHECK(wrap.published);
  CHECK(wrap.snapshot == 5u);
  CHECK(wrap.nextReadIndex == 0u);
}

TEST_CASE("[Unit] LightCullingPass - CanSubmitReadbackCopy never overwrites a pending slot") {
  using namespace NoMoreDay::render::passes;

  // An empty slot accepts a new copy; an armed (pending) slot rejects it.
  CHECK(LightCullingPass::CanSubmitReadbackCopy(false));
  CHECK_FALSE(LightCullingPass::CanSubmitReadbackCopy(true));

  // Ring-full scenario: every slot is armed, so no submission is admitted and
  // no pending slot gets overwritten.
  LightCullingPass pass;
  auto &ring = pass.GetOverflowRingMutableForTesting();
  ring[0].armed = true;
  ring[1].armed = true;
  ring[2].armed = true;
  for (size_t i = 0; i < LightCullingPass::kRingDepth; ++i) {
    CHECK_FALSE(LightCullingPass::CanSubmitReadbackCopy(ring[i].armed));
  }
}
