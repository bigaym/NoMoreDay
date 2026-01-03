#pragma once
#include "TestCommon.hpp"
#include "../src/components/SkillSystem.hpp"
#include <nlohmann/json.hpp>

TEST_CASE("SkillSpecialization: Data Structures") {
    SUBCASE("TalentNode Serialization") {
        NoMoreDay::TalentNode node;
        node.id = 101;
        node.name_key = "test_node";
        node.desc_key = "test_desc";
        node.max_points = 3;
        node.prerequisites = {100};
        node.x = 10.0f;
        node.y = 20.0f;
        node.icon_id = "icon_test";
        
        node.stat_modifiers.push_back({
            NoMoreDay::StatType::PhysicalDamage,
            NoMoreDay::ModifierMode::PercentAdd,
            10.0f
        });

        nlohmann::json j = node;
        auto deserialized = j.get<NoMoreDay::TalentNode>();

        CHECK(deserialized.id == 101);
        CHECK(deserialized.name_key == "test_node");
        CHECK(deserialized.max_points == 3);
        CHECK(deserialized.prerequisites.size() == 1);
        CHECK(deserialized.prerequisites[0] == 100);
        CHECK(deserialized.stat_modifiers.size() == 1);
        CHECK(deserialized.stat_modifiers[0].type == NoMoreDay::StatType::PhysicalDamage);
        CHECK(deserialized.x == 10.0f);
        CHECK(deserialized.y == 20.0f);
    }

    SUBCASE("ActiveSkillsComponent Specialization") {
        NoMoreDay::ActiveSkillsComponent comp;
        comp.available_talent_points = 5;
        comp.specialized_slots[0].skill_id = 1;
        comp.specialized_slots[0].allocated_points[101] = 2;

        nlohmann::json j = comp;
        auto deserialized = j.get<NoMoreDay::ActiveSkillsComponent>();

        CHECK(deserialized.available_talent_points == 5);
        CHECK(deserialized.specialized_slots[0].skill_id == 1);
        CHECK(deserialized.specialized_slots[0].allocated_points.at(101) == 2);
    }
}

#include "../src/core/SkillRegistry.hpp"
#include <fstream>
#include <cstdio>

TEST_CASE("SkillSpecialization: Registry Loading") {
    // Create a temporary JSON file
    std::string path = "temp_skills_test.json";
    std::ofstream out(path);
    out << R"({
        "skills": [
            {
                "id": 999,
                "name_key": "Test Skill",
                "talent_tree": [
                    {
                        "id": 1,
                        "name_key": "Talent 1",
                        "desc_key": "Desc 1",
                        "max_points": 5,
                        "x": 0.0,
                        "y": 0.0,
                        "stat_modifiers": [
                            {
                                "type": 0,
                                "mode": 1,
                                "value": 10.0
                            }
                        ]
                    }
                ]
            }
        ]
    })";
    out.close();

    auto& registry = NoMoreDay::SkillRegistry::Get();
    registry.LoadFromJson(path);

    SUBCASE("Tree Loading") {
        const auto* tree = registry.GetSkillTree(999);
        REQUIRE(tree != nullptr);
        CHECK(tree->skill_id == 999);
        CHECK(tree->nodes.size() == 1);
        CHECK(tree->nodes.count(1) == 1);
        
        const auto& node = tree->nodes.at(1);
        CHECK(node.name_key == "Talent 1");
        CHECK(node.max_points == 5);
        CHECK(node.stat_modifiers.size() == 1);
        CHECK(node.stat_modifiers[0].value == 10.0f);
    }
    
    // Cleanup
    std::remove(path.c_str());
}
