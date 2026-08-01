#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/render/GPUEntitySync.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include <chrono>
#include <vector>


namespace NoMoreDay::tests {

TEST_CASE("[Performance] GPUEntitySync - Sync Performance Benchmark") {
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::components;

  const int TEST_ENTITIES = 20000;
  const int WARMUP_FRAMES = 10;
  const int BENCH_FRAMES = 100;

  entt::registry registry;
  std::vector<GPUEntity> physicsShadow(TEST_ENTITIES);
  std::vector<GPUVisualStats> visualShadow(TEST_ENTITIES);

  // 1. Setup Entities
  for (int i = 0; i < TEST_ENTITIES; ++i) {
    auto e = registry.create();
    registry.emplace<Position>(e, (float)(i % 100), (float)(i / 100));
    registry.emplace<Radius>(e, 5.0f);
    registry.emplace<GPUIndex>(e, i); // Pre-assigned slots for pure sync bench
    registry.emplace<Velocity>(e, 1.0f, 1.0f);
    registry.emplace<CombatStats>(e);
    registry.emplace<StatsDirty>(e); // Mark dirty to trigger visual sync
  }

  GPUPhysicsSync physicsSync;
  GPUPhysicsSync::Config pConfig;
  pConfig.maxEntities = TEST_ENTITIES;
  physicsSync.Init(pConfig);

  GPUVisualSync visualSync;
  GPUVisualSync::Config vConfig;
  vConfig.maxEntities = TEST_ENTITIES;
  vConfig.refreshInterval = 5;
  visualSync.Init(vConfig);

  std::vector<double> physicsTimes;
  std::vector<double> visualTimes;
  physicsTimes.reserve(BENCH_FRAMES);
  visualTimes.reserve(BENCH_FRAMES);

  uint64_t frameCounter = 0;

  // 2. Warmup
  for (int i = 0; i < WARMUP_FRAMES; ++i) {
    physicsSync.Execute(registry, physicsShadow, frameCounter);
    visualSync.Execute(registry, visualShadow, frameCounter, (float)i * 0.016f);
    frameCounter++;
  }

  // 3. Benchmark
  for (int i = 0; i < BENCH_FRAMES; ++i) {
    // Measure PhysicsSync
    auto startP = std::chrono::high_resolution_clock::now();
    physicsSync.Execute(registry, physicsShadow, frameCounter);
    auto endP = std::chrono::high_resolution_clock::now();
    physicsTimes.push_back(
        std::chrono::duration<double, std::milli>(endP - startP).count());

    // Measure VisualSync (Refresh dirty stats)
    // Re-dirty periodically to simulate gameplay
    if (i % 5 == 0) {
      registry.view<CombatStats>().each([&registry](auto entity, auto &) {
        registry.get_or_emplace<StatsDirty>(entity);
      });
    }

    auto startV = std::chrono::high_resolution_clock::now();
    visualSync.Execute(registry, visualShadow, frameCounter, (float)i * 0.016f);
    auto endV = std::chrono::high_resolution_clock::now();
    visualTimes.push_back(
        std::chrono::duration<double, std::milli>(endV - startV).count());

    frameCounter++;
  }

  const BenchmarkStats pStats = CalculateStats(physicsTimes);
  const BenchmarkStats vStats = CalculateStats(visualTimes);

  LOG_WARN("=== GPUEntitySync Benchmark ({} Entities) ===", TEST_ENTITIES);
  LOG_BENCHMARK("PhysicsSync", pStats, "< 1.5ms");
  LOG_BENCHMARK("VisualSync", vStats, "< 0.5ms");

  CHECK(pStats.mean_ms < 1.5);
  CHECK(vStats.mean_ms < 0.5);
}

} // namespace NoMoreDay::tests
