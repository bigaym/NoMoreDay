#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/components/Stats.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/ItemStats.hpp"

using namespace NoMoreDay;

TEST_CASE("Stats Recalculation from Primary Stats") {
    entt::registry registry;
    auto entity = registry.create();

    // Setup base components
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity, PrimaryStats{
        .strength = 10.0f,
        .dexterity = 20.0f,
        .intelligence = 5.0f,
        .vitality = 15.0f
    });
    registry.emplace<StatsDirty>(entity);

    // Run system
    StatsSystem::update(registry);

    const auto& combat = registry.get<CombatStats>(entity);

    // Verify derivations
    // HP: Base 100 + Vit 15 * 10 = 250
    CHECK(combat.max_health == doctest::Approx(250.0f));
    
    // Armor: Base 0 + Str 10 * 1 = 10
    CHECK(combat.armor == doctest::Approx(10.0f));

    // Mana: Base 100 + Int 5 * 2 = 110
    CHECK(combat.max_mana == doctest::Approx(110.0f));
    
    // Check clean
    CHECK_FALSE(registry.all_of<StatsDirty>(entity));
}

TEST_CASE("Stats Modifier Stacking Rules") {
    entt::registry registry;
    auto entity = registry.create();

    // Setup: Base HP 100 (from StatsSystem default)
    // PrimaryStats: Vit 0 for simplicity (so base remains 100)
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity); 
    
    ModifierList mods;
    // 1. Flat: +50 HP -> Base becomes 150
    mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::Flat, 50.0f, ModifierSource::Item});
    
    // 2. PercentAdd: +10% and +20% -> Total +30%
    // Formula: (Base + Flat) * (1 + Sum(PercentAdd))
    // 150 * 1.3 = 195
    mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentAdd, 10.0f, ModifierSource::Skill});
    mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentAdd, 20.0f, ModifierSource::Buff});

    // 3. PercentMult: x1.5 (50% more)
    // Formula: Result * Product(PercentMult)
    // 195 * 1.5 = 292.5
    mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentMult, 50.0f, ModifierSource::Buff});

    registry.emplace<ModifierList>(entity, mods);
    registry.emplace<StatsDirty>(entity);

    // Run System
    StatsSystem::update(registry);

    const auto& combat = registry.get<CombatStats>(entity);
    
    // Expected: (100 + 50) * (1 + 0.10 + 0.20) * (1.5) 
    //         = 150 * 1.3 * 1.5 
    //         = 195 * 1.5 
    //         = 292.5
    CHECK(combat.max_health == doctest::Approx(292.5f));
}

TEST_CASE("Stats System - Equipment Integration") {
    entt::registry registry;
    auto entity = registry.create();

    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity); // All 0
    registry.emplace<StatsDirty>(entity);

    // Create an item entity (Chest Armor)
    auto item = registry.create();
    ItemComponent itemComp;
    itemComp.type = ItemType::Armor;
    itemComp.slot = EquipmentSlot::Chest;
    itemComp.defense = 50.0f; // Base Armor
    
    // Add implicit: +10 Vitality
    itemComp.implicits.push_back({AffixType::Vitality, 10.0f, 1, false, "Implicit Vit"});
    
    // Add explicit: +5% Health
    itemComp.affixes.push_back({AffixType::PercentHealth, 5.0f, 1, false, "Increased Health"});
    
    registry.emplace<ItemComponent>(item, itemComp);

    // Equip the item
    EquipmentComponent equip;
    equip.set(EquipmentSlot::Chest, item);
    registry.emplace<EquipmentComponent>(entity, equip);

    // Run System
    StatsSystem::update(registry);

    const auto& combat = registry.get<CombatStats>(entity);

    // Verification:
    // 1. Vitality: 0 (Base) + 10 (Item) = 10 Total Vit
    // 2. Base Health: 100 + (10 Vit * 10) = 200 Base HP
    // 3. Modifiers: +5% Percent Add
    // 4. Final HP: 200 * 1.05 = 210
    CHECK(combat.max_health == doctest::Approx(210.0f));

    // 5. Armor: 0 (Base) + 50 (Item Base) = 50 Total Armor
    // Note: If Str was increased, it would add to armor too.
    // Vit added 10, Str added 0.
    CHECK(combat.armor == doctest::Approx(50.0f));
}
