#include "game/foundation/combat_v2/CombatV2RuntimeFacade.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace NoMoreDay::CombatV2 {

namespace {

[[nodiscard]] float SumDamagePool(const DamagePool &pool) {
    float total = 0.0f;
    for (const float value : pool.values) {
        total += value;
    }
    return total;
}

[[nodiscard]] DamageResult BuildPrimaryResult(const CombatV2RuntimeRequest &request) {
    DamageResult result;
    result.final_pool = request.damageRequest.base_pool;
    result.total_damage = SumDamagePool(result.final_pool);
    return result;
}

[[nodiscard]] DamageResult BuildCandidateResult(const CombatV2RuntimeRequest &request) {
    DamageResult result;
    result.final_pool = request.damageRequest.base_pool;
    result.total_damage = SumDamagePool(result.final_pool) * 1.05f;
    return result;
}

[[nodiscard]] bool WithinTolerance(const CombatV2RuntimeRequest &request,
                                   const float absoluteDelta,
                                   const float relativeDeltaPct) {
    switch (request.scenarioClass) {
    case CombatV2ScenarioClass::ExactMatch:
        return absoluteDelta <= request.tolerance.exactMatchAbs;
    case CombatV2ScenarioClass::HitFloat:
        return absoluteDelta <= request.tolerance.hitFloatAbs ||
               relativeDeltaPct <= request.tolerance.hitFloatRelPct;
    case CombatV2ScenarioClass::DotAggregate:
        return absoluteDelta <= request.tolerance.dotAggregateAbs ||
               relativeDeltaPct <= request.tolerance.dotAggregateRelPct;
    case CombatV2ScenarioClass::StatusDuration:
        return absoluteDelta <= request.tolerance.statusDurationAbsSeconds;
    }
    return false;
}

[[nodiscard]] uint64_t HashDamage(const DamageResult &damage,
                                  const uint32_t skillId,
                                  const uint8_t tag) {
    constexpr uint64_t kOffsetBasis = 1469598103934665603ULL;
    constexpr uint64_t kPrime = 1099511628211ULL;

    auto mix = [](uint64_t &acc, const uint64_t value) {
        acc ^= value;
        acc *= kPrime;
    };

    uint64_t hash = kOffsetBasis;
    mix(hash, static_cast<uint64_t>(skillId));
    mix(hash, static_cast<uint64_t>(tag));
    mix(hash, std::bit_cast<uint32_t>(damage.total_damage));
    for (const float value : damage.final_pool.values) {
        mix(hash, std::bit_cast<uint32_t>(value));
    }
    return hash;
}

} // namespace

CombatV2RuntimeResult
CombatV2RuntimeFacade::Execute(entt::registry &registry,
                               const CombatV2RuntimeRequest &request) const {
    (void)registry;

    for (const float value : request.damageRequest.base_pool.values) {
        if (!std::isfinite(value)) {
            CombatV2RuntimeResult invalid;
            invalid.status = CombatV2RuntimeStatus::InvalidInput;
            invalid.mode = request.mode;
            return invalid;
        }
    }

    const auto finiteAndNonNegative = [](const float value) {
        return std::isfinite(value) && value >= 0.0f;
    };
    if (!finiteAndNonNegative(request.tolerance.exactMatchAbs) ||
        !finiteAndNonNegative(request.tolerance.hitFloatAbs) ||
        !finiteAndNonNegative(request.tolerance.hitFloatRelPct) ||
        !finiteAndNonNegative(request.tolerance.dotAggregateAbs) ||
        !finiteAndNonNegative(request.tolerance.dotAggregateRelPct) ||
        !finiteAndNonNegative(request.tolerance.statusDurationAbsSeconds)) {
        CombatV2RuntimeResult invalid;
        invalid.status = CombatV2RuntimeStatus::InvalidInput;
        invalid.mode = request.mode;
        return invalid;
    }

    CombatV2RuntimeResult result;
    result.status = CombatV2RuntimeStatus::Ok;

    CombatV2RuntimeMode resolvedMode = request.mode;
    if (request.cutoverModeEnabled) {
        if (request.mode != CombatV2RuntimeMode::CandidateOnly) {
            result.status = CombatV2RuntimeStatus::InvalidInput;
            result.mode = request.mode;
            return result;
        }
        resolvedMode = CombatV2RuntimeMode::CandidateOnly;
    }
    result.mode = resolvedMode;

    switch (resolvedMode) {
    case CombatV2RuntimeMode::PrimaryOnly:
        result.primaryDamage = BuildPrimaryResult(request);
        result.resolvedDamage = result.primaryDamage.value();
        return result;
    case CombatV2RuntimeMode::CandidateOnly:
        result.candidateDamage = BuildCandidateResult(request);
        result.resolvedDamage = result.candidateDamage.value();
        return result;
    case CombatV2RuntimeMode::DualRunCompare: {
        const DamageResult primary = BuildPrimaryResult(request);
        const DamageResult candidate = BuildCandidateResult(request);
        result.primaryDamage = primary;
        result.candidateDamage = candidate;
        result.resolvedDamage = primary;

        const float absoluteDelta = std::fabs(primary.total_damage - candidate.total_damage);
        const float primaryAbs = std::max(std::fabs(primary.total_damage), 1.0e-6f);
        const float relativeDeltaPct = (absoluteDelta / primaryAbs) * 100.0f;
        const bool withinTolerance = WithinTolerance(request, absoluteDelta, relativeDeltaPct);

        if (absoluteDelta > 0.0f || relativeDeltaPct > 0.0f) {
            CombatV2DualRunMismatchReport report;
            report.absoluteDelta = absoluteDelta;
            report.relativeDeltaPct = relativeDeltaPct;
            report.stageMask = CombatV2ParityStageMask::Multipliers | CombatV2ParityStageMask::Final;
            report.skillId = request.damageRequest.skill_id;
            report.classification =
                withinTolerance ? CombatV2MismatchClass::ToleranceMatch : CombatV2MismatchClass::Mismatch;
            report.primaryTraceHash = HashDamage(primary, request.damageRequest.skill_id, 0u);
            report.candidateTraceHash = HashDamage(candidate, request.damageRequest.skill_id, 1u);
            result.mismatchReport = report;
        }
        break;
    }
    default:
        result.status = CombatV2RuntimeStatus::InvalidInput;
        return result;
    }

    return result;
}

} // namespace NoMoreDay::CombatV2
