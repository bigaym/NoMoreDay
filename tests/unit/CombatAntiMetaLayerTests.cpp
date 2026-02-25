#pragma once

#include "TestCommon.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatAntiMeta.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/StatsSystem.hpp"

namespace NoMoreDay {
namespace {

float RunSkill2DamageWithNodes(std::initializer_list<uint32_t> node_ids) {
  TestSetupScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  const auto attacker = registry.create();
  auto &attacker_stats = registry.emplace<CombatStats>(attacker);
  attacker_stats.min_weapon_damage = 0.0f;
  attacker_stats.max_weapon_damage = 0.0f;
  attacker_stats.crit_chance = 0.0f;
  attacker_stats.crit_damage = 1.5f;
  attacker_stats.cached_area_level = 1;
  registry.emplace<Position>(attacker, 0.0f, 0.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(attacker);
  active.specialized_slots[0].skill_id = 2;
  for (const uint32_t node_id : node_ids) {
    active.specialized_slots[0].allocated_points[node_id] = 1;
  }

  const auto defender = registry.create();
  auto &defender_stats = registry.emplace<CombatStats>(defender);
  defender_stats.cached_area_level = 1;
  defender_stats.armor = 0.0f;
  defender_stats.damage_reduction = 0.0f;
  defender_stats.resistances.fill(0.0f);
  registry.emplace<Position>(defender, 4.0f, 0.0f);

  DamageRequest req;
  req.attacker = attacker;
  req.defender = defender;
  req.skill_id = 2;
  req.base_pool.Add(Tag::Physical, 100.0f);
  req.additional_tags = Tag::None;
  req.is_simulation = true;
  return DamagePipeline::Calculate(registry, req).total_damage;
}

} // namespace

TEST_CASE("[Unit] CombatAntiMeta - Cost affix modifies stats and surfaces in runtime") {
  TestSetupScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  const auto player = registry.create();
  auto &stats = registry.emplace<CombatStats>(player);
  stats.crit_chance = 0.0f;
  stats.attack_speed = 1.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].allocated_points[971] = 1;

  auto &pf = registry.emplace<PhantomFlashComponent>(player);
  pf.counter_window = 0.4f;
  pf.triggered = false;

  const float crit_chance =
      StatsSystem::GetStatWithTags(registry, player, StatType::CritChance,
                                   Tag::Hit, 9, entt::null);
  const float attack_speed =
      StatsSystem::GetStatWithTags(registry, player, StatType::AttackSpeed,
                                   Tag::Hit, 9, entt::null);

  CHECK(crit_chance == doctest::Approx(50.0f).epsilon(0.0001f));
  CHECK(attack_speed == doctest::Approx(80.0f).epsilon(0.0001f));
}

TEST_CASE("[Unit] CombatAntiMeta - Diminishing returns clamps stacked same-source more") {
  const auto &cfg = CombatAntiMeta::GetDiminishingReturnsConfig();
  CHECK(cfg.enabled);
  const float single_effective = CombatAntiMeta::ApplyDiminishingReturns(0.22f);
  const float stacked_effective = CombatAntiMeta::ApplyDiminishingReturns(0.44f);
  CHECK(stacked_effective > single_effective);
  CHECK(stacked_effective < single_effective * 2.0f);

  const float baseline = RunSkill2DamageWithNodes({});
  const float with_single = RunSkill2DamageWithNodes({213});
  const float with_stacked = RunSkill2DamageWithNodes({213, 251});

  CHECK(with_single > baseline);
  CHECK(with_stacked > with_single);

  // Adding the second HeavyMomentum node should remain below linear stacking
  // relative to the single-node baseline (1.44 / 1.22 ~= 1.18).
  CHECK(with_stacked < with_single * (1.44f / 1.22f));
}

} // namespace NoMoreDay
