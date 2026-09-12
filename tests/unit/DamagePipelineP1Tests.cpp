#include "TestCommon.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {
namespace {

// 构造一个带近战标签的基础伤害请求，供反击/重入用例复用。
DamageRequest MakeMeleeRequest(entt::entity attacker, entt::entity defender,
                               float base_damage) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool.Add(Tag::Physical, base_damage);
  request.additional_tags = Tag::Melee;
  return request;
}

// 注册与生产钩子语义一致的结算钩子：Execute 落地伤害，calculateBatch 返回
// 逐目标真实结果（P1-1 契约）。
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

} // namespace

// P1-1 回归：批量钩子必须返回真实批量结果，且与逐目标 Calculate 一致、非空。
TEST_CASE("[Unit] DamagePipeline P1 - Batch hook returns real results") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(defender);

  RegisterTestResolutionHooks();

  DamagePool pool;
  pool.Add(Tag::Physical, 120.0f);

  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = 0;
  request.base_pool = pool;
  request.additional_tags = Tag::Melee;
  request.is_simulation = true;

  const std::vector<DamageResult> batch = ResolveDamageBatch(registry, request);
  const DamageResult single = DamagePipeline::Calculate(registry, request);

  // B2/B3: 旧实现恒返回空 vector，这里必须非空。
  CHECK_FALSE(batch.empty());
  CHECK(batch.size() == 1);
  CHECK(batch.front().total_damage > 0.0f);
  CHECK(batch.front().total_damage == doctest::Approx(single.total_damage));
}

// P1-3 回归：Blade Ward 反击在 Execute 收尾结算，而非同步递归。
TEST_CASE("[Unit] DamagePipeline P1 - Counter settles after Execute") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  // 反击以剑阵持有者(防御方)的暴击属性结算，锁定 0 暴击保证数值确定。
  auto &defender_stats = registry.emplace<CombatStats>(defender);
  defender_stats.crit_chance = 0.0f;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.trigger_counter = true;
  ward.sword_count = 3;

  DamageRequest request = MakeMeleeRequest(attacker, defender, 30.0f);
  request.dispatch_damage_events = false;

  const DamageExecutionResult execution =
      DamagePipeline::Execute(registry, request, attacker, false);

  CHECK(execution.damage.total_damage == doctest::Approx(30.0f));
  CHECK(registry.get<HealthComponent>(defender).current ==
        doctest::Approx(70.0f));
  // 反击在 Execute 收尾派发：攻击者应从满血降到 100-35=65。
  CHECK(registry.get<HealthComponent>(attacker).current ==
        doctest::Approx(65.0f));
}

// P1-3 回归：反击位于结算尾段——OnTakeDamage 事件派发时反击尚未落地。
TEST_CASE("[Unit] DamagePipeline P1 - Counter deferred to settlement tail") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  auto &defender_stats = registry.emplace<CombatStats>(defender);
  defender_stats.crit_chance = 0.0f;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.trigger_counter = true;
  ward.sword_count = 3;

  float attacker_hp_at_take_damage = -1.0f;
  const uint32_t listener_id = CombatEventDispatcher::Register(
      CombatEventType::OnTakeDamage,
      [&](entt::registry &reg, const CombatEvent &event) {
        // OnTakeDamage: source=受击方, target=攻击方。
        if (event.source == defender && attacker_hp_at_take_damage < 0.0f) {
          attacker_hp_at_take_damage =
              reg.get<HealthComponent>(attacker).current;
        }
      });
  REQUIRE(listener_id != 0);

  DamageRequest request = MakeMeleeRequest(attacker, defender, 30.0f);
  request.dispatch_damage_events = true;
  DamagePipeline::Execute(registry, request, attacker, false);

  CombatEventDispatcher::Unregister(CombatEventType::OnTakeDamage, listener_id);

  // 事件派发发生在 Execute 返回前，此时反击尚未结算。
  CHECK(attacker_hp_at_take_damage == doctest::Approx(100.0f));
  // Execute 收尾后反击才真正落地。
  CHECK(registry.get<HealthComponent>(attacker).current ==
        doctest::Approx(65.0f));
}

// P1-3 回归：结算中重入被排队，超深动作被丢弃并告警。
TEST_CASE("[Unit] DamagePipeline P1 - Reentrancy depth limit drops nested "
          "counter") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  auto &defender_stats = registry.emplace<CombatStats>(defender);
  defender_stats.crit_chance = 0.0f;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.trigger_counter = true;
  ward.sword_count = 3;

  // 第二防御方：受击事件监听器在结算中途重入结算，其反击位于深度上限处。
  auto second_defender = registry.create();
  registry.emplace<Position>(second_defender, 20.0f, 0.0f);
  registry.emplace<HealthComponent>(second_defender, 100.0f, 100.0f);
  auto &second_stats = registry.emplace<CombatStats>(second_defender);
  second_stats.crit_chance = 0.0f;
  auto &second_ward = registry.emplace<BladeWardComponent>(second_defender);
  second_ward.trigger_counter = true;
  second_ward.sword_count = 3;

  bool reentered = false;
  const uint32_t listener_id = CombatEventDispatcher::Register(
      CombatEventType::OnTakeDamage,
      [&](entt::registry &reg, const CombatEvent &event) {
        // OnTakeDamage: source=受击方, target=攻击方。
        if (event.source != defender || reentered) {
          return;
        }
        reentered = true;
        DamageRequest nested =
            MakeMeleeRequest(attacker, second_defender, 30.0f);
        nested.dispatch_damage_events = false;
        (void)ResolveDamage(reg, nested, attacker);
      });
  REQUIRE(listener_id != 0);

  DamageRequest request = MakeMeleeRequest(attacker, defender, 30.0f);
  request.dispatch_damage_events = true;
  DamagePipeline::Execute(registry, request, attacker, false);

  CombatEventDispatcher::Unregister(CombatEventType::OnTakeDamage, listener_id);

  // 第二防御方在中途被同步结算（重入确实发生）。
  CHECK(registry.get<HealthComponent>(second_defender).current ==
        doctest::Approx(70.0f));
  // 其反击在深度上限处被丢弃，攻击者只承受一次(外层)反击 35。
  CHECK(registry.get<HealthComponent>(attacker).current ==
        doctest::Approx(65.0f));
}

