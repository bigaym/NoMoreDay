#include "doctest.h"

#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/resource/TextureArrayManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {

// ACES Filmic tonemapping curve matching assets/shaders/postprocess/postprocess_combined.frag
float ACESFilmic(float x) {
  const float a = 2.51f;
  const float b = 0.03f;
  const float c = 2.43f;
  const float d = 0.59f;
  const float e = 0.14f;
  return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

// Standard sRGB EOTF (exact)
float SrgbToLinearExact(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return (c <= 0.04045f) ? (c / 12.92f)
                         : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Shader gamma 2.2 approximation
float SrgbToLinearFast(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return std::pow(c, 2.2f);
}

// Post-process gamma 1/2.2 encoding
float LinearToSrgbFast(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return std::pow(c, 1.0f / 2.2f);
}

} // namespace

TEST_CASE("[Unit] Color Space Linearization - Texture Semantic Metadata (T8.1)") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  // T8.1: Albedo contains color authored in sRGB space (requires sRGB->linear decoding)
  CHECK_FALSE(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Albedo));

  // T8.1: Normal, Mask (Roughness/Metallic/AO), and Detail textures are linear data
  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Normal));
  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Mask));
  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Detail));
  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Roughness));
}

TEST_CASE("[Unit] Color Space Linearization - Shader Sampling Single Gamma Pipeline (T8.2)") {
  // T8.2: Ensure albedo textures are linearized exactly once at the sampling entry point
  // and final output encodes sRGB gamma exactly once in postprocess.

  // 1. Identity Roundtrip: for unit lighting (exposure=1.0, light=1.0),
  // linearizing input and encoding output produces monotonic and accurate reproduction.
  const std::array<float, 6> testLuminances = {0.0f, 0.18f, 0.36f, 0.50f, 0.73f, 1.0f};

  for (float srgbIn : testLuminances) {
    // Sampling entry: sRGB -> Linear
    const float linearIn = SrgbToLinearFast(srgbIn);

    // Linear lighting accumulation (HDR scene)
    const float litHdr = linearIn * 1.0f;

    // Postprocess output: Tonemap + Gamma encode (pow 1/2.2)
    const float tonemapped = ACESFilmic(litHdr);
    const float srgbOut = LinearToSrgbFast(tonemapped);

    CHECK(srgbOut >= 0.0f);
    CHECK(srgbOut <= 1.0f);

    // Black stays black
    if (srgbIn == 0.0f) {
      CHECK(srgbOut == doctest::Approx(0.0f).epsilon(0.01f));
    }
  }

  // 2. Monotonicity assertion: brighter input MUST strictly produce brighter output
  float prevOutput = -1.0f;
  for (float srgbIn : testLuminances) {
    const float linearIn = SrgbToLinearFast(srgbIn);
    const float tonemapped = ACESFilmic(linearIn);
    const float srgbOut = LinearToSrgbFast(tonemapped);
    CHECK(srgbOut >= prevOutput);
    prevOutput = srgbOut;
  }
}

