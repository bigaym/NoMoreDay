#include "TestCommon.hpp"

#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/damage/DamageConditions.hpp"
#include "game/systems/combat/damage/DamageSnapshot.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"

#include <array>
#include <vector>

namespace NoMoreDay {
namespace {

using damage::ConditionalDamageOp;
using damage::OpStage;
using damage::TargetCondition;
using damage::TargetConditionState;
using damage::ToMask;

// 构造确定性的战斗属性：0 暴击保证单次结算数值唯一。
void PrepareDeterministicStats(CombatStats &stats) {
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
}

// 加载默认异常契约（与 AilmentEngineTests 一致）。
void RequireDefaultContracts() {
  auto &registry = systems::AilmentRegistry::Get();
  registry.ResetForTests();
  REQUIRE(registry.EnsureLoaded());
}

// 在指定注册表中应用一次毒异常并返回 tick 造成的扣血量。
float RunPoisonScenario(bool mutate_attacker_after_apply) {
  entt::registry registry;
  const auto attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(stats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 1000.0f, 1000.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);

  systems::AilmentApplyRequest request;
  request.ailment = AilmentType::Poison;
  request.source = attacker;
  request.magnitude = 12.0f;
  request.duration = 3.0f;
  request.stacks = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, target, request));

  // 施加后更改攻击者输出属性：快照已冻结，tick 不应受影响。
  if (mutate_attacker_after_apply) {
    stats.damage_multipliers[static_cast<int>(DamageType::Poison)] = 10.0f;
    stats.flat_damage[static_cast<int>(DamageType::Poison)] = 500.0f;
  }

  const float before = registry.get<HealthComponent>(target).current;
  systems::AilmentTickDriver::Tick(registry, 0.5f);
  const float after = registry.get<HealthComponent>(target).current;
  return before - after;
}

} // namespace

// P3-1：条件位掩码逐位求值（冻结/流血/HP>80/受控/命印/气印）。
TEST_CASE("[Unit] DamagePipeline P3 - target condition mask bits") {
  LoggerScope scope;
  entt::registry registry;

  const auto target = registry.create();
  registry.emplace<HealthComponent>(target, 90.0f, 100.0f);
  auto &effects = registry.emplace<ActiveEffectsComponent>(target);

  BuffEffect freeze;
  freeze.id = "Freeze";
  freeze.type = BuffType::Freeze;
  effects.effects.push_back(freeze);

  BuffEffect bleed;
  bleed.id = "Bleed";
  bleed.type = BuffType::Bleed;
  effects.effects.push_back(bleed);

  BuffEffect stun;
  stun.id = "Stun";
  stun.type = BuffType::Stun;
  effects.effects.push_back(stun);

  BuffEffect fate;
  fate.id = "FateMark";
  fate.kind = BuffKind::FateMark;
  fate.stacks = 3;
  effects.effects.push_back(fate);

  BuffEffect qi;
  qi.id = "QiBrand";
  qi.kind = BuffKind::QiBrand;
  qi.stacks = 2;
  effects.effects.push_back(qi);

  const TargetConditionState state =
      damage::EvaluateTargetConditionState(registry, target);
  CHECK((state.mask & ToMask(TargetCondition::Frozen)) != 0);
  CHECK((state.mask & ToMask(TargetCondition::Bleeding)) != 0);
  CHECK((state.mask & ToMask(TargetCondition::HpAbove80)) != 0);
  CHECK((state.mask & ToMask(TargetCondition::Controlled)) != 0);
  CHECK((state.mask & ToMask(TargetCondition::HasFateMark)) != 0);
  CHECK((state.mask & ToMask(TargetCondition::HasQiBrand)) != 0);
  CHECK(damage::StackCountFor(state, BuffKind::FateMark) == 3);
  CHECK(damage::StackCountFor(state, BuffKind::QiBrand) == 2);

  // 无效实体必须返回全零，不崩溃。
  CHECK(damage::EvaluateTargetConditionState(registry, entt::null).mask == 0u);
}

