// 技能4 剑气护体 追补实现回归测试（计划 §5 C2 / B1-02/03/04）。
// 覆盖：节点数值烘焙、476 元素曝光、472/473 雷电法环、435 暴击加成、
// 470 反击剑气实体、474/475 霜铠冰霜风暴。
#include "TestCommon.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>

#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/FactionComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/damage/DamageInterceptors.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/BladeWardRuntime.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 4;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // ModifierRuntimeRegistry 为进程级单例，前置用例可能注入合成 blob；依赖真实
  // 生成数据的烘焙断言前强制重载，避免执行顺序造成跨用例污染。
  REQUIRE(ReloadModifierRuntimeFromAsset());
  SkillBehaviorRegistry::Initialize();
}

// 注册与生产语义一致的结算钩子：让 472 雷电法环脉冲经真实伤害管线落地。
void RegisterTestResolutionHooks() {
  DamageResolutionHooks hooks;
  hooks.execute = [](entt::registry &registry, const DamageRequest &request,
                     entt::entity target) {
    return DamagePipeline::Execute(registry, request, target, false);
  };
  hooks.calculateBatch = [](entt::registry &registry,
                            const DamageRequest &request) {
    return DamagePipeline::CalculateBatchResults(registry, request);
  };
  RegisterDamageResolutionHooks(hooks);
}

entt::entity CreateTestPlayer(entt::registry &registry,
                              const std::unordered_map<uint32_t, int> &allocated_nodes = {}) {
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(player, 10000.0f, 10000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 10000.0f;
  stats.health = 10000.0f;
  stats.max_mana = 200.0f;
  stats.mana = 200.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 10.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].cooldown = 0.0f;

  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, kSkillId, &spec, profile, nullptr);
  active.baked_profiles[0] = profile;

  return player;
}

entt::entity CreateTestEnemy(entt::registry &registry, float x, float y) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, 5000.0f, 5000.0f);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = 5000.0f;
  stats.health = 5000.0f;
  stats.armor = 0.0f;
  // 雷电法环/霜铠风暴以 FactionComponent + EnemyTag 作为敌方筛选标记。
  registry.emplace<FactionComponent>(enemy);
  registry.emplace<EnemyTag>(enemy);
  return enemy;
}

entt::entity CastBladeWard(entt::registry &registry, entt::entity player) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = {0.0f, 0.0f};
  static uint64_t s_cast_id = 100;
  exec.cast_id = ++s_cast_id;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
  return player;
}

const BuffEffect *FindEffectById(const ActiveEffectsComponent *fx,
                                 std::string_view id) {
  if (fx == nullptr) {
    return nullptr;
  }
  for (const auto &effect : fx->effects) {
    if (effect.id == id) {
      return &effect;
    }
  }
  return nullptr;
}

