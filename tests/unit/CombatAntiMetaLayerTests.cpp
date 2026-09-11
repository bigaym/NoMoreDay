#pragma once

#include "TestCommon.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatAntiMeta.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/behaviors/PhantomTrance.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"

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

TEST_CASE("[Unit] CombatAntiMeta - 991 penetration is capped regardless of sword intent stacks") {
  TestSetupScope scope;
  entt::registry registry;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.mana = 200.0f;
  stats.max_mana = 200.0f;
  stats.cached_area_level = 1;
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 9;
  active.slots[0].current_charges = 5;
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].allocated_points[989] = 1; // 冰转质
  active.specialized_slots[0].allocated_points[991] = 9; // 每层剑意 9% 元素穿透
  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
  runtime.active_transmuter_node_by_skill[9] = 989;
  SkillSystem::RebakeSkillProfiles(registry, player);

  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 50.0f, 0.0f);
  registry.emplace<CombatStats>(enemy).cached_area_level = 1;
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);

  SkillExecution exec;
  exec.skill_id = 9;
  exec.owner = player;
  exec.target_pos = {50.0f, 0.0f};
  auto cast = SkillBehaviorRegistry::GetCast(9);
  REQUIRE(cast != nullptr);
  cast(registry, player, exec);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.transmuter_tag == Tag::Cold);
  CHECK(pt->params.enchant_pen_per_intent_pct == doctest::Approx(0.09f));
  CHECK(pt->params.enchant_pen_cap_pct == doctest::Approx(40.0f));

  // 10 层剑意 × 9% = 90%，远超 40% 上限
  registry.emplace<SwordIntentComponent>(player).stacks = 10;

  // 形态自然结束 → 开启附魔窗口
  (void)skills::PhantomTrance::Update(registry, player, *pt, 3.5f);
  // 附魔窗口内刷新一次穿透
  (void)skills::PhantomTrance::Update(registry, player, *pt, 0.1f);

  const auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(effects != nullptr);
  const BuffEffect *shred = effects->Get(BuffId::PhantomTranceEnchant);
  REQUIRE(shred != nullptr);
  REQUIRE(!shred->modifiers.empty());
  // 单节点堆叠不得突破 40% 穿透上限
  CHECK(shred->modifiers.front().value == doctest::Approx(-40.0f));
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