// P3-1：HP 阈值边界与 CombatStats 回退口径。
TEST_CASE("[Unit] DamagePipeline P3 - condition HP threshold boundary") {
  LoggerScope scope;
  entt::registry registry;

  const auto exactly_eighty = registry.create();
  registry.emplace<HealthComponent>(exactly_eighty, 80.0f, 100.0f);
  CHECK((damage::EvaluateTargetConditionState(registry, exactly_eighty).mask &
         ToMask(TargetCondition::HpAbove80)) == 0u);

  const auto above_eighty = registry.create();
  registry.emplace<HealthComponent>(above_eighty, 81.0f, 100.0f);
  CHECK((damage::EvaluateTargetConditionState(registry, above_eighty).mask &
         ToMask(TargetCondition::HpAbove80)) != 0u);

  // 无 HealthComponent 时回退 CombatStats。
  const auto stats_only = registry.create();
  auto &stats = registry.emplace<CombatStats>(stats_only);
  stats.health = 90.0f;
  stats.max_health = 100.0f;
  CHECK((damage::EvaluateTargetConditionState(registry, stats_only).mask &
         ToMask(TargetCondition::HpAbove80)) != 0u);
}

// P3-1：条件 More / CritDamage 的叠层数值与无匹配短路。
TEST_CASE("[Unit] DamagePipeline P3 - conditional op stacking") {
  ConditionalDamageOp frozen_more;
  frozen_more.required_condition = TargetCondition::Frozen;
  frozen_more.stage = OpStage::More;
  frozen_more.base_value = 0.5f;

  ConditionalDamageOp fate_more;
  fate_more.required_condition = TargetCondition::HasFateMark;
  fate_more.stage = OpStage::More;
  fate_more.per_stack_value = 0.03f;
  fate_more.stack_source = BuffKind::FateMark;

  ConditionalDamageOp controlled_crit;
  controlled_crit.required_condition = TargetCondition::Controlled;
  controlled_crit.stage = OpStage::CritDamage;
  controlled_crit.base_value = 0.15f;

  ConditionalDamageOp qi_crit;
  qi_crit.required_condition = TargetCondition::HasQiBrand;
  qi_crit.stage = OpStage::CritDamage;
  qi_crit.per_stack_value = 0.04f;
  qi_crit.stack_source = BuffKind::QiBrand;

  const std::array<ConditionalDamageOp, 4> ops = {frozen_more, fate_more,
                                                  controlled_crit, qi_crit};

  TargetConditionState state;
  state.mask = ToMask(TargetCondition::Frozen) |
               ToMask(TargetCondition::HasFateMark) |
               ToMask(TargetCondition::Controlled) |
               ToMask(TargetCondition::HasQiBrand);
  state.stacks[static_cast<uint32_t>(BuffKind::FateMark)] = 4;
  state.stacks[static_cast<uint32_t>(BuffKind::QiBrand)] = 5;

  CHECK(damage::ApplyConditionalMore(ops, state) ==
        doctest::Approx(1.5f * (1.0f + 0.03f * 4.0f)));
  CHECK(damage::ApplyConditionalCritDamage(ops, state) ==
        doctest::Approx(0.15f + 0.04f * 5.0f));

  // 空条件/无匹配：More 恒为 1，CritDamage 恒为 0。
  const TargetConditionState empty_state;
  CHECK(damage::ApplyConditionalMore(ops, empty_state) == doctest::Approx(1.0f));
  CHECK(damage::ApplyConditionalCritDamage(ops, empty_state) ==
        doctest::Approx(0.0f));
}

// P3-2：快照纳入 payload 暴击倍率覆盖（修 P2 单/批偏差）。
TEST_CASE("[Unit] DamagePipeline P3 - snapshot bakes payload crit multiplier") {
  LoggerScope scope;
  entt::registry registry;

  const auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(stats);
  stats.crit_chance = 1.0f;
  stats.crit_damage = 1.5f;

  DamagePayloadContext payload{};
  payload.base_damage_min = 100.0f;
  payload.base_damage_max = 100.0f;
  payload.crit_chance = 1.0f;
  payload.crit_multiplier = 3.0f;

  DamagePool pool;
  pool.Add(Tag::Physical, 100.0f);

  const damage::AttackerSnapshot snapshot = DamagePipeline::CreateSnapshot(
      registry, attacker, 0, pool, Tag::Hit, entt::null, &payload);
  CHECK(snapshot.crit_chance == doctest::Approx(1.0f));
  CHECK(snapshot.crit_damage == doctest::Approx(3.0f));
  CHECK(snapshot.base_damage[static_cast<int>(DamageType::Physical)] > 0.0f);
}

