#pragma once

#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/components/Common.hpp" // For KilledTag
#include "game/components/PlayerState.hpp" // For PlayerStats
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

TEST_CASE("CombatSystem - Monster to Player Attack") {
    entt::registry registry;
    NoMoreDay::systems::SpatialHashGrid grid(100, 100, 10.0f);
    Camera2D camera = {0};

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    registry.emplace<NoMoreDay::CombatStats>(player);

    auto monster = registry.create();
    registry.emplace<EnemyTag>(monster);
    registry.emplace<Position>(monster, 10.0f, 0.0f);
    auto& ai = registry.emplace<AIComponent>(monster);
    ai.aiType = AIType::ATTACK;
    ai.target = player;
    ai.attackRange = 50.0f;

    auto& eStats = registry.emplace<NoMoreDay::CombatStats>(monster);
    eStats.min_weapon_damage = 10.0f;
    eStats.max_weapon_damage = 10.0f;

    auto& eAttack = registry.emplace<NoMoreDay::AttackState>(monster);
    eAttack.cooldownTimer = 0.0f;
    eAttack.baseAttackInterval = 1.0f;

    // First update: Should attack
    CombatSystem::update(registry, grid, camera, 0.1f);

    auto& pHealth = registry.get<HealthComponent>(player);
    CHECK(pHealth.current < 100.0f); // Damage applied
    CHECK(eAttack.cooldownTimer > 0.0f); // Cooldown set

    float healthAfterFirst = pHealth.current;

    // Second update: Cooldown active, should NOT attack
    CombatSystem::update(registry, grid, camera, 0.1f);
    CHECK(pHealth.current == doctest::Approx(healthAfterFirst));
}
