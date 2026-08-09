#include "DamageKernel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>

namespace NoMoreDay::CombatV2 {

namespace {

struct OrderedStageInstruction {
    DamageStageInstruction instruction{};
    size_t originalIndex{0};
};

[[nodiscard]] bool IsValidBranch(const DamageKernelBranch branch) {
    return branch == DamageKernelBranch::Hit || branch == DamageKernelBranch::Dot;
}

[[nodiscard]] bool IsValidStageOp(const DamageStageOp op) {
    const size_t opIndex = static_cast<size_t>(op);
    return opIndex < kDamageStageOrder.size();
}

[[nodiscard]] bool IsValidInstruction(const DamageStageInstruction &instruction) {
    return IsValidStageOp(instruction.op) && std::isfinite(instruction.value);
}

void HashCombine(uint64_t &seed, const uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

[[nodiscard]] uint64_t FloatBits(const float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<uint64_t>(bits);
}

[[nodiscard]] uint64_t BuildReplayHash(const DamageKernelRequest &request,
                                       const std::vector<DamageStageInstruction> &orderedPayload) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    HashCombine(hash, request.replaySalt);
    HashCombine(hash, static_cast<uint64_t>(request.branch));
    HashCombine(hash, FloatBits(request.baseDamage));

    for (const DamageStageOp op : kDamageStageOrder) {
        HashCombine(hash, static_cast<uint64_t>(op));
    }
    for (const DamageStageInstruction &instruction : orderedPayload) {
        HashCombine(hash, static_cast<uint64_t>(instruction.op));
        HashCombine(hash, static_cast<uint64_t>(instruction.priority));
        HashCombine(hash, static_cast<uint64_t>(instruction.sourceId));
        HashCombine(hash, static_cast<uint64_t>(instruction.nodeId));
        HashCombine(hash, FloatBits(instruction.value));
    }

    return hash;
}

} // namespace

DamageKernelResult DamageKernel::Execute(const DamageKernelRequest &request) const {
    if (!IsValidBranch(request.branch) || !std::isfinite(request.baseDamage)) {
        return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
    }

    std::vector<OrderedStageInstruction> orderedStages;
    orderedStages.reserve(request.stages.size());
    for (size_t index = 0; index < request.stages.size(); ++index) {
        const DamageStageInstruction &instruction = request.stages[index];
        if (!IsValidInstruction(instruction)) {
            return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
        }
        orderedStages.push_back(OrderedStageInstruction{instruction, index});
    }

    std::stable_sort(orderedStages.begin(),
                     orderedStages.end(),
                     [](const OrderedStageInstruction &lhs, const OrderedStageInstruction &rhs) {
                         const auto lhsKey = std::make_tuple(DamageStageOpIndex(lhs.instruction.op),
                                                             lhs.instruction.priority,
                                                             lhs.instruction.sourceId,
                                                             lhs.instruction.nodeId);
                         const auto rhsKey = std::make_tuple(DamageStageOpIndex(rhs.instruction.op),
                                                             rhs.instruction.priority,
                                                             rhs.instruction.sourceId,
                                                             rhs.instruction.nodeId);
                         return lhsKey < rhsKey;
                     });

    std::vector<DamageStageInstruction> orderedPayload;
    orderedPayload.reserve(orderedStages.size());

    float flatTotal = 0.0f;
    float increasedTotal = 0.0f;
    float moreMultiplier = 1.0f;
    float convertTotal = 0.0f;
    float convertValidationTotal = 0.0f;
    float gainExtraTotal = 0.0f;

    for (const OrderedStageInstruction &ordered : orderedStages) {
        const DamageStageInstruction &instruction = ordered.instruction;
        orderedPayload.push_back(instruction);
        switch (instruction.op) {
        case DamageStageOp::Flat:
            flatTotal += instruction.value;
            break;
        case DamageStageOp::Increased:
            increasedTotal += instruction.value;
            break;
        case DamageStageOp::More:
            moreMultiplier *= (1.0f + instruction.value);
            break;
        case DamageStageOp::Convert:
            if (instruction.value < 0.0f) {
                return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
            }
            convertValidationTotal += instruction.value;
            if (request.branch == DamageKernelBranch::Hit) {
                convertTotal += instruction.value;
            }
            break;
        case DamageStageOp::GainExtra:
            if (instruction.value < 0.0f) {
                return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
            }
            if (request.branch == DamageKernelBranch::Hit) {
                gainExtraTotal += instruction.value;
            }
            break;
        default:
            return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
        }
    }

    if (convertValidationTotal > 1.0f) {
        return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
    }

    float finalDamage = request.baseDamage + flatTotal;
    finalDamage *= (1.0f + increasedTotal);
    finalDamage *= moreMultiplier;
    finalDamage *= (1.0f - convertTotal);
    finalDamage += finalDamage * gainExtraTotal;

    if (!std::isfinite(finalDamage)) {
        return DamageKernelResult{DamageKernelStatus::InvalidInput, 0.0f, {}};
    }

    DamageKernelReplay replay;
    replay.branch = request.branch;
    replay.stageOrder.assign(kDamageStageOrder.begin(), kDamageStageOrder.end());
    replay.orderedStagePayload = std::move(orderedPayload);
    replay.deterministicHash = BuildReplayHash(request, replay.orderedStagePayload);

    return DamageKernelResult{DamageKernelStatus::Ok, finalDamage, std::move(replay)};
}

} // namespace NoMoreDay::CombatV2
