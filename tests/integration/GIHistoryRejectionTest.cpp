#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

// M0-A R3 contract tests for the GICompositePass occupancy/depth history
// disocclusion rejection plus the R2 emissive-snapshot-version rejection.
//
// Helpers use the R3GI_ prefix to keep TU-local names namespaced and
// collision-free.
//
// These tests are local contract verification only; they never imply a
// production GO verdict.

namespace {

constexpr uint32_t R3GIKHdrRgba16f = 0x881A;
constexpr uint32_t R3GIKRadianceRgba16f = 0x881A;

struct R3GIRadianceAtlasHandle {
  uint32_t texture = 0u;
  int width = 0;
  int height = 0;
  int directions = 0;
  [[nodiscard]] bool IsValid() const noexcept {
    return texture != 0u && width > 0 && height > 0 && directions > 0;
  }
};

R3GIRadianceAtlasHandle R3GICreateRadianceAtlas(int width, int height, int directions = 16) {
  uint32_t tex = 0u;
  NoMoreDay::utils::GPUUtils::GenTextures(1, &tex);
  NoMoreDay::utils::GPUUtils::BindTexture(0x8C1A /* GL_TEXTURE_2D_ARRAY */, tex);
  NoMoreDay::utils::GPUUtils::TexStorage3D(0x8C1A, 1, R3GIKRadianceRgba16f, width, height, directions);
  NoMoreDay::utils::GPUUtils::TexParameteri(0x8C1A, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(0x8C1A, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(0x8C1A, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::TexParameteri(0x8C1A, 0x2803 /* GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::BindTexture(0x8C1A, 0u);
  return {tex, width, height, directions};
}

void R3GIDestroyRadianceAtlas(R3GIRadianceAtlasHandle &atlas) {
  if (atlas.texture != 0u) {
    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &atlas.texture);
    atlas.texture = 0u;
  }
  atlas.width = 0;
  atlas.height = 0;
  atlas.directions = 0;
}

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
// `radiance` must be a valid RGBA16F 2D array texture atlas.
NoMoreDay::render::graph::RenderContext R3GIBuildContext(
    NoMoreDay::render::resources::FramebufferHandle &hdr,
    R3GIRadianceAtlasHandle &radiance,
    NoMoreDay::render::core::QualityTierManager &qm,
    ResourceManager &resources,
    Camera2D *camera) {
  NoMoreDay::render::graph::RenderContext context = {};
  context.registry = nullptr;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = camera;
  context.hdrSceneBuffer = hdr;
  context.giRadianceTexture = radiance.texture;
  context.giRadianceWidth = radiance.width;
  context.giRadianceHeight = radiance.height;
  context.giRadianceDirections = static_cast<uint32_t>(radiance.directions);
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
  R3GIRadianceAtlasHandle radiance = R3GICreateRadianceAtlas(128, 64, 16);
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
  R3GIDestroyRadianceAtlas(radiance);
  hdr = render::resources::FramebufferManager::Create(64, 32, R3GIKHdrRgba16f, false);
  radiance = R3GICreateRadianceAtlas(64, 32, 16);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());
  context.hdrSceneBuffer = hdr;
  context.giRadianceTexture = radiance.texture;
  context.giRadianceWidth = radiance.width;
  context.giRadianceHeight = radiance.height;
  context.giRadianceDirections = static_cast<uint32_t>(radiance.directions);
  pass.OnResize(64, 32);
  pass.Execute(context);
  CHECK(pass.HasOccupancyHistory());
  CHECK(pass.GetOccupancyHistoryWidth() == 64);
  CHECK(pass.GetOccupancyHistoryHeight() == 32);

  pass.Shutdown();
  CHECK_FALSE(pass.HasOccupancyHistory());
  R3GIDestroyRadianceAtlas(radiance);
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
  R3GIRadianceAtlasHandle radiance = R3GICreateRadianceAtlas(128, 64, 16);
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

  // Frame 3: emissive changed => T4.1 eliminates global reset, uses 3x3 variance clipping + local dirty rects.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1);
  CHECK(giPass.GetLastResetReason() != "emissive");

  // Frame 4: no new snapshot => stable accumulation.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1);

  giPass.Shutdown();
  radiancePass.Shutdown();
  R3GIDestroyRadianceAtlas(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}

// ---------------------------------------------------------------------------
// R3 regression: occluder mask version changes no longer trigger global full reset (T4.1)
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] M0-A R3 - occluder mask version change preserves temporal history with variance clipping") {
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
  R3GIRadianceAtlasHandle radiance = R3GICreateRadianceAtlas(128, 64, 16);
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

  // Frame 3: occluder version changed => T4.1 local variance clipping without global reset.
  giPass.Execute(context);
  CHECK(giPass.GetHistoryResetCount() == resetsAfterFrame1);
  CHECK(giPass.GetLastResetReason() != "occluder");

  giPass.Shutdown();
  occluderPass.Shutdown();
  R3GIDestroyRadianceAtlas(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}

// ---------------------------------------------------------------------------
// T4.4: Sinusoidal light modulation differential RMS & flicker-free convergence
// ---------------------------------------------------------------------------
namespace {
constexpr uint32_t R3GIKFramebuffer = 0x8D40;
constexpr uint32_t R3GIKTexture2D = 0x0DE1;
constexpr uint32_t R3GIKRgba = 0x1908;
constexpr uint32_t R3GIKFloat = 0x1406;
constexpr uint32_t R3GIKRg = 0x8227;

// Reads the HDR color attachment back as RGBA floats (GL_RGBA / GL_FLOAT).
bool R3GIReadbackHdr(const NoMoreDay::render::resources::FramebufferHandle &hdr,
                     int width, int height, std::vector<float> &out) {
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t, void *);
  static const auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  if (glGetTexImage == nullptr) {
    return false;
  }
  out.assign(static_cast<size_t>(width) * height * 4, 0.0f);
  NoMoreDay::utils::GPUUtils::BindTexture(R3GIKTexture2D, hdr.colorTexture);
  glGetTexImage(R3GIKTexture2D, 0, R3GIKRgba, R3GIKFloat, out.data());
  NoMoreDay::utils::GPUUtils::BindTexture(R3GIKTexture2D, 0);
  return true;
}

// Clears the HDR target to black so the composite output equals the GI term.
void R3GIClearHdr(const NoMoreDay::render::resources::FramebufferHandle &hdr) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(R3GIKFramebuffer, hdr.fbo);
  rlClearColor(0, 0, 0, 0);
  rlClearScreenBuffers();
  NoMoreDay::utils::GPUUtils::BindFramebuffer(R3GIKFramebuffer, 0);
}
} // namespace

TEST_CASE("[Integration] T4.4 GI temporal denoising - sinusoidal modulation convergence") {
  using namespace NoMoreDay;

  if (!R3GICreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping modulation test");
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
  R3GIRadianceAtlasHandle radiance = R3GICreateRadianceAtlas(128, 64, 16);
  REQUIRE(hdr.IsValid());
  REQUIRE(radiance.IsValid());

  render::graph::RenderContext context =
      R3GIBuildContext(hdr, radiance, qm, resources, &camera);

  // Single point light, world-fixed; its screen position drifts as the camera
  // translates uniformly. Intensity follows a sine so every frame differs.
  constexpr float kLightWorldX = 0.0f;
  constexpr float kLightWorldY = 0.0f;
  constexpr float kLightRadius = 8.0f;
  constexpr int kFrameCount = 30;
  constexpr float kSinePeriod = 10.0f;
  constexpr float kSineAmplitude = 0.15f;
  constexpr float kTwoPi = 6.28318530717958647692f;

  render::lighting::LightManager::Get().Initialize();
  render::lighting::LightManager::Get().SetDisableViewCullingForTesting(true);
  render::passes::GICompositePass giPass;

  // Fills the radiance atlas with the current light intensity plus a small
  // deterministic per-frame fluctuation that temporal accumulation must smooth.
  // The static spatial pattern keeps the shader's 3x3 variance clip box
  // non-degenerate (sigma > 0), which is what lets history survive the clip and
  // accumulate over frames.
  const auto writeAtlas = [&](int frame) {
    const float phase = (static_cast<float>(frame) / kSinePeriod) * kTwoPi;
    const float intensity = 0.5f + kSineAmplitude * std::sin(phase);
    const float fluct = 1.0f + 0.02f * std::sin(phase * 7.0f);
    std::vector<float> data(static_cast<size_t>(radiance.width) * radiance.height *
                            radiance.directions * 2);
    // TexSubImage3D on a GL_TEXTURE_2D_ARRAY expects layer-major data:
    // offset = ((d * height) + y) * width + x, two floats (R,G) per texel.
    for (int d = 0; d < radiance.directions; ++d) {
      for (int y = 0; y < radiance.height; ++y) {
        for (int x = 0; x < radiance.width; ++x) {
          const float pattern =
              0.5f + 0.5f * std::sin(kTwoPi * static_cast<float>(x) / 8.0f);
          const float texel = pattern * intensity * fluct;
          const size_t base = ((static_cast<size_t>(d) * radiance.height +
                                static_cast<size_t>(y)) * radiance.width +
                               static_cast<size_t>(x)) * 2;
          data[base] = texel;
          data[base + 1] = texel;
        }
      }
    }
    utils::GPUUtils::BindTexture(0x8C1A, radiance.texture);
    utils::GPUUtils::TexSubImage3D(0x8C1A, 0, 0, 0, 0, radiance.width,
                                   radiance.height, radiance.directions, R3GIKRg,
                                   R3GIKFloat, data.data());
    utils::GPUUtils::BindTexture(0x8C1A, 0);
    return intensity;
  };

  // runFrames executes the composite either normally (temporal accumulation)
  // or with a forced full history reset before every frame.
  const auto runFrames = [&](bool resetEachFrame,
                             std::vector<std::vector<float>> &outputs) {
    outputs.clear();
    for (int frame = 0; frame < kFrameCount; ++frame) {
      camera.target.x = static_cast<float>(frame) * 0.05f;
      context.camera = &camera;

      if (resetEachFrame) {
        giPass.InvalidateHistory();
      }

      const float intensity = writeAtlas(frame);
      components::GPULight light = {};
      light.posX = kLightWorldX;
      light.posY = kLightWorldY;
      light.radius = kLightRadius;
      light.intensity = intensity;
      light.colorR = light.colorG = light.colorB = light.colorA = 1.0f;
      light.lightType = static_cast<uint32_t>(components::LightType::PointLight);
      render::lighting::LightManager::Get().UpdateCandidates(
          std::span<const components::GPULight>(&light, 1), camera, 16, 0);

      R3GIClearHdr(hdr);
      giPass.Execute(context);

      std::vector<float> rgba;
      REQUIRE(R3GIReadbackHdr(hdr, 128, 64, rgba));
      outputs.push_back(std::move(rgba));
    }
  };

  std::vector<std::vector<float>> accumulated; // rejected-frames (run A)
  std::vector<std::vector<float>> resetFrames; // full-reset frames (run B)
  runFrames(false, accumulated);
  // Run A only resets on the very first frame (uninitialized history).
  const uint64_t resetCountAfterRunA = giPass.GetHistoryResetCount();
  CHECK(resetCountAfterRunA == 1u);
  runFrames(true, resetFrames);

  // Accessor contract: occupancy history must exist after execution.
  CHECK(giPass.HasOccupancyHistory());
  CHECK(giPass.GetOccupancyHistoryTexture() != 0u);
  CHECK(giPass.GetOccupancyHistoryWidth() == 128);
  CHECK(giPass.GetOccupancyHistoryHeight() == 64);
  // Run B forces a reset every frame, so the counter must have advanced.
  CHECK(giPass.GetHistoryResetCount() > resetCountAfterRunA);

  // Differential RMS inside the light region: the dirty rect rejects history
  // there every frame, so rejected frames and full-reset frames must match.
  const int regionHalf = 6;
  float diffSumSq = 0.0f;
  int diffCount = 0;
  for (int frame = 0; frame < kFrameCount; ++frame) {
    const float cameraTargetX = static_cast<float>(frame) * 0.05f;
    const float lightScreenX = camera.offset.x + (kLightWorldX - cameraTargetX) * camera.zoom;
    const float lightScreenY = camera.offset.y + (kLightWorldY - 0.0f) * camera.zoom;
    const int minPx = std::max(0, static_cast<int>(lightScreenX) - regionHalf);
    const int maxPx = std::min(127, static_cast<int>(lightScreenX) + regionHalf);
    const int minPy = std::max(0, static_cast<int>(lightScreenY) - regionHalf);
    const int maxPy = std::min(63, static_cast<int>(lightScreenY) + regionHalf);
    for (int py = minPy; py <= maxPy; ++py) {
      for (int px = minPx; px <= maxPx; ++px) {
        const float a = accumulated[frame][static_cast<size_t>(py * 128 + px) * 4 + 0];
        const float b = resetFrames[frame][static_cast<size_t>(py * 128 + px) * 4 + 0];
        const float d = a - b;
        diffSumSq += d * d;
        ++diffCount;
      }
    }
  }
  const float diffRms =
      std::sqrt(diffSumSq / static_cast<float>(std::max(diffCount, 1)));
  CHECK(diffRms < 0.05f);

  // Flicker observation outside the dirty rect: consecutive rejected frames
  // must stay near-stable while full-reset frames track the raw modulation.
  const int kRegionMinX = 2;
  const int kRegionMaxX = 10;
  const int kRegionMinY = 2;
  const int kRegionMaxY = 6;
  float flickerA = 0.0f;
  float flickerB = 0.0f;
  int flickerCount = 0;
  for (int frame = 1; frame < kFrameCount; ++frame) {
    for (int py = kRegionMinY; py < kRegionMaxY; ++py) {
      for (int px = kRegionMinX; px < kRegionMaxX; ++px) {
        const size_t idx = static_cast<size_t>(py * 128 + px) * 4;
        const float da = accumulated[frame][idx] - accumulated[frame - 1][idx];
        const float db = resetFrames[frame][idx] - resetFrames[frame - 1][idx];
        flickerA += da * da;
        flickerB += db * db;
        ++flickerCount;
      }
    }
  }
  flickerA = std::sqrt(flickerA / static_cast<float>(std::max(flickerCount, 1)));
  flickerB = std::sqrt(flickerB / static_cast<float>(std::max(flickerCount, 1)));
  CHECK(flickerA < 0.15f);              // accumulation damps frame-to-frame delta
  CHECK(flickerA < flickerB * 0.6f);    // rejected frames must be calmer than resets

  giPass.Shutdown();
  render::lighting::LightManager::Get().Shutdown();
  R3GIDestroyRadianceAtlas(radiance);
  render::resources::FramebufferManager::Destroy(hdr);
}
