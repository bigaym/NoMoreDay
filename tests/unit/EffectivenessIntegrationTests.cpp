#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillAddedEffBaseline = 980001u;
constexpr uint32_t kSkillAddedEffHalf = 980002u;
constexpr uint32_t kSkillAddedEffZero = 980003u;
constexpr uint32_t kSkillCombined = 980004u;

void RegisterSkillWithAddedEff(uint32_t skillId, float addedEff) {
  SkillData data{};
  data.id = skillId;
  data.name_key = "effectiveness_test_skill";
  data.desc_key = "effectiveness_test_skill_desc";
  data.mana_cost = 0.0f;
  data.cooldown = 0.0f;
  data.tags = Tag::Physical;
  data.base_damage = 0.0f;
  data.weapon_damage_mult = 0.0f;
  data.added_damage_effectiveness = addedEff;
  SkillRegistry::Get().RegisterSkill(data);
}

void PrepareEffectivenessStats(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

float RunDamage(entt::registry &registry, entt::entity attacker,
                entt::entity defender, uint32_t skillId, float basePhysical,
                float addedPhysical, float requestAddedEff = 1.0f,
                float requestTriggerEff = 1.0f,
                entt::entity sourceEntity = entt::null) {
  auto &attackerStats = registry.get<CombatStats>(attacker);
  attackerStats.flat_damage[(int)DamageType::Physical] = addedPhysical;

  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = skillId;
  request.base_pool.Add(Tag::Physical, basePhysical);
  request.added_effectiveness = requestAddedEff;
  request.trigger_effectiveness = requestTriggerEff;
  request.additional_tags = Tag::Hit;
  request.source_entity = sourceEntity;
  request.is_simulation = true;
  return DamagePipeline::Calculate(registry, request).total_damage;
}

} // namespace

TEST_CASE("[Unit] EffectivenessIntegration - AddedEff=1 keeps added damage") {
  TestSetupScope scope;
  entt::registry registry;

  RegisterSkillWithAddedEff(kSkillAddedEffBaseline, 1.0f);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareEffectivenessStats(attackerStats);

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareEffectivenessStats(defenderStats);

  const float damage = RunDamage(registry, attacker, defender,
                                 kSkillAddedEffBaseline, 20.0f, 40.0f);
  CHECK(damage == doctest::Approx(60.0f).epsilon(0.0001f));
}

TEST_CASE("[Unit] EffectivenessIntegration - AddedEff scales added damage") {
  TestSetupScope scope;
  entt::registry registry;

  RegisterSkillWithAddedEff(kSkillAddedEffHalf, 0.5f);
  RegisterSkillWithAddedEff(kSkillAddedEffZero, 0.0f);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareEffectivenessStats(attackerStats);

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareEffectivenessStats(defenderStats);

  const float halfDamage =
      RunDamage(registry, attacker, defender, kSkillAddedEffHalf, 20.0f, 40.0f);
  const float zeroDamage =
      RunDamage(registry, attacker, defender, kSkillAddedEffZero, 20.0f, 40.0f);

  CHECK(halfDamage == doctest::Approx(40.0f).epsilon(0.0001f));
  CHECK(zeroDamage == doctest::Approx(20.0f).epsilon(0.0001f));
}

TEST_CASE(
    "[Unit] EffectivenessIntegration - TriggerEff=1 leaves direct cast unchanged") {
  TestSetupScope scope;
  entt::registry registry;

  RegisterSkillWithAddedEff(kSkillAddedEffBaseline, 1.0f);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareEffectivenessStats(attackerStats);

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareEffectivenessStats(defenderStats);

  const float directDefault = RunDamage(
      registry, attacker, defender, kSkillAddedEffBaseline, 30.0f, 20.0f);
  const float directWithUnitTrigger = RunDamage(
      registry, attacker, defender, kSkillAddedEffBaseline, 30.0f, 20.0f, 1.0f,
      1.0f);

  CHECK(directWithUnitTrigger ==
        doctest::Approx(directDefault).epsilon(0.0001f));
}

TEST_CASE("[Unit] EffectivenessIntegration - TriggerEff=0.5 halves damage") {
  TestSetupScope scope;
  entt::registry registry;

  RegisterSkillWithAddedEff(kSkillAddedEffBaseline, 1.0f);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareEffectivenessStats(attackerStats);

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareEffectivenessStats(defenderStats);

  const float direct = RunDamage(registry, attacker, defender,
                                 kSkillAddedEffBaseline, 30.0f, 20.0f);
  const float triggered = RunDamage(registry, attacker, defender,
                                    kSkillAddedEffBaseline, 30.0f, 20.0f, 1.0f,
                                    0.5f);

  CHECK(triggered == doctest::Approx(direct * 0.5f).epsilon(0.0001f));
}

TEST_CASE(
    "[Unit] EffectivenessIntegration - Combined AddedEff and TriggerEff multiply correctly") {
  TestSetupScope scope;
  entt::registry registry;

  RegisterSkillWithAddedEff(kSkillCombined, 0.4f);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareEffectivenessStats(attackerStats);

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareEffectivenessStats(defenderStats);

  const float damage =
      RunDamage(registry, attacker, defender, kSkillCombined, 20.0f, 100.0f,
                1.0f, 0.7f);

  CHECK(damage == doctest::Approx(42.0f).epsilon(0.0001f));
}

} // namespace NoMoreDay
