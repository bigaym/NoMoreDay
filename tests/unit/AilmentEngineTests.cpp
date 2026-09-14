#include "TestCommon.hpp"

#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
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
  request.is_simulation = false;
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

TEST_CASE("[Unit] AilmentEngine - apply request propagates source_skill_id") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  // 显式来源技能应写入 BuffEffect，供 153 饮血刃门控读取。
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.magnitude = 6.0f;
  bleed.duration = 3.0f;
  bleed.source_skill_id = 1;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));

  const auto bleedEffects = CollectAilmentEffects(effects, AilmentType::Bleed);
  REQUIRE(bleedEffects.size() == 1);
  CHECK(bleedEffects.front()->source_skill_id == 1);

  // 默认 0：无归属来源不获得技能归属。
  systems::AilmentApplyRequest poison;
  poison.ailment = AilmentType::Poison;
  poison.magnitude = 6.0f;
  poison.duration = 3.0f;
  CHECK(systems::AilmentApplier::Apply(registry, target, poison));

  const auto poisonEffects = CollectAilmentEffects(effects, AilmentType::Poison);
  REQUIRE(poisonEffects.size() == 1);
  CHECK(poisonEffects.front()->source_skill_id == 0);
}

TEST_CASE("[Unit] AilmentEngine - additive merge updates source_skill_id") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto target = registry.create();
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  // 流血契约 max_stacks=2 / Independent / Additive：两个独立槽分别是技能1
  // 来源与无归属来源，随后第三个来源触发 Additive 合并。
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.magnitude = 10.0f;
  bleed.duration = 2.0f;
  bleed.source_skill_id = 1;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));

  bleed.duration = 3.0f;
  bleed.source_skill_id = 0;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));

  // Additive 合并选中剩余时间更长的第二个槽，来源归属必须一并改写。
  bleed.duration = 1.0f;
  bleed.source_skill_id = 999;
  CHECK(systems::AilmentApplier::Apply(registry, target, bleed));

  const auto bleedEffects = CollectAilmentEffects(effects, AilmentType::Bleed);
  REQUIRE(bleedEffects.size() == 2);
  // 合并前旧实现只同步 source，遗留 0 归属；修复后应为最新来源 999。
  CHECK(bleedEffects[0]->source_skill_id == 1);
  CHECK(bleedEffects[1]->source_skill_id == 999);
}

TEST_CASE("[Unit] AilmentEngine - B2-18 Slow contract registered as non-damaging "
          "identity") {
  TestSetupScope scope;
  auto &contracts = systems::AilmentRegistry::Get();
  contracts.ResetForTests();
  REQUIRE(contracts.EnsureLoaded());

  // 新增 Slow 不得挤掉既有 6 条契约。
  CHECK(contracts.Find(AilmentType::Poison) != nullptr);
  CHECK(contracts.Find(AilmentType::Ignite) != nullptr);
  CHECK(contracts.Find(AilmentType::Bleed) != nullptr);
  CHECK(contracts.Find(AilmentType::Chill) != nullptr);
  CHECK(contracts.Find(AilmentType::Freeze) != nullptr);
  CHECK(contracts.Find(AilmentType::Shock) != nullptr);

  const auto *slow = contracts.Find(AilmentType::Slow);
  REQUIRE(slow != nullptr);
  CHECK(slow->max_stacks == 1);
  CHECK(slow->refresh_policy == systems::RefreshPolicy::Refresh);
  CHECK(slow->overwrite_policy == systems::OverwritePolicy::Strongest);
  CHECK(slow->immunity_and_resistance == doctest::Approx(1.0f));
  CHECK(slow->tick_interval == doctest::Approx(1.0f));
  CHECK(slow->damage_pool_policy == systems::DamagePoolPolicy::PerStack);
  CHECK(slow->base_duration == doctest::Approx(2.5f));
  // D5：Slow 为纯移速减益身份，不产生 tick 伤害。
  CHECK(slow->damage_tag == Tag::None);
  CHECK(slow->legacy_buff_type == BuffType::SpeedDown);

  // 「无伤害」必须能被字符串表达，否则 JSON 的 "None" 会静默回退到默认元素。
  const auto parsedNone = TagFromString("None");
  REQUIRE(parsedNone.has_value());
  CHECK(*parsedNone == Tag::None);
}