const StatModifier *FindModifier(const BuffEffect *effect, StatType type) {
  if (effect == nullptr) {
    return nullptr;
  }
  for (const auto &modifier : effect->modifiers) {
    if (modifier.type == type) {
      return &modifier;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("[Functional] Skill 4 - Followup Nodes Bake Mechanics Values (C2.3/C2.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  const std::unordered_map<uint32_t, int> nodes = {
      {402, 1}, {403, 1}, {410, 1}, {413, 1}, {415, 1}, {431, 1}, {433, 1},
      {434, 1}, {435, 1}, {453, 1}, {454, 1}, {472, 1}, {473, 1}, {474, 1},
      {475, 1}, {476, 1}};
  auto player = CreateTestPlayer(registry, nodes);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);

  // 402 法耗折扣已烘焙进交付档案：30 * (1 - 0.15 * 1) = 25.5；行为层不再持有
  // mana_cost_reduction，折扣唯一事实源为 profile->effective_mana_cost。
  CHECK(registry.get<ActiveSkillsComponent>(player)
            .baked_profiles[0]
            .effective_mana_cost == doctest::Approx(25.5f));
  CHECK(ward->counter_chance_bonus == doctest::Approx(0.10f));
  CHECK(ward->counter_range_bonus == doctest::Approx(0.10f));
  // 410
  CHECK(ward->armor_dr_bonus == doctest::Approx(0.10f));
  CHECK(ward->armor_dr_per_1000 == doctest::Approx(0.01f));
  // 413
  CHECK(ward->last_stand_threshold == doctest::Approx(0.35f));
  CHECK(ward->last_stand_base_dr == doctest::Approx(0.24f));
  CHECK(ward->last_stand_armor_mult == doctest::Approx(2.0f));
  // 415/433/434
  CHECK(ward->missing_hp_regen_pct == doctest::Approx(0.005f));
  CHECK(ward->has_blood_barrier);
  CHECK(ward->perfect_parry_interval == doctest::Approx(5.0f));
  CHECK(ward->perfect_parry_ready);
  // 435/453/454
  CHECK(ward->block_intent_crit == doctest::Approx(0.05f));
  CHECK(ward->block_intent_chance == doctest::Approx(0.15f));
  CHECK(ward->aftermath_heal_pct == doctest::Approx(0.05f));
  CHECK(ward->sword_step_dodge_rating == doctest::Approx(50.0f));
  // 472/473
  CHECK(ward->static_interval == doctest::Approx(0.5f));
  CHECK(ward->static_radius == doctest::Approx(50.0f));
  CHECK(ward->thunder_frequency_bonus == doctest::Approx(0.30f));
  CHECK(ward->shock_slow == doctest::Approx(0.10f));
  CHECK(ward->shock_damage_bonus == doctest::Approx(0.15f));
  // 474/475：半径 = 80 * (1 + 0.20)
  CHECK(ward->frost_knockback == doctest::Approx(100.0f));
  CHECK(ward->frost_radius == doctest::Approx(96.0f));
  CHECK(ward->permafrost_radius_bonus == doctest::Approx(0.20f));
  // 476
  CHECK(ward->exposure_pct == doctest::Approx(0.04f));
  CHECK(ward->exposure_duration == doctest::Approx(3.0f));
}

TEST_CASE("[Functional] Skill 4 - Exposure 476 Shreds Element Resist And Refreshes (C2.1)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{476, 3}, {472, 1}});
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->exposure_pct == doctest::Approx(0.12f));

  // 雷电系剑气护体：曝光按雷元素抗性削减结算。
  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);

  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *exposure = FindEffectById(fx, "blade_ward_exposure");
  REQUIRE(exposure != nullptr);
  const StatModifier *resist = FindModifier(exposure, StatType::ResistLightning);
  REQUIRE(resist != nullptr);
  CHECK(resist->value == doctest::Approx(-12.0f));

  // 重复命中只刷新，不叠加：曝光效果数量恒为 1。
  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);
  int exposureCount = 0;
  for (const auto &effect : fx->effects) {
    if (effect.id == "blade_ward_exposure") {
      ++exposureCount;
    }
  }
  CHECK(exposureCount == 1);
}

TEST_CASE("[Functional] Skill 4 - Lightning Aura 472/473 Pulses Shock (C2.2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();

  entt::registry registry;
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  auto player = CreateTestPlayer(registry, {{472, 1}, {473, 3}, {476, 1}});
  auto enemy = CreateTestEnemy(registry, 30.0f, 0.0f);
  CastBladeWard(registry, player);
  grid.rebuild(registry.view<Position>(), registry);

  auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->is_lightning_ward);

  const auto *enemyHealth = registry.try_get<HealthComponent>(enemy);
  REQUIRE(enemyHealth != nullptr);
  const float before = enemyHealth->current;

  // 单帧 1.0s 足以触发 0.5/(1+0.9) ≈ 0.263s 的雷击脉冲。
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 1.0f);

  CHECK(enemyHealth->current < before);

  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *shock = FindEffectById(fx, "blade_ward_thunder_shock");
  REQUIRE(shock != nullptr);
  const StatModifier *slow = FindModifier(shock, StatType::MoveSpeed);
  REQUIRE(slow != nullptr);
  CHECK(slow->value < 0.0f);
  const StatModifier *vuln = FindModifier(shock, StatType::ResistLightning);
  REQUIRE(vuln != nullptr);
  CHECK(vuln->value < 0.0f);

  // 476 元素曝光在同一脉冲上生效
  CHECK(FindEffectById(fx, "blade_ward_exposure") != nullptr);
  // 473 频率提升后计时器已归零，下一脉冲需重新累积
  CHECK(ward->static_timer == doctest::Approx(0.0f));
}

TEST_CASE("[Functional] Skill 4 - Crit Bonus 435 Baked As Crit Chance (C2.4)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{435, 3}});
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  // crit_bonus 0.05/点 × 3 = 0.15
  CHECK(ward->block_intent_crit == doctest::Approx(0.15f));
  CHECK(ward->block_intent_chance == doctest::Approx(0.45f));
}

