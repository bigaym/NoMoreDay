#include "ConditionIR.hpp"

#include <functional>
#include <vector>

namespace NoMoreDay::CombatV2 {

ConditionEvaluateResult ConditionEvaluator::Evaluate(const ConditionIR &conditionIr, const TagBitset &ownedTags) const {
    if (conditionIr.nodes.empty() || conditionIr.rootNodeIndex >= conditionIr.nodes.size()) {
        return ConditionEvaluateResult{ConditionEvaluateStatus::InvalidIR, false};
    }

    enum class VisitState : uint8_t {
        Unvisited = 0,
        Visiting = 1,
        Evaluated = 2,
    };

    std::vector<VisitState> visitStates(conditionIr.nodes.size(), VisitState::Unvisited);
    std::vector<bool> evaluatedMatches(conditionIr.nodes.size(), false);
    bool invalidIr = false;

    const auto isNodeShapeValid = [&conditionIr](const ConditionNode &node) {
        switch (node.op) {
        case ConditionNodeOp::All:
        case ConditionNodeOp::Any:
        case ConditionNodeOp::None:
            return !node.childIndices.empty() && node.tagIds.empty();
        case ConditionNodeOp::Not:
            return node.childIndices.size() == 1 && node.tagIds.empty();
        case ConditionNodeOp::HasTagsAll:
        case ConditionNodeOp::HasTagsAny:
            if (!node.childIndices.empty() || node.tagIds.empty()) {
                return false;
            }
            for (const ConditionTagId tagId : node.tagIds) {
                if (!TagBitset::IsValidTagId(tagId)) {
                    return false;
                }
            }
            return true;
        default:
            return false;
        }
    };

    std::function<bool(uint32_t)> evaluateNode;
    evaluateNode = [&](const uint32_t nodeIndex) -> bool {
        if (nodeIndex >= conditionIr.nodes.size()) {
            invalidIr = true;
            return false;
        }

        const VisitState state = visitStates[nodeIndex];
        if (state == VisitState::Evaluated) {
            return static_cast<bool>(evaluatedMatches[nodeIndex]);
        }
        if (state == VisitState::Visiting) {
            invalidIr = true;
            return false;
        }

        const ConditionNode &node = conditionIr.nodes[nodeIndex];
        if (!isNodeShapeValid(node)) {
            invalidIr = true;
            return false;
        }

        visitStates[nodeIndex] = VisitState::Visiting;

        bool matched = false;
        switch (node.op) {
        case ConditionNodeOp::All:
            matched = true;
            for (const uint32_t childIndex : node.childIndices) {
                const bool childMatched = evaluateNode(childIndex);
                if (invalidIr) {
                    return false;
                }
                if (!childMatched) {
                    matched = false;
                }
            }
            break;
        case ConditionNodeOp::Any:
            matched = false;
            for (const uint32_t childIndex : node.childIndices) {
                const bool childMatched = evaluateNode(childIndex);
                if (invalidIr) {
                    return false;
                }
                if (childMatched) {
                    matched = true;
                }
            }
            break;
        case ConditionNodeOp::None:
            matched = true;
            for (const uint32_t childIndex : node.childIndices) {
                const bool childMatched = evaluateNode(childIndex);
                if (invalidIr) {
                    return false;
                }
                if (childMatched) {
                    matched = false;
                }
            }
            break;
        case ConditionNodeOp::Not:
            matched = !evaluateNode(node.childIndices[0]);
            if (invalidIr) {
                return false;
            }
            break;
        case ConditionNodeOp::HasTagsAll:
            matched = true;
            for (const ConditionTagId tagId : node.tagIds) {
                if (!ownedTags.Has(tagId)) {
                    matched = false;
                    break;
                }
            }
            break;
        case ConditionNodeOp::HasTagsAny:
            matched = false;
            for (const ConditionTagId tagId : node.tagIds) {
                if (ownedTags.Has(tagId)) {
                    matched = true;
                    break;
                }
            }
            break;
        default:
            invalidIr = true;
            return false;
        }

        visitStates[nodeIndex] = VisitState::Evaluated;
        evaluatedMatches[nodeIndex] = matched;
        return matched;
    };

    const bool matched = evaluateNode(conditionIr.rootNodeIndex);
    if (invalidIr) {
        return ConditionEvaluateResult{ConditionEvaluateStatus::InvalidIR, false};
    }

    return ConditionEvaluateResult{ConditionEvaluateStatus::Ok, matched};
}

} // namespace NoMoreDay::CombatV2
