// 技能9 绝影绝剑 — 专项功能测试
// 覆盖：形态基础、免死/重生、逆脉锁血、虚灵之躯、护甲、死亡螺旋、嗜血、
//       时光逆流、全神贯注、逆命反噬、影剑回响、元素转质脉冲、意念穿透、
//       意随神行、御剑化影、灵气反哺、缩地成寸等关键节点。
// 说明：全部 headless，仅直调行为层与固定状态注入，避免依赖渲染与随机单次结果。

#include "TestCommon.hpp"

#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/DamagePipelineTypes.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/BuffIds.hpp"
#include "game/foundation/data/SkillContract.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/RegenerationSystem.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/PhantomTrance.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {
namespace {

using doctest::Approx;

constexpr uint32_t kSkillId = 9;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  (void)systems::AilmentRegistry::Get().EnsureLoaded();
  // ModifierRuntimeRegistry 为进程级单例，前置用例可能注入合成 blob；依赖真实
  // 生成数据的烘焙断言前强制重载，避免执行顺序造成跨用例污染。
  REQUIRE(ReloadModifierRuntimeFromAsset());
  SkillBehaviorRegistry::Initialize();
  CombatEventDispatcher::Clear();
  SkillSystem::ShutdownHooks();
  SkillSystem::InitHooks();
}

entt::entity MakePlayer(entt::registry &registry,
                        const std::unordered_map<uint32_t, int> &allocated = {},
                        Vector2 pos = {0.0f, 0.0f}) {
  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, pos.x, pos.y);
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(player, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 500.0f;
  stats.mana = 500.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.crit_chance = 10.0f;
  stats.crit_damage = 1.5f;
  stats.effective_intelligence = 100.0f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[0].current_charges = 5;
  active.slots[0].cooldown = 0.0f;
  active.specialized_slots[0].skill_id = kSkillId;
  active.specialized_slots[0].allocated_points = allocated;
  return player;
}

entt::entity MakeEnemy(entt::registry &registry, float x, float y,
                       float max_hp = 1000.0f) {
  auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, max_hp, max_hp);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = max_hp;
  stats.health = max_hp;
  stats.cached_area_level = 1;
  return enemy;
}

void CastTrance(entt::registry &registry, entt::entity player,
                Vector2 target_pos = {0.0f, 0.0f}) {
  SkillSystem::RebakeSkillProfiles(registry, player);
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target_pos;
  static uint64_t s_cast_id = 1;
  exec.cast_id = ++s_cast_id;
  auto cast_func = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(cast_func != nullptr);
  cast_func(registry, player, exec);
}

// 推进形态：返回 true 表示组件仍存在；返回 false 表示已被销毁。
bool AdvanceTrance(entt::registry &registry, entt::entity player, float dt) {
  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  if (!pt) return false;
  if (skills::PhantomTrance::Update(registry, player, *pt, dt)) {
    registry.remove<PhantomTranceComponent>(player);
    return false;
  }
  return true;
}

void Tick(entt::registry &registry, entt::entity player, float dt, int times = 1) {
  for (int i = 0; i < times; ++i) {
    if (!AdvanceTrance(registry, player, dt)) return;
  }
}

const BuffEffect *FindEffect(entt::registry &registry, entt::entity entity,
                             BuffId id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
  if (!effects) return nullptr;
  return effects->Get(id);
}

const BuffEffect *FindAilment(entt::registry &registry, entt::entity entity,
                              AilmentType type) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
  if (!effects) return nullptr;
  for (const auto &b : effects->effects) {
    if (b.managed_ailment && b.ailment_type == static_cast<uint8_t>(type)) {
      return &b;
    }
  }
  return nullptr;
}

