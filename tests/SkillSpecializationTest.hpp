#pragma once
#include "TestCommon.hpp"
#include "../src/core/SkillRegistry.hpp"
#include "../src/systems/SkillSystem.hpp"
#include "../src/components/SkillSystem.hpp"
#include "../src/components/Stats.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/ItemStats.hpp"
#include "../src/components/EquipmentComponent.hpp"
#include "../src/components/ItemComponent.hpp"

TEST_CASE("SkillSpecialization: Data Loading") {
    LoggerScope scope;
    auto& registry = SkillRegistry::Get();
    registry.LoadFromJson("assets/data/skills.json");

    SUBCASE("Flowing Thrust Spec Nodes") {
        const auto* tree = registry.GetSkillTree(1);
        REQUIRE(tree != nullptr);
        
        // Check for node 110 (贯日)
        CHECK(tree->nodes.contains(110));
        CHECK(tree->nodes.at(110).name_key == "贯日");
        
        // Check for node 120 (留影)
        CHECK(tree->nodes.contains(120));
        
        // Check for node 130 (弱点感知)
        CHECK(tree->nodes.contains(130));
        CHECK(tree->nodes.at(130).stat_modifiers.size() > 0);
        CHECK(tree->nodes.at(130).stat_modifiers[0].type == StatType::CritChance);
    }

    SUBCASE("Rending Wave Spec Nodes") {
        const auto* tree = registry.GetSkillTree(2);
        REQUIRE(tree != nullptr);
        
        CHECK(tree->nodes.contains(210)); // 碎裂之刃
        CHECK(tree->nodes.contains(220)); // 回旋剑意
    }
}

TEST_CASE("SkillSpecialization: Point Allocation Logic") {
    LoggerScope scope;
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    
    // Specialize ID 1
    active.specialized_slots[0].skill_id = 1;
    active.available_talent_points = 30; // Lots of points

    SUBCASE("Respect 20 Point Limit") {
        auto& spec = active.specialized_slots[0];
        
        // Fill up to 20
        spec.allocated_points[102] = 1;  // prerequisite node
        spec.allocated_points[999] = 19; // Mock remaining points in some node
        CHECK(spec.GetPointsSpent() == 20);
        CHECK(spec.GetMaxPoints() == 20);
        
        // Try to add one more point to node 130 (which has 0)
        CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, 1, 130));
    }

    SUBCASE("Bonus Levels Breakthrough") {
        auto& spec = active.specialized_slots[0];
        spec.bonus_levels = 2; // Equipment bonus
        
        spec.allocated_points[102] = 1;  // prerequisite node
        spec.allocated_points[999] = 19; // total 20
        CHECK(spec.GetPointsSpent() == 20);
        CHECK(spec.GetMaxPoints() == 22);
        
        // Should be able to add 2 more
        CHECK(SkillSystem::AddTalentPoint(registry, player, 1, 130));
        CHECK(spec.GetPointsSpent() == 21);
        CHECK(SkillSystem::AddTalentPoint(registry, player, 1, 130));
        CHECK(spec.GetPointsSpent() == 22);
        
        // Now it should fail
        CHECK_FALSE(SkillSystem::AddTalentPoint(registry, player, 1, 130));
    }
}

TEST_CASE("SkillSpecialization: Equipment Integration") {
    LoggerScope scope;
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<CombatStats>(player);
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    auto& equip = registry.emplace<EquipmentComponent>(player);
    
    active.specialized_slots[0].skill_id = 1; // Flowing Thrust
    active.specialized_slots[1].skill_id = 2; // Rending Wave

    // Create an item with +2 All Skills and +1 Flowing Thrust
    auto itemEnt = registry.create();
    auto& item = registry.emplace<ItemComponent>(itemEnt);
    item.type = ItemType::Jewelry;
    item.slot = EquipmentSlot::Neck;
    item.affixes.push_back({AffixType::PlusAllSkills, 2.0f, 5, true, "Grand"});
    item.affixes.push_back({AffixType::PlusFlowingThrust, 1.0f, 5, false, "of Striking"});
    
    equip.slots[(int)EquipmentSlot::Neck] = itemEnt;

    SUBCASE("StatsSystem calculates bonus levels") {
        StatsSystem::Recalculate(registry, player);
        
        // Flowing Thrust: 2 (All) + 1 (Specific) = 3
        CHECK(active.specialized_slots[0].bonus_levels == 3);
        // Rending Wave: 2 (All) = 2
        CHECK(active.specialized_slots[1].bonus_levels == 2);
    }
}