#include "doctest.h"

#include "engine/render/resources/GPUResourceRegistry.hpp"

#include <cstdint>

namespace {

constexpr uint32_t kHandleA = 101;
constexpr uint32_t kHandleB = 202;
constexpr size_t kSizeA = 4 * 1024 * 1024;
constexpr size_t kSizeB = 2048;

} // namespace

TEST_CASE("[Unit] GPUResourceRegistry - TakeSnapshot reports object count, bytes, lifecycle and timestamps") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::Texture2D, RenderOwnerTag::Scene, kSizeA, "snap_texture");
  registry.RegisterResource(kHandleB, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, kSizeB, "snap_buffer");

  registry.AdvanceFrame();
  registry.AdvanceFrame();

  const GPUResourceSnapshot snap = registry.TakeSnapshot();
  CHECK(snap.activeResourceCount == 2);
  CHECK(snap.liveReferenceCount == 2);
  CHECK(snap.currentTotalBytes == kSizeA + kSizeB);
  CHECK(snap.peakTotalBytes == kSizeA + kSizeB);
  CHECK(snap.totalCreatedCount == 2);
  CHECK(snap.totalDestroyedCount == 0);
  CHECK(snap.frameIndex == 2);
  CHECK(registry.GetFrameIndex() == 2);

  // Unregistering shrinks the next snapshot.
  registry.UnregisterResource(kHandleA, ResourceKind::Texture2D);
  const GPUResourceSnapshot afterUnregister = registry.TakeSnapshot();
  CHECK(afterUnregister.activeResourceCount == 1);
  CHECK(afterUnregister.currentTotalBytes == kSizeB);
  CHECK(afterUnregister.totalDestroyedCount == 1);
}

TEST_CASE("[Unit] GPUResourceRegistry - pending reference window follows creation-frame aging") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::Texture2D, RenderOwnerTag::Scene, kSizeA, "age_texture");

  // Within the 9-frame pending window the record is still pending.
  for (uint64_t frame = 0; frame < 5; ++frame) {
    registry.AdvanceFrame();
  }
  CHECK(registry.TakeSnapshot().pendingReferenceCount == 1);

  // Beyond the window the record is no longer pending quiescence.
  for (uint64_t frame = 0; frame < 10; ++frame) {
    registry.AdvanceFrame();
  }
  CHECK(registry.TakeSnapshot().pendingReferenceCount == 0);
}

TEST_CASE("[Unit] GPUResourceRegistry - snapshot timestamp is monotonic within an epoch") {
  using namespace NoMoreDay::render::resources;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  const GPUResourceSnapshot first = registry.TakeSnapshot();
  const GPUResourceSnapshot second = registry.TakeSnapshot();
  CHECK(second.wallClockMs >= first.wallClockMs);

  // A new epoch (Reset) restarts the timestamp base at zero.
  registry.Reset();
  const GPUResourceSnapshot afterReset = registry.TakeSnapshot();
  CHECK(afterReset.wallClockMs == 0);
}
