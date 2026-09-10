#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/FlowingThrustComponents.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/FlowingThrust.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"

#include <cmath>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 1;
constexpr uint32_t kMomentumNode = 112;
constexpr uint32_t kPhantomShieldNode = 135;
constexpr uint32_t kBloodDrinkerNode = 153;
constexpr uint32_t kBoneDeepFrostNode = 173;

void LoadSkillMechanics() {
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json"));
}

void MakeDeterministic(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

entt::entity MakePlayer(entt::registry &registry, float x, float y,
                        const std::unordered_map<uint32_t, int> &nodes) {
  auto player = registry.create();
  registry.emplace<Position>(player, x, y);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 100.0f;
  stats.health = 100.0f;
  stats.max_mana = 100.0f;
  stats.mana = 100.0f;
  stats.min_weapon_damage = 40.0f;
  stats.max_weapon_damage = 40.0f;
  stats.effective_dexterity = 50.0f;
  MakeDeterministic(stats);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = kSkillId;
  active.specialized_slots[0].allocated_points = nodes;
  return player;
}

entt::entity MakeEnemy(entt::registry &registry, float x, float y, float health = 10000.0f) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<EnemyTag>(enemy);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = health;
  stats.health = health;
  MakeDeterministic(stats);
  registry.emplace<HealthComponent>(enemy, health, health);
  return enemy;
}

// 814 拔血流云：技能8 专精槽（另一槽位）点出 814。
constexpr uint32_t kBloodRipSkillId = 8;
constexpr uint32_t kBloodRipNode = 814;

void AddSkill8BloodRip(entt::registry &registry, entt::entity player) {
  auto &active = registry.get<ActiveSkillsComponent>(player);
  active.specialized_slots[1].skill_id = kBloodRipSkillId;
  active.specialized_slots[1].allocated_points[kBloodRipNode] = 1;
}

// 生成一把技能8、处于悬停顶点的回旋飞剑，radius 即滞空切割有效半径。
entt::entity MakeHoverBoomerang(entt::registry &registry, Vector2 apex,
                                float radius) {
  auto e = registry.create();
  auto &bc = registry.emplace<BoomerangComponent>(e);
  bc.skill_id = kBloodRipSkillId;
  bc.phase = BoomerangPhase::HoverApex;
  bc.apex_position = apex;
  registry.emplace<Projectile>(e).radius = radius;
  return e;
}

bool ApplyBleed(entt::registry &registry, entt::entity source,
                entt::entity target, float magnitude, float duration,
                int stacks) {
  systems::AilmentApplyRequest req;
  req.ailment = AilmentType::Bleed;
  req.source = source;
  req.magnitude = magnitude;
  req.duration = duration;
  req.stacks = stacks;
  return systems::AilmentApplier::Apply(registry, target, req);
}

bool VictimHasBleed(const entt::registry &registry, entt::entity entity) {
  const auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
  if (!effects) {
    return false;
  }
  for (const auto &b : effects->effects) {
    if ((b.type == BuffType::Bleed ||
         b.id.find("Bleed") != std::string::npos) &&
        b.remaining > 0.0f) {
      return true;
    }
  }
  return false;
}

// 153 饮血刃：流血 DoT tick 实际结算后，按 100% 治疗 DoT 施加者
TEST_CASE("[Unit] Skill - Flowing Thrust 153 Blood Drinker heals on bleed tick") {
  TestSetupScope scope;
  LoadSkillMechanics();
  CombatEventDispatcher::Init();

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  entt::registry registry;

  // 攻击者分配了 153（流云刺），血量不满（留出 ≥80 治疗空间避免 max_health 封顶）
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{kBloodDrinkerNode, 1}});
  auto *playerStats = registry.try_get<CombatStats>(player);
  playerStats->health = 20.0f;
  registry.get<HealthComponent>(player).current = 20.0f;

  // 敌人挂流血
  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100.0f);
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.source = player;
  bleed.magnitude = 10.0f;
  bleed.duration = 4.0f;
  bleed.stacks = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, bleed));

  const float playerBefore = playerStats->health;
  const float enemyBefore = registry.get<HealthComponent>(enemy).current;

  // 驱动 DoT tick（大 dt 确保至少 1 跳）
  systems::AilmentTickDriver::Tick(registry, 10.0f);

  const float healAmount = playerStats->health - playerBefore;
  const float enemyDamage = enemyBefore - registry.get<HealthComponent>(enemy).current;

  // 治疗量 = 实际流血伤害 × 100%
  CHECK(healAmount > 0.0f);
  CHECK(healAmount == doctest::Approx(enemyDamage));
  CHECK(healAmount <= 80.0f); // 不超过 max_health 余量与 tick 总量
  // 治疗事件派发
  // （OnHeal 事件本身不在本用例断言，避免依赖 dispatcher 订阅细节）
}

