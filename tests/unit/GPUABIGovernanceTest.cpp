#include "doctest.h"

#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GPUData.hpp"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

TEST_CASE("[Unit] GPU ABI - Generated include matches CPU contract") {
  using namespace NoMoreDay::render::abi;

  const ShaderABIInfo info =
      ReadShaderABIInfo("assets/shaders/generated/gpu_abi.glslinc");
  CHECK(info.version == static_cast<int>(GPU_ABI_VERSION));
  CHECK(info.compatMinVersion == static_cast<int>(GPU_ABI_COMPAT_MIN_VERSION));
  CHECK(ValidateGeneratedShaderABI("assets/shaders/generated/gpu_abi.glslinc",
                                   false));
}

TEST_CASE("[Unit] GPU ABI - Version mismatch hard fail policy") {
  using namespace NoMoreDay::render::abi;
  namespace fs = std::filesystem;

  const fs::path path = fs::path("bin") / "tmp_gpu_abi_mismatch.glslinc";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out << "#define GPU_ABI_VERSION " << (GPU_ABI_VERSION + 2) << "\n";
    out << "#define GPU_ABI_COMPAT_MIN_VERSION " << (GPU_ABI_VERSION + 1)
        << "\n";
  }

  CHECK(ValidateGeneratedShaderABI(path.string(), false) == false);
  CHECK_THROWS_AS(ValidateGeneratedShaderABI(path.string(), true),
                  std::logic_error);

  std::error_code ec;
  (void)fs::remove(path, ec);
}

TEST_CASE("[Unit] GPU ABI - Generator reproducibility and runtime budget") {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  const int rc = std::system(
      "python tools/render_abi/generate_gpu_abi.py --check > NUL 2>&1");
  const auto end = clock::now();
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  CHECK(rc == 0);
  CHECK(elapsedMs <= 5000.0);
}

TEST_CASE("[Unit] GPU ABI - No manual GLSL struct duplication gate") {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();
  const int rc =
      std::system("python tools/render_abi/check_no_manual_abi_structs.py");
  const auto end = clock::now();
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(end - start).count();

  CHECK(rc == 0);
  CHECK(elapsedMs <= 5000.0);
}

