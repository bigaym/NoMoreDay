#pragma once

#include "../src/components/Stats.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/EquipmentComponent.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/core/ItemFactory.hpp"
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("Affix System Integration Test") {
    ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    
    entt::registry registry;

    SUBCASE("StatsSystem applies Affixes correctly") {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, 10.0f, 10.0f, 10.0f, 10.0f); // Base stats
        
        // Create an item with known affixes
        auto sword = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.attack = 10.0f;
        
        // Add a Prefix: +10 Strength
        Affix strengthAffix;
        strengthAffix.type = AffixType::Strength;
        strengthAffix.value = 10.0f;
        strengthAffix.isPrefix = true;
        item.affixes.push_back(strengthAffix);
        
        // Add a Suffix: +50 Flat Health
        Affix healthAffix;
        healthAffix.type = AffixType::FlatHealth;
        healthAffix.value = 50.0f;
        healthAffix.isPrefix = false;
        item.affixes.push_back(healthAffix);
        
        registry.emplace<ItemComponent>(sword, item);
        
        // Equip it
        EquipmentComponent equipment;
        equipment.slots[0] = sword; // MainHand
        registry.emplace<EquipmentComponent>(entity, equipment);
        
        // Recalculate
        StatsSystem::Recalculate(registry, entity);
        
        const auto& stats = registry.get<CombatStats>(entity);
        
        // Check Strength: 10 Base + 10 Affix = 20
        CHECK(stats.effective_strength == doctest::Approx(20.0f));
        
        // Check Max Health: 
        // Base 100
        // Vitality 10 -> +150 (10 * 15)
        // Affix +50
        // Total = 100 + 150 + 50 = 300
        CHECK(stats.max_health == doctest::Approx(300.0f));
    }
    
    SUBCASE("Affix Generation Constraints") {
        // Test level-based tiering
        // For Strength (ID: strength), T1 is level 1, T2 is level 11, etc.
        
        // Level 5: Should be T1
        Affix aff1 = ItemFactory::generateRandomAffix(5, false, EquipmentSlot::Chest);
        CHECK(aff1.tier == 1);
        
        // Level 25: Should be T3 or lower (Wait, my logic selects HIGHEST available tier)
        // Level 25 has T3 (minLevel 21) available.
        Affix aff2 = ItemFactory::generateRandomAffix(25, false, EquipmentSlot::Chest);
        CHECK(aff2.tier == 3);

        // Level 100: Should be T7 (max)
        Affix aff3 = ItemFactory::generateRandomAffix(100, false, EquipmentSlot::Chest);
        CHECK(aff3.tier == 7);
    }

    SUBCASE("Tag-based filtering") {
        // Boots should only get "boots" or "armor" tagged affixes
        // Move Speed (ID: move_speed) is tagged "boots"
        // Let's see if we can get it.
        bool foundMoveSpeed = false;
        for (int i = 0; i < 50; ++i) {
            Affix aff = ItemFactory::generateRandomAffix(50, false, EquipmentSlot::Feet);
            if (aff.type == AffixType::MoveSpeed) {
                foundMoveSpeed = true;
                break;
            }
        }
        CHECK(foundMoveSpeed);

        // Chest should NEVER get Move Speed (it's tagged "boots" only in JSON)
        bool foundInvalid = false;
        for (int i = 0; i < 50; ++i) {
            Affix aff = ItemFactory::generateRandomAffix(50, false, EquipmentSlot::Chest);
            if (aff.type == AffixType::MoveSpeed) {
                foundInvalid = true;
                break;
            }
        }
        CHECK_FALSE(foundInvalid);
    }
    
    SUBCASE("ModifierList Application") {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        
        ModifierList mods;
        StatModifier mod;
        mod.type = StatType::FireDamage;
        mod.mode = ModifierMode::PercentAdd;
        mod.value = 50.0f; // +50% Fire Damage
        mods.modifiers.push_back(mod);
        
        registry.emplace<ModifierList>(entity, mods);
        
        StatsSystem::Recalculate(registry, entity);
        
        const auto& stats = registry.get<CombatStats>(entity);
        // Default Fire Damage Multiplier is 1.0. With +50%, it should be 1.5.
        // Note: damage_multipliers init to 1.0.
        // In StatsSystem: multiplier = (1.0 + percent_add) * percent_mult
        // Here percent_add = 0.5. percent_mult = 1.0.
        // Result = 1.5.
        // AND we add it to combat.damage_multipliers (which are initialized to 1.0).
        // Wait, StatsSystem code:
        // combat.damage_multipliers[(int)dType] = c.GetMultiplier();
        // c.GetMultiplier() returns (1+add)*mult.
        // So it overrides the default 1.0 in combat stats (since calcs start fresh).
        // Correct.
        
        CHECK(stats.damage_multipliers[(int)DamageType::Fire] == doctest::Approx(1.5f));
    }
}
