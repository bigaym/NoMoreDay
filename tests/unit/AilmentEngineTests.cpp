#include "TestCommon.hpp"

#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {
namespace {

void PrepareDeterministicStats(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

std::vector<const BuffEffect *>
CollectAilmentEffects(const ActiveEffectsComponent &activeEffects,
                     AilmentType ailment) {
  std::vector<const BuffEffect *> output;
  for (const auto &effect : activeEffects.effects) {
    const auto mapped = systems::AilmentAdapter::TryMapLegacyBuff(effect);
    if (mapped && *mapped == ailment) {
      output.push_back(&effect);
    }
  }
  return output;
}

float SimulateTickDamage(entt::registry &registry, entt::entity attacker,
                         entt::entity defender, Tag tag, float amount) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool.Add(tag, amount);
  request.additional_tags = Tag::DamageOverTime;
  request.is_simulation = true;
  return DamagePipeline::Calculate(registry, request).total_damage;
}

void RequireDefaultContracts() {
  auto &registry = systems::AilmentRegistry::Get();
  registry.ResetForTests();
  CHECK(registry.EnsureLoaded());
  REQUIRE(registry.Find(AilmentType::Poison) != nullptr);
  REQUIRE(registry.Find(AilmentType::Ignite) != nullptr);
  REQUIRE(registry.Find(AilmentType::Bleed) != nullptr);
}

} // namespace

TEST_CASE("[Unit] AilmentEngine - stack limit enforcement") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  systems::AilmentApplyRequest request;
  request.ailment = AilmentType::Poison;
  request.source = entt::null;
  request.magnitude = 8.0f;
  request.duration = 2.0f;
  request.stacks = 1;

  for (int i = 0; i < 10; ++i) {
    CHECK(systems::AilmentApplier::Apply(registry, target, request));
  }

  const auto ailmentEffects = CollectAilmentEffects(effects, AilmentType::Poison);
  REQUIRE(ailmentEffects.size() == 1);
  CHECK(ailmentEffects.front()->stacks == 5);
}

TEST_CASE("[Unit] AilmentEngine - refresh extend independent policies") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  systems::AilmentApplyRequest poison;
  poison.ailment = AilmentType::Poison;
  poison.magnitude = 5.0f;
  poison.duration = 2.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));
  auto poisonEffects = CollectAilmentEffects(effects, AilmentType::Poison);
  REQUIRE(poisonEffects.size() == 1);
  CHECK(poisonEffects.front()->remaining == doctest::Approx(2.0f));

  poison.duration = 4.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));
  poisonEffects = CollectAilmentEffects(effects, AilmentType::Poison);
  REQUIRE(poisonEffects.size() == 1);
  CHECK(poisonEffects.front()->remaining == doctest::Approx(4.0f));

  systems::AilmentApplyRequest ignite;
  ignite.ailment = AilmentType::Ignite;
  ignite.magnitude = 6.0f;
  ignite.duration = 1.5f;
  CHECK(systems::AilmentApplier::Apply(registry, target, ignite));
  auto igniteEffects = CollectAilmentEffects(effects, AilmentType::Ignite);
  REQUIRE(igniteEffects.size() == 1);
  CHECK(igniteEffects.front()->remaining == doctest::Approx(1.5f));

  ignite.duration = 2.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, ignite));
  igniteEffects = CollectAilmentEffects(effects, AilmentType::Ignite);
  REQUIRE(igniteEffects.size() == 1);
  CHECK(igniteEffects.front()->remaining == doctest::Approx(3.5f));

  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.magnitude = 3.0f;
  bleed.duration = 2.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));
  const auto bleedEffects = CollectAilmentEffects(effects, AilmentType::Bleed);
  CHECK(bleedEffects.size() == 2);
}

TEST_CASE("[Unit] AilmentEngine - overwrite strongest newest additive") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  systems::AilmentApplyRequest poison;
  poison.ailment = AilmentType::Poison;
  poison.duration = 2.0f;
  poison.magnitude = 11.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));
  poison.magnitude = 7.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));
  auto poisonEffects = CollectAilmentEffects(effects, AilmentType::Poison);
  REQUIRE(poisonEffects.size() == 1);
  CHECK(poisonEffects.front()->tick_damage == doctest::Approx(11.0f));

  systems::AilmentApplyRequest ignite;
  ignite.ailment = AilmentType::Ignite;
  ignite.duration = 2.0f;
  ignite.magnitude = 9.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, ignite));
  ignite.magnitude = 4.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, ignite));
  auto igniteEffects = CollectAilmentEffects(effects, AilmentType::Ignite);
  REQUIRE(igniteEffects.size() == 1);
  CHECK(igniteEffects.front()->tick_damage == doctest::Approx(4.0f));

  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.duration = 2.0f;
  bleed.magnitude = 4.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));
  bleed.magnitude = 5.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));
  bleed.magnitude = 2.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));

  const auto bleedEffects = CollectAilmentEffects(effects, AilmentType::Bleed);
  REQUIRE(bleedEffects.size() == 2);
  const float totalTickDamage =
      bleedEffects[0]->tick_damage + bleedEffects[1]->tick_damage;
  CHECK(totalTickDamage == doctest::Approx(11.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] AilmentEngine - multi-ailment ticks on single target") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  auto &attackerStats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(attackerStats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 6.0f, 4.0f);
  registry.emplace<HealthComponent>(target, 400.0f, 400.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);

  targetStats.resistances[(int)DamageType::Poison] = 0.10f;
  targetStats.resistances[(int)DamageType::Fire] = 0.20f;

  systems::AilmentApplyRequest poison;
  poison.ailment = AilmentType::Poison;
  poison.source = attacker;
  poison.magnitude = 12.0f;
  poison.duration = 3.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));

  systems::AilmentApplyRequest ignite;
  ignite.ailment = AilmentType::Ignite;
  ignite.source = attacker;
  ignite.magnitude = 10.0f;
  ignite.duration = 3.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, ignite));

  const float expectedPoison =
      SimulateTickDamage(registry, attacker, target, Tag::Poison, 12.0f);
  const float expectedIgnite =
      SimulateTickDamage(registry, attacker, target, Tag::Fire, 10.0f);

  const float hpBefore = registry.get<HealthComponent>(target).current;
  systems::AilmentTickDriver::Tick(registry, 0.5f);
  const float hpAfter = registry.get<HealthComponent>(target).current;

  CHECK((hpBefore - hpAfter) ==
        doctest::Approx(expectedPoison + expectedIgnite).epsilon(0.0001f));
}

} // namespace NoMoreDay
