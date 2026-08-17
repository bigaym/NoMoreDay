// ---------------------------------------------------------------------------
// T3.5: Directional radiance atlas integration tests (plan Group 3, AD-3).
//
// Covers:
//  (a) 45-degree side-illumination directional spill: a single radiance
//      direction below-right lights surfaces whose height-field normal faces
//      it and leaves surfaces facing away dark;
//  (b) cosine-weighted L0 aggregation: E(x,n) = (1/N) * sum L0(x, omega_k) *
//      max(0, n . omega_k), verified numerically against shader readback;
//  (c) Low/Medium 1-sector fallback: raysPerProbe per tier is validated
//      statically (ResolveRaysPerProbe) and through the full cascade pass
//      (atlas depth per tier via GetCascadeTarget(0).directions), plus the
//      context wiring that feeds GICompositePass's uRaysPerProbe.
//
// What the tests verify is stated per test; atlas-level readback is exact
// (uniform content, linear filtering at texel centers), tier tests assert at
// the parameter/state level when the full trace chain is not exercised.
//
// Helpers use the R3D_ prefix to keep TU-local names collision-free.
// ---------------------------------------------------------------------------

#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t R3DKHdrRgba16f = 0x881A;
constexpr uint32_t R3DKRgba16f = 0x881A;
constexpr uint32_t R3DKR16f = 0x822D;
constexpr uint32_t R3DKR8 = 0x8229;
constexpr uint32_t R3DKTexture2DArray = 0x8C1A;
constexpr uint32_t R3DKTexture2D = 0x0DE1;
constexpr uint32_t R3DKGlRg = 0x8227;
constexpr uint32_t R3DKGlRed = 0x1903;
constexpr uint32_t R3DKGlRgba = 0x1908;
constexpr uint32_t R3DKGlFloat = 0x1406;
constexpr uint32_t R3DKGlUnsignedByte = 0x1401;
constexpr uint32_t R3DKFramebuffer = 0x8D40;

struct R3DRadianceAtlas {
  uint32_t texture = 0u;
  int width = 0;
  int height = 0;
  int directions = 0;
  [[nodiscard]] bool IsValid() const noexcept {
    return texture != 0u && width > 0 && height > 0 && directions > 0;
  }
};

struct R3DHeightField {
  uint32_t texture = 0u;
  int width = 0;
  int height = 0;
  [[nodiscard]] bool IsValid() const noexcept {
    return texture != 0u && width > 0 && height > 0;
  }
};

bool R3DCreateMinimalGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "Radiance Directional Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