TEST_CASE("[Unit] AilmentEngine - B2-18 Slow contract never ticks damage") {
  TestSetupScope scope;
  RequireDefaultContracts();
  REQUIRE(systems::AilmentRegistry::Get().Find(AilmentType::Slow) != nullptr);

  entt::registry registry;
  const auto target = registry.create();
  registry.emplace<ActiveEffectsComponent>(target);
  registry.emplace<Position>(target, 6.0f, 4.0f);
  registry.emplace<HealthComponent>(target, 400.0f, 400.0f);

  systems::AilmentApplyRequest slow;
  slow.ailment = AilmentType::Slow;
  // 即便误把减速幅度当伤害传入，契约也必须保证不结算 tick 伤害。
  slow.magnitude = 30.0f;
  slow.duration = 2.5f;
  CHECK(systems::AilmentApplier::Apply(registry, target, slow));

  const float hpBefore = registry.get<HealthComponent>(target).current;
  for (int i = 0; i < 120; ++i) {
    systems::AilmentTickDriver::Tick(registry, 0.05f);
  }
  CHECK(registry.get<HealthComponent>(target).current ==
        doctest::Approx(hpBefore));
}

TEST_CASE("[Unit] AilmentEngine - B2-18 move-speed debuff builder is single-source") {
  TestSetupScope scope;

  // 流云刺 172/175 的 FrostSlow 载体（identity=Slow，0.30 比例 → -30%）。
  const auto frostSlow = systems::AilmentAdapter::BuildMoveSpeedDebuff(
      AilmentType::Slow, "FrostSlow", "Frost Slow", "", BuffKind::Slow, 0.30f,
      2.5f);
  CHECK(frostSlow.id == "FrostSlow");
  CHECK(frostSlow.name == "Frost Slow");
  CHECK(frostSlow.type == BuffType::SpeedDown);
  CHECK(frostSlow.kind == BuffKind::Slow);
  CHECK(frostSlow.is_debuff);
  CHECK(frostSlow.duration == doctest::Approx(2.5f));
  CHECK(frostSlow.remaining == doctest::Approx(2.5f));
  REQUIRE(frostSlow.modifiers.size() == 1);
  CHECK(frostSlow.modifiers[0].type == StatType::MoveSpeed);
  CHECK(frostSlow.modifiers[0].mode == ModifierMode::PercentAdd);
  CHECK(frostSlow.modifiers[0].value == doctest::Approx(-30.0f));

  // HazardSystem 冰冻球的 frozen_chill 载体（identity=Chill，0.5 比例 → -50%）。
  const auto frozenChill = systems::AilmentAdapter::BuildMoveSpeedDebuff(
      AilmentType::Chill, "frozen_chill", "冰冻减速", "被冰霜减速", BuffKind::Chill,
      0.5f, 3.0f);
  CHECK(frozenChill.id == "frozen_chill");
  CHECK(frozenChill.name == "冰冻减速");
  CHECK(frozenChill.description == "被冰霜减速");
  CHECK(frozenChill.type == BuffType::SpeedDown);
  CHECK(frozenChill.kind == BuffKind::Chill);
  CHECK(frozenChill.duration == doctest::Approx(3.0f));
  REQUIRE(frozenChill.modifiers.size() == 1);
  CHECK(frozenChill.modifiers[0].type == StatType::MoveSpeed);
  CHECK(frozenChill.modifiers[0].mode == ModifierMode::PercentAdd);
  CHECK(frozenChill.modifiers[0].value == doctest::Approx(-50.0f));

  // 纯移速减益：两个创建点都不携带 tick 伤害。
  CHECK(frostSlow.tick_damage == 0.0f);
  CHECK(frozenChill.tick_damage == 0.0f);
}

} // namespace NoMoreDay
