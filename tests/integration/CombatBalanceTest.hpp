#pragma once

#include "TestCommon.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] CombatBalance - Integrated Flow") {
  TestSetupScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  auto &aStats = registry.emplace<CombatStats>(attacker);
  aStats.min_weapon_damage = 100.0f;
  aStats.max_weapon_damage = 100.0f;
  aStats.damage_multipliers[0] = 1.0f; // Physical

  auto defender = registry.create();
  registry.emplace<Position>(defender, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);
  auto &dStats = registry.emplace<CombatStats>(defender);
  registry.emplace<PlayerStats>(defender).level =
      100; // Cached level will be 100
  dStats.cached_area_level = 100;

  DamagePool pool;
  pool.Add(Tag::Physical, 0.0f); // Uses weapon damage

  // Fix: Explicitly register Basic Attack (ID 0) for the test
  // Fix: Explicitly register Basic Attack (ID 0) for the test

  SkillData basicAttack;
  basicAttack.id = 0;
  basicAttack.name_key = "Basic Attack";
  basicAttack.base_damage = 0.0f;
  basicAttack.weapon_damage_mult = 1.0f;
  basicAttack.tags = Tag::Physical | Tag::Melee;
  SkillRegistry::Get().RegisterSkill(basicAttack);

  SUBCASE("Base Damage (0 Armor)") {
    dStats.armor = 0.0f;
    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null);
    CHECK(result.total_damage == doctest::Approx(100.0f));
  }

  SUBCASE("Armor Mitigation (Level 100)") {
    // Level 100 LF = 560. 560 Armor should give 50% DR.
    dStats.armor = 560.0f;
    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null);
    CHECK(result.total_damage == doctest::Approx(50.0f));
  }

  SUBCASE("Negative Armor (Level 100)") {
    // -560 Armor should give 1.5x damage
    dStats.armor = -560.0f;
    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null);
    CHECK(result.total_damage == doctest::Approx(150.0f));
  }
}

} // namespace NoMoreDay