TEST_CASE("[Unit] GPU ABI - V5 layout snapshot placeholders") {
  using namespace NoMoreDay::components;
  using namespace NoMoreDay::render::abi;

  CHECK(GPU_ABI_VERSION == 5u);
  CHECK(GPU_ABI_COMPAT_MIN_VERSION == 4u);

  CHECK(std::is_standard_layout_v<GPUShadowCaster>);
  CHECK(sizeof(GPUShadowCaster) == 32);
  CHECK(alignof(GPUShadowCaster) == alignof(float));
  CHECK(offsetof(GPUShadowCaster, posX) == 0);
  CHECK(offsetof(GPUShadowCaster, radius) == 8);
  CHECK(offsetof(GPUShadowCaster, shapeIndex) == 16);

  CHECK(std::is_standard_layout_v<GPUShadowLight>);
  CHECK(sizeof(GPUShadowLight) == 48);
  CHECK(alignof(GPUShadowLight) == alignof(float));
  CHECK(offsetof(GPUShadowLight, lightId) == 0);
  CHECK(offsetof(GPUShadowLight, atlasRect) == 16);
  CHECK(offsetof(GPUShadowLight, penumbraParams) == 32);

  CHECK(std::is_standard_layout_v<GPUShadowAtlasMeta>);
  CHECK(sizeof(GPUShadowAtlasMeta) == 16);
  CHECK(alignof(GPUShadowAtlasMeta) == alignof(float));
  CHECK(offsetof(GPUShadowAtlasMeta, tileIndex) == 0);
  CHECK(offsetof(GPUShadowAtlasMeta, priorityScore) == 8);

  CHECK(std::is_standard_layout_v<GPUClusterHeader>);
  CHECK(sizeof(GPUClusterHeader) == 16);
  CHECK(alignof(GPUClusterHeader) == alignof(uint32_t));
  CHECK(offsetof(GPUClusterHeader, offset) == 0);
  CHECK(offsetof(GPUClusterHeader, pointCount) == 4);
  CHECK(offsetof(GPUClusterHeader, spotCount) == 8);
  CHECK(offsetof(GPUClusterHeader, areaCount) == 12);

  CHECK(std::is_standard_layout_v<GPUClusterCounters>);
  CHECK(sizeof(GPUClusterCounters) == 32);
  CHECK(alignof(GPUClusterCounters) == alignof(uint32_t));
  CHECK(offsetof(GPUClusterCounters, writeCursor) == 0);
  CHECK(offsetof(GPUClusterCounters, overflowPoint) == 4);
  CHECK(offsetof(GPUClusterCounters, overflowLine) == 16);

  CHECK(std::is_standard_layout_v<GPUClusterLightIndex>);
  CHECK(sizeof(GPUClusterLightIndex) == 4);
  CHECK(alignof(GPUClusterLightIndex) == alignof(uint32_t));
  CHECK(offsetof(GPUClusterLightIndex, lightIndex) == 0);

  CHECK(std::is_standard_layout_v<GPULightBounds>);
  CHECK(sizeof(GPULightBounds) == 32);
  CHECK(alignof(GPULightBounds) == alignof(float));
  CHECK(offsetof(GPULightBounds, minXY) == 0);
  CHECK(offsetof(GPULightBounds, maxXY) == 8);
  CHECK(offsetof(GPULightBounds, minLayer) == 16);
  CHECK(offsetof(GPULightBounds, lightIndex) == 24);

  CHECK(std::is_standard_layout_v<GPUMaterialDataV3>);
  CHECK(sizeof(GPUMaterialDataV3) == 128);
  CHECK(alignof(GPUMaterialDataV3) == 16);
  CHECK(offsetof(GPUMaterialDataV3, baseColor) == 0);
  CHECK(offsetof(GPUMaterialDataV3, pbrParams) == 32);
  CHECK(offsetof(GPUMaterialDataV3, uvParams) == 80);
  CHECK(offsetof(GPUMaterialDataV3, reserved1) == 112);

  CHECK(std::is_standard_layout_v<GPULootInstance>);
  CHECK(sizeof(GPULootInstance) == 32);
  CHECK(alignof(GPULootInstance) == alignof(float));
  CHECK(offsetof(GPULootInstance, worldPosX) == 0);
  CHECK(offsetof(GPULootInstance, labelOffsetX) == 8);
  CHECK(offsetof(GPULootInstance, itemId) == 16);
  CHECK(offsetof(GPULootInstance, glowIntensity) == 24);

  CHECK(std::is_standard_layout_v<RadianceCascadeConfig>);
  CHECK(sizeof(RadianceCascadeConfig) == 32);
  CHECK(alignof(RadianceCascadeConfig) == alignof(uint32_t));
  CHECK(offsetof(RadianceCascadeConfig, numLevels) == 0);
  CHECK(offsetof(RadianceCascadeConfig, temporalWeight) == 12);
  CHECK(offsetof(RadianceCascadeConfig, giIntensity) == 24);

  CHECK(std::is_standard_layout_v<GPUFluidParticle>);
  CHECK(sizeof(GPUFluidParticle) == 48);
  CHECK(alignof(GPUFluidParticle) == 16);
  CHECK(offsetof(GPUFluidParticle, position) == 0);
  CHECK(offsetof(GPUFluidParticle, velocity) == 8);
  CHECK(offsetof(GPUFluidParticle, color) == 16);
  CHECK(offsetof(GPUFluidParticle, density) == 32);
  CHECK(offsetof(GPUFluidParticle, pressure) == 36);
  CHECK(offsetof(GPUFluidParticle, lifetime) == 40);
  CHECK(offsetof(GPUFluidParticle, flags) == 44);

  CHECK(std::is_standard_layout_v<GPUFluidConfig>);
  CHECK(sizeof(GPUFluidConfig) == 32);
  CHECK(alignof(GPUFluidConfig) == alignof(float));
  CHECK(offsetof(GPUFluidConfig, smoothingRadius) == 0);
  CHECK(offsetof(GPUFluidConfig, gravity) == 16);
  CHECK(offsetof(GPUFluidConfig, surfaceTension) == 24);
  CHECK(offsetof(GPUFluidConfig, maxParticles) == 28);
}