const TriggerRule *FindRule(entt::registry &registry, entt::entity entity,
                            uint32_t rule_id) {
  auto *triggers = registry.try_get<TriggerRuleComponent>(entity);
  if (!triggers) return nullptr;
  for (uint8_t i = 0; i < triggers->rule_count; ++i) {
    if (triggers->rules[i].rule_id == rule_id) return &triggers->rules[i];
  }
  return nullptr;
}

} // namespace

TEST_CASE("[Functional] PhantomTrance - 形态基础：施放进入形态并结束时爆发") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry);
  auto enemy = MakeEnemy(registry, 80.0f, 0.0f);
  const float enemy_hp_before = registry.get<HealthComponent>(enemy).current;

  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->remaining == Approx(3.0f));
  CHECK(pt->duration == Approx(3.0f));
  CHECK(pt->params.duration_sec == Approx(3.0f));
  CHECK_FALSE(pt->ending);

  // 移动形态 Buff 存在（未点 902 时数值为 0，但形态 Buff 本身必须存在）。
  const auto *form = FindEffect(registry, player, BuffId::PhantomTranceForm);
  REQUIRE(form != nullptr);

  // 形态自然结束后触发范围爆发。
  Tick(registry, player, 3.5f);
  CHECK(registry.try_get<PhantomTranceComponent>(player) == nullptr);
  CHECK(registry.get<HealthComponent>(enemy).current < enemy_hp_before);
}

TEST_CASE("[Functional] PhantomTrance - 902/974/976/934 形态数值烘焙") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(
      registry, {{902, 1}, {974, 1}, {976, 2}, {934, 2}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.move_speed_pct == Approx(30.0f)); // 形态基础 20 + 902 单点 10
  CHECK(pt->params.dodge_pct == Approx(10.0f));
  CHECK(pt->params.recovery_pct == Approx(10.0f));
  CHECK(pt->params.burst_damage_mult == Approx(1.30f));
  CHECK(pt->params.atk_cast_speed_pct == Approx(10.0f));

  // 934 攻速/施法速度仅在逆脉形态下进入形态 Buff。
  const auto *form = FindEffect(registry, player, BuffId::PhantomTranceForm);
  REQUIRE(form != nullptr);
  CHECK(FindEffect(registry, player, BuffId::PhantomTranceDeathSeal) == nullptr);
}

TEST_CASE("[Functional] PhantomTrance - 981 逆脉锁血与禁疗") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{981, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.death_seal);
  CHECK(IsDeathSealActive(*pt));

  auto &hp = registry.get<HealthComponent>(player);
  auto &stats = registry.get<CombatStats>(player);
  CHECK(hp.max == Approx(330.0f));
  CHECK(stats.max_health == Approx(1000.0f));

  const auto *seal = FindEffect(registry, player, BuffId::PhantomTranceDeathSeal);
  REQUIRE(seal != nullptr);
  CHECK(seal->modifiers.size() == 6);
  for (const auto &mod : seal->modifiers) {
    CHECK(mod.value == Approx(33.0f));
  }

  // 禁疗：逆脉窗口内自然回复被阻断，且生命上限不随 stats 同步。
  hp.current = 100.0f;
  stats.health_regen = 50.0f;
  RegenerationSystem::update(registry, 1.0f);
  CHECK(hp.current == Approx(100.0f));
  CHECK(hp.max == Approx(330.0f));

  // 真实帧序中的属性重算（StatsSystem::update）不得把锁血上限刷回满值，
  // 否则 983 的"临时上限损失"会恒为零。
  registry.emplace_or_replace<StatsDirty>(player);
  StatsSystem::update(registry);
  CHECK(hp.max == Approx(stats.max_health * 0.33f));
  CHECK(hp.current == Approx(100.0f));

  // 装备回血入口（life_on_hit）同样被禁疗阻断。
  auto enemy = MakeEnemy(registry, 30.0f, 0.0f, 100000.0f);
  stats.life_on_hit = 100.0f;
  CombatEventDispatcher::Dispatch(
      registry,
      CombatEventFactory::CreateSkillHit(player, enemy, kSkillId, Tag::Hit));
  CHECK(hp.current == Approx(100.0f));
  CHECK(hp.max == Approx(stats.max_health * 0.33f));
}

