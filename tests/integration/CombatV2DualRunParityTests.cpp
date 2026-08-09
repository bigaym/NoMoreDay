#include "doctest.h"

#include "game/foundation/combat_v2/CombatV2RuntimeFacade.hpp"

#include <limits>

namespace {

NoMoreDay::CombatV2::CombatV2RuntimeRequest
MakeRequest(NoMoreDay::CombatV2::CombatV2RuntimeMode mode,
            NoMoreDay::CombatV2::CombatV2ScenarioClass scenarioClass,
            float hitAbsTolerance = 0.0f, float hitRelTolerancePct = 0.0f) {
    NoMoreDay::CombatV2::CombatV2RuntimeRequest request;
    request.mode = mode;
    request.scenarioClass = scenarioClass;
    request.damageRequest.skill_id = 1;
    request.damageRequest.dispatch_damage_events = false;
    request.damageRequest.is_simulation = true;
    request.damageRequest.base_pool.Add(NoMoreDay::Tag::Physical, 100.0f);
    request.tolerance.hitFloatAbs = hitAbsTolerance;
    request.tolerance.hitFloatRelPct = hitRelTolerancePct;
    return request;
}

} // namespace

TEST_CASE("[Integration] CombatV2DualRunParity - LegacyOnly/V2Only/DualRunCompare mode behavior") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    SUBCASE("PrimaryOnly") {
        const auto result = facade.Execute(
            registry,
            MakeRequest(CV2::CombatV2RuntimeMode::PrimaryOnly, CV2::CombatV2ScenarioClass::HitFloat));
        REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
        CHECK(result.mode == CV2::CombatV2RuntimeMode::PrimaryOnly);
        CHECK(result.primaryDamage.has_value());
        CHECK_FALSE(result.candidateDamage.has_value());
    }

    SUBCASE("CandidateOnly") {
        const auto result = facade.Execute(
            registry,
            MakeRequest(CV2::CombatV2RuntimeMode::CandidateOnly, CV2::CombatV2ScenarioClass::HitFloat));
        REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
        CHECK(result.mode == CV2::CombatV2RuntimeMode::CandidateOnly);
        CHECK_FALSE(result.primaryDamage.has_value());
        CHECK(result.candidateDamage.has_value());
    }

    SUBCASE("DualRunCompare") {
        const auto result = facade.Execute(
            registry,
            MakeRequest(CV2::CombatV2RuntimeMode::DualRunCompare, CV2::CombatV2ScenarioClass::HitFloat));
        REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
        CHECK(result.mode == CV2::CombatV2RuntimeMode::DualRunCompare);
        CHECK(result.primaryDamage.has_value());
        CHECK(result.candidateDamage.has_value());
        CHECK(result.resolvedDamage.total_damage == doctest::Approx(result.primaryDamage->total_damage));
    }
}

TEST_CASE("[Integration] CombatV2DualRunParity - mismatch report includes abs/rel delta, stage mask, trace hashes") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    const auto result = facade.Execute(
        registry,
        MakeRequest(CV2::CombatV2RuntimeMode::DualRunCompare,
                    CV2::CombatV2ScenarioClass::ExactMatch,
                    0.0f,
                    0.0f));

    REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
    REQUIRE(result.mismatchReport.has_value());

    const auto &report = result.mismatchReport.value();
    CHECK(report.absoluteDelta > 0.0f);
    CHECK(report.relativeDeltaPct > 0.0f);
    CHECK(CV2::HasStage(report.stageMask, CV2::CombatV2ParityStageMask::Final));
    CHECK(report.skillId == 1u);
    CHECK(report.classification == CV2::CombatV2MismatchClass::Mismatch);
    CHECK(report.primaryTraceHash != 0ULL);
    CHECK(report.candidateTraceHash != 0ULL);
}

TEST_CASE("[Integration] CombatV2DualRunParity - tolerance policy gates mismatch reporting") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    auto tolerantRequest =
        MakeRequest(CV2::CombatV2RuntimeMode::DualRunCompare, CV2::CombatV2ScenarioClass::HitFloat, 10.0f, 1.0f);
    const auto result = facade.Execute(registry, tolerantRequest);

    REQUIRE(result.status == CV2::CombatV2RuntimeStatus::Ok);
    REQUIRE(result.mismatchReport.has_value());
    CHECK(result.mismatchReport->classification == CV2::CombatV2MismatchClass::ToleranceMatch);

    auto strictRequest =
        MakeRequest(CV2::CombatV2RuntimeMode::DualRunCompare, CV2::CombatV2ScenarioClass::HitFloat, 0.0f, 0.0f);
    const auto strictResult = facade.Execute(registry, strictRequest);
    REQUIRE(strictResult.status == CV2::CombatV2RuntimeStatus::Ok);
    REQUIRE(strictResult.mismatchReport.has_value());
    CHECK(strictResult.mismatchReport->classification == CV2::CombatV2MismatchClass::Mismatch);
}

TEST_CASE("[Integration] CombatV2DualRunParity - invalid mode and non-finite tolerance are rejected") {
    namespace CV2 = NoMoreDay::CombatV2;

    entt::registry registry;
    CV2::CombatV2RuntimeFacade facade;

    auto invalidMode =
        MakeRequest(CV2::CombatV2RuntimeMode::PrimaryOnly, CV2::CombatV2ScenarioClass::HitFloat, 0.0f, 0.0f);
    invalidMode.mode = static_cast<CV2::CombatV2RuntimeMode>(255);
    const auto invalidModeResult = facade.Execute(registry, invalidMode);
    CHECK(invalidModeResult.status == CV2::CombatV2RuntimeStatus::InvalidInput);

    auto invalidTolerance =
        MakeRequest(CV2::CombatV2RuntimeMode::DualRunCompare, CV2::CombatV2ScenarioClass::HitFloat, 0.0f, 0.0f);
    invalidTolerance.tolerance.hitFloatAbs = std::numeric_limits<float>::infinity();
    const auto invalidToleranceResult = facade.Execute(registry, invalidTolerance);
    CHECK(invalidToleranceResult.status == CV2::CombatV2RuntimeStatus::InvalidInput);
}