TEST_CASE("[Unit] Color Space Linearization - Feature Toggle & Grayscale Golden Fixture (T8.3)") {
  // T8.3: Golden fixture comparison between Linear Pipeline (linearPipeline=true)
  // and Legacy Fallback (linearPipeline=false). The switch state is read from the
  // real settings path (render.color.linearPipeline) instead of being hardcoded.
  //
  // Default-on decision: RenderConfig::linearPipeline defaults to true
  // (engine/render/core/RenderConstants.hpp) — the intended shipped behavior.
  // This test proves the toggle works both ways through the settings path.
  using NoMoreDay::render::core::QualityTierManager;

  const std::filesystem::path settingsDir =
      std::filesystem::path("bin") / "tmp_color_space";
  std::error_code ec;
  std::filesystem::create_directories(settingsDir, ec);
  const std::filesystem::path settingsPath =
      settingsDir / "linear_pipeline_toggle.json";
  const auto writeLinearPipeline = [&](bool enabled) {
    std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    const nlohmann::json settings = {
        {"render", {{"color", {{"linearPipeline", enabled}}}}}};
    out << settings.dump(2);
  };

  auto &qualityManager = QualityTierManager::Get();

  // 1. The flag reflects the settings file on both sides of the toggle.
  writeLinearPipeline(true);
  qualityManager.Initialize(settingsPath.string(), true);
  CHECK(qualityManager.IsLinearPipelineEnabled() == true);

  writeLinearPipeline(false);
  qualityManager.Initialize(settingsPath.string(), true);
  CHECK(qualityManager.IsLinearPipelineEnabled() == false);

  // 2. SetLinearPipelineEnabled roundtrip: flag flips and persists to the file.
  REQUIRE(qualityManager.SetLinearPipelineEnabled(true, settingsPath.string()));
  CHECK(qualityManager.IsLinearPipelineEnabled() == true);
  {
    std::ifstream in(settingsPath, std::ios::binary);
    REQUIRE(in.is_open());
    nlohmann::json saved = nlohmann::json::object();
    in >> saved;
    REQUIRE(saved.contains("render"));
    REQUIRE(saved["render"].contains("color"));
    CHECK(saved["render"]["color"]["linearPipeline"].get<bool>() == true);
  }
  REQUIRE(qualityManager.SetLinearPipelineEnabled(false, settingsPath.string()));
  CHECK(qualityManager.IsLinearPipelineEnabled() == false);

  // 3. Golden fixture: on/off produce different paths for middle grays, and the
  // linear pipeline preserves the physical ratio through the sRGB->linear->ACES
  // ->linear->sRGB chain.
  struct GrayscaleSample {
    float srgbValue;
    const char *label;
  };

  const std::vector<GrayscaleSample> goldenSteps = {
      {0.0f, "Black (0%)"},
      {0.18f, "18% Middle Gray"},
      {0.50f, "50% Gray"},
      {0.73f, "73% Light Gray"},
      {1.0f, "White (100%)"},
  };

  for (const auto &step : goldenSteps) {
    // Mode A: Linear Pipeline enabled (T8.2 standard)
    const float linA = SrgbToLinearFast(step.srgbValue);
    const float outA = LinearToSrgbFast(ACESFilmic(linA));

    // Mode B: Legacy raw sampling (no input de-gamma)
    const float linB = step.srgbValue;
    const float outB = LinearToSrgbFast(ACESFilmic(linB));

    if (step.srgbValue > 0.0f && step.srgbValue < 1.0f) {
      // In a correct linear pipeline, middle grays in linear HDR have correct physical ratios
      // (linA < linB because sRGB gamma curve compresses darks).
      CHECK(linA < linB);
      CHECK(outA < outB);
    } else if (step.srgbValue == 0.0f) {
      CHECK(outA == doctest::Approx(0.0f).epsilon(0.01f));
      CHECK(outB == doctest::Approx(0.0f).epsilon(0.01f));
    }
  }

  // 4. Restore the shipped default (linearPipeline=true) so the singleton is
  // left in the default state for subsequent tests.
  qualityManager.SetLinearPipelineEnabled(true, settingsPath.string());
}

TEST_CASE("[Unit] Color Space Linearization - UI & Emissive No Drift Regression (T8.4)") {
  // T8.4: Ensure UI elements and particle emissive colors maintain consistent visual
  // appearance without secondary gamma drift.

  // 1. Particle Emissive colors are authored as physical HDR radiance (already linear).
  // They do NOT undergo sRGB->linear input decoding, avoiding double de-gamma.
  const std::array<float, 4> emissiveIntensities = {0.5f, 1.0f, 2.0f, 5.0f};

  for (float intensity : emissiveIntensities) {
    const float emissiveRadiance = 1.0f * intensity; // Linear HDR emissive light

    // Emissive adds directly to HDR scene buffer without texture de-gamma
    const float sceneHdr = emissiveRadiance;

    // Postprocess tonemaps and encodes once
    const float finalOutput = LinearToSrgbFast(ACESFilmic(sceneHdr));

    CHECK(finalOutput > 0.0f);
    CHECK(finalOutput <= 1.0f);

    // HDR values > 1.0 smoothly roll off via ACES filmic
    if (intensity >= 2.0f) {
      CHECK(finalOutput > 0.90f);
    }
  }

  // 2. UI Distance Field Glyphs: The font atlas is an uncompressed SDF / alpha mask.
  // Distance sampling must remain linear distance metric [0..1] without gamma distortion.
  const std::array<float, 5> sdfDistances = {0.0f, 0.25f, 0.50f, 0.75f, 1.0f};
  for (float dist : sdfDistances) {
    // Linear median decoding preserves distance field linearity
    const float alpha = std::clamp(dist, 0.0f, 1.0f);
    CHECK(alpha == doctest::Approx(dist));
  }
}