TEST_CASE("[Functional] Skill 4 - Counter Swords Spawn From 470 (C2.5)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{470, 1}});
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);

  std::size_t before = 0;
  for (auto entity : registry.view<Projectile, SkillComponent>()) {
    (void)entity;
    ++before;
  }

  const DamageRequest request =
      damage::ResolveSkill4CounterEffects(registry, player, enemy, *ward, true);
  CHECK(request.skill_id == kSkillId);
  CHECK(request.base_pool.Get(Tag::Physical) > 0.0f);

  std::size_t counterSwords = 0;
  bool allVisualOnly = true;
  for (auto entity : registry.view<Projectile, SkillComponent>()) {
    const auto &projectile = registry.get<Projectile>(entity);
    const auto &skill = registry.get<SkillComponent>(entity);
    if (projectile.owner == player && skill.skill_id == kSkillId) {
      ++counterSwords;
      allVisualOnly = allVisualOnly && projectile.visual_only;
    }
  }
  CHECK(counterSwords == 5u);
  CHECK(counterSwords > before);
  // 反击剑气为纯表现实体：命中不得二次结算伤害（单源由 ResolveSkill4Counter 承担）。
  CHECK(allVisualOnly);
}

TEST_CASE("[Functional] Skill 4 - Cold Counter 474/475 Frost Storm And Exposure (C2.2)") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{474, 1}, {475, 1}, {476, 1}});
  auto enemy = CreateTestEnemy(registry, 30.0f, 0.0f);
  CastBladeWard(registry, player);

  const auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  CHECK(ward->is_cold_ward);
  CHECK(ward->frost_radius == doctest::Approx(96.0f));

  skills::ApplyBladeWardCounterOnHit(registry, player, enemy, *ward);

  // 击退：目标获得远离施放者的速度分量
  const auto *velocity = registry.try_get<Velocity>(enemy);
  REQUIRE(velocity != nullptr);
  CHECK(velocity->vx > 0.0f);

  // 476：曝光按冰元素抗性削减
  const auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  const BuffEffect *exposure = FindEffectById(fx, "blade_ward_exposure");
  REQUIRE(exposure != nullptr);
  const StatModifier *resist = FindModifier(exposure, StatType::ResistCold);
  REQUIRE(resist != nullptr);
  CHECK(resist->value == doctest::Approx(-4.0f));
}

TEST_CASE("[Functional] Skill 4 - Perfect Parry 434 Charge Gates Interception") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{434, 1}});
  CastBladeWard(registry, player);

  auto *ward = registry.try_get<BladeWardComponent>(player);
  REQUIRE(ward != nullptr);
  REQUIRE(ward->perfect_parry_ready);
  CHECK(ward->perfect_parry_interval == doctest::Approx(5.0f));

  // 充能就绪：投射物命中被完全招架，充能被消费。
  const auto intercepted = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK(intercepted.intercepted);
  CHECK_FALSE(ward->perfect_parry_ready);

  // 间隔内不再拦截：剑数归零以排除几率拦截路径，仅验证充能门控。
  ward->sword_count = 0;
  const auto gated = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK_FALSE(gated.intercepted);

  // 间隔尚未填满（1s < 5s）：充能不恢复，继续不拦截。
  systems::SpatialHashGrid grid(100, 100, 64.0f);
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 1.0f);
  CHECK_FALSE(ward->perfect_parry_ready);

  // 间隔填满（累计 6s ≥ 5s）：充能恢复，重新具备完全招架能力。
  skills::UpdateBladeWardRuntime(registry, grid, player, *ward, 5.0f);
  CHECK(ward->perfect_parry_ready);
  const auto recharged = damage::EvaluateBladeWardInterception(
      registry, player, entt::null, Tag::Projectile, false);
  CHECK(recharged.intercepted);
}

// 构造一次 1 号技能物理攻击并走真实伤害管线结算。
static float ResolvePlayerAttack(entt::registry &registry, entt::entity player,
                                 entt::entity target) {
  DamageRequest request{};
  request.attacker = player;
  request.defender = target;
  request.skill_id = 1u;
  request.base_pool.Add(Tag::Physical, 100.0f);
  return DamagePipeline::Execute(registry, request, player, false)
      .damage.total_damage;
}

