#pragma once

#include "../src/components/Stats.hpp"
#include "../src/components/Combat.hpp"
#include "../src/systems/CombatSystem.hpp"
#include "../src/components/Common.hpp" // For KilledTag
#include "../src/components/PlayerState.hpp" // For PlayerStats
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("Damage Calculation - Armor Mitigation") {

    CombatStats attacker;
    CombatStats defender;
    
    // Reset stats
    attacker = CombatStats();
    defender = CombatStats();
    
    float baseDamage = 100.0f;
    
    // Case 1: 0 Armor -> 100 Damage
    defender.armor = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(100.0f));
    
    // Case 2: 100 Armor. 
    // Formula: Reduction = Armor / (Armor + 100).
    // 100 / 200 = 0.5. Damage = 100 * 0.5 = 50.
    defender.armor = 100.0f;
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(50.0f));
}

TEST_CASE("Damage Calculation - Resistance") {

    CombatStats attacker;
    CombatStats defender;
    
    attacker = CombatStats();
    defender = CombatStats();
    
    float baseDamage = 100.0f;
    
    // Case 1: 0 Res -> 100 Damage
    defender.resistances[(int)DamageType::Fire] = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(100.0f));
    
    // Case 2: 50% Res -> 50 Damage
    defender.resistances[(int)DamageType::Fire] = 0.5f; // 50%
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(50.0f));
    
    // Case 3: Cap check (75%)?
    // Spec says "Resistance caps".
    defender.resistances[(int)DamageType::Fire] = 0.9f; // 90%
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    // Assuming hard cap at 75% for now, so reduction is 75%, dmg is 25.
    CHECK(damage == doctest::Approx(25.0f));
}

TEST_CASE("CombatSystem - ApplyDamage") {

    entt::registry registry;
    auto entity = registry.create();
    
    // Setup Health
    HealthComponent health;
    health.max = 100.0f;
    health.current = 100.0f;
    registry.emplace<HealthComponent>(entity, health);

    // Apply non-fatal damage
    bool dead = CombatSystem::ApplyDamage(registry, entity, 30.0f);
    CHECK(dead == false);
    CHECK(registry.get<HealthComponent>(entity).current == doctest::Approx(70.0f));

    // Apply fatal damage
    dead = CombatSystem::ApplyDamage(registry, entity, 80.0f);
    CHECK(dead == true);
    
    // Entity should still be valid because destruction is deferred to XPAwardingSystem
    CHECK(registry.valid(entity) == true);
    // Entity should have KilledTag
    CHECK(registry.all_of<KilledTag>(entity) == true);
}
