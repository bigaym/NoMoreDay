#pragma once

#include "TestCommon.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] CombatDefenseContract - Armor mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.cached_area_level = 100;
  defenderStats.armor = 560.0f;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 991001u;
  req.base_pool.Add(Tag::Physical, 100.0f);
  req.additional_tags = Tag::Hit;
  req.is_simulation = true;

  const auto result = DamagePipeline::Calculate(registry, req);
  CHECK(result.total_damage == doctest::Approx(50.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatDefenseContract - Resistance mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.resistances[static_cast<int>(DamageType::Fire)] = 0.40f;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 991002u;
  req.base_pool.Add(Tag::Fire, 100.0f);
  req.additional_tags = Tag::Hit;
  req.is_simulation = true;

  const auto result = DamagePipeline::Calculate(registry, req);
  CHECK(result.total_damage == doctest::Approx(60.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatDefenseContract - Barrier absorbs before HP") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  registry.emplace<CombatStats>(attacker).crit_chance = 0.0f;

  const auto defender = registry.create();
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  registry.emplace<BarrierComponent>(defender);
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.barrier = 50.0f;
  defenderStats.max_barrier = 50.0f;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 991003u;
  req.base_pool.Add(Tag::Physical, 80.0f);
  req.additional_tags = Tag::Hit;
  req.is_simulation = true;

  const auto result = DamagePipeline::Calculate(registry, req);
  CombatSystem::ApplyDamage(registry, defender, result.total_damage, attacker,
                            result.is_crit, false);

  const auto &hp = registry.get<HealthComponent>(defender);
  const auto &postStats = registry.get<CombatStats>(defender);
  CHECK(postStats.barrier == doctest::Approx(0.0f).epsilon(0.0001f));
  CHECK(hp.current == doctest::Approx(70.0f).epsilon(0.0001f));
}

} // namespace NoMoreDay
