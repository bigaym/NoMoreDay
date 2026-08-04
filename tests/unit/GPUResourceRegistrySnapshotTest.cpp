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

// Phase G (RG-3 completeness): the 8 dedicated VAO/VBO sites, ResourceManager
// shader programs (VS/FS + compute) and GPUTimerQueryRing query pairs all
// follow the same observer-only pairing contract: RegisterResource immediately
// after successful GL creation and UnregisterResource BEFORE the matching GL
// release. This test locks the register/unregister pairing and lifecycle
// counters for exactly those kinds (VertexArray / VertexBuffer / ShaderProgram
// / QueryRing).
TEST_CASE("[Unit] GPUResourceRegistry - Phase G kinds pair register with unregister before release") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  // Distinct numeric handles (as distinct GL objects would have).
  constexpr uint32_t kVao = 1;
  constexpr uint32_t kVbo = 2;
  constexpr uint32_t kShader = 3;
  constexpr uint32_t kQuery = 4;

  // 1. Successful creation -> register (mirrors each G1/G2/G3 site).
  registry.RegisterResource(kVao, ResourceKind::VertexArray, RenderOwnerTag::Unknown, 0u, "QuadVAO");
  registry.RegisterResource(kVbo, ResourceKind::VertexBuffer, RenderOwnerTag::Unknown, 96u, "QuadVBO");
  registry.RegisterResource(kShader, ResourceKind::ShaderProgram, RenderOwnerTag::Unknown, 0u, "ResourceManagerShader");
  registry.RegisterResource(kQuery, ResourceKind::QueryRing, RenderOwnerTag::Unknown, 0u, "GPUTimerQueryRing");

  GPUResourceStats stats = registry.GetStats();
  CHECK(stats.activeCount == 4);
  CHECK(stats.totalCreatedCount == 4);
  CHECK(stats.totalDestroyedCount == 0);
  CHECK(stats.currentTotalBytes == 96u);

  auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 4);
  for (const auto &rec : active) {
    CHECK(rec.ownerTag == RenderOwnerTag::Unknown);
    CHECK(rec.creationFrame == 0);
  }

  // The (kind << 32 | handle) key keeps same-handle/different-kind records
  // distinct, exactly as the registry contract requires.
  registry.RegisterResource(kVao, ResourceKind::VertexBuffer, RenderOwnerTag::Unknown, 32u, "SiblingVBO");
  stats = registry.GetStats();
  CHECK(stats.activeCount == 5);
  CHECK(stats.totalCreatedCount == 5);

  // 2. GL release -> unregister first (mirrors Shutdown ordering). Unregistering
  // each record individually must decrement lifecycle counters exactly once.
  registry.UnregisterResource(kVbo, ResourceKind::VertexBuffer);
  registry.UnregisterResource(kVao, ResourceKind::VertexArray);
  registry.UnregisterResource(kVao, ResourceKind::VertexBuffer);
  registry.UnregisterResource(kShader, ResourceKind::ShaderProgram);
  registry.UnregisterResource(kQuery, ResourceKind::QueryRing);

  stats = registry.GetStats();
  CHECK(stats.activeCount == 0);
  CHECK(stats.totalCreatedCount == 5);
  CHECK(stats.totalDestroyedCount == 5);
  CHECK(stats.currentTotalBytes == 0);
  CHECK(stats.peakTotalBytes == 96u + 32u);

  active = registry.GetActiveResources();
  CHECK(active.empty());

  // 3. Unknown-kind unregister after the fact is a diagnostic no-op.
  registry.UnregisterResource(kVao, ResourceKind::VertexArray);
  stats = registry.GetStats();
  CHECK(stats.activeCount == 0);
  CHECK(stats.totalDestroyedCount == 5);

  // 4. Snapshot reflects the full lifecycle: created then destroyed, bytes 0.
  const GPUResourceSnapshot snap = registry.TakeSnapshot();
  CHECK(snap.activeResourceCount == 0);
  CHECK(snap.liveReferenceCount == 0);
  CHECK(snap.currentTotalBytes == 0);
  CHECK(snap.totalCreatedCount == 5);
  CHECK(snap.totalDestroyedCount == 5);
}

