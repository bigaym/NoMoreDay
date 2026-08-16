#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"

#include <chrono>
#include <vector>

namespace {

bool EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "GPUIndirectPassBenchmark Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return false;
  }
  return NoMoreDay::utils::GPUUtils::CheckSupport().computeShaderSupported;
}

} // namespace

namespace NoMoreDay::tests {

TEST_CASE("[Performance] GPUTextSystem - Layout and Indirect Arguments Generation Benchmark") {
  if (!EnsureGpuContext()) {
    WARN("OpenGL 4.3 compute shader context unavailable; skipping GL benchmark");
    return;
  }

  using namespace NoMoreDay::render;
  using namespace NoMoreDay::components;

  const int WARMUP_FRAMES = 30;
  const int BENCH_FRAMES = 200;
  const uint32_t COMMAND_COUNT = 512;

  ResourceManager resources;
  auto &textSystem = GPUTextSystem::Get();
  textSystem.Init(resources, 2048, 16384);

  // Setup glyph metrics and string table
  GPUGlyphMetrics gm = {};
  gm.advance = 12.0f;
  gm.sizeX = 10.0f;
  gm.sizeY = 14.0f;
  gm.uvMaxX = 1.0f;
  gm.uvMaxY = 1.0f;
  textSystem.UploadGlyphMetrics({gm});

  GPUTextStringMeta meta = {};
  meta.glyphOffset = 0;
  meta.glyphCount = 6;
  meta.animStyle = 0;
  textSystem.UploadStringTable({0u, 0u, 0u, 0u, 0u, 0u}, {meta});

  // Warmup
  for (int i = 0; i < WARMUP_FRAMES; ++i) {
    textSystem.BeginFrame();
    for (uint32_t c = 0; c < COMMAND_COUNT; ++c) {
      GPUTextCommand cmd = {};
      cmd.worldPosX = static_cast<float>(c % 32 * 20);
      cmd.worldPosY = static_cast<float>(c / 32 * 20);
      cmd.stringId = 0;
      cmd.colorAndFlags = 0xFF00FFFFu;
      textSystem.EnqueueCommand(cmd);
    }
    textSystem.DispatchLayout(static_cast<float>(i) * 0.016f, 1.0f);
  }

  // Benchmark
  std::vector<double> layoutTimes;
  layoutTimes.reserve(BENCH_FRAMES);

  for (int i = 0; i < BENCH_FRAMES; ++i) {
    textSystem.BeginFrame();
    for (uint32_t c = 0; c < COMMAND_COUNT; ++c) {
      GPUTextCommand cmd = {};
      cmd.worldPosX = static_cast<float>(c % 32 * 20);
      cmd.worldPosY = static_cast<float>(c / 32 * 20);
      cmd.stringId = 0;
      cmd.colorAndFlags = 0xFF00FFFFu;
      textSystem.EnqueueCommand(cmd);
    }

    auto start = std::chrono::high_resolution_clock::now();
    textSystem.DispatchLayout(static_cast<float>(i) * 0.016f, 1.0f);
    auto end = std::chrono::high_resolution_clock::now();

    layoutTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  const BenchmarkStats stats = CalculateStats(layoutTimes);
  LOG_WARN("=== GPUTextSystem Dispatch Benchmark ({} Commands) ===", COMMAND_COUNT);
  LOG_BENCHMARK("TextLayoutIndirect", stats, "< 1.0ms");

  CHECK(stats.mean_ms < 1.0);
  CHECK(stats.median_ms < 0.5);

  textSystem.Shutdown();
}

TEST_CASE("[Performance] GPULootSystem - Cull and Force-Directed Dispatch Benchmark") {
  if (!EnsureGpuContext()) {
    WARN("OpenGL 4.3 compute shader context unavailable; skipping GL benchmark");
    return;
  }

  using namespace NoMoreDay::render;
  using namespace NoMoreDay::components;

  const int WARMUP_FRAMES = 30;
  const int BENCH_FRAMES = 200;
  const uint32_t LOOT_COUNT = 1024;

  auto &lootSystem = GPULootSystem::Get();
  lootSystem.Init(2048);

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {640.0f, 360.0f};
  camera.zoom = 1.0f;

  std::vector<GPULootInstance> lootList(LOOT_COUNT);
  for (size_t i = 0; i < lootList.size(); ++i) {
    lootList[i].worldPosX = static_cast<float>((i % 32) * 20 - 320);
    lootList[i].worldPosY = static_cast<float>((i / 32) * 20 - 320);
    lootList[i].labelOffsetX = 0.0f;
    lootList[i].labelOffsetY = 0.0f;
    lootList[i].itemId = static_cast<uint32_t>(1000 + i);
    lootList[i].rarityColor = 0xFFFFAA00u;
    lootList[i].glowIntensity = 1.0f;
    lootList[i].flags = 0u;
  }
  lootSystem.UploadInstances(lootList);

  // Warmup
  for (int i = 0; i < WARMUP_FRAMES; ++i) {
    lootSystem.Dispatch(camera, 1280, 720, true);
  }

  // Benchmark
  std::vector<double> dispatchTimes;
  dispatchTimes.reserve(BENCH_FRAMES);

  for (int i = 0; i < BENCH_FRAMES; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    lootSystem.Dispatch(camera, 1280, 720, true);
    auto end = std::chrono::high_resolution_clock::now();

    dispatchTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  const BenchmarkStats stats = CalculateStats(dispatchTimes);
  LOG_WARN("=== GPULootSystem Dispatch Benchmark ({} Instances) ===", LOOT_COUNT);
  LOG_BENCHMARK("LootDispatchIndirect", stats, "< 1.5ms");

  CHECK(stats.mean_ms < 1.5);
  CHECK(stats.median_ms < 1.0);

  lootSystem.Shutdown();
}

} // namespace NoMoreDay::tests
