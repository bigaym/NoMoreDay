#include "doctest.h"

#include "TestCommon.hpp"
#include "game/combat_v2/CombatV2RuntimeFacade.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

#include <limits>

namespace {

NoMoreDay::CombatV2::CombatV2RuntimeRequest MakeCutoverRequest() {
    NoMoreDay::CombatV2::CombatV2RuntimeRequest request;
    request.damageRequest.skill_id = 42;
    request.damageRequest.dispatch_damage_events = false;
    request.damageRequest.is_simulation = true;
    request.damageRequest.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);
    request.scenarioClass = NoMoreDay::CombatV2::CombatV2ScenarioClass::HitFloat;
    request.cutoverModeEnabled = true;
    return request;
}

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

TEST_CASE("[Integration] CombatV2Cutover - v2-only path is active by default runtime mode") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    const auto result = facade.Execute(registry, MakeCutoverRequest());

    REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
    CHECK(result.mode == CV2::CombatV2RuntimeMode::CandidateOnly);
    CHECK_FALSE(result.primaryDamage.has_value());
    CHECK(result.candidateDamage.has_value());
}

TEST_CASE("[Integration] CombatV2Cutover - legacy runtime branches are unreachable in cutover mode") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    auto primaryOnly = MakeCutoverRequest();
    primaryOnly.mode = CV2::CombatV2RuntimeMode::PrimaryOnly;
    const auto primaryResult = facade.Execute(registry, primaryOnly);
    CHECK(primaryResult.status == CV2::CombatV2RuntimeStatus::InvalidInput);

    auto dualRun = MakeCutoverRequest();
    dualRun.mode = CV2::CombatV2RuntimeMode::DualRunCompare;
    const auto dualRunResult = facade.Execute(registry, dualRun);
    CHECK(dualRunResult.status == CV2::CombatV2RuntimeStatus::InvalidInput);
}

TEST_CASE("[Integration] CombatV2Cutover - facade rejects deprecated mode usage in cutover flow") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    auto request = MakeCutoverRequest();
    request.mode = CV2::CombatV2RuntimeMode::PrimaryOnly;

    const auto result = facade.Execute(registry, request);

    CHECK(result.status == CV2::CombatV2RuntimeStatus::InvalidInput);
    CHECK_FALSE(result.primaryDamage.has_value());
    CHECK_FALSE(result.mismatchReport.has_value());
}

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline routes through candidate runtime when cutover is enabled") {
    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    CHECK(result.total_damage == doctest::Approx(105.0f));
}

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline no longer preserves dual-run primary path") {
    TestSetupScope scope;

    entt::registry registry;
    NoMoreDay::DamageRequest request;
    request.skill_id = 42;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);

    const NoMoreDay::DamageResult result = NoMoreDay::DamagePipeline::Calculate(registry, request);

    CHECK(result.total_damage == doctest::Approx(105.0f));
}

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline returns zero when candidate request is invalid") {
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

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline falls back when candidate base pool is empty") {
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

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline Execute respects dispatch off in non-simulation cutover") {
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

    CHECK(execution.damage.total_damage == doctest::Approx(105.0f));
    CHECK(execution.final_applied_damage == doctest::Approx(105.0f));
    CHECK((hpBefore - hpAfter) == doctest::Approx(105.0f));
    CHECK(dealEvents.Count() == 0);
}

TEST_CASE("[Integration] CombatV2Cutover - DamagePipeline Execute dispatches events when enabled in non-simulation cutover") {
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

    CHECK(execution.damage.total_damage == doctest::Approx(105.0f));
    CHECK(execution.final_applied_damage == doctest::Approx(105.0f));
    CHECK(dealEvents.Count() == 1);
}
