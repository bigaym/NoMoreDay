#pragma once

#include "DamageStages.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay::CombatV2 {

enum class DamageKernelBranch : uint8_t {
    Hit = 0,
    Dot = 1,
};

struct DamageStageInstruction {
    DamageStageOp op{DamageStageOp::Flat};
    float value{0.0f};
    uint16_t priority{0};
    uint32_t sourceId{0};
    uint32_t nodeId{0};
};

struct DamageKernelRequest {
    DamageKernelBranch branch{DamageKernelBranch::Hit};
    float baseDamage{0.0f};
    std::vector<DamageStageInstruction> stages{};
    uint64_t replaySalt{0};
};

enum class DamageKernelStatus : uint8_t {
    Ok = 0,
    InvalidInput = 1,
    NotImplemented = 2,
};

struct DamageKernelReplay {
    DamageKernelBranch branch{DamageKernelBranch::Hit};
    std::vector<DamageStageOp> stageOrder{};
    std::vector<DamageStageInstruction> orderedStagePayload{};
    uint64_t deterministicHash{0};
};

struct DamageKernelResult {
    DamageKernelStatus status{DamageKernelStatus::NotImplemented};
    float finalDamage{0.0f};
    DamageKernelReplay replay{};
};

class DamageKernel {
  public:
    [[nodiscard]] DamageKernelResult Execute(const DamageKernelRequest &request) const;
};

} // namespace NoMoreDay::CombatV2
