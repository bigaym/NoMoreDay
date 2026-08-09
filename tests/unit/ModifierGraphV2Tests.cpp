#include "doctest.h"

#include "game/foundation/combat_v2/ModifierGraph.hpp"
#include "game/foundation/combat_v2/ModifierSourceAdapters.hpp"

#include <cstddef>
#include <limits>
#include <vector>

TEST_CASE("[Unit] ModifierGraphV2 - source normalization consistency") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierSourceAdapters adapters;

    CV2::SourceModifierRecord equipment;
    equipment.sourceKind = CV2::ModifierSourceKind::Equipment;
    equipment.sourceId = 1001;
    equipment.semanticId = 77;
    equipment.stage = CV2::ModifierStage::Hit;
    equipment.op = CV2::ModifierOp::Increased;
    equipment.value = 0.25f;
    equipment.conditionProgramId = 5;
    equipment.priority = 10;
    equipment.nodeWhitelist = {301, 302};

    CV2::SourceModifierRecord talent = equipment;
    talent.sourceKind = CV2::ModifierSourceKind::Talent;
    talent.sourceId = 2001;

    const auto equipmentResult = adapters.Normalize(equipment);
    const auto talentResult = adapters.Normalize(talent);

    REQUIRE(equipmentResult.status == CV2::SourceNormalizeStatus::Ok);
    REQUIRE(talentResult.status == CV2::SourceNormalizeStatus::Ok);
    CHECK(equipmentResult.normalized.node.stage == talentResult.normalized.node.stage);
    CHECK(equipmentResult.normalized.node.op == talentResult.normalized.node.op);
    CHECK(equipmentResult.normalized.node.value == doctest::Approx(talentResult.normalized.node.value));
    CHECK(equipmentResult.normalized.node.conditionProgramId == talentResult.normalized.node.conditionProgramId);
    CHECK(equipmentResult.normalized.node.priority == talentResult.normalized.node.priority);
    CHECK(equipmentResult.normalized.node.nodeId == talentResult.normalized.node.nodeId);
}

TEST_CASE("[Unit] ModifierGraphV2 - forbidden filter pruning") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierGraphCompiler compiler;

    std::vector<CV2::CompiledModifierNode> candidates = {
        CV2::CompiledModifierNode{CV2::ModifierNode{1, CV2::ModifierStage::Hit, CV2::ModifierOp::More, 0.10f, 0, 1, 111},
                                  {42},
                                  {}},
        CV2::CompiledModifierNode{CV2::ModifierNode{2, CV2::ModifierStage::Hit, CV2::ModifierOp::More, 0.20f, 0, 1, 222},
                                  {99},
                                  {}},
    };
    std::vector<uint32_t> activeForbiddenFilters = {42};

    const auto compileResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&candidates, &activeForbiddenFilters});
    REQUIRE(compileResult.status == CV2::ModifierGraphCompileStatus::Ok);

    const auto &hitBucket = compileResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::Hit)];
    REQUIRE(hitBucket.size() == 1);
    CHECK(hitBucket[0].node.nodeId == 2);
}

TEST_CASE("[Unit] ModifierGraphV2 - node whitelist and scope preservation") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierGraphCompiler compiler;

    std::vector<CV2::CompiledModifierNode> candidates = {
        CV2::CompiledModifierNode{
            CV2::ModifierNode{9, CV2::ModifierStage::PostHit, CV2::ModifierOp::GainExtra, 0.15f, 7, 3, 501}, {}, {100, 200}},
    };
    std::vector<uint32_t> activeForbiddenFilters;

    const auto compileResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&candidates, &activeForbiddenFilters});
    REQUIRE(compileResult.status == CV2::ModifierGraphCompileStatus::Ok);

    const auto &bucket = compileResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::PostHit)];
    REQUIRE(bucket.size() == 1);
    CHECK(bucket[0].nodeWhitelist == std::vector<uint32_t>({100, 200}));
}

TEST_CASE("[Unit] ModifierGraphV2 - deterministic ordering inside stage bucket") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierGraphCompiler compiler;
    std::vector<uint32_t> activeForbiddenFilters;

    std::vector<CV2::CompiledModifierNode> firstCandidates = {
        CV2::CompiledModifierNode{CV2::ModifierNode{9, CV2::ModifierStage::Hit, CV2::ModifierOp::More, 1.10f, 1, 20, 2},
                                  {},
                                  {}},
        CV2::CompiledModifierNode{CV2::ModifierNode{7, CV2::ModifierStage::Hit, CV2::ModifierOp::Increased, 0.15f, 3, 10, 1},
                                  {},
                                  {}},
        CV2::CompiledModifierNode{CV2::ModifierNode{8, CV2::ModifierStage::Hit, CV2::ModifierOp::Flat, 5.0f, 2, 10, 1},
                                  {},
                                  {}},
    };
    std::vector<CV2::CompiledModifierNode> secondCandidates = {
        firstCandidates[1],
        firstCandidates[0],
        firstCandidates[2],
    };

    const auto firstResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&firstCandidates, &activeForbiddenFilters});
    const auto secondResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&secondCandidates, &activeForbiddenFilters});

    REQUIRE(firstResult.status == CV2::ModifierGraphCompileStatus::Ok);
    REQUIRE(secondResult.status == CV2::ModifierGraphCompileStatus::Ok);

    const auto &firstBucket = firstResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::Hit)];
    const auto &secondBucket = secondResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::Hit)];
    REQUIRE(firstBucket.size() == secondBucket.size());
    REQUIRE(firstBucket.size() == 3);

    for (size_t index = 0; index < firstBucket.size(); ++index) {
        CHECK(firstBucket[index].node.priority == secondBucket[index].node.priority);
        CHECK(firstBucket[index].node.sourceId == secondBucket[index].node.sourceId);
        CHECK(firstBucket[index].node.nodeId == secondBucket[index].node.nodeId);
        CHECK(firstBucket[index].node.conditionProgramId == secondBucket[index].node.conditionProgramId);
    }
}

TEST_CASE("[Unit] ModifierGraphV2 - source normalization rejects non-finite values") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierSourceAdapters adapters;
    CV2::SourceModifierRecord source;
    source.semanticId = 123;
    source.value = std::numeric_limits<float>::infinity();

    const auto result = adapters.Normalize(source);
    REQUIRE(result.status == CV2::SourceNormalizeStatus::InvalidInput);
}

TEST_CASE("[Unit] ModifierGraphV2 - compiler rejects non-finite candidate values") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierGraphCompiler compiler;
    std::vector<CV2::CompiledModifierNode> candidates = {
        CV2::CompiledModifierNode{CV2::ModifierNode{50,
                                                    CV2::ModifierStage::Hit,
                                                    CV2::ModifierOp::More,
                                                    std::numeric_limits<float>::quiet_NaN(),
                                                    0,
                                                    1,
                                                    1},
                                  {},
                                  {}},
    };
    std::vector<uint32_t> activeForbiddenFilters;

    const auto result = compiler.Compile(CV2::ModifierGraphBuildRequest{&candidates, &activeForbiddenFilters});
    CHECK(result.status == CV2::ModifierGraphCompileStatus::InvalidInput);
}
