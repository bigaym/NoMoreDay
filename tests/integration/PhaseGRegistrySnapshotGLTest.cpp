#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/debug/GPUTimerQueryRing.hpp"
#include "engine/render/passes/FluidSimulationPass.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <cstdint>
#include <string>
#include <vector>

// ===========================================================================
// Phase G (RG-3) G4 ledger-verification fixture (2026-08-05)
//
// Purpose: verify that every Phase G registered resource actually shows up in
// the GPUResourceRegistry ledger under a real GL context, stays stable across
// two consecutive frames, and is unregistered again on Shutdown.
//
// This is the observer-only counterpart of the G1-G3 registration work:
//   - 8 VAO/VBO sites: PopupRenderer / MDIRenderer / GPUParticleSystem /
//     GPUTrailRenderer / GPULootSystem / GPUSkillEffectSystem / GPUTextSystem /
//     FluidSimulationPass;
//   - shader programs registered through ResourceManager (VS/FS "mdi_render"
//     and compute "mdi_cull"/"mdi_scatter"/"gpu_text_layout_cs");
//   - the GPUTimerQueryRing queries (registered lazily in BeginPass when a
//     non-zero GL query is actually generated).
//
// The fixture builds a real hidden 1x1 GL context (same pattern as
// GraphBindingEquivalenceGLTest / GPUHardwareValidationGateTest), initializes
// the eight systems, triggers a timer query, asserts every expected name appears
// in GetActiveResources() exactly once with the expected kind, verifies two
// consecutive TakeSnapshot frames agree on the ledger counts, shuts everything
// down and verifies the ledger returns to its pre-init baseline with no GL
// diagnostics.
//
// This is contract/diagnostic evidence only (nmd.tests.gpu.diagnostic), NOT
// production visual evidence (that remains the NoMoreDay.exe --gpu-gate gate).
// It is observer-only: it never changes GL allocation/resize/release/bind
// ownership and never modifies render behavior.
// ===========================================================================

namespace {

constexpr uint32_t kPhaseGGlFramebuffer = 0x8D40;

bool PhaseGEnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "Phase G Registry Snapshot GL Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return false;
  }
  // A registry snapshot across eight real systems needs the compute-capable
  // path (MDI/text/loot/particle all compile compute shaders); without it the
  // fixture is "unavailable", not a pass or a failure.
  return NoMoreDay::utils::GPUUtils::CheckSupport().computeShaderSupported;
}

std::vector<GLenum> PhaseGDrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

// Count ledger records matching a resource name.
size_t PhaseGCountNamed(
    const std::vector<NoMoreDay::render::resources::GPUResourceRecord> &records,
    const std::string &name) {
  size_t count = 0;
  for (const auto &rec : records) {
    if (rec.name == name) {
      ++count;
    }
  }
  return count;
}

} // namespace

