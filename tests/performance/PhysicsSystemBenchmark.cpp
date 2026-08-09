#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/systems/physics/PhysicsSystem.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include <random>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace NoMoreDay::tests {
namespace physics_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kWorldWidth = 5000;
constexpr int kWorldHeight = 5000;
constexpr float kCellSize = 32.0f;
constexpr int kGridCols = static_cast<int>(kWorldWidth / kCellSize) + 1;
constexpr int kGridRows = static_cast<int>(kWorldHeight / kCellSize) + 1;
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

void PopulateMovingEntities(entt::registry &registry, int count, float minX,
                            float maxX, float minY, float maxY, float speed,
                            bool addEnemyTag = true) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> distX(minX, maxX);
  std::uniform_real_distribution<float> distY(minY, maxY);
  std::uniform_real_distribution<float> vel(-speed, speed);

  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<Position>(e, distX(rng), distY(rng));
    registry.emplace<Velocity>(e, vel(rng), vel(rng));
    registry.emplace<Radius>(e, 4.0f);
    if (addEnemyTag) {
      registry.emplace<::EnemyTag>(e);
    }
  }
}

} // namespace physics_benchmark_detail

TEST_CASE("[Performance] PhysicsSystem - updateAll 10K Entities") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(physics_benchmark_detail::kGridCols,
                                physics_benchmark_detail::kGridRows,
                                physics_benchmark_detail::kCellSize);
  tf::Executor executor;

  physics_benchmark_detail::PopulateMovingEntities(
      registry, 10000, 0.0f,
      static_cast<float>(physics_benchmark_detail::kWorldWidth), 0.0f,
      static_cast<float>(physics_benchmark_detail::kWorldHeight), 90.0f);

  const auto posView = registry.view<Position>();
  for (int i = 0; i < physics_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    PhysicsSystem::updateAll(registry, physics_benchmark_detail::kDt,
                             physics_benchmark_detail::kWorldWidth,
                             physics_benchmark_detail::kWorldHeight, grid,
                             &executor);
  }

  std::vector<double> samples;
  samples.reserve(physics_benchmark_detail::kBenchFrames);

  for (int i = 0; i < physics_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    PhysicsSystem::updateAll(registry, physics_benchmark_detail::kDt,
                             physics_benchmark_detail::kWorldWidth,
                             physics_benchmark_detail::kWorldHeight, grid,
                             &executor);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("PhysicsSystem updateAll 10K", stats, "< 3.0ms / < 5.0ms");
  physics_benchmark_detail::LogThresholdWarn("PhysicsSystem updateAll 10K",
                                             stats, 3.0, 5.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] PhysicsSystem - High Density Collision") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(physics_benchmark_detail::kGridCols,
                                physics_benchmark_detail::kGridRows,
                                physics_benchmark_detail::kCellSize);
  tf::Executor executor;

  // 5000 entities packed into a small zone to maximize collision resolution.
  physics_benchmark_detail::PopulateMovingEntities(
      registry, 5000, 2400.0f, 2600.0f, 2400.0f, 2600.0f, 45.0f);

  const auto posView = registry.view<Position>();
  for (int i = 0; i < physics_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    PhysicsSystem::updateAll(registry, physics_benchmark_detail::kDt,
                             physics_benchmark_detail::kWorldWidth,
                             physics_benchmark_detail::kWorldHeight, grid,
                             &executor);
  }

  std::vector<double> samples;
  samples.reserve(physics_benchmark_detail::kBenchFrames);

  for (int i = 0; i < physics_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    PhysicsSystem::updateAll(registry, physics_benchmark_detail::kDt,
                             physics_benchmark_detail::kWorldWidth,
                             physics_benchmark_detail::kWorldHeight, grid,
                             &executor);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("PhysicsSystem high density", stats, "stress");
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] PhysicsSystem - Force Fields") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(physics_benchmark_detail::kGridCols,
                                physics_benchmark_detail::kGridRows,
                                physics_benchmark_detail::kCellSize);

  physics_benchmark_detail::PopulateMovingEntities(
      registry, 10000, 0.0f,
      static_cast<float>(physics_benchmark_detail::kWorldWidth), 0.0f,
      static_cast<float>(physics_benchmark_detail::kWorldHeight), 60.0f, false);

  std::mt19937 rng(7);
  std::uniform_real_distribution<float> distX(
      0.0f, static_cast<float>(physics_benchmark_detail::kWorldWidth));
  std::uniform_real_distribution<float> distY(
      0.0f, static_cast<float>(physics_benchmark_detail::kWorldHeight));

  for (int i = 0; i < 50; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<Position>(e, distX(rng), distY(rng));
    auto &ff = registry.emplace<ForceFieldComponent>(e);
    ff.strength = (i % 2 == 0) ? 220.0f : -180.0f;
    ff.radius = 220.0f;
    ff.isAlwaysOn = true;
  }

  const auto posView = registry.view<Position>();
  for (int i = 0; i < physics_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    PhysicsSystem::applyForceFields(registry, physics_benchmark_detail::kDt,
                                    grid);
  }

  std::vector<double> samples;
  samples.reserve(physics_benchmark_detail::kBenchFrames);

  for (int i = 0; i < physics_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    PhysicsSystem::applyForceFields(registry, physics_benchmark_detail::kDt,
                                    grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("PhysicsSystem force fields", stats, "< 0.5ms");
  physics_benchmark_detail::LogThresholdWarn("PhysicsSystem force fields",
                                             stats, 0.5, 1.0);
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