TEST_CASE("[Unit] GPU ABI - Exhaustive struct layout coverage") {
  using namespace NoMoreDay::components;

  // GPUParticle
  CHECK(std::is_standard_layout_v<GPUParticle>);
  CHECK(sizeof(GPUParticle) == 64);
  CHECK(offsetof(GPUParticle, position) == 0);
  CHECK(offsetof(GPUParticle, velocity) == 8);
  CHECK(offsetof(GPUParticle, acceleration) == 16);
  CHECK(offsetof(GPUParticle, color) == 24);
  CHECK(offsetof(GPUParticle, lifetime) == 28);
  CHECK(offsetof(GPUParticle, maxLifetime) == 32);
  CHECK(offsetof(GPUParticle, scale) == 36);
  CHECK(offsetof(GPUParticle, flags) == 40);
  CHECK(offsetof(GPUParticle, growthRate) == 44);
  CHECK(offsetof(GPUParticle, rotation) == 48);
  CHECK(offsetof(GPUParticle, textureIndex) == 52);
  CHECK(offsetof(GPUParticle, subUV) == 54);
  CHECK(offsetof(GPUParticle, animFrameCount) == 56);
  CHECK(offsetof(GPUParticle, blendMode) == 58);
  CHECK(offsetof(GPUParticle, subEmitterType) == 59);
  CHECK(offsetof(GPUParticle, subEmitterParam) == 60);

  // GPUDistortionSource
  CHECK(std::is_standard_layout_v<GPUDistortionSource>);
  CHECK(sizeof(GPUDistortionSource) == 16);
  CHECK(offsetof(GPUDistortionSource, posX) == 0);
  CHECK(offsetof(GPUDistortionSource, posY) == 4);
  CHECK(offsetof(GPUDistortionSource, radius) == 8);
  CHECK(offsetof(GPUDistortionSource, strength) == 12);

  // GPUTrailPoint & Header
  CHECK(std::is_standard_layout_v<GPUTrailPoint>);
  CHECK(sizeof(GPUTrailPoint) == 32);
  CHECK(offsetof(GPUTrailPoint, posX) == 0);
  CHECK(offsetof(GPUTrailPoint, flags) == 28);
  CHECK(std::is_standard_layout_v<GPUTrailHeader>);
  CHECK(sizeof(GPUTrailHeader) == 32);
  CHECK(offsetof(GPUTrailHeader, headIndex) == 0);
  CHECK(offsetof(GPUTrailHeader, colorEnd) == 28);

  // GPUForceField & GPULight
  CHECK(std::is_standard_layout_v<GPUForceField>);
  CHECK(sizeof(GPUForceField) == 32);
  CHECK(offsetof(GPUForceField, posX) == 0);
  CHECK(offsetof(GPUForceField, padding) == 28);
  CHECK(std::is_standard_layout_v<GPULight>);
  CHECK(sizeof(GPULight) == 64);
  CHECK(offsetof(GPULight, posX) == 0);
  CHECK(offsetof(GPULight, flags) == 60);

  // GPUEntity
  CHECK(std::is_standard_layout_v<GPUEntity>);
  CHECK(sizeof(GPUEntity) == 64);
  CHECK(offsetof(GPUEntity, position) == 0);
  CHECK(offsetof(GPUEntity, prevPosition) == 8);
  CHECK(offsetof(GPUEntity, velocity) == 16);
  CHECK(offsetof(GPUEntity, padding) == 40);

  // GPUSkillEffect
  CHECK(std::is_standard_layout_v<GPUSkillEffect>);
  CHECK(sizeof(GPUSkillEffect) == 64);
  CHECK(offsetof(GPUSkillEffect, position) == 0);
  CHECK(offsetof(GPUSkillEffect, type) == 60);

  // HoloBladeInstance & GPUPopupInstance
  CHECK(std::is_standard_layout_v<HoloBladeInstance>);
  CHECK(sizeof(HoloBladeInstance) == 48);
  CHECK(offsetof(HoloBladeInstance, position) == 0);
  CHECK(offsetof(HoloBladeInstance, padding) == 40);
  CHECK(std::is_standard_layout_v<GPUPopupInstance>);
  CHECK(sizeof(GPUPopupInstance) == 48);
  CHECK(offsetof(GPUPopupInstance, position) == 0);
  CHECK(offsetof(GPUPopupInstance, padding) == 40);

  // GPUTextCommand & GPUGlyphMetrics & GPUTextQuad
  CHECK(std::is_standard_layout_v<GPUTextCommand>);
  CHECK(sizeof(GPUTextCommand) == 16);
  CHECK(offsetof(GPUTextCommand, worldPosX) == 0);
  CHECK(offsetof(GPUTextCommand, colorAndFlags) == 12);
  CHECK(std::is_standard_layout_v<GPUGlyphMetrics>);
  CHECK(sizeof(GPUGlyphMetrics) == 40);
  CHECK(offsetof(GPUGlyphMetrics, uvMinX) == 0);
  CHECK(offsetof(GPUGlyphMetrics, reserved) == 36);
  CHECK(std::is_standard_layout_v<GPUTextQuad>);
  CHECK(sizeof(GPUTextQuad) == 40);
  CHECK(offsetof(GPUTextQuad, screenPosX) == 0);
  CHECK(offsetof(GPUTextQuad, opacity) == 36);

  // GPUVisualStats & GPULabelInstance & GPUGlyphInstance
  CHECK(std::is_standard_layout_v<GPUVisualStats>);
  CHECK(sizeof(GPUVisualStats) == 64);
  CHECK(offsetof(GPUVisualStats, weaponDamage) == 0);
  CHECK(offsetof(GPUVisualStats, padding) == 40);
  CHECK(std::is_standard_layout_v<GPULabelInstance>);
  CHECK(sizeof(GPULabelInstance) == 64);
  CHECK(offsetof(GPULabelInstance, position) == 0);
  CHECK(offsetof(GPULabelInstance, padding) == 56);
  CHECK(std::is_standard_layout_v<GPUGlyphInstance>);
  CHECK(sizeof(GPUGlyphInstance) == 48);
  CHECK(offsetof(GPUGlyphInstance, position) == 0);
  CHECK(offsetof(GPUGlyphInstance, padding) == 40);

  // GPUPackedEntityInstance (32B compact entity MDI)
  CHECK(std::is_standard_layout_v<GPUPackedEntityInstance>);
  CHECK(sizeof(GPUPackedEntityInstance) == 32);
  CHECK(alignof(GPUPackedEntityInstance) == 16);
  CHECK(offsetof(GPUPackedEntityInstance, position) == 0);
  CHECK(offsetof(GPUPackedEntityInstance, prevPosition) == 8);
  CHECK(offsetof(GPUPackedEntityInstance, words) == 16);
}

