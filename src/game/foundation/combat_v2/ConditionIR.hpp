#pragma once

#include "TagBitset.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay::CombatV2 {

using ConditionTagId = uint16_t;

enum class ConditionNodeOp : uint8_t {
    All = 0,
    Any = 1,
    None = 2,
    Not = 3,
    HasTagsAll = 4,
    HasTagsAny = 5,
};

struct ConditionNode {
    ConditionNodeOp op{ConditionNodeOp::All};
    std::vector<uint32_t> childIndices{};
    std::vector<ConditionTagId> tagIds{};
};

struct ConditionIR {
    std::vector<ConditionNode> nodes{};
    uint32_t rootNodeIndex{0};
};

enum class ConditionEvaluateStatus : uint8_t {
    Ok = 0,
    InvalidIR = 1,
    NotImplemented = 2,
};

struct ConditionEvaluateResult {
    ConditionEvaluateStatus status{ConditionEvaluateStatus::NotImplemented};
    bool matched{false};
};

class ConditionEvaluator {
  public:
    [[nodiscard]] ConditionEvaluateResult Evaluate(const ConditionIR &conditionIr, const TagBitset &ownedTags) const;
};

} // namespace NoMoreDay::CombatV2
