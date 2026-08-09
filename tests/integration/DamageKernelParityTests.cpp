#include "doctest.h"

#include "game/foundation/combat_v2/DamageKernel.hpp"

TEST_CASE("[Integration] DamageKernelParity - hit and dot branch isolation") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::DamageKernel kernel;

    CV2::DamageKernelRequest hitRequest;
    hitRequest.branch = CV2::DamageKernelBranch::Hit;
    hitRequest.baseDamage = 90.0f;
    hitRequest.replaySalt = 0xABC001ULL;
    hitRequest.stages = {
        CV2::DamageStageInstruction{CV2::DamageStageOp::Flat, 10.0f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Increased, 0.20f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::More, 0.10f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Convert, 0.20f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::GainExtra, 0.15f},
    };

    CV2::DamageKernelRequest dotRequest = hitRequest;
    dotRequest.branch = CV2::DamageKernelBranch::Dot;

    const CV2::DamageKernelResult hitResult = kernel.Execute(hitRequest);
    const CV2::DamageKernelResult dotResult = kernel.Execute(dotRequest);

    REQUIRE(hitResult.status == CV2::DamageKernelStatus::Ok);
    REQUIRE(dotResult.status == CV2::DamageKernelStatus::Ok);
    CHECK(hitResult.replay.branch == CV2::DamageKernelBranch::Hit);
    CHECK(dotResult.replay.branch == CV2::DamageKernelBranch::Dot);
    CHECK(hitResult.replay.deterministicHash != dotResult.replay.deterministicHash);
    CHECK(hitResult.finalDamage != doctest::Approx(dotResult.finalDamage));
}