TEST_CASE("[Functional] Skill 4 - 455 Offensive Guard Buffers Next Attack") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{455, 1}});
  // 固定暴击结果，使基线数值可精确比较。
  registry.get<CombatStats>(player).crit_chance = 0.0f;
  registry.emplace<SwordIntentComponent>(player);
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);

  const float baseline = ResolvePlayerAttack(registry, player, enemy);
  REQUIRE(baseline > 0.0f);

  // 基线攻击经 OnDealDamage 已获得 1 层剑意，记录闪避前层数以断言 455 的净增益。
  const int intent_before =
      registry.get<SwordIntentComponent>(player).stacks;

  // 闪避成功：挂起「下一次攻击」标记并额外 +1 层剑意。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, enemy));
  const auto *effects = registry.try_get<ActiveEffectsComponent>(player);
  REQUIRE(effects != nullptr);
  CHECK(FindEffectById(effects, "blade_ward_dodge_power") != nullptr);
  CHECK(registry.get<SwordIntentComponent>(player).stacks == intent_before + 1);

  // 下一次攻击：全局 More +20%，且标记被消费。
  const float boosted = ResolvePlayerAttack(registry, player, enemy);
  CHECK(boosted == doctest::Approx(baseline * 1.2f));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") == nullptr);

  // 之后攻击不再有加成。
  const float after = ResolvePlayerAttack(registry, player, enemy);
  CHECK(after == doctest::Approx(baseline));
}

TEST_CASE("[Functional] Skill 4 - 455 Offensive Guard Missed Swing Consumes Token") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{455, 1}});
  registry.get<CombatStats>(player).crit_chance = 0.0f;
  registry.emplace<SwordIntentComponent>(player);

  auto elusive = CreateTestEnemy(registry, 40.0f, 0.0f);
  registry.get<CombatStats>(elusive).dodge_chance = 1.0f;
  auto dummy = CreateTestEnemy(registry, 60.0f, 0.0f);

  const float baseline = ResolvePlayerAttack(registry, player, dummy);

  // 闪避成功后挂起标记。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, dummy));
  REQUIRE(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                         "blade_ward_dodge_power") != nullptr);

  // 未命中的挥击同样消耗标记。
  DamageRequest missed_request{};
  missed_request.attacker = player;
  missed_request.defender = elusive;
  missed_request.skill_id = 1u;
  missed_request.base_pool.Add(Tag::Physical, 100.0f);
  const auto missed =
      DamagePipeline::Execute(registry, missed_request, player, false);
  CHECK(missed.damage.was_dodged);
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") == nullptr);

  // 消耗后攻击恢复无加成数值。
  const float after = ResolvePlayerAttack(registry, player, dummy);
  CHECK(after == doctest::Approx(baseline));
}

// RD-06：455 标记只由真正攻击消耗，荆棘反伤/地面危险区等非攻击伤害不得消费。
TEST_CASE("[Functional] Skill 4 - 455 Offensive Guard Ignores Non-Attack Damage") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{455, 1}});
  registry.get<CombatStats>(player).crit_chance = 0.0f;
  registry.emplace<SwordIntentComponent>(player);
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);

  auto resolve_origin = [&](DamageOrigin origin) {
    DamageRequest request{};
    request.attacker = player;
    request.defender = enemy;
    request.origin = origin;
    request.skill_id = 1u;
    request.base_pool.Add(Tag::Physical, 100.0f);
    return DamagePipeline::Execute(registry, request, player, false)
        .damage.total_damage;
  };

  // 无标记时的基准值，保证后续比较不受剑意层数变化影响。
  const float thorns_control = resolve_origin(DamageOrigin::ThornsReflect);
  const float hazard_control = resolve_origin(DamageOrigin::HazardEnvironment);
  const float attack_control = ResolvePlayerAttack(registry, player, enemy);

  // 闪避成功挂起「下一次攻击」标记。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, enemy));
  REQUIRE(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                         "blade_ward_dodge_power") != nullptr);

  // 非攻击伤害既不消耗标记，也不吃到 +20%。
  CHECK(resolve_origin(DamageOrigin::ThornsReflect) ==
        doctest::Approx(thorns_control));
  CHECK(resolve_origin(DamageOrigin::HazardEnvironment) ==
        doctest::Approx(hazard_control));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") != nullptr);

  // 随后的正常攻击仍能消费标记并享受 +20%。
  CHECK(ResolvePlayerAttack(registry, player, enemy) ==
        doctest::Approx(attack_control * 1.2f));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") == nullptr);
}