// B11 (RG-3 owner metadata): ReclassifyResourceOwner reclassifies an EXISTING
// record to a new RenderOwnerTag and rebalances the per-owner byte ledger
// without touching lifecycle counters or the total bytes.
TEST_CASE("[Unit] GPUResourceRegistry - owner reclassification rebalances the owner-byte ledger") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  constexpr uint32_t kShadowFbo = 3001;
  constexpr uint32_t kShadowTex = 3002;
  constexpr uint32_t kClusterBuf = 3003;
  constexpr size_t kFboBytes = 8u * 1024u * 1024u;
  constexpr size_t kTexBytes = 4u * 1024u * 1024u;
  constexpr size_t kBufBytes = 2048u;

  // FramebufferManager registers FBO + color texture under the generic Scene
  // owner; ComputeBuffer registers SSBOs under Unknown. That is the pre-B11
  // baseline we reclassify away from.
  registry.RegisterResource(kShadowFbo, ResourceKind::Framebuffer, RenderOwnerTag::Scene,
                            kFboBytes, "shadow_sdf_fbo");
  registry.RegisterResource(kShadowTex, ResourceKind::Texture2D, RenderOwnerTag::Scene,
                            kTexBytes, "shadow_sdf_color");
  registry.RegisterResource(kClusterBuf, ResourceKind::StorageBuffer, RenderOwnerTag::Unknown,
                            kBufBytes, "cluster_header");

  // Reclassify to the RenderGraph owner contract.
  CHECK(registry.ReclassifyResourceOwner(kShadowFbo, ResourceKind::Framebuffer,
                                         RenderOwnerTag::Shadow));
  CHECK(registry.ReclassifyResourceOwner(kShadowTex, ResourceKind::Texture2D,
                                         RenderOwnerTag::Shadow));
  CHECK(registry.ReclassifyResourceOwner(kClusterBuf, ResourceKind::StorageBuffer,
                                         RenderOwnerTag::LightCulling));

  const GPUResourceStats stats = registry.GetStats();
  // Totals and lifecycle counters are untouched by metadata reclassification.
  CHECK(stats.currentTotalBytes == kFboBytes + kTexBytes + kBufBytes);
  CHECK(stats.activeCount == 3);
  CHECK(stats.totalCreatedCount == 3);
  CHECK(stats.totalDestroyedCount == 0);
  // Bytes moved from the old owners to the new owners exactly.
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Scene)) == 0);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Unknown)) == 0);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Shadow)) ==
        kFboBytes + kTexBytes);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::LightCulling)) ==
        kBufBytes);

  const auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 3);
  for (const auto &rec : active) {
    if (rec.handle == kShadowFbo) {
      CHECK(rec.ownerTag == RenderOwnerTag::Shadow);
    } else if (rec.handle == kShadowTex) {
      CHECK(rec.ownerTag == RenderOwnerTag::Shadow);
    } else {
      CHECK(rec.ownerTag == RenderOwnerTag::LightCulling);
    }
  }

  // Reclassifying to the already-held owner is a successful no-op.
  CHECK(registry.ReclassifyResourceOwner(kClusterBuf, ResourceKind::StorageBuffer,
                                         RenderOwnerTag::LightCulling));
  const GPUResourceStats afterNoop = registry.GetStats();
  CHECK(afterNoop.currentTotalBytes == stats.currentTotalBytes);
  CHECK(afterNoop.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::LightCulling)) ==
        kBufBytes);
}

