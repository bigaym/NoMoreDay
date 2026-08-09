#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/EffectSystem.hpp"

namespace NoMoreDay {
namespace {

BuffEffect MakeDoTEffect(const char *id, entt::entity source, Tag elementTag,
                         float tickDamage, float tickInterval) {
  BuffEffect effect;
  effect.id = id;
  effect.name = id;
  effect.type = BuffType::DamageOverTime;
  effect.duration = 5.0f;
  effect.remaining = 5.0f;
  effect.tick_interval = tickInterval;
  effect.tick_damage = tickDamage;
  effect.tick_timer = 0.0f;
  effect.tick_damage_tag = elementTag;
  effect.is_debuff = true;
  effect.source = source;
  return effect;
}

float SimulateDoTTickDamage(entt::registry &registry, entt::entity attacker,
                            entt::entity defender, Tag elementTag,
                            float tickDamage) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool.Add(elementTag, tickDamage);
  request.additional_tags = Tag::DamageOverTime;
  request.is_simulation = false;
  return DamagePipeline::Calculate(registry, request).total_damage;
}

void PrepareDeterministicStats(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

} // namespace

TEST_CASE("[Unit] DoTDamageClosure - tick reduces HP by calculated damage") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(attackerStats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 12.0f, 8.0f);
  registry.emplace<HealthComponent>(target, 300.0f, 300.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);

  auto &effects = registry.emplace<ActiveEffectsComponent>(target);
  effects.effects.push_back(
      MakeDoTEffect("dot_poison", attacker, Tag::Poison, 36.0f, 0.2f));

  const float expectedDamage =
      SimulateDoTTickDamage(registry, attacker, target, Tag::Poison, 36.0f);
  const float hpBefore = registry.get<HealthComponent>(target).current;

  systems::EffectSystem::update(registry, 0.2f);

  const float hpAfter = registry.get<HealthComponent>(target).current;
  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedDamage).epsilon(0.0001f));
}

TEST_CASE(
    "[Unit] DoTDamageClosure - DoT tick does not dispatch OnSkillHit events") {
  TestSetupScope scope;
  entt::registry registry;

  int onSkillHitCount = 0;
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [&onSkillHitCount](entt::registry &, const CombatEvent &) {
        ++onSkillHitCount;
      },
      1000);

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(attackerStats);
  attackerStats.max_mana = 120.0f;
  attackerStats.mana = 40.0f;
  attackerStats.mana_on_hit = 15.0f;

  const auto target = registry.create();
  registry.emplace<Position>(target, 5.0f, 3.0f);
  registry.emplace<HealthComponent>(target, 200.0f, 200.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);

  auto &effects = registry.emplace<ActiveEffectsComponent>(target);
  effects.effects.push_back(
      MakeDoTEffect("dot_fire", attacker, Tag::Fire, 24.0f, 0.15f));

  const float manaBefore = attackerStats.mana;

  systems::EffectSystem::update(registry, 0.15f);

  CHECK(onSkillHitCount == 0);
  CHECK(attackerStats.mana == doctest::Approx(manaBefore).epsilon(0.0001f));
}

TEST_CASE("[Unit] DoTDamageClosure - element tags drive DoT mitigation path") {
  struct ElementCase {
    const char *name;
    Tag tag;
    float tickDamage;
  };

  const ElementCase cases[] = {
      {"poison", Tag::Poison, 30.0f},
      {"fire", Tag::Fire, 30.0f},
      {"cold", Tag::Cold, 30.0f},
  };

  for (const auto &item : cases) {
    SUBCASE(item.name) {
      TestSetupScope scope;
      entt::registry registry;

      const auto attacker = registry.create();
      auto &attackerStats = registry.emplace<CombatStats>(attacker);
      PrepareDeterministicStats(attackerStats);

      const auto target = registry.create();
      registry.emplace<Position>(target, 1.0f, 1.0f);
      registry.emplace<HealthComponent>(target, 250.0f, 250.0f);
      auto &targetStats = registry.emplace<CombatStats>(target);
      PrepareDeterministicStats(targetStats);
      targetStats.resistances[(int)DamageType::Poison] = 0.10f;
      targetStats.resistances[(int)DamageType::Fire] = 0.55f;
      targetStats.resistances[(int)DamageType::Cold] = 0.35f;

      auto &effects = registry.emplace<ActiveEffectsComponent>(target);
      effects.effects.push_back(MakeDoTEffect("dot_element", attacker,
                                              item.tag, item.tickDamage, 0.2f));

      const float expectedDamage = SimulateDoTTickDamage(
          registry, attacker, target, item.tag, item.tickDamage);
      const float hpBefore = registry.get<HealthComponent>(target).current;

      systems::EffectSystem::update(registry, 0.2f);

      const float hpAfter = registry.get<HealthComponent>(target).current;
      CHECK((hpBefore - hpAfter) ==
            doctest::Approx(expectedDamage).epsilon(0.0001f));
    }
  }
}

TEST_CASE(
    "[Unit] DoTDamageClosure - concurrent DoTs tick independently on same target") {
  TestSetupScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(attackerStats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 9.0f, 4.0f);
  registry.emplace<HealthComponent>(target, 500.0f, 500.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);
  targetStats.resistances[(int)DamageType::Poison] = 0.10f;
  targetStats.resistances[(int)DamageType::Fire] = 0.25f;

  auto &effects = registry.emplace<ActiveEffectsComponent>(target);
  effects.effects.push_back(
      MakeDoTEffect("dot_poison_a", attacker, Tag::Poison, 20.0f, 0.25f));
  effects.effects.push_back(
      MakeDoTEffect("dot_fire_b", attacker, Tag::Fire, 18.0f, 0.25f));

  const float expectedDamageA =
      SimulateDoTTickDamage(registry, attacker, target, Tag::Poison, 20.0f);
  const float expectedDamageB =
      SimulateDoTTickDamage(registry, attacker, target, Tag::Fire, 18.0f);
  const float hpBefore = registry.get<HealthComponent>(target).current;

  systems::EffectSystem::update(registry, 0.25f);

  const float hpAfter = registry.get<HealthComponent>(target).current;
  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedDamageA + expectedDamageB).epsilon(0.0001f));
}

} // namespace NoMoreDay