TEST_CASE("[Functional] PhantomTrance - 979 绝影护甲获得比例护盾") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{979, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.ward_pct == Approx(0.10f));
  CHECK(registry.get<CombatStats>(player).barrier == Approx(100.0f));
  CHECK(registry.all_of<BarrierComponent>(player));
}

TEST_CASE("[Functional] PhantomTrance - 980 虚灵之躯扣血潜行") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{980, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.void_body);
  CHECK(registry.get<HealthComponent>(player).current == Approx(850.0f));
  CHECK(FindEffect(registry, player, BuffId::PhantomTranceStealth) != nullptr);
  CHECK(registry.all_of<PhaseTag>(player));

  // 相位必须贯穿形态持续期：SkillSystem 的御剑步相位清理不得在下一帧剥离它。
  systems::SpatialHashGrid grid(100, 100, 50);
  SkillSystem::Update(registry, grid, 0.1f);
  CHECK(registry.all_of<PhaseTag>(player));

  // 形态结束后由同一清理收回相位（未处于御剑步时）。
  Tick(registry, player, 3.5f);
  CHECK(registry.try_get<PhantomTranceComponent>(player) == nullptr);
  SkillSystem::Update(registry, grid, 0.1f);
  CHECK_FALSE(registry.all_of<PhaseTag>(player));
}

TEST_CASE("[Functional] PhantomTrance - 983 死亡螺旋定时生成飞剑") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{981, 1}, {983, 2}});
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.death_spiral_count == 2);

  // 模拟真实帧序：属性重算后锁血上限必须保持，否则螺旋伤害会退化为 0。
  auto &hp = registry.get<HealthComponent>(player);
  auto &stats = registry.get<CombatStats>(player);
  registry.emplace_or_replace<StatsDirty>(player);
  StatsSystem::update(registry);
  CHECK(hp.max == Approx(stats.max_health * 0.33f));

  // spiral_interval=0.5s，推进 0.5s 应生成一拨飞剑。
  Tick(registry, player, 0.5f);
  std::size_t projectile_count = 0;
  float spiral_damage = -1.0f;
  for (auto e : registry.view<Projectile>()) {
    const auto &proj = registry.get<Projectile>(e);
    if (proj.owner == player) {
      ++projectile_count;
      if (proj.payload_context.has_value()) {
        spiral_damage = proj.payload_context->base_damage_min;
      }
    }
  }
  CHECK(projectile_count == 2);
  // 伤害按"满上限 - 被锁定的临时上限"计算，必须非零。
  CHECK(spiral_damage == Approx((stats.max_health - hp.max) * 0.08f));
  CHECK(registry.valid(enemy));
}

TEST_CASE("[Functional] PhantomTrance - 984 嗜血按累计伤害回复") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{984, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.bloodthirst_pct == Approx(0.05f));

  registry.get<HealthComponent>(player).current = 500.0f;
  pt->damage_dealt_accum = 200.0f;

  Tick(registry, player, 3.5f);
  CHECK(registry.get<HealthComponent>(player).current == Approx(510.0f));
}

TEST_CASE("[Functional] PhantomTrance - 954 时光逆流返还其他技能冷却") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{954, 1}});
  registry.get<ActiveSkillsComponent>(player).slots[1].id = 2;
  registry.get<ActiveSkillsComponent>(player).slots[1].cooldown = 10.0f;
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.time_reversal_sec == Approx(3.0f));

  Tick(registry, player, 3.5f);
  CHECK(registry.get<ActiveSkillsComponent>(player).slots[1].cooldown == Approx(7.0f));
}

