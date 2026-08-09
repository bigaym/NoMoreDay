#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/DamageMitigationService.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] DamageMitigationService - Physical uses armor and global reduction") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.cached_area_level = 100;
  defenderStats.armor = 560.0f;
  defenderStats.damage_reduction = 0.2f;

  systems::EndgameModifierAggregate endgame{};
  const float mitigated = DamageMitigationService::Apply(
      registry, attacker, defender, 0u, Tag::Hit | Tag::Physical,
      Tag::Physical, 100.0f,
      &defenderStats, endgame, false, false, 1.0f, entt::null);

  CHECK(mitigated == doctest::Approx(40.0f).epsilon(0.0001f));
}

TEST_CASE("[Unit] DamageMitigationService - Elemental applies resistance cap and DR") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.resistances[static_cast<int>(DamageType::Fire)] = 0.90f;
  defenderStats.damage_reduction = 0.25f;

  systems::EndgameModifierAggregate endgame{};
  const float mitigated = DamageMitigationService::Apply(
      registry, attacker, defender, 0u, Tag::Hit | Tag::Fire, Tag::Fire,
      100.0f,
      &defenderStats, endgame, false, false, 1.0f, entt::null);

  CHECK(mitigated == doctest::Approx(18.75f).epsilon(0.0001f));
}

TEST_CASE("[Unit] DamageMitigationService - Bloodthirst stacks increase damage taken") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.cached_area_level = 1;

  systems::EndgameModifierAggregate endgame{};

  SUBCASE("two Bloodthirst stacks apply 1.06x multiplier") {
    auto &resource = registry.emplace<BladeResourceComponent>(defender);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 2;
    resource.max = 10;

    const float mitigated = DamageMitigationService::Apply(
        registry, attacker, defender, 0u, Tag::Hit | Tag::Physical,
        Tag::Physical, 100.0f, &defenderStats, endgame, false, false, 1.0f,
        entt::null);

    CHECK(mitigated == doctest::Approx(106.0f).epsilon(0.0001f));
  }

  SUBCASE("no Bloodthirst stacks leaves damage unchanged") {
    auto &resource = registry.emplace<BladeResourceComponent>(defender);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 0;
    resource.max = 10;

    const float mitigated = DamageMitigationService::Apply(
        registry, attacker, defender, 0u, Tag::Hit | Tag::Physical,
        Tag::Physical, 100.0f, &defenderStats, endgame, false, false, 1.0f,
        entt::null);

    CHECK(mitigated == doctest::Approx(100.0f).epsilon(0.0001f));
  }
}

} // namespace NoMoreDay
