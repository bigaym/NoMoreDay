#pragma once

#include "../src/components/Stats.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/ItemStats.hpp"
#include "../src/components/Common.hpp" // For WeaponComponent
#include <entt/entt.hpp>
#include "TestCommon.hpp"

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
    // HP: Base 100 + Vit 15 * 15 = 325
    CHECK(combat.max_health == doctest::Approx(325.0f));
    
    // Armor: Base 0 + Str 10 * 2 = 20
    CHECK(combat.armor == doctest::Approx(20.0f));

    // Mana: Base 100 + Int 5 * 5 = 125
    CHECK(combat.max_mana == doctest::Approx(125.0f));
    
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
    // 2. Base Health: 100 + (10 Vit * 15) = 250 Base HP
    // 3. Modifiers: +5% Percent Add
    // 4. Final HP: 250 * 1.05 = 262.5
    CHECK(combat.max_health == doctest::Approx(262.5f));

    // 5. Armor: 0 (Base) + 50 (Item Base) = 50 Total Armor
    // Note: If Str was increased, it would add to armor too.
    // Vit added 10, Str added 0.
    CHECK(combat.armor == doctest::Approx(50.0f));
}

TEST_CASE("Stats System - Weapon Damage Logic") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    SUBCASE("Unarmed (No Equipment)") {
        // Ensure EquipmentComponent exists but is empty
        registry.emplace<EquipmentComponent>(entity);
        
        StatsSystem::update(registry);
        const auto& combat = registry.get<CombatStats>(entity);

        // Default unarmed damage defined in StatsSystem
        CHECK(combat.min_weapon_damage == doctest::Approx(2.0f));
        CHECK(combat.max_weapon_damage == doctest::Approx(3.0f));
    }

    SUBCASE("Equipped Weapon") {
        EquipmentComponent equip;
        auto weapon = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.attack = 100.0f;
        registry.emplace<ItemComponent>(weapon, item);
        
        equip.set(EquipmentSlot::MainHand, weapon);
        registry.emplace<EquipmentComponent>(entity, equip);

        StatsSystem::update(registry);
        const auto& combat = registry.get<CombatStats>(entity);

        // Logic: Min = Attack * 0.9, Max = Attack * 1.1
        CHECK(combat.min_weapon_damage == doctest::Approx(90.0f));
        CHECK(combat.max_weapon_damage == doctest::Approx(110.0f));
    }

    SUBCASE("Monster Weapon Component (Fallback)") {
        // Monsters might not have EquipmentComponent, but have WeaponComponent
        WeaponComponent weaponComp;
        weaponComp.damage = 50.0f;
        registry.emplace<WeaponComponent>(entity, weaponComp);
        
        StatsSystem::update(registry);
        const auto& combat = registry.get<CombatStats>(entity);

        CHECK(combat.min_weapon_damage == doctest::Approx(50.0f));
        CHECK(combat.max_weapon_damage == doctest::Approx(50.0f));
    }
}

TEST_CASE("Stats System - Offensive Stats & Dexterity") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    
    // 50 Dexterity
    // Effect: +0.2% Crit Chance per Dex -> 50 * 0.002 = 0.10 (10%)
    registry.emplace<PrimaryStats>(entity, PrimaryStats{.dexterity = 50.0f});
    
    // Item with Attack Speed and Crit Chance
    auto item = registry.create();
    ItemComponent itemComp;
    itemComp.slot = EquipmentSlot::Hands; // Gloves
    itemComp.affixes.push_back({AffixType::CritChance, 5.0f, 1, false, "+5% Crit"});
    itemComp.affixes.push_back({AffixType::AttackSpeed, 15.0f, 1, false, "+15% AS"});
    registry.emplace<ItemComponent>(item, itemComp);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::Hands, item);
    registry.emplace<EquipmentComponent>(entity, equip);

    registry.emplace<StatsDirty>(entity);
    StatsSystem::update(registry);

    const auto& combat = registry.get<CombatStats>(entity);

    // Crit Chance: Base 0.05 + Dex 0.10 + Item 0.05 = 0.20 (20%)
    CHECK(combat.crit_chance == doctest::Approx(0.20f));
    
    // Attack Speed: Base 1.0 + Item 0.15 = 1.15
    CHECK(combat.attack_speed == doctest::Approx(1.15f));
}