// P3-2：投射物命中优先消费组件快照，攻击者属性变化与 caster 移除均不影响。
TEST_CASE("[Unit] DamagePipeline P3 - projectile snapshot freezes attacker") {
  LoggerScope scope;
  entt::registry registry;

  const auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  auto &casterStats = registry.emplace<CombatStats>(caster);
  PrepareDeterministicStats(casterStats);

  const auto projectile = registry.create();
  registry.emplace<Position>(projectile, 0.0f, 0.0f);
  auto &projectileStats = registry.emplace<CombatStats>(projectile);
  PrepareDeterministicStats(projectileStats);

  const auto defender = registry.create();
  registry.emplace<Position>(defender, 5.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);
  auto &defenderStats = registry.emplace<CombatStats>(defender);
  PrepareDeterministicStats(defenderStats);

  DamagePool pool;
  pool.Add(Tag::Physical, 100.0f);
  DamagePipeline::AttachSnapshotComponent(
      registry, projectile, caster, 0, pool, Tag::Projectile | Tag::Hit,
      projectile, nullptr, DamageOrigin::DirectSkillCast);

  REQUIRE(registry.all_of<damage::DamageSnapshotComponent>(projectile));
  const auto &component =
      registry.get<damage::DamageSnapshotComponent>(projectile);
  CHECK(component.caster == caster);
  CHECK(component.skill_id == 0u);

  DamageRequest request;
  request.attacker = caster;
  request.defender = defender;
  request.skill_id = 0;
  request.additional_tags = Tag::Projectile | Tag::Hit;
  request.source_entity = projectile;

  const DamageResult baseline = DamagePipeline::Calculate(registry, request);
  CHECK(baseline.total_damage > 0.0f);

  // 攻击者输出属性大幅变化后，命中伤害必须保持不变（快照冻结）。
  casterStats.damage_multipliers[static_cast<int>(DamageType::Physical)] = 8.0f;
  casterStats.flat_damage[static_cast<int>(DamageType::Physical)] = 500.0f;
  const DamageResult after_mutation = DamagePipeline::Calculate(registry, request);
  CHECK(after_mutation.total_damage == doctest::Approx(baseline.total_damage));

  // caster 移除后，Holder 内的 POD 快照仍可独立结算。
  registry.destroy(caster);
  request.attacker = projectile;
  const DamageResult after_caster_death =
      DamagePipeline::Calculate(registry, request);
  CHECK(after_caster_death.total_damage == doctest::Approx(baseline.total_damage));
}

