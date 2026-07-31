#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include <algorithm>
#include <array>
#include <random>
#include <vector>

namespace NoMoreDay::tests {
namespace monster_affix_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 100;

constexpr float kCellSize = 32.0f;
constexpr int kGridCols = 256;
constexpr int kGridRows = 256;

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

entt::entity CreatePlayer(entt::registry &registry, float x, float y) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, x, y);
  registry.emplace<CombatStats>(player);
  registry.emplace<HealthComponent>(player, 5000.0f, 5000.0f);
  return player;
}

void PrimeAffixTimers(MonsterAffixComponent &affix) {
  for (size_t i = 0; i < affix.affixes.size(); ++i) {
    switch (affix.affixes[i]) {
    case MonsterAffixType::Molten:
      affix.timers[i] = MonsterAffixRegistry::Params::MOLTEN_TICK_INTERVAL;
      break;
    case MonsterAffixType::Teleporter:
      affix.timers[i] = MonsterAffixRegistry::Params::TELEPORT_COOLDOWN;
      break;
    case MonsterAffixType::Frozen:
      affix.timers[i] = MonsterAffixRegistry::Params::FROZEN_ORB_INTERVAL;
      break;
    case MonsterAffixType::VoidZone:
      affix.timers[i] = MonsterAffixRegistry::Params::VOIDZONE_SPAWN_INTERVAL_MAX;
      break;
    case MonsterAffixType::Vortex:
      affix.timers[i] = MonsterAffixRegistry::Params::VORTEX_INTERVAL;
      break;
    default:
      affix.timers[i] = 1.0f;
      break;
    }
  }
}

void CreateAffixedEnemies(entt::registry &registry, int count,
                          const std::vector<MonsterAffixType> &pool,
                          int minAffixCount, int maxAffixCount) {
  std::mt19937 rng(4201);
  std::uniform_real_distribution<float> posDist(850.0f, 1350.0f);
  std::uniform_int_distribution<int> affixCountDist(minAffixCount,
                                                     maxAffixCount);

  for (int i = 0; i < count; ++i) {
    const entt::entity enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, posDist(rng), posDist(rng));
    registry.emplace<Radius>(enemy, 10.0f);
    registry.emplace<HealthComponent>(enemy, 2000.0f, 2000.0f);
    registry.emplace<CombatStats>(enemy);

    auto &affixComp = registry.emplace<MonsterAffixComponent>(enemy);
    std::vector<MonsterAffixType> localPool = pool;
    std::shuffle(localPool.begin(), localPool.end(), rng);

    const int affixCount = std::min(affixCountDist(rng),
                                    static_cast<int>(localPool.size()));
    for (int idx = 0; idx < affixCount; ++idx) {
      affixComp.AddAffix(localPool[idx]);
    }

    PrimeAffixTimers(affixComp);
  }
}

} // namespace monster_affix_benchmark_detail

TEST_CASE("[Performance] MonsterAffixSystem - 500 Affixed Enemies") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(monster_affix_benchmark_detail::kGridCols,
                                monster_affix_benchmark_detail::kGridRows,
                                monster_affix_benchmark_detail::kCellSize);

  const entt::entity player =
      monster_affix_benchmark_detail::CreatePlayer(registry, 500.0f, 500.0f);
  (void)player;

  const std::vector<MonsterAffixType> affixPool = {
      MonsterAffixType::Molten, MonsterAffixType::Teleporter,
      MonsterAffixType::Frozen, MonsterAffixType::VoidZone};
  monster_affix_benchmark_detail::CreateAffixedEnemies(registry, 500, affixPool,
                                                        2, 4);

  const auto posView = registry.view<Position>();
  for (int i = 0; i < monster_affix_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    MonsterAffixSystem::Update(registry, monster_affix_benchmark_detail::kDt,
                               grid);
  }

  std::vector<double> samples;
  samples.reserve(monster_affix_benchmark_detail::kBenchFrames);
  for (int i = 0; i < monster_affix_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    MonsterAffixSystem::Update(registry, monster_affix_benchmark_detail::kDt,
                               grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("MonsterAffixSystem update 500", stats, "< 0.5ms / < 1.0ms");
  monster_affix_benchmark_detail::LogThresholdWarn(
      "MonsterAffixSystem update 500", stats, 0.5, 1.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] MonsterAffixSystem - Mixed Affixes Stress") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(monster_affix_benchmark_detail::kGridCols,
                                monster_affix_benchmark_detail::kGridRows,
                                monster_affix_benchmark_detail::kCellSize);

  const entt::entity player =
      monster_affix_benchmark_detail::CreatePlayer(registry, 500.0f, 500.0f);
  (void)player;

  const std::vector<MonsterAffixType> stressPool = {
      MonsterAffixType::Molten, MonsterAffixType::Teleporter,
      MonsterAffixType::Frozen, MonsterAffixType::VoidZone,
      MonsterAffixType::Vortex};
  monster_affix_benchmark_detail::CreateAffixedEnemies(registry, 500, stressPool,
                                                        3, 4);

  const auto posView = registry.view<Position>();
  for (int i = 0; i < monster_affix_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    MonsterAffixSystem::Update(registry, monster_affix_benchmark_detail::kDt,
                               grid);
  }

  std::vector<double> samples;
  samples.reserve(80);
  for (int i = 0; i < 80; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    MonsterAffixSystem::Update(registry, monster_affix_benchmark_detail::kDt,
                               grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("MonsterAffixSystem mixed stress", stats, "500 enemies x 3-4");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
