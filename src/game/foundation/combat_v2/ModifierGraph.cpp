#include "ModifierGraph.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace NoMoreDay::CombatV2 {

namespace {

[[nodiscard]] bool HasAnyForbiddenFilter(const CompiledModifierNode &candidate,
                                         const std::vector<uint32_t> &activeForbiddenFilters) {
    for (const uint32_t filterId : candidate.forbiddenFilterIds) {
        if (std::find(activeForbiddenFilters.begin(), activeForbiddenFilters.end(), filterId) !=
            activeForbiddenFilters.end()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] size_t StageToIndex(ModifierStage stage) {
    return static_cast<size_t>(stage);
}

} // namespace

ModifierGraphCompileResult ModifierGraphCompiler::Compile(const ModifierGraphBuildRequest &request) const {
    if (request.candidates == nullptr || request.activeForbiddenFilters == nullptr) {
        return ModifierGraphCompileResult{ModifierGraphCompileStatus::InvalidInput, {}};
    }

    ModifierGraph graph;
    for (const CompiledModifierNode &candidate : *request.candidates) {
        if (!std::isfinite(candidate.node.value)) {
            return ModifierGraphCompileResult{ModifierGraphCompileStatus::InvalidInput, {}};
        }
        if (HasAnyForbiddenFilter(candidate, *request.activeForbiddenFilters)) {
            continue;
        }
        const size_t stageIndex = StageToIndex(candidate.node.stage);
        if (stageIndex >= graph.stageBuckets.size()) {
            return ModifierGraphCompileResult{ModifierGraphCompileStatus::InvalidInput, {}};
        }
        graph.stageBuckets[stageIndex].push_back(candidate);
    }

    for (std::vector<CompiledModifierNode> &bucket : graph.stageBuckets) {
        std::sort(bucket.begin(), bucket.end(), [](const CompiledModifierNode &lhs, const CompiledModifierNode &rhs) {
            const auto lhsPrimary =
                std::tie(lhs.node.priority, lhs.node.sourceId, lhs.node.nodeId, lhs.node.conditionProgramId, lhs.node.op);
            const auto rhsPrimary =
                std::tie(rhs.node.priority, rhs.node.sourceId, rhs.node.nodeId, rhs.node.conditionProgramId, rhs.node.op);
            if (lhsPrimary != rhsPrimary) {
                return lhsPrimary < rhsPrimary;
            }
            if (lhs.node.value != rhs.node.value) {
                return lhs.node.value < rhs.node.value;
            }
            if (lhs.forbiddenFilterIds != rhs.forbiddenFilterIds) {
                return lhs.forbiddenFilterIds < rhs.forbiddenFilterIds;
            }
            return lhs.nodeWhitelist < rhs.nodeWhitelist;
        });
    }

    return ModifierGraphCompileResult{ModifierGraphCompileStatus::Ok, graph};
}

} // namespace NoMoreDay::CombatV2
