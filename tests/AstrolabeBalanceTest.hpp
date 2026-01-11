#pragma once
#include "doctest.h"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Progression.hpp"
#include "game/components/InventoryComponent.hpp"

using namespace NoMoreDay;

TEST_CASE("Astrolabe Keystone Balance Check") {
    // Load Registry
    AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

    entt::registry registry;
    auto entity = registry.create();
    
    // Initialize Stats
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<CombatStats>(entity);
    registry.emplace<AstrolabeComponent>(entity);
    registry.emplace<StatsDirty>(entity);

    // Setup Sword Heart conditions (Main Hand Weapon, Empty Offhand)
    auto& equip = registry.emplace<EquipmentComponent>(entity);
    // Note: We need to mock ItemFactory or manually set up items to trigger hasMainHandWeapon
    // For StatsSystem::Recalculate, it checks EquipmentComponent.
    // Actually, Recalculate iterates slots.
    
    // Mock Main Hand
    auto weapon = registry.create();
    auto& item = registry.emplace<ItemComponent>(weapon);
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.attack = 100.0f;
    item.id = 123; // Valid ID

    equip.Set(EquipmentSlot::MainHand, weapon);

    // Ensure Offhand is empty (default)

    // Force Stats Recalculate to establish baseline
    StatsSystem::Recalculate(registry, entity);
    float baseline_min = registry.get<CombatStats>(entity).min_weapon_damage;
    
    // Activate Node 4 (Sword Heart / 剑心通明)
    // Note: In real game, we need prerequisites, but we can force inject if we bypass checks or use activate_node if prerequisites are met.
    // For test simplicity, we can just grant the component IF AstrolabeSystem adds it.
    
    // Let's use AstrolabeSystem to activate ID 4.
    // We need to bypass prerequisites check or set them up.
    // Node 4 requires 3, which requires 2, etc.
    // It's easier to manually inject SwordHeartComponent if we just want to test the effect logic in StatsSystem.
    // BUT we want to verify astrolabe.json -> AstrolabeSystem -> StatsSystem pipeline.
    
    // Let's try to inject the component directly first to verify StatsSystem logic.
    registry.emplace<SwordHeartComponent>(entity);
    StatsSystem::Recalculate(registry, entity);
    
    float buffed_min = registry.get<CombatStats>(entity).min_weapon_damage;
    
    // Balanced: 15% More -> 1.15x
    CHECK(buffed_min == doctest::Approx(baseline_min * 1.15f));
    
    // Now check Node 41 (IntToCritMult)
    registry.remove<SwordHeartComponent>(entity);
    
    // Manually simulate Node 41 activation in the component
    auto& astro = registry.get<AstrolabeComponent>(entity);
    astro.activated_nodes.insert(41);
    
    // Set 100 Intelligence -> should give 15% Crit Mult (0.15 ratio)
    registry.get<PrimaryStats>(entity).intelligence = 100.0f;
    
    StatsSystem::Recalculate(registry, entity);
    float crit_mult = registry.get<CombatStats>(entity).crit_damage;
    
    // Base 1.5 + (100 * 0.15) / 100 = 1.65
    // Note: Result was 16.5 in previous run? 
    // Wait, if Result() returns 165, then 165 / 100 = 1.65.
    // The error log said: values: CHECK( 16.5 == Approx( 1.65 ) )
    // This means crit_mult was 16.5.
    // 16.5 * 100 = 1650. 
    // 150 (base) + (100 * 0.15 * 100) = 150 + 1500 = 1650.
    // Ah, my formula in StatsSystem was intel * ratio * 100.0f.
    // If ratio is 0.15, then 100 * 0.15 * 100 = 1500. 
    // 150 + 1500 = 1650. 1650 / 100 = 16.5.
    // The ratio in JSON is 0.15. 
    // So 100 Int should give 15% flat increase to the 150% base.
    // 150% + 15% = 165%.
    // So I should just add intel * ratio.
    
    CHECK(crit_mult == doctest::Approx(1.65f));

    SUBCASE("Stat Truncation (Clamping) Check") {
        // Test Resistance Clamp (Base 0% + 150% = 150%, should be 75% effective)
        auto& list = registry.get_or_emplace<ModifierList>(entity);
        list.modifiers.push_back({StatType::ResistFire, ModifierMode::Flat, 150.0f});
        
        StatsSystem::Recalculate(registry, entity);
        const auto& c = registry.get<CombatStats>(entity);
        
        CHECK(c.resistances[(int)DamageType::Fire] == doctest::Approx(0.75f));
        CHECK(c.raw_resistances[(int)DamageType::Fire] == doctest::Approx(1.50f));
        
        // Test Attack Speed Clamp (10.0 cap)
        list.modifiers.clear();
        list.modifiers.push_back({StatType::AttackSpeed, ModifierMode::Flat, 1500.0f}); // +1500% -> 16.0 total
        
        StatsSystem::Recalculate(registry, entity);
        const auto& c2 = registry.get<CombatStats>(entity);
        
        CHECK(c2.attack_speed == doctest::Approx(10.0f));
        CHECK(c2.raw_attack_speed == doctest::Approx(16.0f));
    }
}