R3DRadianceAtlas R3DCreateRadianceAtlas(int width, int height, int directions) {
  uint32_t tex = 0u;
  NoMoreDay::utils::GPUUtils::GenTextures(1, &tex);
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2DArray, tex);
  NoMoreDay::utils::GPUUtils::TexStorage3D(R3DKTexture2DArray, 1, R3DKRgba16f, width, height, directions);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2DArray, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2DArray, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2DArray, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2DArray, 0x2803 /* GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2DArray, 0u);
  return {tex, width, height, directions};
}

void R3DDestroyRadianceAtlas(R3DRadianceAtlas &atlas) {
  if (atlas.texture != 0u) {
    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &atlas.texture);
    atlas.texture = 0u;
  }
  atlas.width = 0;
  atlas.height = 0;
  atlas.directions = 0;
}

// Builds an R8 height field whose gradient yields the surface normal (nx, ny):
//   h(x, y) = clamp(128 - s*(nx*x + ny*y), 0, 255)
// The composite shader computes grad = (hR - hL, hU - hD) = (-s*nx, -s*ny) and
// normal = normalize(-grad) = normalize(nx, ny). s keeps the central region
// away from the clamp so the gradient is uniform there.
R3DHeightField R3DCreateHeightField(int width, int height, float nx, float ny) {
  constexpr float kScale = 2.0f;
  std::vector<uint8_t> data(static_cast<size_t>(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float h = 128.0f - kScale * (nx * static_cast<float>(x) +
                                         ny * static_cast<float>(y));
      data[static_cast<size_t>(y * width) + static_cast<size_t>(x)] =
          static_cast<uint8_t>(std::clamp(h, 0.0f, 255.0f));
    }
  }
  uint32_t tex = 0u;
  NoMoreDay::utils::GPUUtils::GenTextures(1, &tex);
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2D, tex);
  NoMoreDay::utils::GPUUtils::TexImage2D(R3DKTexture2D, 0, R3DKR8, width, height, 0,
                                         R3DKGlRed, R3DKGlUnsignedByte, data.data());
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2601 /* GL_LINEAR */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2803 /* GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2D, 0u);
  return {tex, width, height};
}

void R3DDestroyHeightField(R3DHeightField &field) {
  if (field.texture != 0u) {
    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &field.texture);
    field.texture = 0u;
  }
  field.width = 0;
  field.height = 0;
}

bool R3DReadbackHdr(const NoMoreDay::render::resources::FramebufferHandle &hdr,
                    int width, int height, std::vector<float> &out) {
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t, void *);
  static const auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  if (glGetTexImage == nullptr) {
    return false;
  }
  out.assign(static_cast<size_t>(width) * height * 4, 0.0f);
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2D, hdr.colorTexture);
  glGetTexImage(R3DKTexture2D, 0, R3DKGlRgba, R3DKGlFloat, out.data());
  NoMoreDay::utils::GPUUtils::BindTexture(R3DKTexture2D, 0);
  return true;
}

void R3DClearHdr(const NoMoreDay::render::resources::FramebufferHandle &hdr) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(R3DKFramebuffer, hdr.fbo);
  rlClearColor(0, 0, 0, 0);
  rlClearScreenBuffers();
  NoMoreDay::utils::GPUUtils::BindFramebuffer(R3DKFramebuffer, 0);
}

} // namespace

// ---------------------------------------------------------------------------
// T3.5 (a)+(b): cosine-weighted L0 aggregation with height-field normal.
//
// Radiance exists only in direction 0, which points 45 degrees below-right:
//   omega_0 = (cos(pi/4), sin(pi/4)).
// Expected irradiance (uniform content, giIntensity = 1):
//   n aligned with omega_0: E = (1/4) * 1 * max(0, 1)         = 0.25
//   n facing down:          E = (1/4) * 1 * max(0, cos45)     = 0.25 * cos45
//   n facing up (away):     E = (1/4) * 1 * max(0, -cos45)    = 0
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] T3.5 GI composite - cosine-weighted directional aggregation") {
  using namespace NoMoreDay;

  if (!R3DCreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping aggregation test");
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::High);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  cfg.giEnabled = true;
  cfg.giTemporalWeight = 0.9f;
  cfg.giIntensity = 1.0f;
  cfg.giCascadeLevels = 4u;

  constexpr int kWidth = 64;
  constexpr int kHeight = 32;
  constexpr int kDirections = 4;

  ResourceManager resources;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {kWidth / 2.0f, kHeight / 2.0f};

  render::resources::FramebufferHandle hdr =
      render::resources::FramebufferManager::Create(kWidth, kHeight, R3DKHdrRgba16f, false);
  REQUIRE(hdr.IsValid());

  R3DRadianceAtlas atlas = R3DCreateRadianceAtlas(kWidth, kHeight, kDirections);
  REQUIRE(atlas.IsValid());

  // Direction 0 carries radiance (1, 1); directions 1..3 are dark.
  // TexSubImage3D on a GL_TEXTURE_2D_ARRAY expects layer-major data:
  // offset = ((d * height) + y) * width + x, two floats (R,G) per texel.
  {
    std::vector<float> data(static_cast<size_t>(kWidth) * kHeight * kDirections * 2, 0.0f);
    for (int d = 0; d < kDirections; ++d) {
      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
          const size_t base = ((static_cast<size_t>(d) * kHeight + static_cast<size_t>(y)) *
                               kWidth + static_cast<size_t>(x)) * 2;
          data[base + 0] = (d == 0) ? 1.0f : 0.0f; // dir0.r
          data[base + 1] = (d == 0) ? 1.0f : 0.0f; // dir0.g
        }
      }
    }
    utils::GPUUtils::BindTexture(R3DKTexture2DArray, atlas.texture);
    utils::GPUUtils::TexSubImage3D(R3DKTexture2DArray, 0, 0, 0, 0, kWidth, kHeight,
                                   kDirections, R3DKGlRg, R3DKGlFloat, data.data());
    utils::GPUUtils::BindTexture(R3DKTexture2DArray, 0u);
  }

  // Executes one fresh composite frame with the given height-field normal and
  // returns the mean red channel over the central region (uniform gradient).
  const auto runWithNormal = [&](float nx, float ny) {
    R3DHeightField heightField = R3DCreateHeightField(kWidth, kHeight, nx, ny);
    REQUIRE(heightField.IsValid());

    render::graph::RenderContext context = {};
    context.registry = nullptr;
    context.resources = &resources;
    context.qualityManager = &qm;
    context.camera = &camera;
    context.hdrSceneBuffer = hdr;
    context.giRadianceTexture = atlas.texture;
    context.giRadianceWidth = atlas.width;
    context.giRadianceHeight = atlas.height;
    context.giRadianceDirections = static_cast<uint32_t>(atlas.directions);
    context.heightFieldTexture = heightField.texture;

    render::passes::GICompositePass giPass;
    R3DClearHdr(hdr);
    giPass.Execute(context);
    giPass.Shutdown();

    std::vector<float> rgba;
    REQUIRE(R3DReadbackHdr(hdr, kWidth, kHeight, rgba));

    R3DDestroyHeightField(heightField);

    float sum = 0.0f;
    int count = 0;
    for (int y = 8; y < 24; ++y) {
      for (int x = 16; x < 48; ++x) {
        sum += rgba[static_cast<size_t>(y * kWidth + x) * 4 + 0];
        ++count;
      }
    }
    return sum / static_cast<float>(count);
  };

  constexpr float kCos45 = 0.70710678f;

  // Surface normal aligned with the light direction: cosWeight = 1.
  const float aligned = runWithNormal(kCos45, kCos45);
  CHECK(std::abs(aligned - 0.25f) < 0.02f);

  // Surface facing the light (normal straight down): cosWeight = cos45.
  const float facingDown = runWithNormal(0.0f, 1.0f);
  CHECK(std::abs(facingDown - 0.25f * kCos45) < 0.02f);

  // Surface facing away (normal straight up): cosWeight = 0.
  const float facingUp = runWithNormal(0.0f, -1.0f);
  CHECK(std::abs(facingUp) < 0.02f);

  // Directional spill: irradiance must fall toward the light direction.
  CHECK(facingDown > facingUp + 0.01f);
  CHECK(aligned > facingDown);

  R3DDestroyRadianceAtlas(atlas);
  render::resources::FramebufferManager::Destroy(hdr);
}

// ---------------------------------------------------------------------------
// T3.5 (c): Low/Medium 1-sector fallback.
//
// Static policy (ResolveRaysPerProbe) is asserted unconditionally; the full
// cascade pass is then driven per tier to verify the atlas is actually created
// with the expected direction count and that the shared context carries the
// same count for GICompositePass's uRaysPerProbe when the trace chain runs.
// ---------------------------------------------------------------------------
TEST_CASE("[Integration] T3.5 RC - Low/Medium 1-sector fallback (rays per probe per tier)") {
  using namespace NoMoreDay;

  using render::core::QualityTier;
  using render::passes::RadianceCascadesPass;

  // Static tier policy (level 0 atlas direction count).
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(0u, 4u, QualityTier::Low) == 1u);
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(0u, 4u, QualityTier::Medium) == 1u);
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(0u, 4u, QualityTier::High) == 4u);
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(0u, 4u, QualityTier::Ultra) == 16u);
  // Higher cascade levels halve the direction count (High: 4u>>level, Ultra: 16u>>level).
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(1u, 4u, QualityTier::High) == 2u);
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(1u, 4u, QualityTier::Ultra) == 8u);
  CHECK(RadianceCascadesPass::ResolveRaysPerProbe(2u, 4u, QualityTier::Ultra) == 4u);

  if (!R3DCreateMinimalGpuContext()) {
    FAIL("Cannot create GPU context; skipping tier test");
  }

  auto &qm = render::core::QualityTierManager::Get();
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());

  ResourceManager resources;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {32.0f, 16.0f};

  render::resources::FramebufferHandle hdr =
      render::resources::FramebufferManager::Create(64, 32, R3DKHdrRgba16f, false);
  REQUIRE(hdr.IsValid());

  // R16F distance field; zero content only needs to exist for the trace gate.
  uint32_t distanceField = 0u;
  utils::GPUUtils::GenTextures(1, &distanceField);
  utils::GPUUtils::BindTexture(R3DKTexture2D, distanceField);
  std::vector<float> zeroDf(64u * 32u, 0.0f);
  utils::GPUUtils::TexImage2D(R3DKTexture2D, 0, R3DKR16f, 64, 32, 0, R3DKGlRed,
                              R3DKGlFloat, zeroDf.data());
  utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2802 /* GL_TEXTURE_WRAP_S */, 0x812F /* GL_CLAMP_TO_EDGE */);
  utils::GPUUtils::TexParameteri(R3DKTexture2D, 0x2803 /* GL_TEXTURE_WRAP_T */, 0x812F /* GL_CLAMP_TO_EDGE */);
  utils::GPUUtils::BindTexture(R3DKTexture2D, 0u);

  render::lighting::LightManager::Get().Initialize();

  const struct TierExpectation {
    QualityTier tier;
    uint32_t expectedDirections;
  } kTierExpectations[] = {
      {QualityTier::Low, 1u},
      {QualityTier::Medium, 1u},
      {QualityTier::High, 4u},
      {QualityTier::Ultra, 16u},
  };

  for (const auto &expectation : kTierExpectations) {
    qm.ForceTier(expectation.tier);
    // ForceTier re-applies the tier defaults; re-enable GI for the trace path.
    cfg.giEnabled = true;
    cfg.giCascadeLevels = 4u;
    cfg.giHalfResolution = false;

    render::graph::RenderContext context = {};
    context.registry = nullptr;
    context.resources = &resources;
    context.qualityManager = &qm;
    context.camera = &camera;
    context.hdrSceneBuffer = hdr;
    context.giDistanceFieldTexture = distanceField;

    render::passes::RadianceCascadesPass radiancePass;
    REQUIRE(radiancePass.PrepareVfxEmissionSnapshot(context));
    radiancePass.Execute(context);

    // Atlas depth per tier (created in EnsureResources regardless of trace
    // shader success, so this is the primary state-level assertion).
    REQUIRE(radiancePass.GetCascadeTarget(0).IsValid());
    CHECK(radiancePass.GetCascadeTarget(0).directions == expectation.expectedDirections);
    CHECK(radiancePass.GetRadianceDirections() == expectation.expectedDirections);
    // When the full chain runs, the shared context carries the same count and
    // GICompositePass consumes it as uRaysPerProbe.
    if (context.giRadianceTexture != 0u) {
      CHECK(context.giRadianceDirections == expectation.expectedDirections);
    }

    radiancePass.Shutdown();
  }

  render::lighting::LightManager::Get().Shutdown();
  utils::GPUUtils::DeleteTextures(1, &distanceField);
  render::resources::FramebufferManager::Destroy(hdr);
}
