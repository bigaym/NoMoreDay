#include "ModifierSourceAdapters.hpp"

#include <cmath>

namespace NoMoreDay::CombatV2 {

SourceNormalizeResult ModifierSourceAdapters::Normalize(const SourceModifierRecord &sourceRecord) const {
    if (sourceRecord.semanticId == 0 || !std::isfinite(sourceRecord.value)) {
        return SourceNormalizeResult{SourceNormalizeStatus::InvalidInput, {}};
    }

    CompiledModifierNode normalized;
    normalized.node.nodeId = sourceRecord.semanticId;
    normalized.node.stage = sourceRecord.stage;
    normalized.node.op = sourceRecord.op;
    normalized.node.value = sourceRecord.value;
    normalized.node.conditionProgramId = sourceRecord.conditionProgramId;
    normalized.node.priority = sourceRecord.priority;
    normalized.node.sourceId = sourceRecord.sourceId;
    normalized.forbiddenFilterIds = sourceRecord.forbiddenFilterIds;
    normalized.nodeWhitelist = sourceRecord.nodeWhitelist;

    return SourceNormalizeResult{SourceNormalizeStatus::Ok, normalized};
}

} // namespace NoMoreDay::CombatV2