// B3：快照暴击/穿透在施法者销毁后仍按快照口径结算，不再回退实时属性。
TEST_CASE("[Unit] DamagePipeline P3 - snapshot crit and armor pen survive caster death") {
  LoggerScope scope;
  entt::registry registry;

  const auto caster = registry.create();
  registry.emplace<Position>(caster, 0.0f, 0.0f);
  auto &casterStats = registry.emplace<CombatStats>(caster);
  PrepareDeterministicStats(casterStats);
  casterStats.crit_chance = 0.0f; // 实时口径永不暴击
  casterStats.crit_damage = 2.0f;
  casterStats.armor_pen = 0.0f;   // 实时口径无穿透

  const auto defender = registry.create();
  registry.emplace<Position>(defender, 5.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100000.0f, 100000.0f);
  auto &defStats = registry.emplace<CombatStats>(defender);
  PrepareDeterministicStats(defStats);
  defStats.armor = 100.0f;
  defStats.dodge_chance = 0.0f;
  defStats.block_chance = 0.0f;

  // 手工构造三个冻结快照：暴击/穿透两组对照。
  auto make_projectile = [&](float crit_chance, float armor_pen) {
    const auto e = registry.create();
    registry.emplace<Position>(e, 0.0f, 0.0f);
    auto &stats = registry.emplace<CombatStats>(e);
    PrepareDeterministicStats(stats);
    damage::DamageSnapshotComponent component;
    component.snapshot.base_damage[0] = 100.0f; // Physical
    component.snapshot.crit_chance = crit_chance;
    component.snapshot.crit_damage = 2.0f;
    component.snapshot.armor_pen = armor_pen;
    component.snapshot.accuracy = 1.0f;
    component.snapshot.hit_tags = Tag::Projectile | Tag::Hit;
    component.caster = caster;
    component.skill_id = 0;
    registry.emplace<damage::DamageSnapshotComponent>(e, component);
    return e;
  };

  const auto crit_pen = make_projectile(1.0f, 50.0f);
  const auto no_crit_pen = make_projectile(0.0f, 50.0f);
  const auto no_crit_no_pen = make_projectile(0.0f, 0.0f);

  // 施法者销毁：快照必须独立结算（旧实现 attacker_stats==nullptr 且无 payload 时跳过暴击）。
  registry.destroy(caster);

  auto settle = [&](entt::entity projectile, bool skip_mitigation) {
    DamageRequest request;
    request.attacker = entt::null; // 施法者已消亡，无实时属性
    request.defender = defender;
    request.skill_id = 0;
    request.additional_tags = Tag::Projectile | Tag::Hit;
    request.source_entity = projectile;
    request.skip_mitigation = skip_mitigation;
    return DamagePipeline::Calculate(registry, request);
  };

  const DamageResult crit_pen_res = settle(crit_pen, false);
  const DamageResult no_crit_pen_res = settle(no_crit_pen, false);
  const DamageResult no_crit_no_pen_res = settle(no_crit_no_pen, false);

  CHECK(crit_pen_res.is_crit);
  CHECK_FALSE(no_crit_pen_res.is_crit);
  // 快照暴击倍率 ×2：暴击伤害与同穿透非暴击完全成比例。
  CHECK(crit_pen_res.total_damage ==
        doctest::Approx(2.0f * no_crit_pen_res.total_damage));
  // 快照穿透 50 使有效护甲减半，优于零穿透。
  CHECK(no_crit_pen_res.total_damage > no_crit_no_pen_res.total_damage);
}

// P3-2：ProjectileSystem 串行预扫描为投射物挂载快照组件。
TEST_CASE("[Unit] DamagePipeline P3 - projectile system attaches snapshot") {
  LoggerScope scope;
  entt::registry registry;

  const auto player = registry.create();
  auto &playerStats = registry.emplace<CombatStats>(player);
  PrepareDeterministicStats(playerStats);
  // 专精点挂在施法者身上：ProjectileSystem 必须把 owner 作为攻击者写入快照，
  // 否则 BuildConditionalOps 在投射物实体上取不到 150/172 等动态条件（B2）。
  ActiveSkillsComponent active;
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[150] = 3;
  registry.emplace<ActiveSkillsComponent>(player, active);

  const auto projectile = registry.create();
  registry.emplace<Position>(projectile, 0.0f, 0.0f);
  registry.emplace<Velocity>(projectile, 100.0f, 0.0f);
  auto &proj = registry.emplace<Projectile>(projectile);
  proj.owner = player;
  proj.speed = 100.0f;
  proj.lifeTime = 5.0f;
  proj.radius = 5.0f;
  proj.pierce = false;
  auto &projectileStats = registry.emplace<CombatStats>(projectile);
  PrepareDeterministicStats(projectileStats);

  auto &skill = registry.emplace<SkillComponent>(projectile);
  skill.skill_id = 1;
  skill.owner = player;

  systems::SpatialHashGrid grid(128, 128, 32.0f);
  grid.rebuild(registry.view<Position>(), registry);
  ProjectileSystem::Update(registry, grid, 0.016f);

  CHECK(registry.valid(projectile));
  REQUIRE(registry.all_of<damage::DamageSnapshotComponent>(projectile));
  const auto &component =
      registry.get<damage::DamageSnapshotComponent>(projectile);
  CHECK(component.skill_id == 1u);
  // 攻击者必须是施法者 owner，而非投射物自身。
  CHECK(component.caster == player);

  // 150 弱点专精（3 点 → CritDamage +0.45）只能通过 owner 的 ActiveSkills 取到。
  bool has_weakness_op = false;
  for (const auto &op : component.snapshot.conditional_ops) {
    if (op.stage == OpStage::CritDamage &&
        op.base_value == doctest::Approx(0.45f)) {
      has_weakness_op = true;
    }
  }
  CHECK(has_weakness_op);
}