TEST_CASE("[Functional] PhantomTrance - 955 全神贯注降低形态内技能耗蓝") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 955 只降低形态期间「其他技能」的耗蓝，因此先进入形态再施放另一技能比较蓝耗。
  auto mana_cost = [&](bool with_focus) {
    entt::registry registry;
    std::unordered_map<uint32_t, int> alloc;
    if (with_focus) alloc[955] = 1;
    auto player = MakePlayer(registry, alloc);
    auto &active = registry.get<ActiveSkillsComponent>(player);
    active.slots[1].id = 1; // 作为形态期间的“其他技能”
    active.slots[1].current_charges = 5;
    registry.get<CombatStats>(player).mana = 500.0f;

    CastTrance(registry, player); // 直接进入形态
    REQUIRE(registry.try_get<PhantomTranceComponent>(player) != nullptr);

    const float before = registry.get<CombatStats>(player).mana;
    REQUIRE(SkillSystem::TryCast(registry, player, 1, {32.0f, 0.0f}));
    return before - registry.get<CombatStats>(player).mana;
  };

  const float baseline = mana_cost(false);
  const float focused = mana_cost(true);
  CHECK(baseline > 0.0f);
  CHECK(focused == Approx(baseline * 0.8f).epsilon(0.02));
}

TEST_CASE("[Functional] PhantomTrance - 935 逆命反噬在逆脉窗口内近战触发") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{981, 1}, {935, 1}});
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  REQUIRE(IsDeathSealActive(*pt));

  const auto *rule = FindRule(registry, player, 935);
  REQUIRE(rule != nullptr);
  CHECK(rule->base_chance == Approx(0.2f));
  CHECK(rule->requires_melee_hit);
  CHECK(rule->required_window == TriggerWindow::DeathSeal);
  CHECK(rule->internal_cooldown == Approx(0.5f));

  // 强制确定命中概率，专注验证窗口/近战过滤与 ICD。
  for (auto &r : registry.get<TriggerRuleComponent>(player).rules) {
    if (r.rule_id == 935) r.base_chance = 1.0f;
  }

  CombatEventDispatcher::Dispatch(
      registry,
      CombatEventFactory::CreateSkillHit(player, enemy, 8, Tag::Hit | Tag::Melee));

  const auto *hit_rule = FindRule(registry, player, 935);
  REQUIRE(hit_rule != nullptr);
  CHECK(hit_rule->current_cooldown > 0.0f);

  // 非近战命中：冷却清零后仍不得触发。
  for (auto &r : registry.get<TriggerRuleComponent>(player).rules) {
    if (r.rule_id == 935) r.current_cooldown = 0.0f;
  }
  CombatEventDispatcher::Dispatch(
      registry,
      CombatEventFactory::CreateSkillHit(player, enemy, 8, Tag::Hit));
  CHECK(FindRule(registry, player, 935)->current_cooldown == Approx(0.0f));

  // 逆脉窗口外：即使近战命中也不得触发。
  for (auto &r : registry.get<TriggerRuleComponent>(player).rules) {
    if (r.rule_id == 935) r.current_cooldown = 0.0f;
  }
  pt->remaining = 0.0f;
  CombatEventDispatcher::Dispatch(
      registry,
      CombatEventFactory::CreateSkillHit(player, enemy, 8, Tag::Hit | Tag::Melee));
  CHECK(FindRule(registry, player, 935)->current_cooldown == Approx(0.0f));
}

TEST_CASE("[Functional] PhantomTrance - 993 影剑回响注册 proc 规则") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{993, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.echo_synergy);

  const auto *rule = FindRule(registry, player, 993);
  REQUIRE(rule != nullptr);
  CHECK(rule->required_skill_id == 8);
  CHECK(rule->cast_skill_id == 8);
  CHECK(rule->base_chance == Approx(0.4f));
  CHECK(rule->internal_cooldown == Approx(0.5f));
}