// 153 对照组：未分配 153 的施加者不因流血 tick 回血
TEST_CASE("[Unit] Skill - Flowing Thrust 153 requires allocated node") {
  TestSetupScope scope;
  LoadSkillMechanics();

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {}); // 未分配 153
  auto *playerStats = registry.try_get<CombatStats>(player);
  playerStats->health = 50.0f;
  registry.get<HealthComponent>(player).current = 50.0f;

  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100.0f);
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.source = player;
  bleed.magnitude = 10.0f;
  bleed.duration = 4.0f;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, bleed));

  systems::AilmentTickDriver::Tick(registry, 10.0f);

  CHECK(playerStats->health == doctest::Approx(50.0f)); // 不回血
}

// 112 势如破竹：133 传送距离 ×(1+10%×点数)，伤害 More 按实际位移 floor(距离/10)×2%
TEST_CASE("[Unit] Skill - Flowing Thrust 112 Momentum extends dash and scales damage") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{kMomentumNode, 1}});

  SkillExecution exec;
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(133 % 100); // 移形换位 Swap
  exec.active_nodes.set(kMomentumNode % 100); // 势如破竹

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);

  // 距离加成：100 × (1 + 0.10×1) = 110
  const auto &playerPos = registry.get<Position>(player);
  CHECK(playerPos.x == doctest::Approx(110.0f));
  CHECK(playerPos.y == doctest::Approx(0.0f));

  // 伤害 More：110/10=11 → (1 + 0.02×11) = 1.22
  bool foundExplosion = false;
  for (auto ent : registry.view<DirectStrikeComponent>()) {
    const auto &ds = registry.get<DirectStrikeComponent>(ent);
    if (ds.has_payload) {
      CHECK(ds.payload_context.more_damage == doctest::Approx(1.22f));
      foundExplosion = true;
    }
  }
  CHECK(foundExplosion);
}

// 112 对照组：未分配 112 时无距离加成、无位移 More
TEST_CASE("[Unit] Skill - Flowing Thrust 112 no bonus without allocation") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {});

  SkillExecution exec;
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(133 % 100); // 仅 Swap，不设 112

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);

  const auto &playerPos = registry.get<Position>(player);
  CHECK(playerPos.x == doctest::Approx(100.0f)); // 无距离加成

  bool foundExplosion = false;
  for (auto ent : registry.view<DirectStrikeComponent>()) {
    const auto &ds = registry.get<DirectStrikeComponent>(ent);
    if (ds.has_payload) {
      CHECK(ds.payload_context.more_damage == doctest::Approx(1.0f)); // 无位移 More
      foundExplosion = true;
    }
  }
  CHECK(foundExplosion);
}