TEST_CASE("[Unit] GPU ABI - FluidParticle shader mirrors layout match") {
  const std::vector<std::string> shaderPaths = {
      "assets/shaders/lighting/v5_fluid_density.comp",
      "assets/shaders/lighting/v5_fluid_gridhash.comp",
      "assets/shaders/lighting/v5_fluid_emissive_inject.comp",
      "assets/shaders/lighting/v5_fluid_force.comp",
      "assets/shaders/lighting/v5_fluid_integrate.comp",
      "assets/shaders/lighting/v5_fluid_occluder_inject.comp",
      "assets/shaders/lighting/v5_fluid_render.vert",
      "assets/shaders/lighting/v5_fluid_neighbor_search.comp",
  };

  const std::string expectedStruct =
      "struct FluidParticle {\n"
      "    vec2 position;\n"
      "    vec2 velocity;\n"
      "    vec4 color;\n"
      "    float density;\n"
      "    float pressure;\n"
      "    float lifetime;\n"
      "    uint flags;\n"
      "};";

  for (const auto &pathStr : shaderPaths) {
    std::ifstream file(pathStr);
    INFO("Checking shader file: " << pathStr);
    REQUIRE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    std::string normalized;
    normalized.reserve(content.size());
    for (char c : content) {
      if (c != '\r') {
        normalized.push_back(c);
      }
    }
    CHECK(normalized.find(expectedStruct) != std::string::npos);
  }
}
