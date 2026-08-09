#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/systems/ai/AISystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace NoMoreDay::tests {
namespace ai_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 100;

constexpr float kCellSize = 32.0f;
constexpr int kGridCols = 256;
constexpr int kGridRows = 256;

entt::entity CreatePlayer(entt::registry &registry, float x, float y) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, x, y);
  return player;
}

void CreateChaseEnemies(entt::registry &registry, entt::entity player,
                        int count, float centerX, float centerY,
                        float spread) {
  std::mt19937 rng(52);
  std::uniform_real_distribution<float> offset(-spread, spread);

  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<::EnemyTag>(e);
    registry.emplace<Position>(e, centerX + offset(rng), centerY + offset(rng));
    registry.emplace<Velocity>(e, 0.0f, 0.0f);
    auto &ai = registry.emplace<::AIComponent>(e);
    ai.aiType = ::AIType::CHASE;
    ai.target = player;
    ai.detectionRange = 1200.0f;
    ai.attackRange = 50.0f;
    ai.speed = 120.0f;
  }
}

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

} // namespace ai_benchmark_detail

TEST_CASE("[Performance] AISystem - 5000 Enemies Update") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(ai_benchmark_detail::kGridCols,
                                ai_benchmark_detail::kGridRows,
                                ai_benchmark_detail::kCellSize);

  MapSystem mapSystem;
  mapSystem.generateTownMap(256, 256);

  const Position playerPos = {500.0f, 500.0f};
  const entt::entity player =
      ai_benchmark_detail::CreatePlayer(registry, playerPos.x, playerPos.y);
  (void)player;

  ai_benchmark_detail::CreateChaseEnemies(registry, player, 5000, 500.0f,
                                          500.0f, 450.0f);

  const auto posView = registry.view<Position>();
  for (int i = 0; i < ai_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    AISystem::update(registry, grid, mapSystem, playerPos,
                     ai_benchmark_detail::kDt);
  }

  std::vector<double> samples;
  samples.reserve(ai_benchmark_detail::kBenchFrames);
  for (int i = 0; i < ai_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    AISystem::update(registry, grid, mapSystem, playerPos,
                     ai_benchmark_detail::kDt);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("AISystem update 5000", stats, "< 2.0ms / < 4.0ms");
  ai_benchmark_detail::LogThresholdWarn("AISystem update 5000", stats, 2.0,
                                        4.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] AISystem - findNearestTarget Scaling") {
  TestSetupScope scope;
  MapSystem mapSystem;
  mapSystem.generateTownMap(256, 256);
  systems::SpatialHashGrid grid(ai_benchmark_detail::kGridCols,
                                ai_benchmark_detail::kGridRows,
                                ai_benchmark_detail::kCellSize);

  constexpr int kTargetCount = 10000;
  constexpr int kQueryCount = 1000;
  constexpr int kFrames = 30;
  const Position playerPos = {0.0f, 0.0f};

  const std::array<float, 3> ranges = {100.0f, 500.0f, 1000.0f};
  std::array<double, 3> means = {0.0, 0.0, 0.0};

  for (size_t idx = 0; idx < ranges.size(); ++idx) {
    entt::registry registry;
    std::mt19937 rng(80 + static_cast<int>(idx));
    std::uniform_real_distribution<float> pDist(-1200.0f, 1200.0f);

    for (int i = 0; i < kTargetCount; ++i) {
      const entt::entity p = registry.create();
      registry.emplace<PlayerTag>(p);
      registry.emplace<Position>(p, pDist(rng), pDist(rng));
    }

    for (int i = 0; i < kQueryCount; ++i) {
      const entt::entity e = registry.create();
      registry.emplace<::EnemyTag>(e);
      registry.emplace<Position>(e, pDist(rng) * 0.2f, pDist(rng) * 0.2f);
      registry.emplace<Velocity>(e, 0.0f, 0.0f);
      auto &ai = registry.emplace<::AIComponent>(e);
      ai.aiType = ::AIType::IDLE;
      ai.detectionRange = ranges[idx];
      ai.decisionInterval = 0.0f;
      ai.lastDecisionTime = 1.0f;
      registry.emplace<::EnemyStateComponent>(e);
    }

    const auto posView = registry.view<Position>();
    for (int i = 0; i < 3; ++i) {
      grid.rebuild(posView, registry);
      AISystem::update(registry, grid, mapSystem, playerPos,
                       ai_benchmark_detail::kDt);
    }

    std::vector<double> samples;
    samples.reserve(kFrames);
    for (int f = 0; f < kFrames; ++f) {
      auto aiView = registry.view<::AIComponent>();
      for (entt::entity e : aiView) {
        auto &ai = aiView.get<::AIComponent>(e);
        ai.aiType = ::AIType::IDLE;
        ai.target = entt::null;
        ai.lastDecisionTime = 1.0f;
      }

      grid.rebuild(posView, registry);
      ScopedTimer timer(samples);
      AISystem::update(registry, grid, mapSystem, playerPos,
                       ai_benchmark_detail::kDt);
    }

    const BenchmarkStats stats = CalculateStats(samples);
    means[idx] = stats.mean_ms;
    LOG_BENCHMARK("AISystem findNearestTarget scaling", stats,
                  std::string("range=") + std::to_string((int)ranges[idx]));
  }

  CHECK(means[0] > 0.0);
  CHECK(means[2] >= means[0] * 0.5);
}

} // namespace NoMoreDay::tests
