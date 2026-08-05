// ResourceManager shader ownership contract (owner blocker, 2026-08-04):
//
//   Shaders returned by ResourceManager::loadShader / loadComputeShader are
//   OWNED by the ResourceManager. It registers them with the GPU resource
//   registry on load and is the SOLE entity allowed to release them (raylib
//   UnloadShader) during unloadAll() / the destructor. Consumers BORROW these
//   Shader objects and must never call UnloadShader themselves.
//
// The bug being pinned: ShadowBuildPass::Shutdown used to UnloadShader the
// compute/atlas shaders it had loaded through ResourceManager. Then
// RenderSystem::Shutdown + Game::cleanup kept the ids in the manager cache, so
// unloadAll() UnloadShader'd the same programs a second time ->
// RL_FREE(shader.locs) double-free -> heap corruption (0xC0000374). These
// tests prove, through the ownership-release ledger, that pass shutdown no
// longer releases manager-owned shaders and that manager teardown performs
// exactly one GL release per shader.
#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <entt/entt.hpp>

#include <vector>

namespace {

// Real (hidden) GL context guard: the test runner already initializes a
// process-wide context in tests/main.cpp, so this only fails loudly when the
// host truly has no GL (repo convention: explicit FAIL, never DOCTEST_SKIP).
bool RmEnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "ResourceManager Shader Ownership Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> RmDrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

}  // namespace

TEST_CASE(
    "[Unit] ResourceManager - shader ownership contract: pass shutdown must "
    "not release manager-owned shaders") {
  using namespace NoMoreDay;

  if (!RmEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping ResourceManager shader "
         "ownership teardown test");
  }
  (void)RmDrainGlErrors();

  const entt::id_type kSdfId = entt::hashed_string{"shadow_sdf_compute"}.value();
  const entt::id_type kTileId = entt::hashed_string{"shadow_atlas_tile"}.value();

  ResourceManager resources;
  CHECK(resources.GetShaderReleaseCount() == 0);
  CHECK(resources.GetShaderReleaseIds().empty());

  // The real production pass loads both shaders through the manager (borrow).
  render::passes::ShadowBuildPass pass;
  REQUIRE(pass.Initialize(resources));
  CHECK(resources.getShader(kSdfId).id != 0);
  CHECK(resources.getShader(kTileId).id != 0);
  CHECK(resources.GetShaderReleaseCount() == 0);

  // Pass shutdown must ONLY drop local references: no GL release, no registry
  // unregister, no ledger entry; the manager cache keeps both shaders alive.
  pass.Shutdown();
  CHECK(resources.getShader(kSdfId).id != 0);
  CHECK(resources.getShader(kTileId).id != 0);
  CHECK(resources.GetShaderReleaseCount() == 0);

  // Idempotent shutdown (destructor path) still must not release.
  pass.Shutdown();
  CHECK(resources.GetShaderReleaseCount() == 0);

  // Real owner teardown (Game::cleanup path): exactly-once GL release per
  // shader. The ledger is the evidence — not a "did not crash" claim.
  resources.unloadAll();
  CHECK(resources.GetShaderReleaseCount() == 2);
  CHECK(resources.getShader(kSdfId).id == 0);
  CHECK(resources.getShader(kTileId).id == 0);

  const std::vector<entt::id_type> expectedIds = {kSdfId, kTileId};
  CHECK(resources.GetShaderReleaseIds() == expectedIds);

  // A second teardown releases nothing more.
  resources.unloadAll();
  CHECK(resources.GetShaderReleaseCount() == 2);

  // The unload path itself left no GL error surface.
  const std::vector<GLenum> errors = RmDrainGlErrors();
  for (GLenum err : errors) {
    CAPTURE(err);
  }
  CHECK(errors.empty());
}

TEST_CASE("[Unit] ResourceManager - ReleaseShader fail-safe and idempotent") {
  ResourceManager resources;
  resources.SetHeadless(true);

  const entt::id_type kUnknown = entt::hashed_string{"never_loaded"}.value();
  const entt::id_type kDummy = entt::hashed_string{"headless_dummy"}.value();

  // Unknown id: fail-safe no-op.
  CHECK_FALSE(resources.ReleaseShader(kUnknown));
  CHECK(resources.GetShaderReleaseCount() == 0);

  Shader shader = resources.loadShader(kDummy, "vs", "fs");
  REQUIRE(shader.id != 0);  // headless dummy shader

  // First release: true, ledger records the ownership release, cache cleared.
  CHECK(resources.ReleaseShader(kDummy));
  CHECK(resources.GetShaderReleaseCount() == 1);
  CHECK(resources.getShader(kDummy).id == 0);

  // Releasing the same id again: idempotent no-op, no double release.
  CHECK_FALSE(resources.ReleaseShader(kDummy));
  CHECK(resources.GetShaderReleaseCount() == 1);

  // unloadAll must not release it a second time.
  resources.unloadAll();
  CHECK(resources.GetShaderReleaseCount() == 1);
  CHECK(resources.GetShaderReleaseIds() == std::vector<entt::id_type>{kDummy});
}
