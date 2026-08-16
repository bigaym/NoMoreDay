#include "doctest.h"

#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUData.hpp"
#include <raylib.h>
#include <vector>

using namespace NoMoreDay::systems;

TEST_CASE("[Unit] GPUParticleSystem - Uninitialized Fail-Closed State") {
  auto &ps = GPUParticleSystem::Get();

  // Ensure particle system is shut down initially
  ps.Shutdown();
  REQUIRE_FALSE(ps.IsInitialized());

  // Operations on uninitialized system should be safe, no-op, and non-crashing
  CHECK_NOTHROW(ps.Update(0.016f));

  Camera2D camera = {};
  camera.zoom = 1.0f;
  CHECK_NOTHROW(ps.Render(camera));

  CHECK_NOTHROW(ps.Clear());

  NoMoreDay::components::GPUParticle p = {};
  CHECK_NOTHROW(ps.Emit(p));
  CHECK_NOTHROW(ps.Emit(p, 1));

  std::vector<NoMoreDay::components::GPUParticle> batch = {p, p};
  CHECK_NOTHROW(ps.EmitBatch(batch));
  CHECK_NOTHROW(ps.EmitBatch(batch, 1));

  CHECK_FALSE(ps.IsInitialized());
}

TEST_CASE("[Unit] GPUParticleSystem - Idempotent Shutdown") {
  auto &ps = GPUParticleSystem::Get();

  // Ensure shutdown from uninitialized state
  ps.Shutdown();
  CHECK_FALSE(ps.IsInitialized());

  // Calling Shutdown multiple times should be safe and idempotent
  CHECK_NOTHROW(ps.Shutdown());
  CHECK_NOTHROW(ps.Shutdown());
  CHECK_NOTHROW(ps.Shutdown());
  CHECK_FALSE(ps.IsInitialized());
}

TEST_CASE("[Unit] GPUParticleSystem - Direct Shader Failure Injection Fail-Closed Init") {
  auto &ps = GPUParticleSystem::Get();

  ps.Shutdown();
  CHECK_FALSE(ps.IsInitialized());

  // Inject explicit shader loading failure via test seam
  ps.SetFailShadersForTesting(true);

  // Attempt Init -> must fail closed unconditionally regardless of host GL environment
  ps.Init(1000);
  CHECK_FALSE(ps.IsInitialized());

  // In fail-closed state, all runtime methods must remain safe no-ops
  CHECK_NOTHROW(ps.Update(0.016f));
  Camera2D camera = {};
  camera.zoom = 1.0f;
  CHECK_NOTHROW(ps.Render(camera));
  CHECK_NOTHROW(ps.Clear());

  // Post-failure shutdown must be clean and idempotent
  CHECK_NOTHROW(ps.Shutdown());
  CHECK_NOTHROW(ps.Shutdown());
  CHECK_FALSE(ps.IsInitialized());

  // Reset test seam
  ps.SetFailShadersForTesting(false);
}