TEST_CASE("[Functional] PhantomTrance - 989 冰转质脉冲施加冰缓") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{989, 1}});
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.transmuter_tag == Tag::Cold);

  Tick(registry, player, 1.0f);
  CHECK(FindAilment(registry, enemy, AilmentType::Chill) != nullptr);
}

TEST_CASE("[Functional] PhantomTrance - 972 雷转质脉冲施加感电并造成伤害") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{972, 1}});
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.transmuter_tag == Tag::Lightning);

  const float hp_before = registry.get<HealthComponent>(enemy).current;
  Tick(registry, player, 1.0f);
  CHECK(FindAilment(registry, enemy, AilmentType::Shock) != nullptr);
  CHECK(registry.get<HealthComponent>(enemy).current < hp_before);
}

TEST_CASE("[Functional] PhantomTrance - 973 过载转质提升移速攻速") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{972, 1}, {973, 2}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.overload_speed_pct == Approx(0.20f));

  const auto *form = FindEffect(registry, player, BuffId::PhantomTranceForm);
  REQUIRE(form != nullptr);
  bool has_move = false;
  bool has_attack = false;
  for (const auto &mod : form->modifiers) {
    if (mod.type == StatType::MoveSpeed && mod.value == Approx(20.0f)) has_move = true;
    if (mod.type == StatType::AttackSpeed && mod.value == Approx(20.0f)) has_attack = true;
  }
  CHECK(has_move);
  CHECK(has_attack);
}

TEST_CASE("[Functional] PhantomTrance - 973 过载护盾为受击反击而非命中触发") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{972, 1}, {973, 2}});
  auto enemy = MakeEnemy(registry, 50.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  // 命中敌人不触发（旧实现错误地挂在了输出命中路径）。
  for (int i = 0; i < 10; ++i) {
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      player, enemy, kSkillId, Tag::Hit | Tag::Melee));
  }
  CHECK(FindAilment(registry, enemy, AilmentType::Shock) == nullptr);

  // 受击触发：概率从 mechanics 读取，按内置冷却节流；循环至首次成功。
  bool retaliated = false;
  for (int i = 0; i < 100 && !retaliated; ++i) {
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateTakeDamage(
                      player, enemy, 1u, Tag::Hit | Tag::Melee, 10.0f, false));
    if (FindAilment(registry, enemy, AilmentType::Shock) != nullptr) {
      retaliated = true;
    }
  }
  CHECK(retaliated);
}

TEST_CASE("[Functional] PhantomTrance - 982 孤注一掷按缺失生命提供暴击伤害") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  auto crit_after_cast = [](bool with_last_stand) {
    entt::registry registry;
    auto player = MakePlayer(
        registry,
        with_last_stand
            ? std::unordered_map<uint32_t, int>{{981, 1}, {982, 4}}
            : std::unordered_map<uint32_t, int>{{981, 1}});
    CastTrance(registry, player);
    auto *pt = registry.try_get<PhantomTranceComponent>(player);
    REQUIRE(pt != nullptr);
    auto &hp = registry.get<HealthComponent>(player);
    hp.current = hp.max * 0.5f; // 缺失 50%
    Tick(registry, player, 0.1f);
    registry.emplace_or_replace<StatsDirty>(player);
    StatsSystem::update(registry);
    return registry.get<CombatStats>(player).crit_damage;
  };

  const float baseline = crit_after_cast(false);
  const float with_stand = crit_after_cast(true);
  // rank4 = 每缺失 1% 提供 4 个百分点：50% 缺失 => +200 个百分点。
  // 引擎 PercentAdd 作用于基础暴伤倍率（1.5），故结果为 baseline * (1 + 200%)。
  CHECK(with_stand == Approx(baseline * 3.0f));

  // Buff 修正值本身也必须与缺失生命比例成正比。
  entt::registry registry;
  auto player = MakePlayer(registry, {{981, 1}, {982, 4}});
  CastTrance(registry, player);
  auto &hp = registry.get<HealthComponent>(player);
  hp.current = hp.max * 0.5f;
  Tick(registry, player, 0.1f);
  const auto *last = FindEffect(registry, player, BuffId::PhantomTranceLastStand);
  REQUIRE(last != nullptr);
  REQUIRE(!last->modifiers.empty());
  CHECK(last->modifiers[0].value == Approx(200.0f));
}

