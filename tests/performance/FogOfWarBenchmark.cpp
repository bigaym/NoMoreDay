#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "rlgl.h"
#include <vector>

namespace NoMoreDay::tests {
namespace fog_of_war_benchmark_detail {

constexpr int kMapSize = 256;
constexpr float kViewRadius = 320.0f;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 100;

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

} // namespace fog_of_war_benchmark_detail

TEST_CASE("[Performance] FogOfWarSystem - updateVisibility 256x256") {
  TestSetupScope scope;
  ResourceManager resources;
  FogOfWarSystem fog;
  fog.initialize(resources, fog_of_war_benchmark_detail::kMapSize,
                 fog_of_war_benchmark_detail::kMapSize);

  for (int i = 0; i < fog_of_war_benchmark_detail::kWarmupFrames; ++i) {
    const Position playerPos = {
        1200.0f + static_cast<float>((i % 5) * 20),
        1200.0f + static_cast<float>(((i + 2) % 5) * 20),
    };
    fog.updateVisibility(playerPos, fog_of_war_benchmark_detail::kViewRadius);
    glFinish();
  }

  std::vector<double> samples;
  samples.reserve(fog_of_war_benchmark_detail::kBenchFrames);
  for (int i = 0; i < fog_of_war_benchmark_detail::kBenchFrames; ++i) {
    const Position playerPos = {
        900.0f + static_cast<float>((i % 8) * 18),
        1400.0f + static_cast<float>(((i / 2) % 8) * 18),
    };

    ScopedTimer timer(samples);
    fog.updateVisibility(playerPos, fog_of_war_benchmark_detail::kViewRadius);
    glFinish();
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("FogOfWar updateVisibility 256x256", stats,
                "< 0.3ms / < 0.8ms");
  fog_of_war_benchmark_detail::LogThresholdWarn(
      "FogOfWar updateVisibility 256x256", stats, 0.3, 0.8);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] FogOfWarSystem - syncToCPU") {
  TestSetupScope scope;
  ResourceManager resources;
  FogOfWarSystem fog;
  fog.initialize(resources, fog_of_war_benchmark_detail::kMapSize,
                 fog_of_war_benchmark_detail::kMapSize);

  // Prime with one update.
  fog.updateVisibility({1280.0f, 1280.0f}, fog_of_war_benchmark_detail::kViewRadius);
  glFinish();

  std::vector<double> samples;
  samples.reserve(80);
  for (int i = 0; i < 80; ++i) {
    const Position playerPos = {
        1100.0f + static_cast<float>((i % 6) * 16),
        1100.0f + static_cast<float>(((i + 1) % 6) * 16),
    };

    fog.updateVisibility(playerPos, fog_of_war_benchmark_detail::kViewRadius);
    glFinish();

    ScopedTimer timer(samples);
    fog.syncToCPU();
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("FogOfWar syncToCPU", stats, "GPU->CPU readback");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