// P1-3 回归：裸 Calculate 无收尾帧，反击动作被丢弃（有意的时序变更）。
TEST_CASE("[Unit] DamagePipeline P1 - Bare Calculate drops counter") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(attacker, 100.0f, 100.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  auto &defender_stats = registry.emplace<CombatStats>(defender);
  defender_stats.crit_chance = 0.0f;
  auto &ward = registry.emplace<BladeWardComponent>(defender);
  ward.trigger_counter = true;
  ward.sword_count = 3;

  DamagePool pool;
  pool.Add(Tag::Physical, 30.0f);
  const DamageResult result =
      DamagePipeline::Calculate(registry, attacker, defender, 0, pool,
                                Tag::Melee, entt::null);
  CHECK(result.total_damage == doctest::Approx(30.0f));
  // Calculate 不落地伤害，也不持有结算帧，反击无处派发。
  CHECK(registry.get<HealthComponent>(defender).current ==
        doctest::Approx(100.0f));
  CHECK(registry.get<HealthComponent>(attacker).current ==
        doctest::Approx(100.0f));
}

// T1.2/T3.3 回归：990 冰增幅在单目标 Calculate 与生产批量链路
// (ResolveDamageBatch -> CalculateBatchResults -> 逐目标 Calculate) 上口径一致，
// 且仅冻结目标享受增伤，避免批量路径与单目标路径出现口径分叉。
TEST_CASE(
    "[Unit] DamagePipeline P1 - Frost amp consistent across single and batch") {
  LoggerScope scope;
  entt::registry registry;

  // 攻击方：冰霜附魔窗口 + 35% 冰增幅。
  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;
  auto &trance = registry.emplace<PhantomTranceComponent>(attacker);
  trance.enchant_tag = Tag::Cold;
  trance.enchant_remaining = 1.0f;
  trance.params.frost_amp_pct = 0.35f;

  const auto make_defender = [&](float x, bool frozen) {
    auto defender = registry.create();
    registry.emplace<Position>(defender, x, 0.0f);
    registry.emplace<HealthComponent>(defender, 1000.0f, 1000.0f);
    registry.emplace<CombatStats>(defender);
    auto &effects = registry.emplace<ActiveEffectsComponent>(defender);
    if (frozen) {
      BuffEffect freeze;
      freeze.id = "Freeze";
      freeze.type = BuffType::Freeze;
      freeze.duration = 1.0f;
      // must be > 0，否则 990 判定会跳过该效果。
      freeze.remaining = 1.0f;
      effects.effects.push_back(freeze);
    }
    return defender;
  };

  const auto frozen = make_defender(10.0f, true);
  const auto normal = make_defender(20.0f, false);

  RegisterTestResolutionHooks();

  const auto make_request = [&](entt::entity defender) {
    DamageRequest request;
    request.attacker = attacker;
    request.defender = defender;
    request.skill_id = 0;
    request.base_pool.Add(Tag::Physical, 120.0f);
    request.additional_tags = Tag::Melee;
    request.is_simulation = true;
    return request;
  };
  const DamageRequest frozen_request = make_request(frozen);
  const DamageRequest normal_request = make_request(normal);

  const DamageResult frozen_single =
      DamagePipeline::Calculate(registry, frozen_request);
  const DamageResult normal_single =
      DamagePipeline::Calculate(registry, normal_request);
  const std::vector<DamageResult> frozen_batch =
      ResolveDamageBatch(registry, frozen_request);
  const std::vector<DamageResult> normal_batch =
      ResolveDamageBatch(registry, normal_request);

  // 生产批量链路必须逐目标返回真实结果，并与单目标 Calculate 完全一致。
  REQUIRE(frozen_batch.size() == 1);
  REQUIRE(normal_batch.size() == 1);
  CHECK(frozen_batch.front().total_damage ==
        doctest::Approx(frozen_single.total_damage));
  CHECK(normal_batch.front().total_damage ==
        doctest::Approx(normal_single.total_damage));

  // 冻结目标吃 1.35 倍；若非冻结目标也被误增伤，比例将退回 1.0 而失败。
  CHECK(frozen_single.total_damage ==
        doctest::Approx(normal_single.total_damage * 1.35f));
  CHECK(normal_single.total_damage > 0.0f);
  CHECK(frozen_single.total_damage > normal_single.total_damage);
}

} // namespace NoMoreDay