// RD-06 补裁（设计 §3.4 / 计划 §10）：一次「攻击行为」（一次施法或一次普攻
// 挥击）产生的全部伤害实例共享同一份 +20%，而非仅首个结算目标获得加成。
TEST_CASE("[Functional] Skill 4 - 455 Offensive Guard Shared Across Attack Behavior") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{455, 1}});
  registry.get<CombatStats>(player).crit_chance = 0.0f;
  registry.emplace<SwordIntentComponent>(player);
  auto enemyA = CreateTestEnemy(registry, 40.0f, 0.0f);
  auto enemyB = CreateTestEnemy(registry, -40.0f, 0.0f);

  const float baseline = ResolvePlayerAttack(registry, player, enemyA);
  REQUIRE(baseline > 0.0f);

  // 闪避成功：挂起「下一次攻击」标记。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, enemyA));
  REQUIRE(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                         "blade_ward_dodge_power") != nullptr);

  constexpr uint64_t kSharedAttackKey = 987654321ull;
  auto resolve_with_key = [&](entt::entity target, uint64_t attackKey) {
    DamageRequest request{};
    request.attacker = player;
    request.defender = target;
    request.skill_id = 1u;
    request.base_pool.Add(Tag::Physical, 100.0f);
    request.attack_key = attackKey;
    return DamagePipeline::Execute(registry, request, player, false)
        .damage.total_damage;
  };

  // 同一攻击行为首个实例：吃到 +20%，标记进入「已消费、本行为内继续生效」态。
  const float first = resolve_with_key(enemyA, kSharedAttackKey);
  CHECK(first == doctest::Approx(baseline * 1.2f));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") != nullptr);

  // 同一攻击行为第二个实例（另一目标）：仍吃到 +20%。
  const float second = resolve_with_key(enemyB, kSharedAttackKey);
  CHECK(second == doctest::Approx(baseline * 1.2f));

  // 新攻击行为（不同标识）：不加成且清除标记。
  const float after = resolve_with_key(enemyA, kSharedAttackKey + 1);
  CHECK(after == doctest::Approx(baseline));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") == nullptr);

  // 标记已清除：后续攻击无加成。
  CHECK(resolve_with_key(enemyB, kSharedAttackKey + 2) ==
        doctest::Approx(baseline));
}

// Wave D 边界缺陷回归：攻击消费后标记保留（consumed=true/key=K），若 2s 窗口内
// 再次闪避，AddOrRefresh 必须把瞬态消费字段复位为新一次挂起，否则新闪避的加成
// 会被残留的 consumed 态吞掉。
TEST_CASE("[Functional] Skill 4 - 455 Refresh Resets Consumed State") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  RegisterTestResolutionHooks();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();

  entt::registry registry;
  auto player = CreateTestPlayer(registry, {{455, 1}});
  registry.get<CombatStats>(player).crit_chance = 0.0f;
  registry.emplace<SwordIntentComponent>(player);
  auto enemy = CreateTestEnemy(registry, 40.0f, 0.0f);

  const float baseline = ResolvePlayerAttack(registry, player, enemy);
  REQUIRE(baseline > 0.0f);

  constexpr uint64_t kKey1 = 111111ull;
  constexpr uint64_t kKey2 = 222222ull;
  constexpr uint64_t kKey3 = 333333ull;
  auto resolve_with_key = [&](uint64_t attackKey) {
    DamageRequest request{};
    request.attacker = player;
    request.defender = enemy;
    request.skill_id = 1u;
    request.base_pool.Add(Tag::Physical, 100.0f);
    request.attack_key = attackKey;
    return DamagePipeline::Execute(registry, request, player, false)
        .damage.total_damage;
  };

  // 第一次闪避挂起标记。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, enemy));
  REQUIRE(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                         "blade_ward_dodge_power") != nullptr);

  // K1 攻击消费：加成生效，标记进入已消费态并保留。
  CHECK(resolve_with_key(kKey1) == doctest::Approx(baseline * 1.2f));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") != nullptr);

  // 2s 窗口内再次闪避：刷新必须复位消费状态（标记仍存在）。
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateOnDodge(player, enemy));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") != nullptr);

  // 新一次挂起应重新吃到 +20%（缺陷未修时会退化为 baseline）。
  CHECK(resolve_with_key(kKey2) == doctest::Approx(baseline * 1.2f));

  // 换攻击行为：无加成且清除标记。
  CHECK(resolve_with_key(kKey3) == doctest::Approx(baseline));
  CHECK(FindEffectById(registry.try_get<ActiveEffectsComponent>(player),
                       "blade_ward_dodge_power") == nullptr);
}

