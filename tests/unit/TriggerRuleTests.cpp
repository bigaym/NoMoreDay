#include "TestCommon.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/systems/skill/ProcEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <type_traits>

namespace NoMoreDay {

TEST_CASE("[Unit] Skill TriggerRule - Component POD and Standard Layout") {
  static_assert(std::is_standard_layout_v<TriggerRule>);
  static_assert(std::is_trivially_destructible_v<TriggerRule>);
  static_assert(std::is_standard_layout_v<TriggerRuleComponent>);
  static_assert(std::is_trivially_destructible_v<TriggerRuleComponent>);

  TriggerRuleComponent comp;
  CHECK(comp.rule_count == 0);
  CHECK(comp.rules.size() == TriggerRuleComponent::kMaxRules);

  TriggerRule rule;
  rule.rule_id = 1;
  rule.cast_skill_id = 1;
  rule.base_chance = 1.0f;
  CHECK(comp.AddRule(rule));
  CHECK(comp.rule_count == 1);
  CHECK(comp.rules[0].rule_id == 1);
}

TEST_CASE("[Unit] Skill TriggerRule - Event Listening and Target Resolution") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 200.0f);
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 300.0f, 400.0f);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule hitRule;
  hitRule.rule_id = 101;
  hitRule.listen_event = CombatEventType::OnSkillHit;
  hitRule.cast_skill_id = 1;
  hitRule.base_chance = 1.0f;
  hitRule.use_proc_scaling = false;
  hitRule.target_mode = TriggerTargetPolicy::Victim;
  CHECK(triggers.AddRule(hitRule));

  // Dispatch an OnSkillHit event from player to enemy
  CombatEvent hitEvent;
  hitEvent.type = CombatEventType::OnSkillHit;
  hitEvent.source = player;
  hitEvent.target = enemy;
  hitEvent.skill_id = 1;
  hitEvent.trigger_depth = 0;

  ProcEngine::DispatchEvent(registry, player, hitEvent);

  // Check that SkillExecution was generated targeting enemy position
  auto execView = registry.view<SkillExecution>();
  int count = 0;
  for (auto e : execView) {
    const auto &exec = execView.get<SkillExecution>(e);
    if (exec.owner == player && exec.skill_id == 1) {
      count++;
      CHECK(exec.trigger_depth == 1);
      CHECK(exec.target_pos.x == doctest::Approx(300.0f));
      CHECK(exec.target_pos.y == doctest::Approx(400.0f));
    }
  }
  CHECK(count == 1);
}

TEST_CASE("[Unit] Skill TriggerRule - Cooldown and ICD Recovery") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 10.0f, 10.0f);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule icdRule;
  icdRule.rule_id = 102;
  icdRule.listen_event = CombatEventType::OnTakeDamage;
  icdRule.cast_skill_id = 1;
  icdRule.base_chance = 1.0f;
  icdRule.use_proc_scaling = false;
  icdRule.internal_cooldown = 2.0f;
  icdRule.current_cooldown = 0.0f;
  icdRule.target_mode = TriggerTargetPolicy::Attacker;
  CHECK(triggers.AddRule(icdRule));

  CombatEvent takeEvent;
  takeEvent.type = CombatEventType::OnTakeDamage;
  takeEvent.source = player; // victim
  takeEvent.target = enemy;  // attacker
  takeEvent.trigger_depth = 0;

  // First trigger: should succeed and put rule on 2.0s cooldown
  ProcEngine::DispatchEvent(registry, player, takeEvent);
  CHECK(triggers.rules[0].current_cooldown == doctest::Approx(2.0f));

  // Count executions
  auto countExecs = [&]() {
    int c = 0;
    for (auto e : registry.view<SkillExecution>()) {
      (void)e;
      c++;
    }
    return c;
  };
  CHECK(countExecs() == 1);

  // Second trigger immediately: should be blocked by ICD
  ProcEngine::DispatchEvent(registry, player, takeEvent);
  CHECK(countExecs() == 1);

  // Advance 1.0s: cooldown remaining = 1.0s, still blocked
  ProcEngine::UpdateCooldowns(registry, 1.0f);
  CHECK(triggers.rules[0].current_cooldown == doctest::Approx(1.0f));
  ProcEngine::DispatchEvent(registry, player, takeEvent);
  CHECK(countExecs() == 1);

  // Advance another 1.0s: cooldown reaches 0.0s, can trigger again
  ProcEngine::UpdateCooldowns(registry, 1.0f);
  CHECK(triggers.rules[0].current_cooldown == doctest::Approx(0.0f));
  ProcEngine::DispatchEvent(registry, player, takeEvent);
  CHECK(countExecs() == 2);
}

