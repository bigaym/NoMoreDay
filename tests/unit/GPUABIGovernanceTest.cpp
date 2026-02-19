#include "doctest.h"

#include "engine/render/GPUABIContract.hpp"
#include "engine/render/GPUData.hpp"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>

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
  CHECK(elapsedMs <= 2000.0);
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
  CHECK(elapsedMs <= 2000.0);
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
  CHECK(alignof(GPUFluidParticle) == alignof(float));
  CHECK(offsetof(GPUFluidParticle, position) == 0);
  CHECK(offsetof(GPUFluidParticle, pressure) == 20);
  CHECK(offsetof(GPUFluidParticle, color) == 24);
  CHECK(offsetof(GPUFluidParticle, flags) == 44);

  CHECK(std::is_standard_layout_v<GPUFluidConfig>);
  CHECK(sizeof(GPUFluidConfig) == 32);
  CHECK(alignof(GPUFluidConfig) == alignof(float));
  CHECK(offsetof(GPUFluidConfig, smoothingRadius) == 0);
  CHECK(offsetof(GPUFluidConfig, gravity) == 16);
  CHECK(offsetof(GPUFluidConfig, surfaceTension) == 24);
  CHECK(offsetof(GPUFluidConfig, maxParticles) == 28);
}
