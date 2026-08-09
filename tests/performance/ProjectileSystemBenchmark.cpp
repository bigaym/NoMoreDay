#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include <algorithm>
#include <random>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace NoMoreDay::tests {
namespace projectile_benchmark_detail {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kWarmupFrames = 10;
constexpr int kBenchFrames = 100;

entt::entity CreatePlayer(entt::registry &registry) {
  const entt::entity player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 500.0f, 500.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.min_weapon_damage = 10.0f;
  stats.max_weapon_damage = 20.0f;
  return player;
}

void CreateEnemyTargets(entt::registry &registry, int count, float centerX,
                        float centerY, float spread) {
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> offset(-spread, spread);
  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<::EnemyTag>(e);
    registry.emplace<Position>(e, centerX + offset(rng), centerY + offset(rng));
    registry.emplace<Velocity>(e, 0.0f, 0.0f);
    registry.emplace<CombatStats>(e);
  }
}

void CreateProjectiles(entt::registry &registry, entt::entity owner, int count,
                       float centerX, float centerY, float spread) {
  std::mt19937 rng(321);
  std::uniform_real_distribution<float> offset(-spread, spread);
  std::uniform_real_distribution<float> vel(-120.0f, 120.0f);

  for (int i = 0; i < count; ++i) {
    const entt::entity e = registry.create();
    registry.emplace<Position>(e, centerX + offset(rng), centerY + offset(rng));
    registry.emplace<Velocity>(e, vel(rng), vel(rng));
    auto &proj = registry.emplace<Projectile>(e);
    proj.owner = owner;
    proj.lifeTime = 10.0f;
    proj.speed = 600.0f;
    proj.radius = 8.0f;
    proj.pierce = true;
    proj.pierceCount = 10000;
    proj.hasRendered = true;
    proj.hitLimitReached = false;
  }
}

void ResetProjectiles(entt::registry &registry) {
  auto view = registry.view<Projectile>();
  for (entt::entity e : view) {
    auto &proj = view.get<Projectile>(e);
    proj.hitEntities.clear();
    proj.hitLimitReached = false;
    proj.pierce = true;
    proj.pierceCount = 10000;
    proj.hasRendered = true;
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

} // namespace projectile_benchmark_detail

TEST_CASE("[Performance] ProjectileSystem - 500 Projectiles Update") {
  TestSetupScope scope;
  entt::registry registry;
  const entt::entity player = projectile_benchmark_detail::CreatePlayer(registry);
  projectile_benchmark_detail::CreateEnemyTargets(registry, 2000, 500.0f,
                                                  500.0f, 450.0f);
  projectile_benchmark_detail::CreateProjectiles(registry, player, 500, 500.0f,
                                                 500.0f, 400.0f);

  systems::SpatialHashGrid dummyGrid(256, 256, 32.0f);
  tf::Executor executor;

  for (int i = 0; i < projectile_benchmark_detail::kWarmupFrames; ++i) {
    projectile_benchmark_detail::ResetProjectiles(registry);
    ProjectileSystem::Update(registry, dummyGrid,
                             projectile_benchmark_detail::kDt, &executor);
  }

  std::vector<double> samples;
  samples.reserve(projectile_benchmark_detail::kBenchFrames);
  for (int i = 0; i < projectile_benchmark_detail::kBenchFrames; ++i) {
    projectile_benchmark_detail::ResetProjectiles(registry);
    ScopedTimer timer(samples);
    ProjectileSystem::Update(registry, dummyGrid,
                             projectile_benchmark_detail::kDt, &executor);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ProjectileSystem 500", stats, "< 1.0ms / < 2.0ms");
  projectile_benchmark_detail::LogThresholdWarn("ProjectileSystem 500", stats,
                                                1.0, 2.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] ProjectileSystem - 2000 Projectiles Stress") {
  TestSetupScope scope;
  entt::registry registry;
  const entt::entity player = projectile_benchmark_detail::CreatePlayer(registry);
  projectile_benchmark_detail::CreateEnemyTargets(registry, 2000, 500.0f,
                                                  500.0f, 450.0f);
  projectile_benchmark_detail::CreateProjectiles(registry, player, 2000, 500.0f,
                                                 500.0f, 420.0f);

  systems::SpatialHashGrid dummyGrid(256, 256, 32.0f);
  tf::Executor executor;

  for (int i = 0; i < projectile_benchmark_detail::kWarmupFrames; ++i) {
    projectile_benchmark_detail::ResetProjectiles(registry);
    ProjectileSystem::Update(registry, dummyGrid,
                             projectile_benchmark_detail::kDt, &executor);
  }

  std::vector<double> samples;
  samples.reserve(projectile_benchmark_detail::kBenchFrames);
  for (int i = 0; i < projectile_benchmark_detail::kBenchFrames; ++i) {
    projectile_benchmark_detail::ResetProjectiles(registry);
    ScopedTimer timer(samples);
    ProjectileSystem::Update(registry, dummyGrid,
                             projectile_benchmark_detail::kDt, &executor);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ProjectileSystem 2000", stats, "< 4.0ms");
  projectile_benchmark_detail::LogThresholdWarn("ProjectileSystem 2000", stats,
                                                4.0, 8.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] ProjectileSystem - Split Cascade") {
  TestSetupScope scope;
  systems::SpatialHashGrid dummyGrid(256, 256, 32.0f);
  tf::Executor executor;

  std::vector<double> samples;
  samples.reserve(60);
  int maxProjectileCount = 0;

  for (int i = 0; i < 60; ++i) {
    entt::registry registry;
    const entt::entity player = projectile_benchmark_detail::CreatePlayer(registry);
    projectile_benchmark_detail::CreateEnemyTargets(registry, 500, 500.0f,
                                                    500.0f, 300.0f);

    for (int j = 0; j < 50; ++j) {
      const entt::entity e = registry.create();
      registry.emplace<Position>(e, 500.0f + static_cast<float>(j), 500.0f);
      registry.emplace<Velocity>(e, 0.0f, 0.0f);
      auto &proj = registry.emplace<Projectile>(e);
      proj.owner = player;
      proj.lifeTime = 0.0001f;
      proj.radius = 8.0f;
      proj.on_death = Projectile::OnDeathBehavior::Split;
      proj.split_count = 8;
      proj.split_damage_mult = 0.6f;
      proj.split_speed_mult = 0.8f;
      proj.split_radius_mult = 0.7f;
      proj.split_spread = 1.0f;
      proj.hasRendered = true;
    }

    ScopedTimer timer(samples);
    ProjectileSystem::Update(registry, dummyGrid,
                             projectile_benchmark_detail::kDt, &executor);

    maxProjectileCount = std::max(
        maxProjectileCount, static_cast<int>(registry.view<Projectile>().size()));
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("ProjectileSystem split cascade", stats, "peak frame");
  CHECK(maxProjectileCount > 50);
}

} // namespace NoMoreDay::tests
