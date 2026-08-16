#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTextureMinFilter = 0x2801;
constexpr uint32_t kGLTextureMagFilter = 0x2800;
constexpr uint32_t kGLNearest = 0x2600;
constexpr uint32_t kGLR8 = 0x8229;
constexpr uint32_t kGLR16F = 0x822D;
constexpr uint32_t kGLRed = 0x1903;
constexpr uint32_t kGLUnsignedByte = 0x1401;
constexpr uint32_t kGLFloat = 0x1406;

bool EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "JFAPass Upsample Mask Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> DrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

bool ReadbackTextureR16F(uint32_t texture, int width, int height, std::vector<float> &out) {
  if (texture == 0u || width <= 0 || height <= 0) {
    return false;
  }
  using GlGetTexImageFn = void(APIENTRY *)(uint32_t, int, uint32_t, uint32_t, void *);
  auto glGetTexImage =
      reinterpret_cast<GlGetTexImageFn>(glfwGetProcAddress("glGetTexImage"));
  if (glGetTexImage == nullptr) {
    return false;
  }
  out.resize(static_cast<size_t>(width * height), 0.0f);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, texture);
  glGetTexImage(kGLTexture2D, 0, kGLRed, kGLFloat, out.data());
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0u);
  return true;
}

} // namespace

TEST_CASE("[Integration] JFAPass - RunUpsample binds uMaskTexture and preserves occluder sign") {
  if (!EnsureGpuContext()) {
    FAIL("Cannot initialize OpenGL context for JFAPassUpsampleMaskTest");
  }
  (void)DrainGlErrors();

  ResourceManager resources;
  NoMoreDay::render::passes::JFAPass jfaPass;
  REQUIRE(jfaPass.Initialize(resources));

  constexpr int kFullWidth = 32;
  constexpr int kFullHeight = 32;
  constexpr int kWorkWidth = 16;
  constexpr int kWorkHeight = 16;

  REQUIRE(jfaPass.EnsureResourcesForTesting(kFullWidth, kFullHeight, true));
  const auto workFb = jfaPass.GetWorkDistanceFieldForTesting();
  REQUIRE(workFb.IsValid());
  REQUIRE(jfaPass.HasDistanceField());

  // 1. Fill the half-resolution work distance field with positive distance (e.g. +6.0f).
  std::vector<float> halfDistanceData(static_cast<size_t>(kWorkWidth * kWorkHeight), 6.0f);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, workFb.colorTexture);
  NoMoreDay::utils::GPUUtils::TexImage2D(
      kGLTexture2D, 0, kGLR16F, kWorkWidth, kWorkHeight, 0, kGLRed, kGLFloat,
      halfDistanceData.data());
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0u);

  // 2. Create an occluder mask texture with a centered box [10..21] x [10..21].
  uint32_t maskTex = 0u;
  NoMoreDay::utils::GPUUtils::GenTextures(1, &maskTex);
  REQUIRE(maskTex != 0u);

  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, maskTex);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMinFilter, kGLNearest);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMagFilter, kGLNearest);

  std::vector<uint8_t> maskPixels(static_cast<size_t>(kFullWidth * kFullHeight), 0u);
  constexpr int kBoxMinX = 10;
  constexpr int kBoxMaxX = 22;
  constexpr int kBoxMinY = 10;
  constexpr int kBoxMaxY = 22;

  for (int y = kBoxMinY; y < kBoxMaxY; ++y) {
    for (int x = kBoxMinX; x < kBoxMaxX; ++x) {
      maskPixels[static_cast<size_t>(y * kFullWidth + x)] = 255u; // texelFetch > 0.5
    }
  }

  NoMoreDay::utils::GPUUtils::TexImage2D(
      kGLTexture2D, 0, kGLR8, kFullWidth, kFullHeight, 0, kGLRed, kGLUnsignedByte,
      maskPixels.data());
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0u);

  SUBCASE("Fail-closed on invalid mask texture") {
    CHECK_FALSE(jfaPass.RunUpsampleForTesting(0u, kFullWidth, kFullHeight, nullptr));
  }

  SUBCASE("Upsample preserves negative sign inside occluder and positive sign outside") {
    const bool success =
        jfaPass.RunUpsampleForTesting(maskTex, kFullWidth, kFullHeight, nullptr);
    REQUIRE(success);

    std::vector<float> outputDistances;
    REQUIRE(ReadbackTextureR16F(jfaPass.GetDistanceFieldTexture(), kFullWidth, kFullHeight,
                                outputDistances));

    // Verify inside the occluder region: must be negative (-max(abs(upsampled), 0.001))
    int occluderSampleCount = 0;
    for (int y = kBoxMinY; y < kBoxMaxY; ++y) {
      for (int x = kBoxMinX; x < kBoxMaxX; ++x) {
        const float val = outputDistances[static_cast<size_t>(y * kFullWidth + x)];
        CHECK(val < 0.0f);
        CHECK(val <= -0.001f);
        ++occluderSampleCount;
      }
    }
    CHECK(occluderSampleCount == (kBoxMaxX - kBoxMinX) * (kBoxMaxY - kBoxMinY));

    // Verify outside the occluder region (clear background): must remain positive (+6.0f)
    int outsideSampleCount = 0;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        const float val = outputDistances[static_cast<size_t>(y * kFullWidth + x)];
        CHECK(val > 0.0f);
        CHECK(std::abs(val - 6.0f) < 0.01f);
        ++outsideSampleCount;
      }
    }
    CHECK(outsideSampleCount == 64);
  }

  SUBCASE("Comparison with empty mask shows occluder sign difference") {
    // With empty mask (all zeroes), the center box should NOT be inverted.
    uint32_t emptyMaskTex = 0u;
    NoMoreDay::utils::GPUUtils::GenTextures(1, &emptyMaskTex);
    REQUIRE(emptyMaskTex != 0u);

    NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, emptyMaskTex);
    NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMinFilter, kGLNearest);
    NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMagFilter, kGLNearest);

    std::vector<uint8_t> zeroPixels(static_cast<size_t>(kFullWidth * kFullHeight), 0u);
    NoMoreDay::utils::GPUUtils::TexImage2D(
        kGLTexture2D, 0, kGLR8, kFullWidth, kFullHeight, 0, kGLRed, kGLUnsignedByte,
        zeroPixels.data());
    NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0u);

    const bool success =
        jfaPass.RunUpsampleForTesting(emptyMaskTex, kFullWidth, kFullHeight, nullptr);
    REQUIRE(success);

    std::vector<float> emptyOutputDistances;
    REQUIRE(ReadbackTextureR16F(jfaPass.GetDistanceFieldTexture(), kFullWidth, kFullHeight,
                                emptyOutputDistances));

    // Center region must remain positive when no occluder is present
    for (int y = kBoxMinY; y < kBoxMaxY; ++y) {
      for (int x = kBoxMinX; x < kBoxMaxX; ++x) {
        const float val = emptyOutputDistances[static_cast<size_t>(y * kFullWidth + x)];
        CHECK(val > 0.0f);
      }
    }

    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &emptyMaskTex);
  }

  NoMoreDay::utils::GPUUtils::DeleteTextures(1, &maskTex);
  jfaPass.Shutdown();

  const auto glErrors = DrainGlErrors();
  CHECK(glErrors.empty());
}