// 470 反击剑数覆盖「档案命中」与「档案缺失回落机制表」两条路径。这里把机制表
// 的 4/470.counter_swords 改写为 7，用不同数值区分两条路径：命中档案须仍读档案
// 的 sub_count=5，只有回退路径才反映机制表的 7，避免测试退化到只看默认值 5。
// 471 增伤则以「清空档案」与「保留档案」分别锁定 0.0 与 0.6 的迁移契约。
TEST_CASE("[Functional] Skill 4 - 470 Counter Sword Count Parity") {
  TestSetupScope scope;
  EnsureSkillMechanics();

  // 合成机制表：把 4/470.counter_swords 置 7.0。加载要求技能 id 1..12 全部在
  // 场（缺一即拒载），其余技能补空对象占位。
  const auto mechanics_dir =
      std::filesystem::temp_directory_path() / "nmd_skill4_470_parity";
  std::filesystem::create_directories(mechanics_dir);
  const auto mechanics_path = mechanics_dir / "counter_swords_7.json";
  {
    std::ofstream out(mechanics_path, std::ios::binary);
    REQUIRE(out.good());
    out << R"({"version":1,"1":{},"2":{},"3":{},)"
           R"("4":{"470":{"counter_swords":7.0}},)"
           R"("5":{},"6":{},"7":{},"8":{},"9":{},"10":{},"11":{},"12":{}})";
  }
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      mechanics_path.string()));
  // 文件内容已读入内存，立即删除以避开子用例断言失败时的残留清理路径。
  std::filesystem::remove(mechanics_path);

  SUBCASE("cached baked profile path") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{470, 1}});
    CastBladeWard(registry, player);

    const auto *ward = registry.try_get<BladeWardComponent>(player);
    REQUIRE(ward != nullptr);
    // 机制表为 7 时仍取烘焙档案 sub_count=5，证明本路径读的是档案而非机制表。
    CHECK(ward->counter_sword_count == 5);
  }

  SUBCASE("no profile falls back to mechanics default") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{470, 1}});

    // 同时清空缓存档案与槽位专精，迫使 ResolveBakedProfile 返回 nullptr，
    // DoCast 走机制表 counter_swords 回退。
    auto &active = registry.get<ActiveSkillsComponent>(player);
    active.baked_profiles[0] = BakedSkillProfile{};
    active.specialized_slots[0] = SpecializedSkill{};

    CastBladeWard(registry, player);
    const auto *ward = registry.try_get<BladeWardComponent>(player);
    REQUIRE(ward != nullptr);
    // 机制表为 7 时回退路径反映该值，证明回退确为数据驱动而非默认 5。
    CHECK(ward->counter_sword_count == 7);
  }

  SUBCASE("no profile 471 more damage is fail-closed zero") {
    entt::registry registry;
    auto player = CreateTestPlayer(registry, {{471, 3}});

    // 清空档案与专精槽迫使 ResolveBakedProfile 返回 nullptr。profile==nullptr
    // 意味着专精槽同样缺失（ResolveBakedProfile 会即时烘焙），故 specState 与
    // exec.active_nodes 均为空，迁移前的 nodePoints 回退本就为 0；该分支与 470
    // 的机制表回退不对称是有意为之的「失败关闭」契约。
    auto &active = registry.get<ActiveSkillsComponent>(player);
    active.baked_profiles[0] = BakedSkillProfile{};
    active.specialized_slots[0] = SpecializedSkill{};

    CastBladeWard(registry, player);
    const auto *ward = registry.try_get<BladeWardComponent>(player);
    REQUIRE(ward != nullptr);
    CHECK(ward->counter_damage_more == doctest::Approx(0.0f));
  }

  SUBCASE("baked profile 471 more damage is single source") {
    entt::registry registry;
    // 不清档案：profile 命中，471 取 profile->more_damage_mult - 1.0
    // = (1.0 + 0.20 * 3) - 1.0 = 0.60。
    auto player = CreateTestPlayer(registry, {{471, 3}});
    CastBladeWard(registry, player);

    const auto *ward = registry.try_get<BladeWardComponent>(player);
    REQUIRE(ward != nullptr);
    CHECK(ward->counter_damage_more == doctest::Approx(0.60f));
  }

  // 恢复真实机制表，避免合成表泄漏到依赖技能数据的后续用例。
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
}

} // namespace NoMoreDay
