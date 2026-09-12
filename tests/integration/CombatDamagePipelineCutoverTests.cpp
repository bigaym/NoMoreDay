#include "doctest.h"

#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

#include <limits>

namespace {

entt::entity CreateAttacker(entt::registry &registry) {
    const entt::entity attacker = registry.create();
    auto &stats = registry.emplace<NoMoreDay::CombatStats>(attacker);
    stats.cached_area_level = 1;
    registry.emplace<Position>(attacker, 0.0f, 0.0f);
    return attacker;
}

entt::entity CreateTarget(entt::registry &registry, float healthValue) {
    const entt::entity target = registry.create();
    registry.emplace<Position>(target, 0.0f, 0.0f);
    registry.emplace<HealthComponent>(target, healthValue, healthValue);
    auto &stats = registry.emplace<NoMoreDay::CombatStats>(target);
    stats.cached_area_level = 1;
    return target;
}

class ScopedDealDamageCounter {
  public:
    explicit ScopedDealDamageCounter(entt::entity expectedTarget) : m_expectedTarget(expectedTarget) {
        m_token = NoMoreDay::CombatEventDispatcher::Register(
            NoMoreDay::CombatEventType::OnDealDamage,
            [this](entt::registry &, const NoMoreDay::CombatEvent &event) {
                if (event.target == m_expectedTarget) {
                    ++m_count;
                }
            },
            2000);
    }

    ~ScopedDealDamageCounter() {
        NoMoreDay::CombatEventDispatcher::Unregister(NoMoreDay::CombatEventType::OnDealDamage, m_token);
    }

    [[nodiscard]] int Count() const { return m_count; }

  private:
    entt::entity m_expectedTarget{entt::null};
    uint32_t m_token{0};
    int m_count{0};
};

} // namespace

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline settles base pool directly without candidate runtime") {
    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    // 候选运行时已删除；无攻击者实体时属性乘区按 100% 基础处理，
    // base_pool 原值不再被乘区清零（B4 修复）。
    CHECK(result.total_damage == doctest::Approx(100.0f));
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline no longer preserves dual-run primary path") {
    TestSetupScope scope;

    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    // 无攻击者实体时属性乘区按 100% 基础处理，双跑主路径已彻底移除。
    CHECK(result.total_damage == doctest::Approx(100.0f));
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - HazardEnvironment keeps base pool without attacker") {
    TestSetupScope scope;

    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.origin = NoMoreDay::DamageOrigin::HazardEnvironment;
    request.attacker = entt::null;
    request.defender = entt::null;
    request.skill_id = 0;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.additional_tags = NoMoreDay::Tag::Hit;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    // 环境/陷阱/落石等无攻击者伤害必须保留基础池，不得因缺省乘区归零。
    CHECK(result.total_damage == doctest::Approx(100.0f));
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline returns zero when candidate request is invalid") {
    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.base_pool.Add(NoMoreDay::Tag::Physical,
                          std::numeric_limits<float>::quiet_NaN());

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    CHECK(result.total_damage == doctest::Approx(0.0f));
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline falls back when candidate base pool is empty") {
    TestSetupScope scope;

    entt::registry registry;
    const entt::entity attacker = CreateAttacker(registry);
    const entt::entity target = CreateTarget(registry, 500.0f);

    auto &attackerStats = registry.get<NoMoreDay::CombatStats>(attacker);
    attackerStats.min_weapon_damage = 40.0f;
    attackerStats.max_weapon_damage = 60.0f;
    attackerStats.flat_damage[0] = 50.0f;
    attackerStats.accuracy = 1.0f;

    auto &targetStats = registry.get<NoMoreDay::CombatStats>(target);
    targetStats.dodge_chance = 0.0f;

    NoMoreDay::DamageRequest request;
    request.attacker = attacker;
    request.defender = target;
    request.skill_id = 42;
    request.added_effectiveness = 1.0f;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.skip_mitigation = true;
    // Intentionally leave base_pool empty to verify fallback.

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    CHECK(result.total_damage > 0.0f);
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline Execute respects dispatch off in non-simulation cutover") {
    TestSetupScope scope;

    entt::registry registry;
    const entt::entity attacker = CreateAttacker(registry);
    const entt::entity target = CreateTarget(registry, 500.0f);

    NoMoreDay::DamageRequest request;
    request.attacker = attacker;
    request.defender = target;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.skip_mitigation = true;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    ScopedDealDamageCounter dealEvents(target);
    const float hpBefore = registry.get<HealthComponent>(target).current;
    const NoMoreDay::DamageExecutionResult execution = NoMoreDay::DamagePipeline::Execute(registry, request, attacker, false);
    const float hpAfter = registry.get<HealthComponent>(target).current;

    CHECK(execution.damage.total_damage == doctest::Approx(100.0f));
    CHECK(execution.final_applied_damage == doctest::Approx(100.0f));
    CHECK((hpBefore - hpAfter) == doctest::Approx(100.0f));
    CHECK(dealEvents.Count() == 0);
}

TEST_CASE("[Integration] CombatDamagePipelineCutover - DamagePipeline Execute dispatches events when enabled in non-simulation cutover") {
    TestSetupScope scope;

    entt::registry registry;
    const entt::entity attacker = CreateAttacker(registry);
    const entt::entity target = CreateTarget(registry, 500.0f);

    NoMoreDay::DamageRequest request;
    request.attacker = attacker;
    request.defender = target;
    request.skill_id = 42;
    request.dispatch_damage_events = true;
    request.is_simulation = false;
    request.skip_mitigation = true;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    ScopedDealDamageCounter dealEvents(target);
    const NoMoreDay::DamageExecutionResult execution = NoMoreDay::DamagePipeline::Execute(registry, request, attacker, false);

    CHECK(execution.damage.total_damage == doctest::Approx(100.0f));
    CHECK(execution.final_applied_damage == doctest::Approx(100.0f));
    CHECK(dealEvents.Count() == 1);
}