TEST_CASE("[Functional] PhantomTrance - 982 孤注一掷暴伤加成封顶至 200") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 返回指定配点与缺血比例下 982 写入的暴伤加成总量（百分点），
  // 并通过 crit_damage_out 回传结算后的暴击伤害倍率。
  auto last_stand_value = [](const std::unordered_map<uint32_t, int> &alloc,
                             float missing_frac, float &crit_damage_out) {
    entt::registry registry;
    auto player = MakePlayer(registry, alloc);
    CastTrance(registry, player);
    auto *pt = registry.try_get<PhantomTranceComponent>(player);
    REQUIRE(pt != nullptr);
    auto &hp = registry.get<HealthComponent>(player);
    hp.current = hp.max * (1.0f - missing_frac);
    Tick(registry, player, 0.1f);
    registry.emplace_or_replace<StatsDirty>(player);
    StatsSystem::update(registry);
    crit_damage_out = registry.get<CombatStats>(player).crit_damage;
    const auto *last =
        FindEffect(registry, player, BuffId::PhantomTranceLastStand);
    if (last == nullptr || last->modifiers.empty()) {
      return 0.0f;
    }
    return last->modifiers[0].value;
  };

  // 基准：仅 981、无 982 时不写入孤注一掷 Buff。
  float base_crit = 0.0f;
  CHECK(last_stand_value({{981, 1}}, 1.0f, base_crit) == Approx(0.0f));

  // rank4 满损血原始为 400 个百分点，须封顶到 +200。
  float crit_rank4 = 0.0f;
  const float value_rank4 =
      last_stand_value({{981, 1}, {982, 4}}, 1.0f, crit_rank4);
  CHECK(value_rank4 == Approx(200.0f));
  CHECK(crit_rank4 == Approx(base_crit * 3.0f));

  // rank1 满损血为 100 个百分点，低于上限，不受封顶影响。
  float crit_rank1 = 0.0f;
  const float value_rank1 =
      last_stand_value({{981, 1}, {982, 1}}, 1.0f, crit_rank1);
  CHECK(value_rank1 == Approx(100.0f));
  CHECK(crit_rank1 == Approx(base_crit * 2.0f));

  // 981 锁血压低生命上限后，缺血 75%（原始 300）仍封顶到 +200。
  float crit_locked = 0.0f;
  const float value_locked =
      last_stand_value({{981, 1}, {982, 4}}, 0.75f, crit_locked);
  CHECK(value_locked == Approx(200.0f));
  CHECK(crit_locked == Approx(base_crit * 3.0f));
}

TEST_CASE("[Functional] PhantomTrance - 987 意随神行每秒获取剑意") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{987, 2}});
  registry.emplace<SwordIntentComponent>(player).stacks = 0;
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.intent_per_sec == 2);

  Tick(registry, player, 1.0f);
  auto *intent = registry.try_get<SwordIntentComponent>(player);
  REQUIRE(intent != nullptr);
  CHECK(intent->stacks >= 2);
}

