#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <cstdint>
#include <string>
#include <vector>

// Phase D (D7): render-level smoke over the production RenderSystem::render
// path while toggling the HDR / GI / distortion config matrix. Exercises the
// unified RenderGraph::OnResize dispatch (GI re-enable gate fires it for the
// real GI pass nodes), the graph-derived composite input selection, and the
// exact-one AdvanceFrame anchor - with no GL errors across every toggle.
namespace {

bool PhaseDEnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "Phase D RenderSystem Toggle Smoke Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> PhaseDDrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

} // namespace

TEST_CASE("[Integration] RenderSystem Phase D - resize/toggle smoke across HDR/GI/distortion configs") {
  using namespace NoMoreDay;
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::render::core;

  if (!PhaseDEnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping Phase D toggle smoke");
  }
  (void)PhaseDDrainGlErrors();

  auto &qm = QualityTierManager::Get();
  qm.Initialize("settings.json");

  CHECK_NOTHROW(RenderSystem::Initialize());

  auto &cfg = const_cast<RenderConfig &>(qm.GetConfig());
  const bool originalV3 = cfg.v3Enabled;
  const bool originalDynamic = cfg.dynamicLightingEnabled;
  const bool originalBloom = cfg.bloomEnabled;
  const bool originalDistortion = cfg.distortionEnabled;
  const bool originalGi = cfg.giEnabled;
  const bool originalVolumetric = cfg.volumetricLightEnabled;
  const bool originalClustered = cfg.clusteredLightingEnabled;

  // Full HDR/GI/distortion matrix. Cluster reads stay gated (LightCullingPass
  // remains an observer) so the validation-clean graph is the same shape the
  // render path produces under this config.
  cfg.v3Enabled = true;
  cfg.clusteredLightingEnabled = false;
  cfg.dynamicLightingEnabled = true;
  cfg.bloomEnabled = true;
  cfg.distortionEnabled = true;
  cfg.giEnabled = true;
  cfg.volumetricLightEnabled = true;

  entt::registry registry;
  RenderFrameInput input;
  Camera2D camera{};
  camera.zoom = 1.0f;

  auto &resReg = resources::GPUResourceRegistry::Get();
  const uint64_t frameBefore = resReg.GetFrameIndex();

  const auto renderFrames = [&](int frames) {
    for (int f = 0; f < frames; ++f) {
      RenderSystem::render(registry, input, camera);
      const auto errors = PhaseDDrainGlErrors();
      for (GLenum err : errors) {
        CAPTURE(err);
      }
      CHECK(errors.empty());
    }
    const auto &order = RenderSystem::GetLastExecutedPassOrder();
    REQUIRE(!order.empty());
    CHECK_EQ(order.back(), "CompositePass");
  };

  // HDR + GI + distortion on. The GI re-enable gate fires the unified
  // graph.OnResize for the real GI pass nodes on the first frame.
  renderFrames(3);
  {
    const auto &order = RenderSystem::GetLastExecutedPassOrder();
    bool hasPostProcess = false;
    bool hasDistortion = false;
    bool hasGiComposite = false;
    for (const auto &passName : order) {
      if (passName == "PostProcessPass") {
        hasPostProcess = true;
      } else if (passName == "DistortionPass") {
        hasDistortion = true;
      } else if (passName == "GICompositePass") {
        hasGiComposite = true;
      }
    }
    CHECK(hasPostProcess);
    CHECK(hasDistortion);
    CHECK(hasGiComposite);
  }

  // GI off -> the GI chain leaves the graph; composite still ends the order.
  cfg.giEnabled = false;
  renderFrames(2);

  // HDR off -> minimal Scene/VFX/UIWorld/Composite graph.
  cfg.dynamicLightingEnabled = false;
  cfg.bloomEnabled = false;
  cfg.volumetricLightEnabled = false;
  renderFrames(2);

  // HDR back on (HDR buffer already exists) -> postprocess chain returns.
  cfg.dynamicLightingEnabled = true;
  cfg.bloomEnabled = true;
  renderFrames(2);

  // GI re-enable -> the s_giPassesSized gate was reset by the GI-off frame, so
  // the unified OnResize dispatch fires exactly once for the re-enabled chain.
  cfg.giEnabled = true;
  renderFrames(2);

  // Distortion off/on while the HDR chain is live.
  cfg.distortionEnabled = false;
  renderFrames(1);
  cfg.distortionEnabled = true;
  renderFrames(1);

  CHECK(resReg.GetFrameIndex() > frameBefore);

  // Restore the config baseline and tear down.
  cfg.v3Enabled = originalV3;
  cfg.clusteredLightingEnabled = originalClustered;
  cfg.dynamicLightingEnabled = originalDynamic;
  cfg.bloomEnabled = originalBloom;
  cfg.distortionEnabled = originalDistortion;
  cfg.giEnabled = originalGi;
  cfg.volumetricLightEnabled = originalVolumetric;

  RenderSystem::Shutdown();
  (void)PhaseDDrainGlErrors();
}
