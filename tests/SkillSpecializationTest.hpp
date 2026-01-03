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

#include "../src/systems/SkillSystem.hpp"

TEST_CASE("SkillSpecialization: AddTalentPoint Logic") {
    LoggerScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& active = registry.emplace<NoMoreDay::ActiveSkillsComponent>(player);
    active.available_talent_points = 5;
    
    // Mock Skill Data & Tree
    std::string path = "temp_skills_logic.json";
    std::ofstream out(path);
    out << R"({
        "skills": [
            {
                "id": 1,
                "name_key": "Skill 1",
                "talent_tree": [
                    { "id": 10, "name_key": "Root", "desc_key": "Root Desc", "max_points": 5 },
                    { "id": 11, "name_key": "Child", "desc_key": "Child Desc", "max_points": 1, "prerequisites": [10] }
                ]
            }
        ]
    })";
    out.close();
    NoMoreDay::SkillRegistry::Get().LoadFromJson(path);

    SUBCASE("Basic Allocation") {
        active.specialized_slots[0].skill_id = 1;
        
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10));
        CHECK(active.available_talent_points == 4);
        CHECK(active.specialized_slots[0].allocated_points[10] == 1);
        
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10));
        CHECK(active.specialized_slots[0].allocated_points[10] == 2);
        CHECK(active.available_talent_points == 3);
    }

    SUBCASE("Prerequisite Validation") {
        active.specialized_slots[0].skill_id = 1;
        
        // Cannot add to 11 without 10
        CHECK_FALSE(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 11));
        
        // Add to 10
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10));
        
        // Now can add to 11
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 11));
        CHECK(active.specialized_slots[0].allocated_points[11] == 1);
    }

    SUBCASE("Max Points Validation") {
        active.specialized_slots[0].skill_id = 1;
        active.available_talent_points = 10;
        
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 1
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 2
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 3
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 4
        CHECK(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 5
        CHECK_FALSE(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10)); // 6 (Max is 5)
    }

    SUBCASE("Not Specialized Validation") {
        // Skill 1 is NOT in specialized_slots
        CHECK_FALSE(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10));
    }

    SUBCASE("No Points Validation") {
        active.specialized_slots[0].skill_id = 1;
        active.available_talent_points = 0;
        CHECK_FALSE(NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 10));
    }

    std::remove(path.c_str());
}

#include "../src/systems/DamagePipeline.hpp"

