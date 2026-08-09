#include "doctest.h"

#include "game/foundation/combat_v2/ModifierGraph.hpp"
#include "game/foundation/combat_v2/ModifierSourceAdapters.hpp"

#include <cstddef>
#include <vector>

TEST_CASE("[Integration] ModifierGraphV2 - adapters feed a unified graph") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierSourceAdapters adapters;
    CV2::ModifierGraphCompiler compiler;

    CV2::SourceModifierRecord equipment;
    equipment.sourceKind = CV2::ModifierSourceKind::Equipment;
    equipment.sourceId = 7001;
    equipment.semanticId = 810;
    equipment.stage = CV2::ModifierStage::PreHit;
    equipment.op = CV2::ModifierOp::Flat;
    equipment.value = 12.0f;
    equipment.priority = 1;
    equipment.nodeWhitelist = {11, 12};

    CV2::SourceModifierRecord skillSpec = equipment;
    skillSpec.sourceKind = CV2::ModifierSourceKind::SkillSpec;
    skillSpec.sourceId = 7002;

    const auto equipmentNormalized = adapters.Normalize(equipment);
    const auto skillSpecNormalized = adapters.Normalize(skillSpec);

    REQUIRE(equipmentNormalized.status == CV2::SourceNormalizeStatus::Ok);
    REQUIRE(skillSpecNormalized.status == CV2::SourceNormalizeStatus::Ok);

    std::vector<CV2::CompiledModifierNode> candidates = {
        equipmentNormalized.normalized,
        skillSpecNormalized.normalized,
    };
    std::vector<uint32_t> activeForbiddenFilters;

    const auto compileResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&candidates, &activeForbiddenFilters});
    REQUIRE(compileResult.status == CV2::ModifierGraphCompileStatus::Ok);

    const auto &preHitBucket = compileResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::PreHit)];
    CHECK(preHitBucket.size() == 2);
}

TEST_CASE("[Integration] ModifierGraphV2 - forbidden filters prune and whitelist survives") {
    namespace CV2 = NoMoreDay::CombatV2;

    CV2::ModifierSourceAdapters adapters;
    CV2::ModifierGraphCompiler compiler;

    CV2::SourceModifierRecord allowed;
    allowed.sourceKind = CV2::ModifierSourceKind::Global;
    allowed.sourceId = 8801;
    allowed.semanticId = 910;
    allowed.stage = CV2::ModifierStage::DotTick;
    allowed.op = CV2::ModifierOp::More;
    allowed.value = 0.30f;
    allowed.forbiddenFilterIds = {44};
    allowed.nodeWhitelist = {91, 92};

    CV2::SourceModifierRecord pruned = allowed;
    pruned.sourceId = 8802;
    pruned.semanticId = 911;
    pruned.forbiddenFilterIds = {66};

    const auto allowedNormalized = adapters.Normalize(allowed);
    const auto prunedNormalized = adapters.Normalize(pruned);

    REQUIRE(allowedNormalized.status == CV2::SourceNormalizeStatus::Ok);
    REQUIRE(prunedNormalized.status == CV2::SourceNormalizeStatus::Ok);

    std::vector<CV2::CompiledModifierNode> candidates = {
        allowedNormalized.normalized,
        prunedNormalized.normalized,
    };
    std::vector<uint32_t> activeForbiddenFilters = {66};

    const auto compileResult = compiler.Compile(CV2::ModifierGraphBuildRequest{&candidates, &activeForbiddenFilters});
    REQUIRE(compileResult.status == CV2::ModifierGraphCompileStatus::Ok);

    const auto &dotBucket = compileResult.graph.stageBuckets[static_cast<size_t>(CV2::ModifierStage::DotTick)];
    REQUIRE(dotBucket.size() == 1);
    CHECK(dotBucket[0].nodeWhitelist == std::vector<uint32_t>({91, 92}));
}
