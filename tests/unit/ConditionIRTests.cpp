#include "doctest.h"

#include "game/foundation/combat_v2/ConditionCompiler.hpp"
#include "game/foundation/combat_v2/ConditionIR.hpp"
#include "game/foundation/combat_v2/TagDomain.hpp"

namespace {

NoMoreDay::CombatV2::ConditionIR BuildNestedSemanticsIr() {
    using namespace NoMoreDay::CombatV2;

    ConditionIR ir;
    ir.rootNodeIndex = 0;
    ir.nodes = {
        ConditionNode{ConditionNodeOp::All, {1, 2, 3, 4}, {}},
        ConditionNode{ConditionNodeOp::HasTagsAll, {}, {1}},
        ConditionNode{ConditionNodeOp::Any, {5, 6}, {}},
        ConditionNode{ConditionNodeOp::None, {7, 8}, {}},
        ConditionNode{ConditionNodeOp::Not, {9}, {}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {2}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {3}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {8}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {9}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {10}},
    };
    return ir;
}

} // namespace

TEST_CASE("[Unit] ConditionIR - nested all/any/none/not semantics") {
    using namespace NoMoreDay::CombatV2;

    TagDomain tagDomain;
    ConditionEvaluator evaluator;

    const ConditionIR ir = BuildNestedSemanticsIr();
    const TagBitset shouldPass = tagDomain.BuildBitset({1, 3});
    const TagBitset shouldFail = tagDomain.BuildBitset({1, 2, 10});

    const auto passResult = evaluator.Evaluate(ir, shouldPass);
    const auto failResult = evaluator.Evaluate(ir, shouldFail);

    REQUIRE(passResult.status == ConditionEvaluateStatus::Ok);
    REQUIRE(failResult.status == ConditionEvaluateStatus::Ok);
    CHECK(passResult.matched);
    CHECK_FALSE(failResult.matched);
}

TEST_CASE("[Unit] ConditionIR - invalid schema compile failure signal") {
    using namespace NoMoreDay::CombatV2;

    TagDomain tagDomain;
    ConditionCompiler compiler;
    const auto result = compiler.CompileFromText(R"({"all":[]})", tagDomain);

    CHECK(result.status == ConditionCompileStatus::InvalidSchema);
}

TEST_CASE("[Unit] ConditionIR - evaluator deterministic behavior contract") {
    using namespace NoMoreDay::CombatV2;

    TagDomain tagDomain;
    ConditionEvaluator evaluator;

    ConditionIR ir;
    ir.rootNodeIndex = 0;
    ir.nodes = {
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {1}},
    };

    const TagBitset ownedTags = tagDomain.BuildBitset({1, 3, 7});
    const auto first = evaluator.Evaluate(ir, ownedTags);
    const auto second = evaluator.Evaluate(ir, ownedTags);
    const auto third = evaluator.Evaluate(ir, ownedTags);

    REQUIRE(first.status == ConditionEvaluateStatus::Ok);
    REQUIRE(second.status == ConditionEvaluateStatus::Ok);
    REQUIRE(third.status == ConditionEvaluateStatus::Ok);
    CHECK(first.matched == second.matched);
    CHECK(second.matched == third.matched);
}

TEST_CASE("[Unit] ConditionIR - invalid reachable child fails even with short-circuit") {
    using namespace NoMoreDay::CombatV2;

    TagDomain tagDomain;
    ConditionEvaluator evaluator;

    ConditionIR ir;
    ir.rootNodeIndex = 0;
    ir.nodes = {
        ConditionNode{ConditionNodeOp::Any, {1, 2}, {}},
        ConditionNode{ConditionNodeOp::HasTagsAny, {}, {1}},
        ConditionNode{ConditionNodeOp::All, {99}, {}},
    };

    const TagBitset ownedTags = tagDomain.BuildBitset({1});
    const auto result = evaluator.Evaluate(ir, ownedTags);

    CHECK(result.status == ConditionEvaluateStatus::InvalidIR);
    CHECK_FALSE(result.matched);
}
