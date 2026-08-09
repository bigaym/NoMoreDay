#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include "game/contracts/CombatFormula.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include <algorithm>

namespace NoMoreDay {
namespace {

float ComputeLegacyDamageReference(const CombatStats &attacker,
                                   const CombatStats &defender,
                                   const float baseDamage,
                                   const DamageType type) {
  float multiplier = attacker.damage_multipliers[(int)type];
  float effectiveMult = (multiplier > 0.001f) ? multiplier : 1.0f;
  float damage = baseDamage * effectiveMult;

  float mitigation = 0.0f;
  if (type == DamageType::Physical) {
    float effectiveArmor = defender.armor - attacker.armor_pen;
    int areaLevel = defender.cached_area_level;
    if (areaLevel < 1) {
      areaLevel = 1;
    }
    float armorMult =
        CombatFormula::CalculateArmorMultiplier(effectiveArmor, areaLevel);
    mitigation = 1.0f - armorMult;
  } else {
    using namespace NoMoreDay::Constants::Combat;
    mitigation =
        std::min(defender.resistances[(int)type], Cap::RESISTANCE);
  }
  damage *= (1.0f - mitigation);

  using namespace NoMoreDay::Constants::Combat;
  float reduction = std::min(defender.damage_reduction, Cap::DR);
  float effectiveDr = reduction > 0.0f ? reduction : 0.0f;
  damage *= (1.0f - effectiveDr);
  return std::max(0.0f, damage);
}

} // namespace

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Legacy formula equivalence") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.damage_multipliers[(int)DamageType::Physical] = 1.0f;
  attackerStats.damage_multipliers[(int)DamageType::Fire] = 1.0f;
  attackerStats.damage_multipliers[(int)DamageType::Lightning] = 1.0f;
  attackerStats.armor_pen = 0.0f;
  attackerStats.crit_chance = 0.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.armor = 260.0f;
  defenderStats.resistances[(int)DamageType::Fire] = 0.35f;
  defenderStats.resistances[(int)DamageType::Lightning] = 0.2f;
  defenderStats.damage_reduction = 0.1f;
  defenderStats.cached_area_level = 45;

  auto run_equivalence = [&](DamageType type, Tag typeTag, float baseDamage) {
    DamageRequest req;
    req.attacker = attacker;
    req.defender = defender;
    req.skill_id = 987654321u;
    req.base_pool.Add(typeTag, baseDamage);
    req.is_simulation = true;
    req.additional_tags = Tag::None;

    const auto pipelineResult = DamagePipeline::Calculate(registry, req);
    const float legacyDamage = ComputeLegacyDamageReference(
        attackerStats, defenderStats, baseDamage, type);
    CHECK(pipelineResult.total_damage ==
          doctest::Approx(legacyDamage).epsilon(0.0001f));
  };

  run_equivalence(DamageType::Physical, Tag::Physical, 120.0f);
  run_equivalence(DamageType::Fire, Tag::Fire, 95.0f);
  run_equivalence(DamageType::Lightning, Tag::Lightning, 88.0f);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Thorns damage bypasses crit and mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  attackerStats.crit_chance = 100.0f;
  attackerStats.crit_damage = 3.0f;
  attackerStats.damage_multipliers[(int)DamageType::Physical] = 2.0f;

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  defenderStats.armor = 800.0f;
  defenderStats.damage_reduction = 0.6f;
  defenderStats.cached_area_level = 70;

  DamageRequest normalReq;
  normalReq.attacker = attacker;
  normalReq.defender = defender;
  normalReq.skill_id = 987654322u;
  normalReq.base_pool.Add(Tag::Physical, 37.0f);
  normalReq.additional_tags = Tag::Hit;
  normalReq.is_simulation = true;

  DamageRequest thornsReq = normalReq;
  thornsReq.skip_mitigation = true;
  thornsReq.thorns_like_damage = true;

  const auto normalResult = DamagePipeline::Calculate(registry, normalReq);
  const auto thornsResult = DamagePipeline::Calculate(registry, thornsReq);

  CHECK(thornsResult.total_damage == doctest::Approx(37.0f).epsilon(0.0001f));
  CHECK(thornsResult.is_crit == false);
  CHECK(thornsResult.total_damage > normalResult.total_damage);
}

TEST_CASE("[Unit] DamagePipelineUnifiedEntry - Self damage supports skip_mitigation") {
  TestSetupScope scope;
  entt::registry registry;

  const auto self = registry.create();
  auto &stats = registry.emplace<CombatStats>(self);
  stats.armor = 500.0f;
  stats.damage_reduction = 0.4f;
  stats.cached_area_level = 60;
  stats.crit_chance = 0.0f;
  stats.damage_multipliers[(int)DamageType::Physical] = 1.0f;

  DamageRequest mitigatedReq;
  mitigatedReq.attacker = self;
  mitigatedReq.defender = self;
  mitigatedReq.skill_id = 987654323u;
  mitigatedReq.base_pool.Add(Tag::Physical, 50.0f);
  mitigatedReq.additional_tags = Tag::Hit;
  mitigatedReq.is_simulation = true;

  DamageRequest bypassReq = mitigatedReq;
  bypassReq.skip_mitigation = true;

  const auto mitigated = DamagePipeline::Calculate(registry, mitigatedReq);
  const auto bypassed = DamagePipeline::Calculate(registry, bypassReq);

  CHECK(bypassed.total_damage == doctest::Approx(50.0f).epsilon(0.0001f));
  CHECK(bypassed.total_damage > mitigated.total_damage);
}

} // namespace NoMoreDay
