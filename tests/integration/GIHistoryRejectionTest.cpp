#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <cstdint>

// M0-A R3 contract tests for the GICompositePass occupancy/depth history
// disocclusion rejection plus the R2 emissive-snapshot-version rejection.
//
// NOTE: this file is compiled under UNITY_BUILD (tests/CMakeLists.txt does not
// exclude it), so all helpers below use the R3GI_ prefix to avoid collisions
// with other translation units merged into the same unity TU.
//
// These tests are local contract verification only; they never imply a
// production GO verdict.

namespace {

constexpr uint32_t R3GIKHdrRgba16f = 0x881A;
constexpr uint32_t R3GIKRadianceRgba16f = 0x881A;

bool R3GICreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "GI History Rejection Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

// A minimal executable render context for GICompositePass::Execute.
// `radiance` must be a valid RGBA16F framebuffer.
NoMoreDay::render::graph::RenderContext R3GIBuildContext(
    NoMoreDay::render::resources::FramebufferHandle &hdr,
    NoMoreDay::render::resources::FramebufferHandle &radiance,
    NoMoreDay::render::core::QualityTierManager &qm,
    ResourceManager &resources,
    Camera2D *camera) {
  NoMoreDay::render::graph::RenderContext context = {};
  context.registry = nullptr;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = camera;
  context.hdrSceneBuffer = hdr;
  context.giRadianceTexture = radiance.colorTexture;
  context.giRadianceWidth = radiance.width;
  context.giRadianceHeight = radiance.height;
  return context;
}

} // namespace

// ---------------------------------------------------------------------------
// R3 occupancy/depth history resource contract: after a successful Execute the
// pass owns a persistent R8 ping-pong occupancy history sized to the HDR
// buffer, and a resize updates both the metadata and the underlying texture.
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] M0-A R3 - GICompositePass occupancy history resource contract") {
  using namespace NoMoreDay;

  if (!R3GICreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping occupancy resource test");
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  cfg.giEnabled = true;
  cfg.giTemporalWeight = 0.9f;
  cfg.giIntensity = 1.0f;
  cfg.giCascadeLevels = 4u;

  ResourceManager resources;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {64.0f, 32.0f};

  render::resources::FramebufferHandle hdr =
      render::resources::FramebufferManager::Create(128, 64, R3GIKHdrRgba16f, false);
  render::resources::FramebufferHandle radiance =
      render::resources::FramebufferManager::Create(128, 64, R3GIKRadianceRgba16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());

  render::graph::RenderContext context =
      R3GIBuildContext(hdr, radiance, qm, resources, &camera);

  render::passes::GICompositePass pass;
  // First execute: no occluder mask, occupancy tracking stays disabled but the
  // history resources must still be created and stay persistent.
  pass.Execute(context);
  CHECK(pass.HasOccupancyHistory());
  CHECK(pass.GetOccupancyHistoryTexture() != 0u);
  CHECK(pass.GetOccupancyHistoryWidth() == 128);
  CHECK(pass.GetOccupancyHistoryHeight() == 64);
  CHECK(pass.GetHistoryResetCount() >= 1u); // fresh start / extent

  // Resize the HDR buffer and verify the occupancy history follows.
  render::resources::FramebufferManager::Destroy(hdr);
  render::resources::FramebufferManager::Destroy(radiance);
  hdr = render::resources::FramebufferManager::Create(64, 32, R3GIKHdrRgba16f, false);
  radiance = render::resources::FramebufferManager::Create(64, 32, R3GIKRadianceRgba16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());
  context.hdrSceneBuffer = hdr;
  context.giRadianceTexture = radiance.colorTexture;
  context.giRadianceWidth = radiance.width;
  context.giRadianceHeight = radiance.height;
  pass.OnResize(64, 32);
  pass.Execute(context);
  CHECK(pass.HasOccupancyHistory());
  CHECK(pass.GetOccupancyHistoryWidth() == 64);
  CHECK(pass.GetOccupancyHistoryHeight() == 32);

  pass.Shutdown();
  CHECK_FALSE(pass.HasOccupancyHistory());
  render::resources::FramebufferManager::Destroy(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}

// ---------------------------------------------------------------------------
// R2 closure: the VFX emission snapshot version participates in history
// rejection. When the radiance pass produces a new frozen emission snapshot,
// the next composite must reset its temporal history.
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] M0-A R2 closure - emissive snapshot version change rejects history") {
  using namespace NoMoreDay;

  if (!R3GICreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping emissive rejection test");
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  cfg.giEnabled = true;
  cfg.giTemporalWeight = 0.9f;
  cfg.giIntensity = 1.0f;
  cfg.giCascadeLevels = 4u;

  ResourceManager resources;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {64.0f, 32.0f};

  render::resources::FramebufferHandle hdr =
      render::resources::FramebufferManager::Create(128, 64, R3GIKHdrRgba16f, false);
  render::resources::FramebufferHandle radiance =
      render::resources::FramebufferManager::Create(128, 64, R3GIKRadianceRgba16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());

  render::graph::RenderContext context =
      R3GIBuildContext(hdr, radiance, qm, resources, &camera);

  render::passes::RadianceCascadesPass radiancePass;
  render::passes::GICompositePass giPass;
  giPass.SetRadianceCascadesPass(&radiancePass);

  // Frame 1: fresh start, always a reset (extent/initial).
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() >= 1u);
  const uint64_t resetsAfterFrame1 = giPass.GetHistoryResetCount();

  // Frame 2: no new snapshot version, no other change => no reset.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1);
  CHECK(giPass.GetLastResetReason() != "emissive");

  // A new frozen VFX emission snapshot increments the radiance pass version.
  const uint64_t versionBefore = radiancePass.GetVfxEmissionSnapshotVersion();
  CHECK(radiancePass.PrepareVfxEmissionSnapshot(context));
  CHECK(radiancePass.GetVfxEmissionSnapshotVersion() == versionBefore + 1u);

  // Frame 3: emissive changed => history must be rejected.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1 + 1u);
  CHECK(giPass.GetLastResetReason() == "emissive");

  // Frame 4: no new snapshot => no reset again.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1 + 1u);

  giPass.Shutdown();
  radiancePass.Shutdown();
  render::resources::FramebufferManager::Destroy(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}