TEST_CASE("[Functional] PhantomTrance - 991 意念穿透附魔窗口减抗并按上限封顶") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{989, 1}, {991, 1}});
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.enchant_pen_per_intent_pct == Approx(0.01f));
  CHECK(pt->params.enchant_pen_cap_pct == Approx(40.0f));

  // 进入附魔窗口。
  Tick(registry, player, 3.5f);
  REQUIRE(registry.try_get<PhantomTranceComponent>(player) != nullptr);
  CHECK(pt->enchant_tag == Tag::Cold);

  // 剑意 5 层 → 减抗 5 点。
  registry.get_or_emplace<SwordIntentComponent>(player).stacks = 5;
  Tick(registry, player, 0.1f);
  const auto *shred = FindEffect(registry, enemy, BuffId::PhantomTranceEnchant);
  REQUIRE(shred != nullptr);
  REQUIRE_FALSE(shred->modifiers.empty());
  CHECK(shred->modifiers[0].value == Approx(-5.0f));

  // 剑意 100 层 → 减抗封顶 40 点。
  registry.get<SwordIntentComponent>(player).stacks = 100;
  Tick(registry, player, 0.1f);
  const auto *capped = FindEffect(registry, enemy, BuffId::PhantomTranceEnchant);
  REQUIRE(capped != nullptr);
  REQUIRE_FALSE(capped->modifiers.empty());
  CHECK(capped->modifiers[0].value == Approx(-40.0f));
}

TEST_CASE("[Functional] PhantomTrance - 992 灵气反哺以元素异常击杀刷新附魔窗口") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{989, 1}, {992, 1}});
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 1000.0f);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.enchant_refresh_on_kill);

  Tick(registry, player, 3.5f);
  REQUIRE(registry.try_get<PhantomTranceComponent>(player) != nullptr);
  REQUIRE(pt->enchant_remaining > 0.0f);
  pt->enchant_remaining = 1.0f;

  systems::AilmentApplyRequest chill;
  chill.ailment = AilmentType::Chill;
  chill.source = player;
  chill.magnitude = 20.0f;
  chill.duration = 3.0f;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, chill));

  CombatSystem::KillEnemy(registry, enemy, player, 0.0f, 100.0f, kSkillId);
  CHECK(pt->enchant_remaining == Approx(4.0f));
}

TEST_CASE("[Functional] PhantomTrance - 986 缩地成寸降低有效冷却") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{986, 1}});
  SkillSystem::RebakeSkillProfiles(registry, player);

  const auto *profile = SkillSystem::GetBakedSkillProfile(registry, player, kSkillId);
  REQUIRE(profile != nullptr);
  const auto *skill = SkillRegistry::Get().GetSkill(kSkillId);
  REQUIRE(skill != nullptr);
  const float expected = std::max(1.0f, skill->cooldown - 1.0f);
  CHECK(profile->effective_cooldown == Approx(expected));
}

TEST_CASE("[Functional] PhantomTrance - 985 破空一闪瞬移至光标") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{985, 1}});
  CastTrance(registry, player, {300.0f, 120.0f});

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.blink);
  const auto &pos = registry.get<Position>(player);
  CHECK(pos.x == Approx(300.0f));
  CHECK(pos.y == Approx(120.0f));
}

TEST_CASE("[Functional] PhantomTrance - 988 御剑化影在御剑步中叠加闪避") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{988, 2}});
  registry.emplace_or_replace<PhaseTag>(player);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.sword_step_dodge_pct == Approx(20.0f));

  const auto *form = FindEffect(registry, player, BuffId::PhantomTranceForm);
  REQUIRE(form != nullptr);
  float dodge_total = 0.0f;
  for (const auto &mod : form->modifiers) {
    if (mod.type == StatType::DodgeChance) dodge_total += mod.value;
  }
  CHECK(dodge_total == Approx(20.0f));
}

TEST_CASE("[Functional] PhantomTrance - 914 虚境馈赠回蓝") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{914, 2}});
  registry.get<CombatStats>(player).mana = 100.0f;
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.void_gift_mana_per_sec == Approx(4.0f));
  CHECK(pt->params.void_gift_dr_pct == Approx(0.20f));

  Tick(registry, player, 1.0f);
  CHECK(registry.get<CombatStats>(player).mana == Approx(104.0f));
}

