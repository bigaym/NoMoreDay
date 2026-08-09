#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/HazardComponents.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/HazardSystem.hpp"
#include <random>
#include <vector>

namespace NoMoreDay::tests {
namespace hazard_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
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

entt::entity CreateOwner(entt::registry &registry) {
  const entt::entity owner = registry.create();
  registry.emplace<Position>(owner, 500.0f, 500.0f);
  registry.emplace<CombatStats>(owner);
  return owner;
}

void CreateTargets(entt::registry &registry, int count, float centerX,
                   float centerY, float spread) {
  std::mt19937 rng(920);
  std::uniform_real_distribution<float> offset(-spread, spread);
  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<::EnemyTag>(e);
    registry.emplace<Position>(e, centerX + offset(rng), centerY + offset(rng));
    registry.emplace<Radius>(e, 6.0f);
    registry.emplace<CombatStats>(e);
    registry.emplace<HealthComponent>(e, 10000.0f, 10000.0f);
  }
}

} // namespace hazard_benchmark_detail

TEST_CASE("[Performance] HazardSystem - 200 Hazards Update") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(256, 256, 32.0f);
  const entt::entity owner = hazard_benchmark_detail::CreateOwner(registry);
  (void)owner;

  hazard_benchmark_detail::CreateTargets(registry, 500, 500.0f, 500.0f, 420.0f);

  std::mt19937 rng(12);
  std::uniform_real_distribution<float> dist(120.0f, 880.0f);
  for (int i = 0; i < 200; ++i) {
    const entt::entity h = registry.create();
    registry.emplace<Position>(h, dist(rng), dist(rng));
    registry.emplace<Radius>(h, 60.0f);
    auto &hazard = registry.emplace<HazardComponent>(h);
    hazard.owner = owner;
    hazard.duration = 8.0f;
    hazard.tickInterval = 0.2f;
    hazard.currentTickTimer = 0.15f;
    hazard.damagePerTick = 20.0f;
    hazard.damageType = DamageType::Fire;
    hazard.hitsEnemies = true;
    hazard.hitsPlayers = false;
  }

  const auto posView = registry.view<Position>();
  for (int i = 0; i < hazard_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  std::vector<double> samples;
  samples.reserve(hazard_benchmark_detail::kBenchFrames);
  for (int i = 0; i < hazard_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("HazardSystem update 200", stats, "< 0.5ms / < 1.0ms");
  hazard_benchmark_detail::LogThresholdWarn("HazardSystem update 200", stats,
                                            0.5, 1.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] HazardSystem - DealAreaDamage Stress") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(256, 256, 32.0f);
  const entt::entity owner = hazard_benchmark_detail::CreateOwner(registry);

  // 500 potential victims.
  hazard_benchmark_detail::CreateTargets(registry, 500, 500.0f, 500.0f, 350.0f);

  // 100 frequent-tick hazards to stress area query + damage path.
  for (int i = 0; i < 100; ++i) {
    const entt::entity h = registry.create();
    const float x = 300.0f + static_cast<float>((i % 10) * 40);
    const float y = 300.0f + static_cast<float>((i / 10) * 40);
    registry.emplace<Position>(h, x, y);
    registry.emplace<Radius>(h, 100.0f);
    auto &hazard = registry.emplace<HazardComponent>(h);
    hazard.owner = owner;
    hazard.duration = 6.0f;
    hazard.tickInterval = 0.01f;
    hazard.currentTickTimer = 0.009f;
    hazard.damagePerTick = 10.0f;
    hazard.damageType = DamageType::Cold;
    hazard.hitsEnemies = true;
    hazard.hitsPlayers = false;
  }

  const auto posView = registry.view<Position>();
  for (int i = 0; i < hazard_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  std::vector<double> samples;
  samples.reserve(hazard_benchmark_detail::kBenchFrames);
  for (int i = 0; i < hazard_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("HazardSystem DealAreaDamage stress", stats, "100x500");
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] HazardSystem - ProcessFrozenOrbs 50") {
  TestSetupScope scope;
  entt::registry registry;
  systems::SpatialHashGrid grid(256, 256, 32.0f);

  // Keep one player for potential target checks.
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 500.0f, 500.0f);
  registry.emplace<HealthComponent>(player, 10000.0f, 10000.0f);
  registry.emplace<CombatStats>(player);

  for (int i = 0; i < 50; ++i) {
    const entt::entity orb = registry.create();
    registry.emplace<FrozenOrbTag>(orb);
    registry.emplace<Position>(orb, 300.0f + i * 5.0f, 500.0f);
    registry.emplace<Velocity>(orb, 80.0f, 0.0f);
    auto &frozen = registry.emplace<FrozenOrbComponent>(orb);
    frozen.travelDuration = 5.0f;
    frozen.stopDuration = 2.0f;
    frozen.currentTimer = 0.0f;
    frozen.isTraveling = true;
    frozen.hasStopped = false;
  }

  const auto posView = registry.view<Position>();
  for (int i = 0; i < hazard_benchmark_detail::kWarmupFrames; ++i) {
    grid.rebuild(posView, registry);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  std::vector<double> samples;
  samples.reserve(hazard_benchmark_detail::kBenchFrames);
  for (int i = 0; i < hazard_benchmark_detail::kBenchFrames; ++i) {
    grid.rebuild(posView, registry);
    ScopedTimer timer(samples);
    HazardSystem::Update(registry, hazard_benchmark_detail::kDt, grid);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("HazardSystem frozen orbs 50", stats, "tracking path");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