TEST_CASE("[Unit] Skill TriggerRule - Recursion Depth Guard (Depth <= 2)") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule rule;
  rule.rule_id = 103;
  rule.listen_event = CombatEventType::OnSkillHit;
  rule.cast_skill_id = 1;
  rule.base_chance = 1.0f;
  rule.use_proc_scaling = false;
  CHECK(triggers.AddRule(rule));

  // Depth 0 -> dispatches with depth 1
  CombatEvent evt0;
  evt0.type = CombatEventType::OnSkillHit;
  evt0.source = player;
  evt0.target = player;
  evt0.trigger_depth = 0;
  ProcEngine::DispatchEvent(registry, player, evt0);

  auto view = registry.view<SkillExecution>();
  int d1_count = 0;
  for (auto e : view) {
    if (view.get<SkillExecution>(e).trigger_depth == 1) d1_count++;
  }
  CHECK(d1_count == 1);

  // Depth 1 -> dispatches with depth 2
  CombatEvent evt1;
  evt1.type = CombatEventType::OnSkillHit;
  evt1.source = player;
  evt1.target = player;
  evt1.trigger_depth = 1;
  ProcEngine::DispatchEvent(registry, player, evt1);

  int d2_count = 0;
  for (auto e : view) {
    if (view.get<SkillExecution>(e).trigger_depth == 2) d2_count++;
  }
  CHECK(d2_count == 1);

  // Depth 2 -> Recursion limit reached, NO new execution should be spawned!
  const size_t total_before = registry.storage<SkillExecution>().size();
  CombatEvent evt2;
  evt2.type = CombatEventType::OnSkillHit;
  evt2.source = player;
  evt2.target = player;
  evt2.trigger_depth = 2;
  ProcEngine::DispatchEvent(registry, player, evt2);
  const size_t total_after = registry.storage<SkillExecution>().size();

  CHECK(total_before == total_after);
}

TEST_CASE("[Unit] Skill TriggerRule - Adaptive Cooldown Scaling") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule rule;
  rule.rule_id = 104;
  rule.listen_event = CombatEventType::OnSkillHit;
  rule.cast_skill_id = 1;
  rule.base_chance = 0.5f; // 50% base
  rule.use_proc_scaling = true;
  CHECK(triggers.AddRule(rule));

  // When parent_skill_cd = 5.0f, real_chance = 0.5 * (1 + 5.0 * 0.2) = 0.5 * 2.0 = 1.0 (100%)
  CombatEvent evt;
  evt.type = CombatEventType::OnSkillHit;
  evt.source = player;
  evt.target = player;
  evt.trigger_depth = 0;
  evt.parent_skill_cd = 5.0f;

  ProcEngine::DispatchEvent(registry, player, evt);

  CHECK(registry.storage<SkillExecution>().size() == 1);
}

TEST_CASE("[Unit] Skill TriggerRule - Two-Phase Safe Dispatch Pointer Resilience") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  for (uint32_t i = 1; i <= 4; ++i) {
    TriggerRule r;
    r.rule_id = i;
    r.listen_event = CombatEventType::OnSkillHit;
    r.cast_skill_id = 1;
    r.base_chance = 1.0f;
    r.use_proc_scaling = false;
    triggers.AddRule(r);
  }

  // Pre-populate registry with hundreds of entities to force reallocation during TriggerCast
  for (int i = 0; i < 200; ++i) {
    auto dummy = registry.create();
    registry.emplace<Position>(dummy, (float)i, (float)i);
  }

  CombatEvent evt;
  evt.type = CombatEventType::OnSkillHit;
  evt.source = player;
  evt.target = player;
  evt.trigger_depth = 0;

  // This should not crash, violate memory, or cause UAF
  CHECK_NOTHROW(ProcEngine::DispatchEvent(registry, player, evt));

  // Should have triggered up to 4 actions
  size_t execCount = 0;
  for (auto e : registry.view<SkillExecution>()) {
    if (registry.get<SkillExecution>(e).owner == player) {
      execCount++;
    }
  }
  CHECK(execCount == 4);
}

