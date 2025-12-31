#pragma once

#include "../src/components/Stats.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/ItemStats.hpp"
#include "../src/systems/CraftingSystem.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/core/ItemFactory.hpp"
#include "../src/components/EquipmentComponent.hpp"
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("Item Modification System Test") {
    ItemFactory::initialize();
    entt::registry registry;

    SUBCASE("Refinement: Affix Values") {
        ItemComponent item;
        item.name = "Test Sword";
        item.forgingPotential = 50;
        
        Affix aff;
        aff.type = AffixType::Strength;
        aff.tier = 1;
        aff.value = 1.0f; // Below typical T1 range
        item.affixes.push_back(aff);
        
        // Strength T1 range in affixes.json is typically [3, 5]
        auto result = CraftingSystem::refineAffixValues(item, 0);
        CHECK(result == CraftingResult::Success);
        
        // Value should now be in some valid range (at least not 1.0 if it rerolled)
        // If it used fallback, it might still be 0 if not found, but strength is in definitions.
        CHECK(item.affixes[0].value != 1.0f);
        CHECK(item.forgingPotential < 50);
    }

    SUBCASE("Refinement: Base Stats") {
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.name = "Rusty Sword"; // Valid base name in ItemFactory
        item.attack = 1.0f;
        item.forgingPotential = 50;
        
        auto result = CraftingSystem::refineBaseStats(item);
        CHECK(result == CraftingResult::Success);
        // Rusty Sword range is [5.0, 8.0]
        CHECK(item.attack >= 5.0f);
        CHECK(item.attack <= 8.0f);
        CHECK(item.forgingPotential < 50);
    }

    SUBCASE("Rune Socketing and Stats Integration") {
        auto player = registry.create();
        registry.emplace<CombatStats>(player);
        registry.emplace<PrimaryStats>(player);
        
        auto swordEntity = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.name = "Socketed Sword";
        item.socketCount = 2;
        item.sockets.resize(2, entt::null);
        registry.emplace<ItemComponent>(swordEntity, item);
        
        auto runeEntity = registry.create();
        ItemComponent runeItem;
        runeItem.name = "Fire Rune";
        runeItem.type = ItemType::Material;
        registry.emplace<ItemComponent>(runeEntity, runeItem);
        
        RuneComponent runeComp;
        Affix fireDmg;
        fireDmg.type = AffixType::FlatFireDamage;
        fireDmg.value = 10.0f;
        runeComp.weaponEffects.push_back(fireDmg);
        registry.emplace<RuneComponent>(runeEntity, runeComp);
        
        // Socket it
        auto result = CraftingSystem::socketRune(registry, swordEntity, runeEntity, 0);
        CHECK(result == CraftingResult::Success);
        
        // Re-get item from registry to check
        auto& itemInReg = registry.get<ItemComponent>(swordEntity);
        bool isCorrectRune = (itemInReg.sockets[0] == runeEntity);
        CHECK(isCorrectRune);

        // Check if rune has component
        CHECK(registry.all_of<RuneComponent>(runeEntity));

        // Equip sword
        EquipmentComponent equipment;
        equipment.slots[0] = swordEntity;
        registry.emplace<EquipmentComponent>(player, equipment);
        
        // Recalculate stats
        StatsSystem::Recalculate(registry, player);
        
        const auto& stats = registry.get<CombatStats>(player);
        // Fire Damage should be 10.0 from rune
        CHECK(stats.flat_damage[(int)DamageType::Fire] == doctest::Approx(10.0f));
        
        // Unsocket
        auto result2 = CraftingSystem::unsocketRune(registry, swordEntity, 0);
        CHECK(result2 == CraftingResult::Success);
        
        bool isNull = (itemInReg.sockets[0] == entt::null);
        CHECK(isNull);

        StatsSystem::Recalculate(registry, player);
        const auto& stats2 = registry.get<CombatStats>(player);
        CHECK(stats2.flat_damage[(int)DamageType::Fire] == doctest::Approx(0.0f));
    }
}
