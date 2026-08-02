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

// W5.6/RG-3 contract: the pending window is exactly 9 frames (ages 0..8); the
// first frame after that (age 9) the record is quiesced. Locks the boundary so
// a future off-by-one cannot silently widen the window.
TEST_CASE("[Unit] GPUResourceRegistry - pending window boundary is age 8 pending / age 9 quiesced") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::Texture2D, RenderOwnerTag::Scene, kSizeA, "boundary_texture");

  // Advance to frame 8: age == 8, still inside the 9-frame window.
  for (uint64_t frame = 0; frame < 8; ++frame) {
    registry.AdvanceFrame();
  }
  CHECK(registry.TakeSnapshot().frameIndex == 8);
  CHECK(registry.TakeSnapshot().pendingReferenceCount == 1);

  // Advance to frame 9: age == 9, the record is quiesced.
  registry.AdvanceFrame();
  CHECK(registry.TakeSnapshot().frameIndex == 9);
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

// W5.2/RG-3 contract: a duplicate (handle, kind) registration must be rejected
// with the original record and every aggregate counter preserved. Counters may
// never inflate on a duplicate map key.
TEST_CASE("[Unit] GPUResourceRegistry - duplicate registration is rejected without counter inflation") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, kSizeA, "dup_buffer");
  const GPUResourceStats afterFirst = registry.GetStats();
  CHECK(afterFirst.activeCount == 1);
  CHECK(afterFirst.totalCreatedCount == 1);
  CHECK(afterFirst.currentTotalBytes == kSizeA);

  // Re-registering the identical (handle, kind) with a larger size must be
  // rejected: record, active/created counts and bytes stay exactly as before.
  registry.RegisterResource(kHandleA, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, kSizeB, "dup_buffer_again");
  const GPUResourceStats afterDuplicate = registry.GetStats();
  CHECK(afterDuplicate.activeCount == 1);
  CHECK(afterDuplicate.totalCreatedCount == 1);
  CHECK(afterDuplicate.currentTotalBytes == kSizeA);
  CHECK(afterDuplicate.peakTotalBytes == kSizeA);

  const auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 1);
  CHECK(active[0].sizeBytes == kSizeA);
  CHECK(active[0].name == "dup_buffer");

  // A different kind under the same numeric handle is a distinct key and is
  // legal (the registry key is (kind << 32) | handle).
  registry.RegisterResource(kHandleA, ResourceKind::VertexBuffer, RenderOwnerTag::Scene, kSizeB, "dup_vao_vbo");
  const GPUResourceStats afterSiblingKind = registry.GetStats();
  CHECK(afterSiblingKind.activeCount == 2);
  CHECK(afterSiblingKind.currentTotalBytes == kSizeA + kSizeB);
}

// W5.2/RG-3 contract: UpdateResourceSize flows through one accounting-safe path
// and keeps current/peak totals, kind bytes and owner bytes mutually consistent.
TEST_CASE("[Unit] GPUResourceRegistry - size update keeps aggregate counters consistent") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, kSizeA, "size_buffer");
  registry.RegisterResource(kHandleB, ResourceKind::Texture2D, RenderOwnerTag::Scene, kSizeB, "size_texture");
  CHECK(registry.GetStats().currentTotalBytes == kSizeA + kSizeB);

  // Grow handle A; totals and per-kind/owner maps must track the delta exactly.
  const size_t grown = kSizeA + 4096;
  registry.UpdateResourceSize(kHandleA, ResourceKind::StorageBuffer, grown);
  const GPUResourceStats stats = registry.GetStats();
  CHECK(stats.currentTotalBytes == grown + kSizeB);
  CHECK(stats.peakTotalBytes == grown + kSizeB);
  CHECK(stats.bytesByKind.at(static_cast<uint8_t>(ResourceKind::StorageBuffer)) == grown);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Lighting)) == grown);
  CHECK(stats.bytesByKind.at(static_cast<uint8_t>(ResourceKind::Texture2D)) == kSizeB);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Scene)) == kSizeB);
  CHECK(stats.activeCount == 2);
  CHECK(stats.totalCreatedCount == 2);
  CHECK(stats.totalDestroyedCount == 0);

  // Shrink back; counters must follow monotonically without going negative.
  registry.UpdateResourceSize(kHandleA, ResourceKind::StorageBuffer, kSizeB);
  const GPUResourceStats shrunk = registry.GetStats();
  CHECK(shrunk.currentTotalBytes == kSizeB + kSizeB);
  CHECK(shrunk.bytesByKind.at(static_cast<uint8_t>(ResourceKind::StorageBuffer)) == kSizeB);
}

// W5.2/RG-3 contract: a numeric handle is only reusable after prior
// unregistration; the registry must treat the re-registration as a fresh
// record with correct counters.
TEST_CASE("[Unit] GPUResourceRegistry - numeric handle reuse after unregister") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::UniformBuffer, RenderOwnerTag::PostProcess, kSizeA, "reuse_a");
  registry.UnregisterResource(kHandleA, ResourceKind::UniformBuffer);
  GPUResourceStats stats = registry.GetStats();
  CHECK(stats.activeCount == 0);
  CHECK(stats.totalDestroyedCount == 1);
  CHECK(stats.currentTotalBytes == 0);

  // Reuse of the same numeric handle is valid only after prior unregistration.
  registry.RegisterResource(kHandleA, ResourceKind::UniformBuffer, RenderOwnerTag::PostProcess, kSizeB, "reuse_a_2");
  stats = registry.GetStats();
  CHECK(stats.activeCount == 1);
  CHECK(stats.totalCreatedCount == 2);
  CHECK(stats.totalDestroyedCount == 1);
  CHECK(stats.currentTotalBytes == kSizeB);

  const auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 1);
  CHECK(active[0].sizeBytes == kSizeB);
}

// W5.2/RG-3 contract: unregister/size-update on an unknown record is a
// diagnostic no-op that never mutates any counter.
TEST_CASE("[Unit] GPUResourceRegistry - unknown-record unregister and size update are no-ops") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  registry.RegisterResource(kHandleA, ResourceKind::StorageBuffer, RenderOwnerTag::Lighting, kSizeA, "stable");
  const GPUResourceStats before = registry.GetStats();

  registry.UnregisterResource(kHandleB, ResourceKind::StorageBuffer);
  registry.UpdateResourceSize(kHandleB, ResourceKind::StorageBuffer, 12345);
  registry.UpdateResourceSize(kHandleA, ResourceKind::Texture2D, 6789);

  const GPUResourceStats after = registry.GetStats();
  CHECK(after.activeCount == before.activeCount);
  CHECK(after.totalCreatedCount == before.totalCreatedCount);
  CHECK(after.totalDestroyedCount == before.totalDestroyedCount);
  CHECK(after.currentTotalBytes == before.currentTotalBytes);
  CHECK(after.peakTotalBytes == before.peakTotalBytes);
}
