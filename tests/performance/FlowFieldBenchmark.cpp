#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "rlgl.h"
#include <random>
#include <vector>

namespace NoMoreDay::tests {
namespace flow_field_benchmark_detail {

constexpr int kGridSize = 256;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 50;
constexpr float kCellSize = 10.0f;

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

struct FlowFieldScope {
  ResourceManager resources;
  NoMoreDay::systems::GPUFlowFieldSystem &flowSystem =
      NoMoreDay::systems::GPUFlowFieldSystem::Get();

  FlowFieldScope() { flowSystem.Init(resources, kGridSize, kGridSize); }
  ~FlowFieldScope() { flowSystem.Shutdown(); }
};

std::vector<unsigned char> BuildCostMap(int width, int height) {
  std::vector<unsigned char> map(static_cast<size_t>(width) * height, 1);

  // Add deterministic wall strips so path integration does real work.
  for (int y = 32; y < 224; ++y) {
    map[static_cast<size_t>(y) * width + 96] = 255;
    map[static_cast<size_t>(y) * width + 160] = 255;
  }
  for (int x = 64; x < 192; ++x) {
    map[static_cast<size_t>(112) * width + x] = 255;
    map[static_cast<size_t>(176) * width + x] = 255;
  }

  return map;
}

void FillEntityBuffer(render::PersistentBuffer &entityBuffer, int entityCount) {
  entityBuffer.Create(static_cast<size_t>(entityCount) *
                      sizeof(NoMoreDay::components::GPUEntity));

  auto *gpuEntities = static_cast<NoMoreDay::components::GPUEntity *>(
      entityBuffer.BeginWrite());

  std::mt19937 rng(2402);
  std::uniform_real_distribution<float> posDist(0.0f,
                                                kGridSize * kCellSize - 1.0f);

  for (int i = 0; i < entityCount; ++i) {
    auto &e = gpuEntities[i];
    e.position = {posDist(rng), posDist(rng)};
    e.prevPosition = e.position;
    e.velocity = {0.0f, 0.0f};
    e.radius = 4.0f;
    e.type = 1;
    e.flags = 0;
    e.frameId = static_cast<uint32_t>(i);
  }

  entityBuffer.Flush();
}

} // namespace flow_field_benchmark_detail

TEST_CASE("[Performance] GPUFlowFieldSystem - 256x256 Map Update") {
  TestSetupScope scope;
  flow_field_benchmark_detail::FlowFieldScope flowScope;
  auto &flow = flowScope.flowSystem;

  const std::vector<unsigned char> costMap =
      flow_field_benchmark_detail::BuildCostMap(
          flow_field_benchmark_detail::kGridSize,
          flow_field_benchmark_detail::kGridSize);

  const Vector2 gridOrigin = {0.0f, 0.0f};

  for (int i = 0; i < flow_field_benchmark_detail::kWarmupFrames; ++i) {
    const float targetX = 900.0f + static_cast<float>((i % 3) * 20);
    const float targetY = 1200.0f + static_cast<float>((i % 2) * 20);
    flow.Update(costMap, flow_field_benchmark_detail::kGridSize,
                flow_field_benchmark_detail::kGridSize, {targetX, targetY},
                gridOrigin);
    glFinish();
  }

  std::vector<double> samples;
  samples.reserve(flow_field_benchmark_detail::kBenchFrames);
  for (int i = 0; i < flow_field_benchmark_detail::kBenchFrames; ++i) {
    const float targetX = 1000.0f + static_cast<float>((i % 4) * 20);
    const float targetY = 1000.0f + static_cast<float>(((i / 2) % 4) * 20);
    ScopedTimer timer(samples);
    flow.Update(costMap, flow_field_benchmark_detail::kGridSize,
                flow_field_benchmark_detail::kGridSize, {targetX, targetY},
                gridOrigin);
    glFinish();
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("GPUFlowFieldSystem update 256x256", stats,
                "< 0.8ms / < 1.5ms");
  flow_field_benchmark_detail::LogThresholdWarn(
      "GPUFlowFieldSystem update 256x256", stats, 0.8, 1.5);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] GPUFlowFieldSystem - CrowdDensity 5000") {
  TestSetupScope scope;
  flow_field_benchmark_detail::FlowFieldScope flowScope;
  auto &flow = flowScope.flowSystem;

  const std::vector<unsigned char> costMap =
      flow_field_benchmark_detail::BuildCostMap(
          flow_field_benchmark_detail::kGridSize,
          flow_field_benchmark_detail::kGridSize);

  const Vector2 gridOrigin = {0.0f, 0.0f};
  const Vector2 targetPos = {1280.0f, 1280.0f};
  flow.Update(costMap, flow_field_benchmark_detail::kGridSize,
              flow_field_benchmark_detail::kGridSize, targetPos, gridOrigin);
  glFinish();

  render::PersistentBuffer entityBuffer;
  constexpr int kEntityCount = 5000;
  flow_field_benchmark_detail::FillEntityBuffer(entityBuffer, kEntityCount);

  for (int i = 0; i < flow_field_benchmark_detail::kWarmupFrames; ++i) {
    flow.UpdateCrowdDensity(entityBuffer, kEntityCount,
                            flow_field_benchmark_detail::kCellSize);
    glFinish();
  }

  std::vector<double> samples;
  samples.reserve(flow_field_benchmark_detail::kBenchFrames);
  for (int i = 0; i < flow_field_benchmark_detail::kBenchFrames; ++i) {
    ScopedTimer timer(samples);
    flow.UpdateCrowdDensity(entityBuffer, kEntityCount,
                            flow_field_benchmark_detail::kCellSize);
    glFinish();
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("GPUFlowFieldSystem crowd density 5000", stats, "< 0.5ms");
  flow_field_benchmark_detail::LogThresholdWarn(
      "GPUFlowFieldSystem crowd density 5000", stats, 0.5, 1.0);
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