// P3-2：DoT 在施加时冻结快照，施加后攻击者增益不影响 tick。
TEST_CASE("[Unit] DamagePipeline P3 - dot snapshot frozen at application") {
  TestSetupScope scope;
  RequireDefaultContracts();

  const float baseline = RunPoisonScenario(false);
  const float mutated = RunPoisonScenario(true);
  CHECK(baseline > 0.0f);
  CHECK(mutated == doctest::Approx(baseline));
}

// P3-2：caster 移除后 DoT 快照仍可结算，tick 造成伤害。
TEST_CASE("[Integration] DamagePipeline P3 - dot settles after caster death") {
  TestSetupScope scope;
  RequireDefaultContracts();

  entt::registry registry;
  const auto attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  PrepareDeterministicStats(stats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 1000.0f, 1000.0f);
  auto &targetStats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(targetStats);

  systems::AilmentApplyRequest request;
  request.ailment = AilmentType::Poison;
  request.source = attacker;
  request.magnitude = 12.0f;
  request.duration = 3.0f;
  request.stacks = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, target, request));

  entt::entity holder = entt::null;
  if (auto *effects = registry.try_get<ActiveEffectsComponent>(target)) {
    for (const auto &effect : effects->effects) {
      if (effect.snapshot_source != entt::null) {
        holder = effect.snapshot_source;
        break;
      }
    }
  }
  REQUIRE(holder != entt::null);
  REQUIRE(registry.all_of<damage::DamageSnapshotComponent>(holder));

  registry.destroy(attacker);

  const float before = registry.get<HealthComponent>(target).current;
  systems::AilmentTickDriver::Tick(registry, 0.5f);
  const float after = registry.get<HealthComponent>(target).current;
  CHECK((before - after) > 0.0f);
  CHECK(registry.valid(holder));
}

// P4 审查修复：投射物命中应以真实施法者（owner）为攻击者归因，投射物自身仅在
// 施法者不可用时兜底。投射物常带复制来的 CombatStats，若以自身归因会丢失
// 施法者的 ActiveSkills/GlobalModifier（专精点与条件 More 归零）。
// 说明：ProjectileSystem::Update 的串行预扫描在 owner 拥有 CombatStats 时总会
// 挂载快照，无法通过 Update 稳定构造"owner 有 CombatStats 但无快照"的回退分支；
// 此处以等效 Calculate 回归锁定归因差异：owner 攻击者生效、投射物攻击者归零。
TEST_CASE("[Unit] DamagePipeline P3 - hit attacker attribution prefers owner") {
  LoggerScope scope;
  entt::registry registry;

  const auto owner = registry.create();
  auto &owner_stats = registry.emplace<CombatStats>(owner);
  PrepareDeterministicStats(owner_stats);
  owner_stats.flat_damage[static_cast<int>(DamageType::Physical)] = 50.0f;

  const auto projectile = registry.create();
  auto &projectile_stats = registry.emplace<CombatStats>(projectile);
  PrepareDeterministicStats(projectile_stats);

  const auto target = registry.create();
  registry.emplace<Position>(target, 5.0f, 0.0f);
  registry.emplace<HealthComponent>(target, 1000.0f, 1000.0f);
  auto &target_stats = registry.emplace<CombatStats>(target);
  PrepareDeterministicStats(target_stats);

  DamagePool empty_pool; // 伤害仅由攻方 flat_damage 提供

  auto settle = [&](entt::entity attacker) {
    DamageRequest request;
    request.attacker = attacker;
    request.defender = target;
    request.skill_id = 0;
    request.base_pool = empty_pool;
    request.additional_tags = Tag::Projectile | Tag::Hit;
    // source_entity 指向无 DamageSnapshotComponent 的投射物，强制走攻击者归因。
    request.source_entity = projectile;
    request.skip_mitigation = true;
    return DamagePipeline::Calculate(registry, request);
  };

  const DamageResult owner_attributed = settle(owner);
  const DamageResult projectile_attributed = settle(projectile);
  CHECK(owner_attributed.total_damage == doctest::Approx(50.0f));
  CHECK(projectile_attributed.total_damage == doctest::Approx(0.0f));
}

} // namespace NoMoreDay
