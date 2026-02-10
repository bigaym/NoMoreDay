#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace NoMoreDay::tests {
namespace enemy_spawn_benchmark_detail {

constexpr int kMapSize = 128;

std::filesystem::path ResolveBiomeJsonPath() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }

  throw std::runtime_error("Unable to locate assets/data/biomes.json");
}

void PrepareBiomeRegistry() {
  BiomeRegistry::Get().LoadFromJSON(ResolveBiomeJsonPath().string());
}

size_t CountEnemies(entt::registry &registry) {
  size_t count = 0;
  auto view = registry.view<::EnemyTag>();
  for (entt::entity e : view) {
    (void)e;
    ++count;
  }
  return count;
}

Position CenterWorldPos() {
  constexpr float kHalf = kMapSize * Constants::World::GRID_TILE_SIZE * 0.5f;
  return {kHalf, kHalf};
}

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.3f}ms (target {:.3f}ms), "
             "P99={:.3f}ms (target {:.3f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

} // namespace enemy_spawn_benchmark_detail

TEST_CASE("[Performance] EnemySpawnSystem - Batch Spawn 100 Enemies") {
  TestSetupScope scope;
  enemy_spawn_benchmark_detail::PrepareBiomeRegistry();

  MapSystem mapSystem;
  mapSystem.generateMap(enemy_spawn_benchmark_detail::kMapSize,
                        enemy_spawn_benchmark_detail::kMapSize, "cave");

  const Position center = enemy_spawn_benchmark_detail::CenterWorldPos();
  std::vector<double> samples;
  samples.reserve(40);

  for (int iter = 0; iter < 40; ++iter) {
    entt::registry registry;
    EnemySpawnSystem spawnSystem;
    spawnSystem.initData(enemy_spawn_benchmark_detail::kMapSize,
                         enemy_spawn_benchmark_detail::kMapSize, 10, mapSystem,
                         BiomeID::Cave);

    ScopedTimer timer(samples);
    int frameGuard = 0;
    while (enemy_spawn_benchmark_detail::CountEnemies(registry) < 100 &&
           frameGuard < 240) {
      spawnSystem.updateEnemySpawning(center, registry, 0.016f, &mapSystem);
      ++frameGuard;
    }
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("EnemySpawnSystem batch spawn 100", stats, "< 5.0ms total");
  enemy_spawn_benchmark_detail::LogThresholdWarn(
      "EnemySpawnSystem batch spawn 100", stats, 5.0, 10.0);
  CHECK(!samples.empty());
}

TEST_CASE("[Performance] EnemySpawnSystem - updateEnemySpawning") {
  TestSetupScope scope;
  enemy_spawn_benchmark_detail::PrepareBiomeRegistry();

  MapSystem mapSystem;
  mapSystem.generateMap(enemy_spawn_benchmark_detail::kMapSize,
                        enemy_spawn_benchmark_detail::kMapSize, "cave");

  EnemySpawnSystem spawnSystem;
  entt::registry registry;
  spawnSystem.initData(enemy_spawn_benchmark_detail::kMapSize,
                       enemy_spawn_benchmark_detail::kMapSize, 10, mapSystem,
                       BiomeID::Cave);

  const Position center = enemy_spawn_benchmark_detail::CenterWorldPos();
  for (int i = 0; i < 30; ++i) {
    spawnSystem.updateEnemySpawning(center, registry, 0.016f, &mapSystem);
  }

  std::vector<double> samples;
  samples.reserve(100);

  constexpr float kRadius = 400.0f;
  for (int i = 0; i < 100; ++i) {
    const float angle = static_cast<float>(i) * 0.11f;
    const Position playerPos = {
        center.x + std::cos(angle) * kRadius,
        center.y + std::sin(angle) * kRadius,
    };

    ScopedTimer timer(samples);
    spawnSystem.updateEnemySpawning(playerPos, registry, 0.016f, &mapSystem);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("EnemySpawnSystem updateEnemySpawning", stats,
                "100 frames movement");
  CHECK(!samples.empty());
}

} // namespace NoMoreDay::tests