TEST_CASE("SkillSpecialization: Talent Damage Modification") {
    LoggerScope scope;
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    registry.emplace<NoMoreDay::CombatStats>(attacker);
    registry.emplace<NoMoreDay::CombatStats>(defender);
    auto& active = registry.emplace<NoMoreDay::ActiveSkillsComponent>(attacker);
    active.available_talent_points = 5;

    // Mock Skill Data with Talent Tree
    std::string path = "temp_skills_dmg.json";
    std::ofstream out(path);
    out << R"({
        "skills": [
            {
                "id": 1,
                "name_key": "Skill 1",
                "tags": ["Physical", "Melee"],
                "base_damage": 100,
                "talent_tree": [
                    { 
                        "id": 10, "name_key": "More Dmg", "desc_key": "D", "max_points": 5,
                        "damage_modifiers": [
                            { "source_tag": 1, "target_tag": 0, "type": 2, "value": 0.2 }
                        ]
                    },
                    {
                        "id": 11, "name_key": "Inc Phys", "desc_key": "D", "max_points": 5,
                        "stat_modifiers": [
                            { "type": 8, "mode": 1, "value": 50.0 }
                        ]
                    }
                ]
            }
        ]
    })";
    out.close();
    NoMoreDay::SkillRegistry::Get().LoadFromJson(path);

    active.specialized_slots[0].skill_id = 1;
    NoMoreDay::DamagePool base;

    SUBCASE("No Talents") {
        auto res = NoMoreDay::DamagePipeline::Calculate(registry, attacker, defender, 1, base);
        // Base 100 * 1.0 = 100
        CHECK(res.total_damage == doctest::Approx(100.0f));
    }

    SUBCASE("More Damage Talent") {
        NoMoreDay::SkillSystem::AddTalentPoint(registry, attacker, 1, 10); // 1 point = 20% more
        auto res = NoMoreDay::DamagePipeline::Calculate(registry, attacker, defender, 1, base);
        // Base 100 * 1.2 = 120
        CHECK(res.total_damage == doctest::Approx(120.0f));

        NoMoreDay::SkillSystem::AddTalentPoint(registry, attacker, 1, 10); // 2 points = 40% more
        res = NoMoreDay::DamagePipeline::Calculate(registry, attacker, defender, 1, base);
        CHECK(res.total_damage == doctest::Approx(140.0f));
    }

    SUBCASE("Increased Physical Talent") {
        NoMoreDay::SkillSystem::AddTalentPoint(registry, attacker, 1, 11); // 1 point = 50% inc
        auto res = NoMoreDay::DamagePipeline::Calculate(registry, attacker, defender, 1, base);
        // Base 100 * (1.0 + 0.5) = 150
        CHECK(res.total_damage == doctest::Approx(150.0f));
    }

        SUBCASE("Combined Talents") {

            NoMoreDay::SkillSystem::AddTalentPoint(registry, attacker, 1, 10); // 20% more

            NoMoreDay::SkillSystem::AddTalentPoint(registry, attacker, 1, 11); // 50% inc

            auto res = NoMoreDay::DamagePipeline::Calculate(registry, attacker, defender, 1, base);

            // Base 100 * (1.0 + 0.5) * 1.2 = 180

            CHECK(res.total_damage == doctest::Approx(180.0f));

        }

    

        std::remove(path.c_str());

    }

    

    TEST_CASE("SkillSpecialization: Cooldown & Resource Talents") {

        LoggerScope scope;

        entt::registry registry;

        auto player = registry.create();

        auto& stats = registry.emplace<NoMoreDay::CombatStats>(player);

        auto& active = registry.emplace<NoMoreDay::ActiveSkillsComponent>(player);

        active.available_talent_points = 5;

    

        stats.mana = 100.0f;

    

        std::string path = "temp_skills_utility.json";

        std::ofstream out(path);

        out << R"({

            "skills": [

                {

                    "id": 1,

                    "name_key": "Utility Skill",

                    "mana_cost": 20,

                    "cooldown": 10.0,

                    "tags": ["Spell"],

                                                    "talent_tree": [

                                                        { 

                                                            "id": 20, "name_key": "CDR", "desc_key": "D", "max_points": 5,

                                                            "stat_modifiers": [ { "type": 27, "mode": 0, "value": 10.0 } ] 

                                                        },

                                                        {

                                                            "id": 21, "name_key": "RCR", "desc_key": "D", "max_points": 5,

                                                            "stat_modifiers": [ { "type": 28, "mode": 0, "value": 20.0 } ]

                                                        }

                                                    ]

                                    

                    

                }

            ]

        })";

        out.close();

        NoMoreDay::SkillRegistry::Get().LoadFromJson(path);

    

        active.specialized_slots[0].skill_id = 1;

        active.slots[0].id = 1;

        active.slots[0].current_charges = 1;

    

        SUBCASE("CDR Allocation") {

            NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 20); // 10% CDR

            NoMoreDay::SkillSystem::TryCast(registry, player, 0);

            // 10s * (1 - 0.1) = 9s

            CHECK(active.slots[0].cooldown == doctest::Approx(9.0f));

        }

    

        SUBCASE("RCR Allocation") {

            NoMoreDay::SkillSystem::AddTalentPoint(registry, player, 1, 21); // 20% RCR

            stats.mana = 100.0f;

            NoMoreDay::SkillSystem::TryCast(registry, player, 0);

            // Cost 20 * (1 - 0.2) = 16. Mana 100 - 16 = 84

            CHECK(stats.mana == doctest::Approx(84.0f));

        }

    

        std::remove(path.c_str());

    }

    