#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/SharedContext.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/MDIRenderer.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/render/GPUEntityAdapter.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

namespace NoMoreDay::tests {
namespace mdi_render_benchmark_detail {

constexpr int kEntityCount = 50000;
constexpr int kWarmupFrames = 24;
constexpr int kBenchFrames = 140;

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

void PopulateEntities(entt::registry &registry, int count, float worldSpan) {
  std::mt19937 rng(240215);
  std::uniform_real_distribution<float> posDist(0.0f, worldSpan);

  for (int i = 0; i < count; ++i) {
    const auto entity = registry.create();
    registry.emplace<::Position>(entity, posDist(rng), posDist(rng));
    registry.emplace<::Velocity>(entity, 0.0f, 0.0f);
    registry.emplace<::Radius>(entity, 2.0f);
    registry.emplace<::GPUIndex>(entity, -1);
    registry.emplace<::EnemyTag>(entity);
  }
}

BenchmarkStats MeasureMdiRender(systems::GPUEntitySystem &gpuEntitySystem,
                                const NoMoreDay::SharedContext &context,
                                const Camera2D &camera) {
  for (int i = 0; i < kWarmupFrames; ++i) {
    gpuEntitySystem.Render(
        {context.resources,
         context.renderContext ? &context.renderContext->MDI() : nullptr,
         context.renderAlpha},
        camera);
    glFinish();
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int i = 0; i < kBenchFrames; ++i) {
    ScopedTimer timer(samples);
    gpuEntitySystem.Render(
        {context.resources,
         context.renderContext ? &context.renderContext->MDI() : nullptr,
         context.renderAlpha},
        camera);
    glFinish();
  }
  return CalculateStats(samples);
}

BenchmarkStats MeasureLegacyRender(systems::GPUEntitySystem &gpuEntitySystem) {
  for (int i = 0; i < kWarmupFrames; ++i) {
    gpuEntitySystem.RenderLegacy(0.0f);
    glFinish();
  }

  std::vector<double> samples;
  samples.reserve(kBenchFrames);
  for (int i = 0; i < kBenchFrames; ++i) {
    ScopedTimer timer(samples);
    gpuEntitySystem.RenderLegacy(0.0f);
    glFinish();
  }
  return CalculateStats(samples);
}

Camera2D MakeCamera(float targetX, float targetY, float zoom) {
  Camera2D camera = {};
  const int screenWidth = std::max(GetScreenWidth(), 1280);
  const int screenHeight = std::max(GetScreenHeight(), 720);
  camera.target = {targetX, targetY};
  camera.offset = {0.5f * static_cast<float>(screenWidth),
                   0.5f * static_cast<float>(screenHeight)};
  camera.rotation = 0.0f;
  camera.zoom = zoom;
  return camera;
}

} // namespace mdi_render_benchmark_detail

TEST_CASE("[Performance] MDIRenderer - Scenario Gate (50k)") {
  TestSetupScope scope;

  ResourceManager resources;
  systems::GPUEntitySystem gpuEntitySystem;
  render::MDIRenderer mdiRenderer;

  RenderContext renderContext = {};
  renderContext.gpuEntitySystem = &gpuEntitySystem;
  renderContext.mdiRenderer = &mdiRenderer;
  renderContext.gpuFlowFieldSystem = &systems::GPUFlowFieldSystem::Get();
  renderContext.resources = &resources;

  gpuEntitySystem.Init(resources, mdi_render_benchmark_detail::kEntityCount);
  mdiRenderer.Init(resources, mdi_render_benchmark_detail::kEntityCount);

  entt::registry registry;
  NoMoreDay::GPUEntityAdapter gpuEntityAdapter;
  gpuEntityAdapter.Init(mdi_render_benchmark_detail::kEntityCount, &registry,
                        gpuEntitySystem);
  mdi_render_benchmark_detail::PopulateEntities(
      registry, mdi_render_benchmark_detail::kEntityCount, 4000.0f);

  NoMoreDay::SharedContext context = {};
  context.resources = &resources;
  context.registry = &registry;
  context.renderAlpha = 0.0f;
  context.renderContext = &renderContext;
  gpuEntityAdapter.Update(registry, gpuEntitySystem, 1.0f / 60.0f, 0.0f);
  gpuEntitySystem.UploadGPU(
      {context.resources, &context.renderContext->MDI(), context.renderAlpha});

  const Camera2D allVisibleCamera =
      mdi_render_benchmark_detail::MakeCamera(2000.0f, 2000.0f, 1.0f);
  const Camera2D sparseVisibilityCamera =
      mdi_render_benchmark_detail::MakeCamera(120.0f, 120.0f, 1.0f);

  const BenchmarkStats mdiAllVisible = mdi_render_benchmark_detail::MeasureMdiRender(
      gpuEntitySystem, context, allVisibleCamera);
  const BenchmarkStats mdiSparseVisibility =
      mdi_render_benchmark_detail::MeasureMdiRender(gpuEntitySystem, context,
                                                    sparseVisibilityCamera);
  const BenchmarkStats legacyReference =
      mdi_render_benchmark_detail::MeasureLegacyRender(gpuEntitySystem);

  LOG_BENCHMARK("MDI Scenario A (all-visible pipeline)", mdiAllVisible,
                "< 1.2ms / < 2.5ms");
  mdi_render_benchmark_detail::LogThresholdWarn(
      "MDI Scenario A (all-visible pipeline)", mdiAllVisible, 1.2, 2.5);

  LOG_BENCHMARK("MDI Scenario B (sparse-visibility pipeline)",
                mdiSparseVisibility, "< 1.2ms / < 2.5ms");
  mdi_render_benchmark_detail::LogThresholdWarn(
      "MDI Scenario B (sparse-visibility pipeline)", mdiSparseVisibility, 1.2,
      2.5);

  LOG_BENCHMARK("Legacy Reference (submission-only path)", legacyReference,
                "< 0.35ms / < 0.90ms");
  mdi_render_benchmark_detail::LogThresholdWarn(
      "Legacy Reference (submission-only path)", legacyReference, 0.35, 0.90);

  // Contract: compare labeled scenarios, not a single blended MDI/Legacy ratio.
  CHECK(mdiAllVisible.mean_ms > 0.0);
  CHECK(mdiSparseVisibility.mean_ms > 0.0);
  CHECK(legacyReference.mean_ms > 0.0);
  CHECK(mdiSparseVisibility.mean_ms <= (mdiAllVisible.mean_ms * 1.5 + 0.05));

  const double baselineFps = 1000.0 / std::max(0.0001, mdiAllVisible.mean_ms);
  std::cout << "RELEASE_GATE_METRIC baseline_270_fps=" << baselineFps << "\n";

  gpuEntitySystem.Shutdown();
  mdiRenderer.Shutdown();
}

} // namespace NoMoreDay::tests