// 135 虚实相生：离开残影判定区 → 获得敏捷比例临时护盾 (Ward)
TEST_CASE("[Unit] Skill - Flowing Thrust 135 Ward on leaving shadow zone") {
  TestSetupScope scope;
  LoadSkillMechanics();

  entt::registry registry;
  auto player = MakePlayer(registry, 10.0f, 0.0f, {{kPhantomShieldNode, 1}});

  // 残影位于 (80,0)，leave_radius=80：玩家 (10,0) 在区内
  auto shadow = registry.create();
  registry.emplace<Position>(shadow, 80.0f, 0.0f);
  registry.emplace<ShadowComponent>(shadow);
  auto &summon = registry.emplace<SummonComponent>(shadow);
  summon.owner = player;
  summon.skill_id = kSkillId;
  summon.archetype_id = SummonArchetype::ShadowEcho;

  // 阶段 1：在区内不触发
  skills::UpdateFlowingThrustPhantomShield(registry, 0.016f);
  REQUIRE_FALSE(registry.any_of<ActiveEffectsComponent>(player));
  const auto &state = registry.get<FlowingThrustStateComponent>(player);
  CHECK(state.last_in_shadow_zone_time > 0.0f); // 已记录在区内时刻

  // 阶段 2：离开区外 (200,0)（距残影 120 > 80）→ 触发 Ward
  registry.get<Position>(player).x = 200.0f;
  skills::UpdateFlowingThrustPhantomShield(registry, 0.016f);

  REQUIRE(registry.any_of<ActiveEffectsComponent>(player));
  const auto *ward = registry.get<ActiveEffectsComponent>(player).Get("PhantomShield");
  REQUIRE(ward != nullptr);
  CHECK(ward->type == BuffType::Shield);
  CHECK(ward->duration == doctest::Approx(3.0f));
  // buff 仅作展示，数值载体是 barrier 超额段（Ward Mode）
  CHECK(ward->modifiers.empty());

  // 数值断言：敏捷 50 × (2.0×1 点) = 100 直接计入 stats.barrier（可超额于 max_barrier），
  // 由 RegenerationSystem::Ward Mode 衰减、CombatSystem 承伤结算消耗
  const auto &stats = registry.get<CombatStats>(player);
  CHECK(stats.barrier == doctest::Approx(100.0f));
  REQUIRE(registry.any_of<BarrierComponent>(player));

  // 阶段 3：持续在区外不重复刷新触发（状态已复位）
  skills::UpdateFlowingThrustPhantomShield(registry, 0.016f);
  CHECK(registry.get<ActiveEffectsComponent>(player).Get("PhantomShield") != nullptr);
}

// 173 霜凝寒骨碎裂：命中冻结目标按几率对半径内所有敌人溅射
TEST_CASE("[Unit] Skill - Flowing Thrust 173 Shatter hits all enemies in radius") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  // 7 点 → 触发几率 105%，保证必碎
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{kBoneDeepFrostNode, 7}});

  // 冻结目标（暴击来源=玩家）
  auto victim = registry.create();
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<EnemyTag>(victim);
  auto &vicStats = registry.emplace<CombatStats>(victim);
  vicStats.max_health = 10000.0f;
  vicStats.health = 10000.0f;
  MakeDeterministic(vicStats);
  registry.emplace<HealthComponent>(victim, 10000.0f, 10000.0f);
  BuffEffect frozen{
      .id = "Frozen",
      .name = "Frozen",
      .type = BuffType::Freeze,
      .duration = 2.5f,
      .remaining = 2.5f,
      .is_debuff = true};
  registry.emplace<ActiveEffectsComponent>(victim).AddOrRefresh(frozen);
  registry.emplace<LastCritDamageComponent>(victim, 100.0f, player);

  // 半径 200 内两个敌人 + 半径外一个敌人
  auto near1 = MakeEnemy(registry, 50.0f, 0.0f);
  auto near2 = MakeEnemy(registry, 0.0f, 50.0f);
  auto far = MakeEnemy(registry, 300.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Cold, false);

  // 半径内敌人各受到一次溅射（扣血 > 0）
  CHECK(registry.get<HealthComponent>(near1).current < 10000.0f);
  CHECK(registry.get<HealthComponent>(near2).current < 10000.0f);
  // 半径外敌人不受影响
  CHECK(registry.get<HealthComponent>(far).current == doctest::Approx(10000.0f));
  // 冻结目标本身不被溅射
  CHECK(registry.get<HealthComponent>(victim).current == doctest::Approx(10000.0f));
}

