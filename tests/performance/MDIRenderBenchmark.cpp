#pragma once

#include "TestCommon.hpp"
#include "app/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include <chrono>

TEST_CASE("[Performance] MDIRenderer - MDI vs Legacy Rendering Benchmark") {
  // Note: Raylib Window initialized by main.cpp

  ResourceManager resources;
  const int TEST_ENTITIES = 50000;
  LOG_WARN("Starting Rendering Benchmark with {} entities", TEST_ENTITIES);

  systems::GPUEntitySystem gpuEntitySystem;
  render::MDIRenderer mdiRenderer;
  RenderContext renderContext;
  renderContext.gpuEntitySystem = &gpuEntitySystem;
  renderContext.mdiRenderer = &mdiRenderer;
  renderContext.gpuFlowFieldSystem = &systems::GPUFlowFieldSystem::Get();
  renderContext.resources = &resources;

  gpuEntitySystem.Init(resources, TEST_ENTITIES);
  mdiRenderer.Init(resources, TEST_ENTITIES);

  entt::registry registry;
  for (int i = 0; i < TEST_ENTITIES; ++i) {
    auto e = registry.create();
    registry.emplace<::Position>(e, (float)(rand() % 4000),
                                 (float)(rand() % 4000));
    registry.emplace<::Velocity>(e, 0.0f, 0.0f);
    registry.emplace<::Radius>(e, 2.0f);
    registry.emplace<::GPUIndex>(e, -1);
    registry.emplace<::EnemyTag>(e);
  }

  // Set target FPS to 180 as requested by user
  SetTargetFPS(180);

  const int ITERATIONS = 500;

  NoMoreDay::SharedContext context;
  context.resources = &resources;
  context.registry = &registry;
  context.renderAlpha = 0.0f; // No interpolation for benchmark
  context.renderContext = &renderContext;

  // Warm up
  gpuEntitySystem.Update(context, 0.016f);

  // 1. Benchmark MDI (GPU Culling + Indirect)
  // We measure multiple calls to get a stable average, then glFinish to ensure
  // GPU caught up
  Camera2D camera = {0};
  camera.zoom = 1.0f;

  auto startMDI = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    gpuEntitySystem.Render(context, camera);
  }
  glFinish();
  auto endMDI = std::chrono::high_resolution_clock::now();
  double timeMDI =
      std::chrono::duration<double, std::milli>(endMDI - startMDI).count() /
      ITERATIONS;

  // 2. Benchmark Legacy (Instanced, but CPU-driven submission)
  auto startLegacy = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    gpuEntitySystem.RenderLegacy(0.0f);
  }
  glFinish();
  auto endLegacy = std::chrono::high_resolution_clock::now();
  double timeLegacy =
      std::chrono::duration<double, std::milli>(endLegacy - startLegacy)
          .count() /
      ITERATIONS;

  LOG_WARN("Benchmark Results ({} entities):", TEST_ENTITIES);
  LOG_WARN("  - MDI Render Time (CPU+GPU):    {:.4f} ms", timeMDI);
  LOG_WARN("  - Legacy Render Time (CPU+GPU): {:.4f} ms", timeLegacy);
  LOG_WARN("  - Improvement:                  {:.1f}x", timeLegacy / timeMDI);
  LOG_WARN("  - MDI Potential FPS:           {:.1f}", 1000.0 / timeMDI);
  LOG_WARN("  - Legacy Potential FPS:        {:.1f}", 1000.0 / timeLegacy);

  gpuEntitySystem.Shutdown();
  mdiRenderer.Shutdown();
}
