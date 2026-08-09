#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace NoMoreDay {
namespace {

struct ArchetypeScenario {
  entt::registry registry;
  entt::entity attacker = entt::null;
  entt::entity defender = entt::null;
};

float Percentile(std::vector<float> values, float ratio) {
  if (values.empty()) {
    return 0.0f;
  }
  std::sort(values.begin(), values.end());
  const float clamped = std::clamp(ratio, 0.0f, 1.0f);
  const float index = clamped * static_cast<float>(values.size() - 1);
  const size_t lo = static_cast<size_t>(std::floor(index));
  const size_t hi = static_cast<size_t>(std::ceil(index));
  if (lo == hi) {
    return values[lo];
  }
  const float t = index - static_cast<float>(lo);
  return values[lo] * (1.0f - t) + values[hi] * t;
}

ArchetypeScenario BuildScenario(std::initializer_list<uint32_t> nodes) {
  ArchetypeScenario scenario;
  auto &registry = scenario.registry;

  scenario.attacker = registry.create();
  auto &attacker_stats = registry.emplace<CombatStats>(scenario.attacker);
  attacker_stats.min_weapon_damage = 0.0f;
  attacker_stats.max_weapon_damage = 0.0f;
  attacker_stats.crit_chance = 0.0f;
  attacker_stats.crit_damage = 1.5f;
  attacker_stats.cached_area_level = 1;
  registry.emplace<Position>(scenario.attacker, 0.0f, 0.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(scenario.attacker);
  active.specialized_slots[0].skill_id = 2;
  for (const uint32_t node_id : nodes) {
    active.specialized_slots[0].allocated_points[node_id] = 1;
  }

  scenario.defender = registry.create();
  auto &defender_stats = registry.emplace<CombatStats>(scenario.defender);
  defender_stats.cached_area_level = 1;
  defender_stats.armor = 0.0f;
  defender_stats.damage_reduction = 0.0f;
  defender_stats.resistances.fill(0.0f);
  registry.emplace<Position>(scenario.defender, 6.0f, 0.0f);

  return scenario;
}

} // namespace

TEST_CASE("[Integration] CombatAntiMeta - Four archetypes stay within P50/P90 spread gates") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  std::array<ArchetypeScenario, 4> scenarios = {
      BuildScenario({213}),
      BuildScenario({214}),
      BuildScenario({251}),
      BuildScenario({214, 251}),
  };

  std::array<float, 4> p50 = {};
  std::array<float, 4> p90 = {};

  for (size_t idx = 0; idx < scenarios.size(); ++idx) {
    auto &scenario = scenarios[idx];
    std::vector<float> dps_samples;
    dps_samples.reserve(160);

    DamageRequest req;
    req.attacker = scenario.attacker;
    req.defender = scenario.defender;
    req.skill_id = 2;
    req.additional_tags = Tag::None;
    req.is_simulation = true;

    for (int sample = 0; sample < 160; ++sample) {
      req.base_pool.Clear();
      const float base_damage = 80.0f + static_cast<float>((sample % 23) * 4);
      req.base_pool.Add(Tag::Physical, base_damage);
      const float dps = DamagePipeline::Calculate(scenario.registry, req).total_damage;
      dps_samples.push_back(dps);
    }

    p50[idx] = Percentile(dps_samples, 0.50f);
    p90[idx] = Percentile(dps_samples, 0.90f);
  }

  const auto [p50_min_it, p50_max_it] = std::minmax_element(p50.begin(), p50.end());
  const auto [p90_min_it, p90_max_it] = std::minmax_element(p90.begin(), p90.end());
  const float p50_spread_pct =
      (*p50_max_it > 0.0f) ? ((*p50_max_it - *p50_min_it) / *p50_max_it) * 100.0f
                           : 0.0f;
  const float p90_spread_pct =
      (*p90_max_it > 0.0f) ? ((*p90_max_it - *p90_min_it) / *p90_max_it) * 100.0f
                           : 0.0f;

  CHECK(p50_spread_pct <= 15.0f);
  CHECK(p90_spread_pct <= 30.0f);
}

} // namespace NoMoreDay

