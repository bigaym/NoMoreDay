#pragma once

#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <algorithm>
#include <vector>

namespace NoMoreDay::tests {
namespace {

constexpr uint32_t kSkillId = 0;
constexpr Tag kHitTags = Tag::Hit | Tag::Melee;
constexpr int kWarmupIterations = 200;
constexpr int kBenchmarkIterations = 6000;

DamagePool CreateBasePool() {
  DamagePool pool;
  pool.Add(Tag::Physical, 120.0f);
  pool.Add(Tag::Fire, 40.0f);
  return pool;
}

entt::entity CreateAttacker(entt::registry &registry) {
  const entt::entity attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.min_weapon_damage = 110.0f;
  stats.max_weapon_damage = 135.0f;
  stats.crit_chance = 0.20f;
  stats.crit_damage = 1.75f;
  stats.armor_pen = 20.0f;
  stats.accuracy = 1.0f;
  return attacker;
}

entt::entity CreateDefender(entt::registry &registry) {
  const entt::entity defender = registry.create();
  auto &stats = registry.emplace<CombatStats>(defender);
  stats.cached_area_level = 15;
  stats.armor = 120.0f;
  stats.damage_reduction = 0.08f;
  stats.resistances[0] = 0.15f;
  stats.resistances[1] = 0.2f;
  stats.resistances[2] = 0.1f;
  stats.resistances[3] = 0.1f;
  stats.resistances[4] = 0.0f;
  stats.resistances[5] = 0.0f;
  registry.emplace<HealthComponent>(defender, 100000.0f, 100000.0f);
  return defender;
}

double MeasureMeanMs(entt::registry &registry, const entt::entity attacker,
                     const entt::entity defender, const DamagePool &basePool,
                     const bool telemetryEnabled) {
  auto &telemetry = CombatTelemetry::Get();
  telemetry.ResetForTests();
  telemetry.SetRuntimeEnabled(telemetryEnabled);
  telemetry.SetOutputEnabled(false);
  telemetry.SetOutputIntervalSeconds(30.0f);

  for (int i = 0; i < kWarmupIterations; ++i) {
    (void)DamagePipeline::Calculate(registry, attacker, defender, kSkillId,
                                    basePool, kHitTags, entt::null, true);
  }

  std::vector<double> samples;
  samples.reserve(kBenchmarkIterations);
  double sink = 0.0;
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    ScopedTimer timer(samples);
    const auto result = DamagePipeline::Calculate(
        registry, attacker, defender, kSkillId, basePool, kHitTags, entt::null,
        true);
    sink += result.total_damage;
  }
  CHECK(sink > 0.0);

  const BenchmarkStats stats = CalculateStats(samples);
  return stats.mean_ms;
}

} // namespace

TEST_CASE("[Performance] CombatTelemetry - Instrumentation Overhead") {
  TestSetupScope scope;
  entt::registry registry;
  const auto attacker = CreateAttacker(registry);
  const auto defender = CreateDefender(registry);
  const DamagePool basePool = CreateBasePool();

  const double baselineMeanMs =
      MeasureMeanMs(registry, attacker, defender, basePool, false);
  const double telemetryMeanMs =
      MeasureMeanMs(registry, attacker, defender, basePool, true);
  const double overheadMs = (std::max)(0.0, telemetryMeanMs - baselineMeanMs);

  LOG_WARN(
      "[Benchmark] CombatTelemetry Overhead: baseline={:.4f}ms, telemetry={:.4f}ms, "
      "overhead={:.4f}ms",
      baselineMeanMs, telemetryMeanMs, overheadMs);

  CHECK(overheadMs <= 0.1);
}

} // namespace NoMoreDay::tests
