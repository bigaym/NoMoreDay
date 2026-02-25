#pragma once

#include "TestCommon.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] DefenseMitigationChain - Dodge resolves first and zeros hit damage") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.accuracy = 1.0f;
  attackerStats.crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.dodge_chance = 1.0f;
  defenderStats.block_chance = 0.0f;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 990001u;
  req.base_pool.Add(Tag::Physical, 120.0f);
  req.additional_tags = Tag::Melee | Tag::Hit;

  const auto result = DamagePipeline::Calculate(registry, req);
  CHECK(result.was_dodged == true);
  CHECK(result.was_blocked == false);
  CHECK(result.total_damage == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] DefenseMitigationChain - Block reduces damage before mitigation chain settlement") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.accuracy = 1.0f;
  attackerStats.crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.dodge_chance = 0.0f;
  defenderStats.block_chance = 1.0f;
  defenderStats.effective_block_eff = 0.25f;

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 990002u;
  req.base_pool.Add(Tag::Fire, 80.0f);
  req.additional_tags = Tag::Hit;

  const auto result = DamagePipeline::Calculate(registry, req);
  CHECK(result.was_dodged == false);
  CHECK(result.was_blocked == true);
  CHECK(result.block_multiplier == doctest::Approx(0.75f));
  CHECK(result.total_damage == doctest::Approx(60.0f).epsilon(0.0001f));
}

TEST_CASE("[Unit] DefenseMitigationChain - Physical mitigation applies armor then global reduction") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.accuracy = 1.0f;
  attackerStats.crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.cached_area_level = 100;
  defenderStats.armor = 560.0f;          // 50% DR at level 100
  defenderStats.damage_reduction = 0.2f; // then 20% global DR

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 990003u;
  req.base_pool.Add(Tag::Physical, 100.0f);
  req.additional_tags = Tag::Hit;

  const auto result = DamagePipeline::Calculate(registry, req);
  CHECK(result.was_dodged == false);
  CHECK(result.was_blocked == false);
  CHECK(result.total_damage == doctest::Approx(40.0f).epsilon(0.0001f));
}

} // namespace NoMoreDay