// B11 (RG-3 contract): ReclassifyResourceOwner on an unknown (handle, kind) or
// on a zero handle fails closed - diagnostic no-op, no counter mutation.
TEST_CASE("[Unit] GPUResourceRegistry - owner reclassification on unknown or zero handle fails closed") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  constexpr uint32_t kKnownBuf = 3011;
  constexpr uint32_t kNeverRegistered = 3012;
  constexpr size_t kBufBytes = 4096u;

  registry.RegisterResource(kKnownBuf, ResourceKind::StorageBuffer, RenderOwnerTag::Unknown,
                            kBufBytes, "stable_buffer");
  const GPUResourceStats before = registry.GetStats();

  // Unknown numeric handle -> false, counters untouched.
  CHECK_FALSE(registry.ReclassifyResourceOwner(kNeverRegistered,
                                               ResourceKind::StorageBuffer,
                                               RenderOwnerTag::Shadow));
  // Known handle but wrong kind -> different registry key -> unknown record.
  CHECK_FALSE(registry.ReclassifyResourceOwner(kKnownBuf, ResourceKind::Texture2D,
                                               RenderOwnerTag::Shadow));
  // Zero handle is never a valid registry key.
  CHECK_FALSE(registry.ReclassifyResourceOwner(0, ResourceKind::StorageBuffer,
                                               RenderOwnerTag::Shadow));

  const GPUResourceStats after = registry.GetStats();
  CHECK(after.activeCount == before.activeCount);
  CHECK(after.totalCreatedCount == before.totalCreatedCount);
  CHECK(after.totalDestroyedCount == before.totalDestroyedCount);
  CHECK(after.currentTotalBytes == before.currentTotalBytes);
  CHECK(after.peakTotalBytes == before.peakTotalBytes);

  // The known record still carries its original owner.
  const auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 1);
  CHECK(active[0].ownerTag == RenderOwnerTag::Unknown);
}

// B11 (RG-3 owner metadata): the resize lifecycle of FramebufferManager/ComputeBuffer
// is Destroy + Create. The recreate registers a NEW record under the generic owner,
// so the reclassify must be re-applied to the NEW handle after every recreate.
TEST_CASE("[Unit] GPUResourceRegistry - resize recreation pairs reclassify with the recreated handle") {
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::render::graph;

  auto &registry = GPUResourceRegistry::Get();
  registry.Reset();

  constexpr uint32_t kOldFbo = 3021;
  constexpr uint32_t kNewFbo = 3022;
  constexpr size_t kOldBytes = 4u * 1024u * 1024u;
  constexpr size_t kNewBytes = 8u * 1024u * 1024u;

  // First allocation: FramebufferManager::Create registers under Scene, then the
  // owner reclassifies to Shadow (the B11 wiring after create).
  registry.RegisterResource(kOldFbo, ResourceKind::Framebuffer, RenderOwnerTag::Scene,
                            kOldBytes, "shadow_mask_fbo");
  CHECK(registry.ReclassifyResourceOwner(kOldFbo, ResourceKind::Framebuffer,
                                         RenderOwnerTag::Shadow));

  // Resize: FramebufferManager::Resize destroys the old backing (unregister) and
  // creates a fresh GL object (register under Scene again).
  registry.UnregisterResource(kOldFbo, ResourceKind::Framebuffer);
  registry.RegisterResource(kNewFbo, ResourceKind::Framebuffer, RenderOwnerTag::Scene,
                            kNewBytes, "shadow_mask_fbo_resized");

  // Before the reclassify is re-applied, the new record still carries the generic
  // Scene owner - exactly the debt B11 removes.
  {
    const auto active = registry.GetActiveResources();
    REQUIRE(active.size() == 1);
    CHECK(active[0].handle == kNewFbo);
    CHECK(active[0].ownerTag == RenderOwnerTag::Scene);
  }

  // Owner re-applies the contract after the recreate (B11 wiring after resize).
  CHECK(registry.ReclassifyResourceOwner(kNewFbo, ResourceKind::Framebuffer,
                                         RenderOwnerTag::Shadow));

  const GPUResourceStats stats = registry.GetStats();
  CHECK(stats.activeCount == 1);
  CHECK(stats.totalCreatedCount == 2);
  CHECK(stats.totalDestroyedCount == 1);
  CHECK(stats.currentTotalBytes == kNewBytes);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Shadow)) == kNewBytes);
  CHECK(stats.bytesByOwner.at(static_cast<uint8_t>(RenderOwnerTag::Scene)) == 0);

  const auto active = registry.GetActiveResources();
  REQUIRE(active.size() == 1);
  CHECK(active[0].handle == kNewFbo);
  CHECK(active[0].sizeBytes == kNewBytes);
}