TEST_CASE("Stats System - Elemental Damage & Resistances") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    
    // Item with Flat Fire Damage and Fire Resistance
    auto item = registry.create();
    ItemComponent itemComp;
    itemComp.slot = EquipmentSlot::Ring1;
    itemComp.affixes.push_back({AffixType::FlatFireDamage, 25.0f, 1, false, "Add Fire Dmg"});
    itemComp.affixes.push_back({AffixType::ResistFire, 30.0f, 1, false, "Fire Res"});
    itemComp.affixes.push_back({AffixType::PercentLightningDamage, 10.0f, 1, false, "Inc Lightning Dmg"});
    registry.emplace<ItemComponent>(item, itemComp);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::Ring1, item);
    registry.emplace<EquipmentComponent>(entity, equip);

    registry.emplace<StatsDirty>(entity);
    StatsSystem::update(registry);

    const auto& combat = registry.get<CombatStats>(entity);

    // Flat Fire Damage
    CHECK(combat.flat_damage[(int)DamageType::Fire] == doctest::Approx(25.0f));
    // Other flat damage should be 0
    CHECK(combat.flat_damage[(int)DamageType::Cold] == doctest::Approx(0.0f));

    // Fire Resistance
    CHECK(combat.resistances[(int)DamageType::Fire] == doctest::Approx(0.30f));

    // Lightning Damage Multiplier: Base 1.0 + 0.10 = 1.10
    CHECK(combat.damage_multipliers[(int)DamageType::Lightning] == doctest::Approx(1.10f));
    // Physical Multiplier (Base 1.0 + Str 0)
    CHECK(combat.damage_multipliers[(int)DamageType::Physical] == doctest::Approx(1.0f));
}

TEST_CASE("Stats System - Dual Wielding") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    // Main Hand Weapon: 100 dmg
    auto mh = registry.create();
    ItemComponent mhItem;
    mhItem.type = ItemType::Weapon;
    mhItem.slot = EquipmentSlot::MainHand;
    mhItem.attack = 100.0f;
    registry.emplace<ItemComponent>(mh, mhItem);

    // Off Hand Weapon: 50 dmg
    auto oh = registry.create();
    ItemComponent ohItem;
    ohItem.type = ItemType::Weapon;
    ohItem.slot = EquipmentSlot::OffHand;
    ohItem.attack = 50.0f;
    registry.emplace<ItemComponent>(oh, ohItem);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::MainHand, mh);
    equip.set(EquipmentSlot::OffHand, oh);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    // Logic: Avg Attack = (100 + 50) / 2 = 75
    // Min = 75 * 0.9 = 67.5, Max = 75 * 1.1 = 82.5
    CHECK(combat.min_weapon_damage == doctest::Approx(67.5f));
    CHECK(combat.max_weapon_damage == doctest::Approx(82.5f));
    
    // Dual Wield Bonus: +15% Attack Speed (Base 1.0 + 0.15 = 1.15)
    CHECK(combat.attack_speed == doctest::Approx(1.15f));
}

TEST_CASE("Stats System - Shield Logic") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    // Create Shield Item
    auto shield = registry.create();
    ItemComponent item;
    item.type = ItemType::Shield;
    item.slot = EquipmentSlot::OffHand;
    item.defense = 40.0f; // 40 Armor, should also be 40 Block Amount
    registry.emplace<ItemComponent>(shield, item);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::OffHand, shield);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    CHECK(combat.block_chance == doctest::Approx(0.20f)); // Base 20%
    CHECK(combat.block_amount == doctest::Approx(40.0f)); // From defense
    CHECK(combat.armor == doctest::Approx(40.0f));        // Shield defense adds to Armor too
}

