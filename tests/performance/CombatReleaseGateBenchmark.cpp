#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>

namespace NoMoreDay::tests {
namespace {

constexpr uint32_t kSkillId = 0;
constexpr Tag kHitTags = Tag::Hit | Tag::Melee;
constexpr int kTargetCount = 32;
constexpr int kHitsPerFrame = 24;
constexpr int kWarmupFrames = 120;
constexpr int kSampleFrames = 1800;
constexpr double kFrameP95TargetMs = 8.0;
constexpr double kFrameP99TargetMs = 12.0;

struct FramePercentiles {
  double p95_ms = 0.0;
  double p99_ms = 0.0;
};

DamagePool CreateGateDamagePool() {
  DamagePool pool;
  pool.Add(Tag::Physical, 100.0f);
  pool.Add(Tag::Fire, 60.0f);
  pool.Add(Tag::Lightning, 40.0f);
  return pool;
}

entt::entity CreateGateAttacker(entt::registry &registry) {
  const entt::entity attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 95.0f;
  stats.max_weapon_damage = 120.0f;
  stats.crit_chance = 0.18f;
  stats.crit_damage = 1.65f;
  stats.accuracy = 1.0f;
  stats.armor_pen = 14.0f;
  return attacker;
}

entt::entity CreateGateDefender(entt::registry &registry, const int index) {
  const entt::entity defender = registry.create();
  auto &stats = registry.emplace<CombatStats>(defender);
  stats.cached_area_level = 20;
  stats.armor = 125.0f + static_cast<float>(index % 10);
  stats.damage_reduction = 0.08f;
  stats.resistances[0] = 0.20f;
  stats.resistances[1] = 0.14f;
  stats.resistances[2] = 0.10f;
  stats.resistances[3] = 0.05f;
  stats.resistances[4] = 0.0f;
  stats.resistances[5] = 0.0f;
  registry.emplace<HealthComponent>(defender, 150000.0f, 150000.0f);
  return defender;
}

double RunCombatFrame(entt::registry &registry, const entt::entity attacker,
                      const std::vector<entt::entity> &defenders,
                      const DamagePool &basePool, int frameIndex) {
  double totalDamage = 0.0;
  const int defenderCount = static_cast<int>(defenders.size());
  for (int hitIndex = 0; hitIndex < kHitsPerFrame; ++hitIndex) {
    const int targetIndex = (frameIndex + hitIndex) % defenderCount;
    const DamageResult result =
        DamagePipeline::Calculate(registry, attacker, defenders[targetIndex],
                                  kSkillId, basePool, kHitTags, entt::null,
                                  true);
    totalDamage += result.total_damage;
  }
  return totalDamage;
}

FramePercentiles ComputeFramePercentiles(std::vector<double> frameSamplesMs) {
  if (frameSamplesMs.empty()) {
    return {};
  }
  std::sort(frameSamplesMs.begin(), frameSamplesMs.end());
  const size_t size = frameSamplesMs.size();
  size_t idx95 = static_cast<size_t>(size * 0.95);
  size_t idx99 = static_cast<size_t>(size * 0.99);
  if (idx95 >= size) {
    idx95 = size - 1;
  }
  if (idx99 >= size) {
    idx99 = size - 1;
  }
  return {frameSamplesMs[idx95], frameSamplesMs[idx99]};
}

} // namespace

TEST_CASE("[Performance] Combat Release Gate - Frame Percentiles") {
  TestSetupScope scope;
  entt::registry registry;
  const entt::entity attacker = CreateGateAttacker(registry);
  const DamagePool damagePool = CreateGateDamagePool();

  std::vector<entt::entity> defenders;
  defenders.reserve(kTargetCount);
  for (int index = 0; index < kTargetCount; ++index) {
    defenders.push_back(CreateGateDefender(registry, index));
  }

  for (int frame = 0; frame < kWarmupFrames; ++frame) {
    (void)RunCombatFrame(registry, attacker, defenders, damagePool, frame);
  }

  std::vector<double> frameSamplesMs;
  frameSamplesMs.reserve(kSampleFrames);
  double damageSink = 0.0;
  for (int frame = 0; frame < kSampleFrames; ++frame) {
    const auto start = std::chrono::high_resolution_clock::now();
    damageSink += RunCombatFrame(registry, attacker, defenders, damagePool,
                                 frame + kWarmupFrames);
    const auto end = std::chrono::high_resolution_clock::now();
    frameSamplesMs.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  CHECK(damageSink > 0.0);

  const BenchmarkStats stats = CalculateStats(frameSamplesMs);
  const FramePercentiles percentiles = ComputeFramePercentiles(frameSamplesMs);
  LOG_BENCHMARK("Combat Release Gate Frame Time", stats, "p95/p99 budget");

  std::cout << "RELEASE_GATE_METRIC combat_frame_p95_ms=" << percentiles.p95_ms
            << "\n";
  std::cout << "RELEASE_GATE_METRIC combat_frame_p99_ms=" << percentiles.p99_ms
            << "\n";

  CHECK(percentiles.p95_ms <= kFrameP95TargetMs);
  CHECK(percentiles.p99_ms <= kFrameP99TargetMs);
}

} // namespace NoMoreDay::tests
