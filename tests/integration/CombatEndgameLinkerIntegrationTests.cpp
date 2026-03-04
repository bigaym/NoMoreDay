#include "TestCommon.hpp"

#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include <algorithm>

namespace NoMoreDay {
namespace {

void PrepareDeterministicStats(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
  stats.accuracy = 1.0f;
  stats.dodge_chance = 0.0f;
  stats.block_chance = 0.0f;
}

void RequireEndgameContracts() {
  auto &registry = systems::EndgameModifierRegistry::Get();
  registry.ResetForTests();
  REQUIRE(registry.EnsureLoaded());
  REQUIRE(registry.Find(systems::EndgameModifierIds::ExtraDamage) != nullptr);
  REQUIRE(registry.Find(systems::EndgameModifierIds::ResistanceRend) != nullptr);
  REQUIRE(registry.Find(systems::EndgameModifierIds::AilmentAmplification) !=
          nullptr);
  REQUIRE(registry.Find(systems::EndgameModifierIds::ArmorBreaker) != nullptr);
  REQUIRE(registry.Find(systems::EndgameModifierIds::EnduringWard) != nullptr);
}

float SimulateDamage(entt::registry &registry, entt::entity attacker,
                     entt::entity defender, Tag type, float amount,
                     uint32_t skillId = 991000u) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = skillId;
  request.base_pool.Add(type, amount);
  request.additional_tags = Tag::Hit;
  request.is_simulation = true;
  return DamagePipeline::Calculate(registry, request).total_damage;
}

} // namespace

TEST_CASE("[Integration] CombatEndgameLinker - extra damage modifier increases outgoing damage") {
  TestSetupScope scope;
  RequireEndgameContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(attacker));
  auto &attackerRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(attacker);
  attackerRuntime.outgoing_modifier_ids = {systems::EndgameModifierIds::ExtraDamage};

  const auto defender = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(defender));

  const float damage = SimulateDamage(registry, attacker, defender, Tag::Physical,
                                      100.0f, 991001u);
  CHECK(damage == doctest::Approx(125.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatEndgameLinker - resistance reduction modifier penetrates mitigation") {
  TestSetupScope scope;
  RequireEndgameContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(attacker));
  auto &attackerRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(attacker);
  attackerRuntime.outgoing_modifier_ids = {
      systems::EndgameModifierIds::ResistanceRend};

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareDeterministicStats(defenderStats);
  defenderStats.resistances[static_cast<int>(DamageType::Fire)] = 0.50f;

  const float damage =
      SimulateDamage(registry, attacker, defender, Tag::Fire, 100.0f, 991002u);
  CHECK(damage == doctest::Approx(75.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatEndgameLinker - armor breaker modifier shifts physical mitigation") {
  TestSetupScope scope;
  RequireEndgameContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(attacker));
  auto &attackerRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(attacker);
  attackerRuntime.outgoing_modifier_ids = {
      systems::EndgameModifierIds::ArmorBreaker};

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareDeterministicStats(defenderStats);
  defenderStats.cached_area_level = 100;
  defenderStats.armor = 560.0f;

  const float damage = SimulateDamage(registry, attacker, defender, Tag::Physical,
                                      100.0f, 991003u);
  CHECK(damage == doctest::Approx(100.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatEndgameLinker - defensive modifier reinforces resistance and DR") {
  TestSetupScope scope;
  RequireEndgameContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(attacker));

  const auto defender = registry.create();
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareDeterministicStats(defenderStats);
  defenderStats.resistances[static_cast<int>(DamageType::Fire)] = 0.20f;
  defenderStats.damage_reduction = 0.10f;
  auto &defenderRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(defender);
  defenderRuntime.incoming_modifier_ids = {
      systems::EndgameModifierIds::EnduringWard};

  const float damage =
      SimulateDamage(registry, attacker, defender, Tag::Fire, 100.0f, 991004u);
  CHECK(damage == doctest::Approx(52.0f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatEndgameLinker - ailment amplification modifier scales magnitude and duration") {
  TestSetupScope scope;
  RequireEndgameContracts();

  auto &ailmentRegistry = systems::AilmentRegistry::Get();
  ailmentRegistry.ResetForTests();
  REQUIRE(ailmentRegistry.EnsureLoaded());

  entt::registry registry;
  const auto attacker = registry.create();
  PrepareDeterministicStats(registry.emplace<CombatStats>(attacker));
  auto &attackerRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(attacker);
  attackerRuntime.outgoing_modifier_ids = {
      systems::EndgameModifierIds::AilmentAmplification};

  const auto target = registry.create();
  registry.emplace<ActiveEffectsComponent>(target);
  registry.emplace<HealthComponent>(target, 500.0f, 500.0f);
  registry.emplace<Position>(target, 0.0f, 0.0f);
  PrepareDeterministicStats(registry.emplace<CombatStats>(target));

  systems::AilmentApplyRequest request;
  request.ailment = AilmentType::Poison;
  request.source = attacker;
  request.magnitude = 10.0f;
  request.duration = 2.0f;
  request.stacks = 1;

  REQUIRE(systems::AilmentApplier::Apply(registry, target, request));
  const auto &effects = registry.get<ActiveEffectsComponent>(target);
  REQUIRE(effects.effects.size() == 1);
  CHECK(effects.effects.front().tick_damage ==
        doctest::Approx(13.0f).epsilon(0.0001f));
  CHECK(effects.effects.front().remaining ==
        doctest::Approx(2.4f).epsilon(0.0001f));
}

TEST_CASE("[Integration] CombatEndgameLinker - regression traceability maps source and target modifier IDs") {
  TestSetupScope scope;
  RequireEndgameContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  const auto defender = registry.create();

  auto &attackerRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(attacker);
  attackerRuntime.outgoing_modifier_ids = {
      systems::EndgameModifierIds::ExtraDamage,
      systems::EndgameModifierIds::ResistanceRend};
  auto &defenderRuntime =
      registry.emplace<EndgameModifierRuntimeComponent>(defender);
  defenderRuntime.incoming_modifier_ids = {
      systems::EndgameModifierIds::EnduringWard};

  const auto resolution =
      systems::EndgameModifierRegistry::Get().ResolveForEntities(registry, attacker,
                                                                 defender);
  REQUIRE(resolution.trace.source_count == 2);
  REQUIRE(resolution.trace.target_count == 1);
  CHECK(std::find(resolution.trace.source_ids.begin(),
                  resolution.trace.source_ids.begin() +
                      resolution.trace.source_count,
                  systems::EndgameModifierIds::ExtraDamage) !=
        resolution.trace.source_ids.begin() +
            resolution.trace.source_count);
  CHECK(std::find(resolution.trace.source_ids.begin(),
                  resolution.trace.source_ids.begin() +
                      resolution.trace.source_count,
                  systems::EndgameModifierIds::ResistanceRend) !=
        resolution.trace.source_ids.begin() +
            resolution.trace.source_count);
  CHECK(resolution.trace.target_ids[0] ==
        systems::EndgameModifierIds::EnduringWard);
}

} // namespace NoMoreDay
