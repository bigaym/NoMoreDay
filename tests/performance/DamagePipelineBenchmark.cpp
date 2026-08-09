#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <array>
#include <string>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace NoMoreDay::tests {
namespace {

constexpr uint32_t kSkillId = 0;
constexpr Tag kHitTags = Tag::Hit | Tag::Melee;
constexpr int kSingleWarmup = 200;
constexpr int kSingleIterations = 10000;
constexpr int kBatchWarmup = 10;
constexpr int kBatchIterations = 100;

DamagePool CreateBaseDamagePool() {
  DamagePool pool;
  pool.Add(Tag::Physical, 120.0f);
  pool.Add(Tag::Fire, 80.0f);
  return pool;
}

entt::entity CreateAttacker(entt::registry &registry) {
  const entt::entity attacker = registry.create();
  CombatStats &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 100.0f;
  stats.max_weapon_damage = 120.0f;
  stats.crit_chance = 25.0f;
  stats.crit_damage = 1.8f;
  stats.armor_pen = 25.0f;

  auto &mods = registry.emplace<GlobalModifierComponent>(attacker);
  mods.modifiers.push_back(
      DamageModifier{Tag::Physical, Tag::Fire, 0.2f, ModifierType::Convert});
  mods.modifiers.push_back(
      DamageModifier{Tag::Fire, Tag::None, 0.15f, ModifierType::More});
  mods.modifiers.push_back(
      DamageModifier{Tag::None, Tag::None, 0.1f, ModifierType::More});

  return attacker;
}

entt::entity CreateDefender(entt::registry &registry, int index) {
  const entt::entity defender = registry.create();

  CombatStats &stats = registry.emplace<CombatStats>(defender);
  stats.cached_area_level = 10;
  stats.armor = 75.0f + static_cast<float>(index % 100);
  stats.damage_reduction = 0.05f + 0.001f * static_cast<float>(index % 10);
  stats.resistances[0] = 0.15f + 0.001f * static_cast<float>(index % 10);
  stats.resistances[1] = 0.20f + 0.001f * static_cast<float>(index % 7);
  stats.resistances[2] = 0.10f;
  stats.resistances[3] = 0.05f;
  stats.resistances[4] = 0.00f;
  stats.resistances[5] = 0.00f;

  registry.emplace<HealthComponent>(defender, 1000000.0f, 1000000.0f);
  return defender;
}

void LogThresholdWarn(const char *name, const BenchmarkStats &stats,
                      double meanTarget, double p99Target) {
  if (stats.mean_ms > meanTarget || stats.p99_ms > p99Target) {
    LOG_WARN("{} exceeded target. Mean={:.4f}ms (target {:.4f}ms), "
             "P99={:.4f}ms (target {:.4f}ms)",
             name, stats.mean_ms, meanTarget, stats.p99_ms, p99Target);
  }
}

} // namespace

TEST_CASE("[Performance] DamagePipeline - Single Calculate") {
  TestSetupScope scope;
  entt::registry registry;
  const entt::entity attacker = CreateAttacker(registry);
  const entt::entity defender = CreateDefender(registry, 0);
  const DamagePool basePool = CreateBaseDamagePool();

  for (int i = 0; i < kSingleWarmup; ++i) {
    (void)DamagePipeline::Calculate(registry, attacker, defender, kSkillId,
                                    basePool, kHitTags, entt::null, true);
  }

  std::vector<double> samples;
  samples.reserve(kSingleIterations);
  double damageSink = 0.0;

  for (int i = 0; i < kSingleIterations; ++i) {
    ScopedTimer timer(samples);
    const DamageResult result =
        DamagePipeline::Calculate(registry, attacker, defender, kSkillId,
                                  basePool, kHitTags, entt::null, true);
    damageSink += result.total_damage;
  }

  CHECK(damageSink > 0.0);

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("DamagePipeline Single Calculate", stats, "< 0.01ms");
  LogThresholdWarn("DamagePipeline Single Calculate", stats, 0.01, 0.02);
}

TEST_CASE("[Performance] DamagePipeline - CalculateBatch 200 Targets") {
  TestSetupScope scope;
  entt::registry registry;
  const entt::entity attacker = CreateAttacker(registry);
  const DamagePool basePool = CreateBaseDamagePool();

  std::vector<entt::entity> defenders;
  defenders.reserve(200);
  for (int i = 0; i < 200; ++i) {
    defenders.push_back(CreateDefender(registry, i));
  }

  tf::Executor executor;

  for (int i = 0; i < kBatchWarmup; ++i) {
    DamagePipeline::CalculateBatch(registry, attacker, defenders, kSkillId,
                                   basePool, kHitTags, entt::null, &executor);
  }

  std::vector<double> samples;
  samples.reserve(kBatchIterations);

  for (int i = 0; i < kBatchIterations; ++i) {
    ScopedTimer timer(samples);
    DamagePipeline::CalculateBatch(registry, attacker, defenders, kSkillId,
                                   basePool, kHitTags, entt::null, &executor);
  }

  const BenchmarkStats stats = CalculateStats(samples);
  LOG_BENCHMARK("DamagePipeline Batch 200", stats, "< 1.0ms");
  LogThresholdWarn("DamagePipeline Batch 200", stats, 1.0, 2.0);
}

TEST_CASE("[Performance] DamagePipeline - Batch Scaling") {
  TestSetupScope scope;
  const std::array<int, 4> targetCounts = {50, 100, 200, 500};
  std::array<double, 4> meanTimesMs = {0.0, 0.0, 0.0, 0.0};
  const DamagePool basePool = CreateBaseDamagePool();

  for (size_t idx = 0; idx < targetCounts.size(); ++idx) {
    entt::registry registry;
    const entt::entity attacker = CreateAttacker(registry);
    std::vector<entt::entity> defenders;
    defenders.reserve(targetCounts[idx]);
    for (int i = 0; i < targetCounts[idx]; ++i) {
      defenders.push_back(CreateDefender(registry, i));
    }

    tf::Executor executor;
    for (int i = 0; i < kBatchWarmup; ++i) {
      DamagePipeline::CalculateBatch(registry, attacker, defenders, kSkillId,
                                     basePool, kHitTags, entt::null, &executor);
    }

    std::vector<double> samples;
    samples.reserve(50);
    for (int i = 0; i < 50; ++i) {
      ScopedTimer timer(samples);
      DamagePipeline::CalculateBatch(registry, attacker, defenders, kSkillId,
                                     basePool, kHitTags, entt::null, &executor);
    }

    const BenchmarkStats stats = CalculateStats(samples);
    meanTimesMs[idx] = stats.mean_ms;
    LOG_BENCHMARK("DamagePipeline Batch Scaling", stats,
                  std::to_string(targetCounts[idx]) + " targets");
  }

  CHECK(meanTimesMs[0] > 0.0);
  CHECK(meanTimesMs[3] <= (meanTimesMs[0] * 12.0));
}

} // namespace NoMoreDay::tests