// 814 拔血流云：流云刺命中悬停切割区内的流血敌人 → 拔除全部流血并结算物理真实爆发
TEST_CASE("[Unit] Skill - Flowing Thrust 814 Blood Rip consumes bleed in hover zone") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  entt::registry registry;
  auto player = MakePlayer(registry, 0.0f, 0.0f, {});
  AddSkill8BloodRip(registry, player);

  auto victim = MakeEnemy(registry, 100.0f, 0.0f, 10000.0f);
  // 悬停飞剑顶点与目标重合，目标处于有效半径内
  MakeHoverBoomerang(registry, {100.0f, 0.0f}, 40.0f);

  REQUIRE(ApplyBleed(registry, player, victim, 10.0f, 4.0f, 2));

  const float before = registry.get<HealthComponent>(victim).current;
  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Physical, false);

  // 爆发伤害已结算，且流血层数被完全拔除
  CHECK(registry.get<HealthComponent>(victim).current < before);
  CHECK_FALSE(VictimHasBleed(registry, victim));
}

// 814 对照组：不在悬停区或无流血时不爆发、不消耗
TEST_CASE("[Unit] Skill - Flowing Thrust 814 no burst outside zone or without bleed") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  // A：有流血但目标在悬停区外 → 不爆发、流血保留
  {
    entt::registry registry;
    auto player = MakePlayer(registry, 0.0f, 0.0f, {});
    AddSkill8BloodRip(registry, player);
    auto victim = MakeEnemy(registry, 100.0f, 0.0f, 10000.0f);
    MakeHoverBoomerang(registry, {500.0f, 0.0f}, 40.0f); // 悬停区在远处
    REQUIRE(ApplyBleed(registry, player, victim, 10.0f, 4.0f, 2));

    const float before = registry.get<HealthComponent>(victim).current;
    hitFunc(registry, player, victim, Tag::Physical, false);
    CHECK(registry.get<HealthComponent>(victim).current ==
          doctest::Approx(before));
    CHECK(VictimHasBleed(registry, victim));
  }

  // B：目标在悬停区内但无流血 → 不爆发
  {
    entt::registry registry;
    auto player = MakePlayer(registry, 0.0f, 0.0f, {});
    AddSkill8BloodRip(registry, player);
    auto victim = MakeEnemy(registry, 100.0f, 0.0f, 10000.0f);
    MakeHoverBoomerang(registry, {100.0f, 0.0f}, 40.0f);

    const float before = registry.get<HealthComponent>(victim).current;
    hitFunc(registry, player, victim, Tag::Physical, false);
    CHECK(registry.get<HealthComponent>(victim).current ==
          doctest::Approx(before));
  }
}

// 814：爆发伤害随被拔除的流血层数增加
TEST_CASE("[Unit] Skill - Flowing Thrust 814 burst scales with bleed stacks") {
  TestSetupScope scope;
  LoadSkillMechanics();
  SkillBehaviorRegistry::Initialize();

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  auto hitFunc = SkillBehaviorRegistry::GetHit(kSkillId);
  REQUIRE(hitFunc != nullptr);

  auto runBurst = [&](int stacks) {
    entt::registry registry;
    auto player = MakePlayer(registry, 0.0f, 0.0f, {});
    AddSkill8BloodRip(registry, player);
    auto victim = MakeEnemy(registry, 100.0f, 0.0f, 10000.0f);
    MakeHoverBoomerang(registry, {100.0f, 0.0f}, 40.0f);
    // Bleed 契约 RefreshPolicy::Independent：层数=独立实例数，逐层施加
    for (int i = 0; i < stacks; ++i) {
      REQUIRE(ApplyBleed(registry, player, victim, 10.0f, 4.0f, 1));
    }

    const float before = registry.get<HealthComponent>(victim).current;
    hitFunc(registry, player, victim, Tag::Physical, false);
    return before - registry.get<HealthComponent>(victim).current;
  };

  const float oneStack = runBurst(1);
  const float twoStacks = runBurst(2);
  CHECK(oneStack > 0.0f);
  CHECK(twoStacks > oneStack);
}

} // namespace
} // namespace NoMoreDay