TEST_CASE("Stats System - Two Handed Weapon") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    // Create 2H Weapon
    auto weapon = registry.create();
    ItemComponent item;
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.attack = 100.0f;
    item.isTwoHanded = true;
    registry.emplace<ItemComponent>(weapon, item);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::MainHand, weapon);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    // Logic: Base (90-110) * 1.25 (2H Bonus)
    // Min = 90 * 1.25 = 112.5
    // Max = 110 * 1.25 = 137.5
    CHECK(combat.min_weapon_damage == doctest::Approx(112.5f));
    CHECK(combat.max_weapon_damage == doctest::Approx(137.5f));
}

TEST_CASE("Stats System - Set Bonuses") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    // Define Set Bonuses
    std::vector<SetBonus> setBonuses;
    setBonuses.push_back({2, {{AffixType::Strength, 20.0f, 1, false, "+20 Str"}}});
    setBonuses.push_back({3, {{AffixType::PercentFireDamage, 50.0f, 1, false, "+50% Fire Dmg"}}});

    // Item 1: Set Helm
    auto helm = registry.create();
    ItemComponent helmItem;
    helmItem.type = ItemType::Armor;
    helmItem.slot = EquipmentSlot::Head;
    helmItem.rarity = Rarity::Set;
    helmItem.setName = "Fire King's Regalia";
    helmItem.setBonuses = setBonuses;
    registry.emplace<ItemComponent>(helm, helmItem);

    // Item 2: Set Chest
    auto chest = registry.create();
    ItemComponent chestItem;
    chestItem.type = ItemType::Armor;
    chestItem.slot = EquipmentSlot::Chest;
    chestItem.rarity = Rarity::Set;
    chestItem.setName = "Fire King's Regalia";
    chestItem.setBonuses = setBonuses;
    registry.emplace<ItemComponent>(chest, chestItem);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::Head, helm);
    equip.set(EquipmentSlot::Chest, chest);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    // 2 Items equipped -> Should trigger 2-piece bonus (+20 Str)
    // Base Str 0 + 20 = 20
    CHECK(combat.effective_strength == doctest::Approx(20.0f));
    // 3-piece bonus not triggered
    CHECK(combat.damage_multipliers[(int)DamageType::Fire] == doctest::Approx(1.0f));
}

TEST_CASE("Stats System - Life Steal & Life On Hit") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<StatsDirty>(entity);

    // Item with Life Steal and Life On Hit
    auto item = registry.create();
    ItemComponent itemComp;
    itemComp.slot = EquipmentSlot::Ring1;
    itemComp.affixes.push_back({AffixType::LifeSteal, 5.0f, 1, false, "5% LS"});
    itemComp.affixes.push_back({AffixType::LifeOnHit, 10.0f, 1, false, "10 LoH"});
    registry.emplace<ItemComponent>(item, itemComp);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::Ring1, item);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    // Life Steal: 5% -> 0.05
    CHECK(combat.life_steal == doctest::Approx(0.05f));
    // Life On Hit: 10
    CHECK(combat.life_on_hit == doctest::Approx(10.0f));
}

TEST_CASE("Stats System - Accuracy") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity, PrimaryStats{.dexterity = 100.0f}); // 100 Dex -> +10% Accuracy
    registry.emplace<StatsDirty>(entity);

    // Item with Accuracy
    auto item = registry.create();
    ItemComponent itemComp;
    itemComp.slot = EquipmentSlot::Hands;
    itemComp.affixes.push_back({AffixType::Accuracy, 20.0f, 1, false, "+20% Accuracy"});
    registry.emplace<ItemComponent>(item, itemComp);

    EquipmentComponent equip;
    equip.set(EquipmentSlot::Hands, item);
    registry.emplace<EquipmentComponent>(entity, equip);

    StatsSystem::update(registry);
    const auto& combat = registry.get<CombatStats>(entity);

    // Base 0.97 + Dex 0.1 (100*0.001) + Item 0.2 = 1.27
    CHECK(combat.accuracy == doctest::Approx(1.27f));
}
