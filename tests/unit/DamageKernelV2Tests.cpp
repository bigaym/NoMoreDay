#include "doctest.h"

#include "game/combat_v2/DamageKernel.hpp"

#include <limits>
#include <vector>

TEST_CASE("[Unit] DamageKernelV2 - stage ordering invariants") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::DamageKernel kernel;
    CV2::DamageKernelRequest request;
    request.branch = CV2::DamageKernelBranch::Hit;
    request.baseDamage = 100.0f;
    request.stages = {
        CV2::DamageStageInstruction{CV2::DamageStageOp::More, 0.20f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Flat, 12.0f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::GainExtra, 0.30f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Convert, 0.10f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Increased, 0.50f},
    };

    const CV2::DamageKernelResult result = kernel.Execute(request);
    REQUIRE(result.status == CV2::DamageKernelStatus::Ok);

    REQUIRE(result.replay.stageOrder.size() == CV2::kDamageStageOrder.size());
    CHECK(result.replay.stageOrder == std::vector<CV2::DamageStageOp>(
                                         CV2::kDamageStageOrder.begin(), CV2::kDamageStageOrder.end()));
    CHECK(result.finalDamage == doctest::Approx(235.872f));
}

TEST_CASE("[Unit] DamageKernelV2 - deterministic replay hash contract") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::DamageKernel kernel;
    CV2::DamageKernelRequest request;
    request.branch = CV2::DamageKernelBranch::Hit;
    request.baseDamage = 220.0f;
    request.replaySalt = 0x4D4B9A12ULL;
    request.stages = {
        CV2::DamageStageInstruction{CV2::DamageStageOp::Flat, 9.0f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Increased, 0.25f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::More, 0.18f},
    };

    const CV2::DamageKernelResult first = kernel.Execute(request);
    const CV2::DamageKernelResult second = kernel.Execute(request);

    REQUIRE(first.status == CV2::DamageKernelStatus::Ok);
    REQUIRE(second.status == CV2::DamageKernelStatus::Ok);
    CHECK(first.replay.stageOrder == second.replay.stageOrder);
    CHECK(first.replay.deterministicHash == second.replay.deterministicHash);
    CHECK(first.finalDamage == doctest::Approx(second.finalDamage));

    CV2::DamageKernelRequest differentSalt = request;
    differentSalt.replaySalt = request.replaySalt + 1;
    const CV2::DamageKernelResult differentSaltResult = kernel.Execute(differentSalt);
    REQUIRE(differentSaltResult.status == CV2::DamageKernelStatus::Ok);
    CHECK(first.replay.deterministicHash != differentSaltResult.replay.deterministicHash);
}

TEST_CASE("[Unit] DamageKernelV2 - rejects non-finite inputs") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::DamageKernel kernel;

    CV2::DamageKernelRequest nonFiniteBase;
    nonFiniteBase.branch = CV2::DamageKernelBranch::Hit;
    nonFiniteBase.baseDamage = std::numeric_limits<float>::quiet_NaN();
    nonFiniteBase.stages = {CV2::DamageStageInstruction{CV2::DamageStageOp::Flat, 5.0f}};

    const CV2::DamageKernelResult nonFiniteBaseResult = kernel.Execute(nonFiniteBase);
    REQUIRE(nonFiniteBaseResult.status == CV2::DamageKernelStatus::InvalidInput);

    CV2::DamageKernelRequest nonFiniteStage;
    nonFiniteStage.branch = CV2::DamageKernelBranch::Hit;
    nonFiniteStage.baseDamage = 100.0f;
    nonFiniteStage.stages = {CV2::DamageStageInstruction{CV2::DamageStageOp::More,
                                                         std::numeric_limits<float>::infinity()}};

    const CV2::DamageKernelResult nonFiniteStageResult = kernel.Execute(nonFiniteStage);
    REQUIRE(nonFiniteStageResult.status == CV2::DamageKernelStatus::InvalidInput);
}

TEST_CASE("[Unit] DamageKernelV2 - rejects invalid stage op and convert overflow") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::DamageKernel kernel;

    CV2::DamageKernelRequest invalidOp;
    invalidOp.branch = CV2::DamageKernelBranch::Hit;
    invalidOp.baseDamage = 100.0f;
    invalidOp.stages = {
        CV2::DamageStageInstruction{static_cast<CV2::DamageStageOp>(255), 0.1f},
    };

    const CV2::DamageKernelResult invalidOpResult = kernel.Execute(invalidOp);
    CHECK(invalidOpResult.status == CV2::DamageKernelStatus::InvalidInput);

    CV2::DamageKernelRequest convertOverflow;
    convertOverflow.branch = CV2::DamageKernelBranch::Dot;
    convertOverflow.baseDamage = 100.0f;
    convertOverflow.stages = {
        CV2::DamageStageInstruction{CV2::DamageStageOp::Convert, 0.8f},
        CV2::DamageStageInstruction{CV2::DamageStageOp::Convert, 0.3f},
    };

    const CV2::DamageKernelResult convertOverflowResult = kernel.Execute(convertOverflow);
    CHECK(convertOverflowResult.status == CV2::DamageKernelStatus::InvalidInput);
}