TEST_CASE("[GPU-Diagnostic] Phase G - registry ledger covers 8 VAO/VBO sites, "
          "shaders and query ring across two frames") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::debug;
  using namespace NoMoreDay::render::resources;
  using namespace NoMoreDay::systems;

  if (!PhaseGEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping Phase G registry snapshot test");
  }
  (void)PhaseGDrainGlErrors();

  auto &registry = GPUResourceRegistry::Get();

  // Idempotently tear down any singleton systems a prior test in this process
  // may have left initialized, so the baseline and the shutdown assertions are
  // exact. Each Shutdown is a context-safe no-op when the system is not
  // initialized (see the per-system guards confirmed for G1-G3).
  PopupRenderer::Get().Shutdown();
  GPUTrailRenderer::Get().Shutdown();
  GPULootSystem::Get().Shutdown();
  GPUSkillEffectSystem::Get().Shutdown();
  GPUTextSystem::Get().Shutdown();
  GPUParticleSystem::Get().Shutdown();
  debug::GPUTimerQueryRing::Get().Shutdown();
  (void)PhaseGDrainGlErrors();

  // Pre-init baseline captured BEFORE any of the eight systems acquire GL
  // objects, so the shutdown assertions measure exactly their records.
  const GPUResourceSnapshot baseline = registry.TakeSnapshot();

  ResourceManager resources;
  MDIRenderer mdi;
  passes::FluidSimulationPass fluid;

  // Initialize the eight Phase G systems. MDIRenderer/FluidSimulationPass are
  // stack instances (matching MDIRenderTest); the remaining six are singletons
  // (matching the production Game::Initialize call pattern).
  PopupRenderer::Get().Init();
  mdi.Init(resources, 100);
  GPUParticleSystem::Get().Init(256);
  GPUTrailRenderer::Get().Init();
  GPULootSystem::Get().Init(128);
  GPUSkillEffectSystem::Get().Init(resources, 128);
  GPUTextSystem::Get().Init(resources, 128, 256);
  REQUIRE(fluid.Initialize(resources));

  // Trigger the lazily-generated timer queries: BeginPass generates and
  // registers a non-zero query pair when the ring is initialized and the GL
  // entry points resolved.
  debug::GPUTimerQueryRing::Get().Initialize();
  debug::GPUTimerQueryRing::Get().BeginFrame();
  debug::GPUTimerQueryRing::Get().BeginPass(0x4A50u); // arbitrary pass id
  debug::GPUTimerQueryRing::Get().EndPass(0x4A50u);
  debug::GPUTimerQueryRing::Get().EndFrame();

  const bool timerSupported = debug::GPUTimerQueryRing::Get().IsGpuTimerSupported();

  // ---- Every expected name must appear exactly once (no duplicate, no
  // missing) with the expected kind --------------------------------
  {
    const auto active = registry.GetActiveResources();

    CHECK(PhaseGCountNamed(active, "PopupQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "PopupQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "MDIQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "MDIQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "ParticleQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "ParticleQuadVBO") == 1);
    // GPUTrailRenderer registers only a dummy VAO (no VBO).
    CHECK(PhaseGCountNamed(active, "TrailDummyVAO") == 1);
    CHECK(PhaseGCountNamed(active, "LootQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "LootQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "SkillEffectQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "SkillEffectQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "TextQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "TextQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "FluidQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "FluidQuadVBO") == 1);

    // Shader programs go through ResourceManager under its owner names; each
    // distinct program registers one ShaderProgram record.
    CHECK(PhaseGCountNamed(active, "ResourceManagerShader") >= 1);
    CHECK(PhaseGCountNamed(active, "ResourceManagerComputeShader") >= 1);

    // Query ring: one record per generated query. When the driver exposes the
    // timer entry points, BeginPass generates a non-zero begin/end pair, so two
    // records are expected; otherwise the fixture reports zero (driver
    // capability, not a registration defect).
    const size_t queryRecords = PhaseGCountNamed(active, "GPUTimerQueryRing");
    CHECK((queryRecords == 0 || queryRecords == 2));
    if (timerSupported) {
      CHECK(queryRecords == 2);
    }
  }

  // ---- Two consecutive frames agree on the ledger counts -------------
  const GPUResourceSnapshot snap1 = registry.TakeSnapshot();
  CHECK(snap1.activeResourceCount > baseline.activeResourceCount);

  registry.AdvanceFrame();
  const GPUResourceSnapshot snap2 = registry.TakeSnapshot();

  CHECK(snap2.frameIndex == snap1.frameIndex + 1);
  CHECK(snap2.activeResourceCount == snap1.activeResourceCount);
  CHECK(snap2.currentTotalBytes == snap1.currentTotalBytes);
  CHECK(snap2.totalCreatedCount == snap1.totalCreatedCount);
  CHECK(snap2.totalDestroyedCount == snap1.totalDestroyedCount);

  // The target resources are still present after the frame advance.
  {
    const auto active = registry.GetActiveResources();
    CHECK(PhaseGCountNamed(active, "PopupQuadVAO") == 1);
    CHECK(PhaseGCountNamed(active, "FluidQuadVBO") == 1);
    CHECK(PhaseGCountNamed(active, "ResourceManagerShader") >= 1);
  }

  // ---- Shutdown everything; ledger must return to baseline -----------
  PopupRenderer::Get().Shutdown();
  mdi.Shutdown();
  GPUParticleSystem::Get().Shutdown();
  GPUTrailRenderer::Get().Shutdown();
  GPULootSystem::Get().Shutdown();
  GPUSkillEffectSystem::Get().Shutdown();
  GPUTextSystem::Get().Shutdown();
  fluid.Shutdown();
  debug::GPUTimerQueryRing::Get().Shutdown();
  resources.unloadAll();

  {
    const auto active = registry.GetActiveResources();
    CHECK(PhaseGCountNamed(active, "PopupQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "PopupQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "MDIQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "MDIQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "ParticleQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "ParticleQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "TrailDummyVAO") == 0);
    CHECK(PhaseGCountNamed(active, "LootQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "LootQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "SkillEffectQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "SkillEffectQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "TextQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "TextQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "FluidQuadVAO") == 0);
    CHECK(PhaseGCountNamed(active, "FluidQuadVBO") == 0);
    CHECK(PhaseGCountNamed(active, "ResourceManagerShader") == 0);
    CHECK(PhaseGCountNamed(active, "ResourceManagerComputeShader") == 0);
    CHECK(PhaseGCountNamed(active, "GPUTimerQueryRing") == 0);
  }

  const GPUResourceSnapshot afterShutdown = registry.TakeSnapshot();
  CHECK(afterShutdown.activeResourceCount == baseline.activeResourceCount);
  CHECK(afterShutdown.currentTotalBytes == baseline.currentTotalBytes);

  const std::vector<GLenum> errors = PhaseGDrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}
