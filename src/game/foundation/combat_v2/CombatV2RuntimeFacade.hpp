#pragma once

#include "game/systems/combat/DamagePipeline.hpp"

#include <cstdint>
#include <optional>

namespace NoMoreDay::CombatV2 {

enum class CombatV2RuntimeMode : uint8_t {
    PrimaryOnly = 0,
    CandidateOnly = 1,
    DualRunCompare = 2,
};

enum class CombatV2RuntimeStatus : uint8_t {
    Ok = 0,
    InvalidInput = 1,
    NotImplemented = 2,
};

enum class CombatV2ScenarioClass : uint8_t {
    ExactMatch = 0,
    HitFloat = 1,
    DotAggregate = 2,
    StatusDuration = 3,
};

enum class CombatV2ParityStageMask : uint32_t {
    None = 0,
    Base = 1u << 0,
    Conversion = 1u << 1,
    Multipliers = 1u << 2,
    Mitigation = 1u << 3,
    Final = 1u << 4,
};

[[nodiscard]] constexpr uint32_t ToStageBits(CombatV2ParityStageMask stageMask) {
    return static_cast<uint32_t>(stageMask);
}

[[nodiscard]] constexpr CombatV2ParityStageMask
operator|(CombatV2ParityStageMask lhs, CombatV2ParityStageMask rhs) {
    return static_cast<CombatV2ParityStageMask>(ToStageBits(lhs) | ToStageBits(rhs));
}

[[nodiscard]] constexpr bool HasStage(CombatV2ParityStageMask mask, CombatV2ParityStageMask bit) {
    return (ToStageBits(mask) & ToStageBits(bit)) != 0u;
}

enum class CombatV2MismatchClass : uint8_t {
    Match = 0,
    ToleranceMatch = 1,
    Mismatch = 2,
};

struct CombatV2TolerancePolicy {
    float exactMatchAbs = 0.0f;
    float hitFloatAbs = 1.0e-4f;
    float hitFloatRelPct = 0.1f;
    float dotAggregateAbs = 1.0e-3f;
    float dotAggregateRelPct = 0.5f;
    float statusDurationAbsSeconds = 1.0e-4f;
};

struct CombatV2DualRunMismatchReport {
    float absoluteDelta = 0.0f;
    float relativeDeltaPct = 0.0f;
    CombatV2ParityStageMask stageMask = CombatV2ParityStageMask::None;
    uint32_t skillId = 0;
    CombatV2MismatchClass classification = CombatV2MismatchClass::Mismatch;
    uint64_t primaryTraceHash = 0;
    uint64_t candidateTraceHash = 0;
};

struct CombatV2RuntimeRequest {
    DamageRequest damageRequest{};
    CombatV2RuntimeMode mode = CombatV2RuntimeMode::CandidateOnly;
    CombatV2ScenarioClass scenarioClass = CombatV2ScenarioClass::HitFloat;
    CombatV2TolerancePolicy tolerance{};
    bool cutoverModeEnabled = false;
};

struct CombatV2RuntimeResult {
    CombatV2RuntimeStatus status = CombatV2RuntimeStatus::NotImplemented;
    CombatV2RuntimeMode mode = CombatV2RuntimeMode::CandidateOnly;
    DamageResult resolvedDamage{};
    std::optional<DamageResult> primaryDamage{};
    std::optional<DamageResult> candidateDamage{};
    std::optional<CombatV2DualRunMismatchReport> mismatchReport{};
};

class CombatV2RuntimeFacade {
  public:
    [[nodiscard]] CombatV2RuntimeResult Execute(entt::registry &registry,
                                                const CombatV2RuntimeRequest &request) const;
};

} // namespace NoMoreDay::CombatV2
