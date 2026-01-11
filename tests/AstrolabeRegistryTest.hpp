#pragma once
#include "TestCommon.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include <filesystem>

TEST_CASE("AstrolabeRegistry: Loading and Node Retrieval") {
    using namespace NoMoreDay;
    auto& registry = AstrolabeRegistry::Get();

    SUBCASE("Load astrolabe.json") {
        bool success = registry.Load("assets/data/astrolabe.json");
        CHECK(success == true);
        
        auto& all_nodes = registry.GetAllNodes();
        CHECK(all_nodes.size() >= 10);
    }

    SUBCASE("Retrieve specific nodes") {
        // Node 0: 起源
        const AstrolabeNode* node0 = registry.GetNode(0);
        REQUIRE(node0 != nullptr);
        CHECK(node0->id == 0);
        CHECK(node0->name_key == "起源");
        CHECK(node0->type == AstrolabeNodeType::Minor);
        CHECK(node0->modifiers.size() == 4); // All Attributes +1

        // Node 1: 剑术修炼 I
        const AstrolabeNode* node1 = registry.GetNode(1);
        REQUIRE(node1 != nullptr);
        CHECK(node1->id == 1);
        CHECK(node1->name_key == "剑术修炼 I");
        CHECK(node1->modifiers[0].type == StatType::Dexterity);
        CHECK(node1->modifiers[0].value == 5.0f);
        CHECK(node1->prerequisites[0] == 0);

        // Node 4: 剑心通明 (Keystone)
        const AstrolabeNode* keystone = registry.GetNode(4);
        REQUIRE(keystone != nullptr);
        CHECK(keystone->type == AstrolabeNodeType::Keystone);
        CHECK(keystone->prerequisites[0] == 3);
    }

    SUBCASE("Retrieve non-existent node") {
        const AstrolabeNode* invalid = registry.GetNode(9999);
        CHECK(invalid == nullptr);
    }
}
