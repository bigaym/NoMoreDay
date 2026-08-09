#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace NoMoreDay::CombatV2 {

enum class DamageStageOp : uint8_t {
    Flat = 0,
    Increased = 1,
    More = 2,
    Convert = 3,
    GainExtra = 4,
};

constexpr std::array<DamageStageOp, 5> kDamageStageOrder = {
    DamageStageOp::Flat,
    DamageStageOp::Increased,
    DamageStageOp::More,
    DamageStageOp::Convert,
    DamageStageOp::GainExtra,
};

[[nodiscard]] constexpr size_t DamageStageOpIndex(DamageStageOp op) {
    return static_cast<size_t>(op);
}

} // namespace NoMoreDay::CombatV2
