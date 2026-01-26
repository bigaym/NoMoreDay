#pragma once

#include "TestCommon.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

namespace NoMoreDay::tests {

// Helper for collecting statistics
struct BenchmarkStats {
  double min_ms;
  double max_ms;
  double mean_ms;
  double p01_low_ms; // 1% Low (actually 99th percentile of slowness? or 1%
                     // fastest? usually 1% low FPS means 99th percentile frame
                     // time)
  // "1% Low" usually refers to FPS. For duration, we probably want 99th
  // percentile (slowest frames). Let's interpret "1% Low" as "99th percentile
  // time" (the slow spikes).
  double p99_ms;
};

BenchmarkStats CalculateStats(const std::vector<double> &samples) {
  if (samples.empty())
    return {0, 0, 0, 0, 0};
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
  double mean = sum / sorted.size();

  size_t idx99 = (size_t)(sorted.size() * 0.99);
  if (idx99 >= sorted.size())
    idx99 = sorted.size() - 1;

  return {sorted.front(), sorted.back(), mean,
          0.0, // placeholder
          sorted[idx99]};
}

TEST_CASE("Scenario A: Particle Stress Test") {
  using namespace NoMoreDay::systems;

  GPUParticleSystem::Get().Init(100000); // Max particles

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 600;               // 10 seconds
  const int EMISSION_PER_FRAME = 10000 / FPS; // ~166

  std::vector<double> updateTimes;
  updateTimes.reserve(TOTAL_FRAMES);

  NoMoreDay::components::GPUParticle pTemplate =
      InkEffectHelper::CreateInkTrail({0, 0}, {0, 0}, 1.0f, 1.0f);

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // 1. Emit
    for (int i = 0; i < EMISSION_PER_FRAME; ++i) {
      GPUParticleSystem::Get().Emit(pTemplate);
    }

    // 2. Measure Update
    auto start = std::chrono::high_resolution_clock::now();
    GPUParticleSystem::Get().Update(DT);
    auto end = std::chrono::high_resolution_clock::now();

    updateTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(updateTimes);
  LOG_WARN(
      "Scenario A (Particles): Mean={:.3f}ms, P99={:.3f}ms (Target: < 0.5ms)",
      stats.mean_ms, stats.p99_ms);

  CHECK(stats.mean_ms < 0.5);

  GPUParticleSystem::Get().Shutdown();
}

TEST_CASE("Scenario B: Popup Spam Test") {
  using namespace NoMoreDay::render;

  PopupRenderer::Get().Init();

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 300; // 5 seconds
  const int NEW_POPUPS_PER_FRAME = 50;

  std::vector<double> renderTimes;
  renderTimes.reserve(TOTAL_FRAMES);

  Matrix identity = MatrixIdentity(); // Dummy view proj

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // 1. Add Popups
    for (int i = 0; i < NEW_POPUPS_PER_FRAME; ++i) {
      PopupRenderer::Get().Emit({0, 0}, 100 + i, (i % 10 == 0));
    }

    PopupRenderer::Get().Update(DT);

    // 2. Measure Render (Submission)
    auto start = std::chrono::high_resolution_clock::now();
    PopupRenderer::Get().Render(identity);
    auto end = std::chrono::high_resolution_clock::now();

    renderTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(renderTimes);
  LOG_WARN("Scenario B (Popups): Mean={:.3f}ms, P99={:.3f}ms (Target: < 0.3ms)",
           stats.mean_ms, stats.p99_ms);

  CHECK(stats.mean_ms < 0.3);

  PopupRenderer::Get().Shutdown();
}

TEST_CASE("Scenario C: Entity Horde Test") {
  using namespace NoMoreDay::systems;

  ResourceManager resources;
  const int TEST_ENTITIES = 20000;

  systems::GPUEntitySystem gpuEntitySystem;
  render::MDIRenderer mdiRenderer;
  RenderContext renderContext;
  renderContext.gpuEntitySystem = &gpuEntitySystem;
  renderContext.mdiRenderer = &mdiRenderer;
  renderContext.gpuFlowFieldSystem = &systems::GPUFlowFieldSystem::Get();
  renderContext.resources = &resources;

  NoMoreDay::SharedContext context;
  context.resources = &resources;
  context.renderContext = &renderContext;

  gpuEntitySystem.Init(resources, TEST_ENTITIES);
  mdiRenderer.Init(resources, TEST_ENTITIES);

  entt::registry registry;
  context.registry = &registry;
  for (int i = 0; i < TEST_ENTITIES; ++i) {
    auto e = registry.create();
    registry.emplace<::Position>(e, (float)(rand() % 4000),
                                 (float)(rand() % 4000));
    registry.emplace<::Velocity>(e, 0.0f, 0.0f); // 50% moving logic below
    registry.emplace<::Radius>(e, 2.0f);
    registry.emplace<::GPUIndex>(e, -1);

    if (rand() % 2 == 0) { // 50% moving
      registry.emplace<::Velocity>(e, 10.0f, 10.0f);
      // Mark dirty logic? GPU system usually checks changes.
      // If we just set Velocity component, does it count?
      // GPUEntitySystem normally iterates registry to sync.
    }
  }

  const int FPS = 60;
  const float DT = 1.0f / FPS;
  const int TOTAL_FRAMES = 100;

  std::vector<double> updateTimes;
  updateTimes.reserve(TOTAL_FRAMES);

  for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
    // Simulate movement for 50% entities to trigger "Dirty" updates if system
    // tracks it Or essentially GPUEntitySystem::Update iterates all and syncs.

    // Measure Update (Sync + Upload)
    auto start = std::chrono::high_resolution_clock::now();
    gpuEntitySystem.Update(context, DT);
    auto end = std::chrono::high_resolution_clock::now();

    updateTimes.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  BenchmarkStats stats = CalculateStats(updateTimes);
  LOG_WARN(
      "Scenario C (Entities): Mean={:.3f}ms, P99={:.3f}ms (Target: < 3.0ms)",
      stats.mean_ms, stats.p99_ms);

  CHECK(stats.mean_ms < 3.0);

  gpuEntitySystem.Shutdown();
}

} // namespace NoMoreDay::tests