TEST_CASE("[Unit] Skill TriggerRule - Defensive Events (OnBlock, OnDodge) Attacker Targeting") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 100.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 500.0f, 600.0f);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule blockCounterRule;
  blockCounterRule.rule_id = 201;
  blockCounterRule.listen_event = CombatEventType::OnBlock;
  blockCounterRule.cast_skill_id = 1;
  blockCounterRule.base_chance = 1.0f;
  blockCounterRule.use_proc_scaling = false;
  blockCounterRule.target_mode = TriggerTargetPolicy::Attacker;
  CHECK(triggers.AddRule(blockCounterRule));

  // OnBlock event: source = blocker (player), target = attacker (enemy)
  CombatEvent blockEvent = CombatEventFactory::CreateOnBlock(player, enemy, 50.0f, 0);
  ProcEngine::DispatchEvent(registry, player, blockEvent);

  auto execView = registry.view<SkillExecution>();
  int count = 0;
  for (auto e : execView) {
    const auto &exec = execView.get<SkillExecution>(e);
    if (exec.owner == player && exec.skill_id == 1) {
      count++;
      // Must target enemy position (500, 600), NOT player position (100, 100)!
      CHECK(exec.target_pos.x == doctest::Approx(500.0f));
      CHECK(exec.target_pos.y == doctest::Approx(600.0f));
    }
  }
  CHECK(count == 1);
}

TEST_CASE("[Unit] Skill TriggerRule - CombatEventDispatcher Integration and Runtime Hooking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  // Initialize hooks to wire ProcEngine to CombatEventDispatcher
  SkillSystem::InitHooks();

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 50.0f, 50.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  auto enemy = registry.create();
  registry.emplace<Position>(enemy, 250.0f, 250.0f);

  auto &triggers = registry.emplace<TriggerRuleComponent>(player);
  TriggerRule hitRule;
  hitRule.rule_id = 301;
  hitRule.listen_event = CombatEventType::OnSkillHit;
  hitRule.cast_skill_id = 1;
  hitRule.base_chance = 1.0f;
  hitRule.use_proc_scaling = false;
  hitRule.target_mode = TriggerTargetPolicy::Victim;
  triggers.AddRule(hitRule);

  TriggerRule critRule;
  critRule.rule_id = 302;
  critRule.listen_event = CombatEventType::OnCrit;
  critRule.cast_skill_id = 1;
  critRule.base_chance = 1.0f;
  critRule.use_proc_scaling = false;
  critRule.target_mode = TriggerTargetPolicy::Victim;
  triggers.AddRule(critRule);

  // Dispatch OnSkillHit through dispatcher
  CombatEvent hitEvt = CombatEventFactory::CreateSkillHit(player, enemy, 1, Tag::Hit);
  CombatEventDispatcher::Dispatch(registry, hitEvt);

  size_t execsHit = 0;
  for (auto e : registry.view<SkillExecution>()) {
    if (registry.get<SkillExecution>(e).owner == player) execsHit++;
  }
  CHECK(execsHit == 1);

  // Dispatch OnCrit through dispatcher
  CombatEvent critEvt = CombatEventFactory::CreateOnCrit(player, enemy, 1, Tag::Hit, 100.0f);
  CombatEventDispatcher::Dispatch(registry, critEvt);

  size_t execsCrit = 0;
  for (auto e : registry.view<SkillExecution>()) {
    if (registry.get<SkillExecution>(e).owner == player) execsCrit++;
  }
  CHECK(execsCrit == 2);

  SkillSystem::ShutdownHooks();
}

TEST_CASE("[Unit] Skill TriggerRule - Zero Entity Leak on Completed Trigger Cast") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<ActiveSkillsComponent>(player);

  const size_t initialEntityCount = registry.storage<entt::entity>().size();

  // Trigger derived cast
  SkillSystem::TriggerCast(registry, player, 1, entt::null, {10.0f, 10.0f});

  entt::entity helperEntity = entt::null;
  for (auto ent : registry.view<SkillExecution>()) {
    if (ent != player) {
      helperEntity = ent;
      break;
    }
  }
  REQUIRE(registry.valid(helperEntity));

  // Progress through states: Preparing (0.0s) -> Casting (0.05s) -> Settle (0.1s) -> Destroyed
  SkillSystem::UpdateStates(registry, 0.05f); // Transitions to Casting
  SkillSystem::UpdateStates(registry, 0.06f); // Transitions to Settle
  SkillSystem::UpdateStates(registry, 0.15f); // Settle completes, should destroy helper entity

  // Helper entity must be destroyed: entity is invalid and no SkillExecution remains
  CHECK_FALSE(registry.valid(helperEntity));
  CHECK(registry.view<SkillExecution>().empty());
}

