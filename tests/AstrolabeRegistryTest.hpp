#pragma once
#include "TestCommon.hpp"
#include "../src/core/AstrolabeRegistry.hpp"
#include <filesystem>

TEST_CASE("AstrolabeRegistry: Loading and Node Retrieval") {
    using namespace NoMoreDay;
    auto& registry = AstrolabeRegistry::Get();

    SUBCASE("Load astrolabe.json") {
        bool success = registry.Load("assets/data/astrolabe.json");
        CHECK(success == true);
        
        auto& all_nodes = registry.GetAllNodes();
        CHECK(all_nodes.size() >= 3);
    }

    SUBCASE("Retrieve specific nodes") {
        const AstrolabeNode* node1 = registry.GetNode(1);
        REQUIRE(node1 != nullptr);
        CHECK(node1->id == 1);
        CHECK(node1->name_key == "astrolabe.node.str_1");
        CHECK(node1->type == AstrolabeNodeType::Minor);
        CHECK(node1->modifiers.size() == 1);
        CHECK(node1->modifiers[0].type == StatType::Strength);
        CHECK(node1->modifiers[0].value == 5.0f);

        const AstrolabeNode* keystone = registry.GetNode(100);
        REQUIRE(keystone != nullptr);
        CHECK(keystone->type == AstrolabeNodeType::Keystone);
        CHECK(keystone->prerequisites.size() == 1);
        CHECK(keystone->prerequisites[0] == 2);
    }

    SUBCASE("Retrieve non-existent node") {
        const AstrolabeNode* invalid = registry.GetNode(9999);
        CHECK(invalid == nullptr);
    }
}
