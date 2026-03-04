#pragma once

#include "ModifierGraph.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay::CombatV2 {

enum class ModifierSourceKind : uint8_t {
    Equipment = 0,
    Talent = 1,
    SkillSpec = 2,
    Global = 3,
};

struct SourceModifierRecord {
    ModifierSourceKind sourceKind{ModifierSourceKind::Global};
    uint32_t sourceId{0};
    uint32_t semanticId{0};
    ModifierStage stage{ModifierStage::PreHit};
    ModifierOp op{ModifierOp::Flat};
    float value{0.0f};
    uint32_t conditionProgramId{0};
    uint16_t priority{0};
    std::vector<uint32_t> forbiddenFilterIds{};
    std::vector<uint32_t> nodeWhitelist{};
};

enum class SourceNormalizeStatus : uint8_t {
    Ok = 0,
    InvalidInput = 1,
    NotImplemented = 2,
};

struct SourceNormalizeResult {
    SourceNormalizeStatus status{SourceNormalizeStatus::NotImplemented};
    CompiledModifierNode normalized{};
};

class ModifierSourceAdapters {
  public:
    [[nodiscard]] SourceNormalizeResult Normalize(const SourceModifierRecord &sourceRecord) const;
};

} // namespace NoMoreDay::CombatV2