TEST_CASE("[Unit] Skill TriggerRule - 935 逆命反噬 respects DeathSeal window, melee filter and ICD") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  entt::registry registry;

  const auto caster = registry.create();
  registry.emplace<PlayerTag>(caster);
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster);
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  active.slots[0].id = 9;
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].allocated_points[935] = 1;
  SkillSystem::RebakeSkillProfiles(registry, caster);

  auto *triggers = registry.try_get<TriggerRuleComponent>(caster);
  REQUIRE(triggers != nullptr);
  REQUIRE(triggers->HasRule(935));
  TriggerRule *rule = nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == 935) {
      rule = &triggers->rules[i];
    }
  }
  REQUIRE(rule != nullptr);
  // 契约默认值: 20% 概率 + 0.5s 内置冷却
  CHECK(rule->base_chance == doctest::Approx(0.2f));
  CHECK(rule->internal_cooldown == doctest::Approx(0.5f));

  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 10.0f, 0.0f);
  registry.emplace<CombatStats>(enemy);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);

  const auto hit = [&](Tag tags) {
    return CombatEventFactory::CreateSkillHit(caster, enemy, 8, tags, false, 0);
  };

  // 注入必触发概率, 使 ICD/窗口/近战过滤的断言确定性化
  rule->base_chance = 1.0f;

  // 未进入逆脉窗口: 不触发
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown == doctest::Approx(0.0f));

  // 进入逆脉窗口
  auto &pt = registry.emplace<PhantomTranceComponent>(caster);
  pt.remaining = 1.0f;
  pt.params.death_seal = true;

  // 首次近战命中: 触发并进入 ICD
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown > 0.0f);
  const float cooldownAfterFirst = rule->current_cooldown;

  // 同一窗口内再次近战命中: ICD 生效, 不得重复触发
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown == doctest::Approx(cooldownAfterFirst));

  // ICD 结束后可再次触发
  ProcEngine::UpdateCooldowns(registry, 0.6f);
  CHECK(rule->current_cooldown == doctest::Approx(0.0f));
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown > 0.0f);

  // 非近战命中: 即使概率 100% 也不触发
  ProcEngine::UpdateCooldowns(registry, 0.6f);
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Projectile));
  CHECK(rule->current_cooldown == doctest::Approx(0.0f));

  // 退出逆脉窗口: 不触发
  pt.params.death_seal = false;
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown == doctest::Approx(0.0f));
}

TEST_CASE("[Unit] Skill TriggerRule - DeathSeal window excludes PhantomTrance ending phase") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  CombatEventDispatcher::Clear();
  SkillSystem::InitHooks();

  entt::registry registry;

  const auto caster = registry.create();
  registry.emplace<PlayerTag>(caster);
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  registry.emplace<CombatStats>(caster);
  auto &active = registry.emplace<ActiveSkillsComponent>(caster);
  active.slots[0].id = 9;
  active.specialized_slots[0].skill_id = 9;
  active.specialized_slots[0].allocated_points[935] = 1;
  SkillSystem::RebakeSkillProfiles(registry, caster);

  auto *triggers = registry.try_get<TriggerRuleComponent>(caster);
  REQUIRE(triggers != nullptr);
  TriggerRule *rule = nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == 935) {
      rule = &triggers->rules[i];
    }
  }
  REQUIRE(rule != nullptr);
  REQUIRE(rule->required_window == TriggerWindow::DeathSeal);

  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 10.0f, 0.0f);
  registry.emplace<CombatStats>(enemy);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);

  const auto hit = [&](Tag tags) {
    return CombatEventFactory::CreateSkillHit(caster, enemy, 8, tags, false, 0);
  };

  // 强制必触发, 使窗口口径成为唯一变量。
  rule->base_chance = 1.0f;

  // 窗口激活态: death_seal=true 且 remaining>0 且 ending=false -> 属于 DeathSeal 窗口。
  auto &pt = registry.emplace<PhantomTranceComponent>(caster);
  pt.remaining = 1.0f;
  pt.params.death_seal = true;
  pt.ending = false;
  REQUIRE(IsDeathSealActive(pt));
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown > 0.0f);

  // 进入结束结算: ending=true 时即使 remaining>0 也不再算 DeathSeal 窗口, 不触发。
  ProcEngine::UpdateCooldowns(registry, 0.6f);
  pt.ending = true;
  REQUIRE_FALSE(IsDeathSealActive(pt));
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown == doctest::Approx(0.0f));

  // 回到非 ending 且窗口仍激活: 可再次触发, 证明不触发确由 ending 口径导致。
  pt.ending = false;
  ProcEngine::DispatchEvent(registry, caster, hit(Tag::Melee));
  CHECK(rule->current_cooldown > 0.0f);
}

} // namespace NoMoreDay