TEST_CASE("[Functional] PhantomTrance - 978 浴血重生按已损失生命回复") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{977, 1}, {978, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK(pt->params.cheat_death_hold);
  CHECK(pt->params.rebirth_lost_pct == Approx(0.15f));

  // 致死伤害：免死保留 1 点生命，形态保留至自然到期。
  const bool died = CombatSystem::ApplyDamage(registry, player, 5000.0f, entt::null, false, false);
  CHECK_FALSE(died);
  CHECK(registry.get<HealthComponent>(player).current == Approx(1.0f));
  CHECK(pt->lethal_triggered);

  // 形态自然结束：按已损失生命 15% 回复。
  Tick(registry, player, 3.5f);
  CHECK(registry.get<HealthComponent>(player).current > 1.0f);
  CHECK(registry.get<HealthComponent>(player).current == Approx(150.85f).epsilon(0.05));
}

TEST_CASE("[Functional] PhantomTrance - 免死无 977 时形态立即结束") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry);
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  CHECK_FALSE(pt->params.cheat_death_hold);

  const bool died = CombatSystem::ApplyDamage(registry, player, 5000.0f, entt::null, false, false);
  CHECK_FALSE(died);
  CHECK(registry.get<HealthComponent>(player).current == Approx(1.0f));
  CHECK(pt->lethal_triggered);
  CHECK(pt->remaining == Approx(0.0f));

  // 无 977：下一帧组件被移除，不再保留形态。
  const bool alive = AdvanceTrance(registry, player, 0.1f);
  CHECK_FALSE(alive);
  CHECK(registry.try_get<PhantomTranceComponent>(player) == nullptr);
}

TEST_CASE("[Functional] PhantomTrance - 重施法不重置免死（RD-03）") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, {{981, 1}, {977, 1}});
  CastTrance(registry, player);

  auto *pt = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt != nullptr);
  REQUIRE(pt->remaining == Approx(3.0f));

  // 先消耗一次免死：977 使形态保留，lethal_triggered 置位。
  const bool died =
      CombatSystem::ApplyDamage(registry, player, 5000.0f, entt::null, false, false);
  CHECK_FALSE(died);
  CHECK(pt->lethal_triggered);

  // 人为消耗一部分免死窗口，用于验证重施法不会将其重置。
  auto *effects = registry.try_get<ActiveEffectsComponent>(player);
  REQUIRE(effects != nullptr);
  BuffEffect *seal = effects->Get(BuffId::PhantomTranceDeathSeal);
  REQUIRE(seal != nullptr);
  seal->remaining = 0.5f;

  // 形态中重施法：刷新形态时长，但保留免死状态与免死窗口。
  pt->remaining = 1.0f;
  CastTrance(registry, player);

  auto *pt_after = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt_after != nullptr);
  CHECK(pt_after->lethal_triggered);
  CHECK(pt_after->remaining == Approx(3.0f));
  const BuffEffect *seal_after =
      FindEffect(registry, player, BuffId::PhantomTranceDeathSeal);
  REQUIRE(seal_after != nullptr);
  CHECK(seal_after->remaining == Approx(0.5f));

  // 结束形态后再次进入：可重新武装免死并再次抵挡致死伤害。
  Tick(registry, player, 3.5f);
  CHECK(registry.try_get<PhantomTranceComponent>(player) == nullptr);

  CastTrance(registry, player);
  auto *pt_rearmed = registry.try_get<PhantomTranceComponent>(player);
  REQUIRE(pt_rearmed != nullptr);
  CHECK_FALSE(pt_rearmed->lethal_triggered);
  REQUIRE(FindEffect(registry, player, BuffId::PhantomTranceDeathSeal) != nullptr);

  const bool died_again =
      CombatSystem::ApplyDamage(registry, player, 5000.0f, entt::null, false, false);
  CHECK_FALSE(died_again);
  CHECK(pt_rearmed->lethal_triggered);
}

} // namespace NoMoreDay