// ---------------------------------------------------------------------------
// R3 regression: occluder mask version changes still reject history (existing
// R2 behavior must not regress when occupancy tracking is wired up).
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] M0-A R3 - occluder mask version change rejects history (R2 regression)") {
  using namespace NoMoreDay;

  if (!R3GICreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping occluder rejection test");
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  cfg.giEnabled = true;
  cfg.giTemporalWeight = 0.9f;
  cfg.giIntensity = 1.0f;
  cfg.giCascadeLevels = 4u;

  ResourceManager resources;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {64.0f, 32.0f};

  render::resources::FramebufferHandle hdr =
      render::resources::FramebufferManager::Create(128, 64, R3GIKHdrRgba16f, false);
  render::resources::FramebufferHandle radiance =
      render::resources::FramebufferManager::Create(128, 64, R3GIKRadianceRgba16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());

  render::graph::RenderContext context =
      R3GIBuildContext(hdr, radiance, qm, resources, &camera);

  // One occluder so OccluderExtractPass can build a mask and advance version.
  NoMoreDay::components::GPUShadowCaster occluders[1] = {};
  occluders[0].posX = 10.0f;
  occluders[0].posY = 10.0f;
  occluders[0].radius = 12.0f;
  occluders[0].occluderHeight = 20.0f;
  occluders[0].shapeIndex = 0u;
  occluders[0].dynamicFlag = 1u;
  context.occluders = occluders;
  context.occluderCount = 1u;
  context.occluderStaticCount = 0u;
  context.occluderDynamicCount = 1u;
  context.occluderStaticSignature = 0u;
  context.occluderDynamicSignature = 12345u;

  render::passes::OccluderExtractPass occluderPass;
  render::passes::GICompositePass giPass;
  giPass.SetOccluderExtractPass(&occluderPass);

  // Build the mask once so GetOccluderMaskTexture() is valid for occupancy.
  occluderPass.Execute(context);
  REQUIRE(occluderPass.HasOccluderMask());
  const uint64_t maskVersionAfterBuild = occluderPass.GetMaskVersion();
  CHECK(maskVersionAfterBuild >= 2u);

  // Frame 1: fresh start => reset; occupancy tracking becomes active.
  giPass.Execute(context);
  CHECK(giPass.HasOccupancyHistory());
  const uint64_t resetsAfterFrame1 = giPass.GetHistoryResetCount();

  // Frame 2: no changes => no reset.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1);

  // Change the dynamic occluder signature and rebuild the mask.
  context.occluderDynamicSignature = 67890u;
  occluderPass.Execute(context);
  CHECK(occluderPass.GetMaskVersion() > maskVersionAfterBuild);

  // Frame 3: occluder version changed => history must be rejected.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1 + 1u);
  CHECK(giPass.GetLastResetReason() == "occluder");

  giPass.Shutdown();
  occluderPass.Shutdown();
  render::resources::FramebufferManager::Destroy(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}
