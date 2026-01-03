#pragma once
#include "doctest.h"
#include "entt/entt.hpp"
#include "../src/systems/MovementStanceSystem.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"

using namespace NoMoreDay;

TEST_CASE("Movement Stance: Lifecycle and Buffs") {
    entt::registry registry;
    
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<MovementStanceComponent>(player);
    registry.emplace<CombatStats>(player);
    
    // Add a modifier that only applies during Sword Riding: +100% Move Speed
    auto& list = registry.emplace<ModifierList>(player);
    list.modifiers.push_back({
        StatType::MoveSpeed,
        ModifierMode::PercentAdd,
        100.0f,
        Tag::SwordRiding
    });

    SUBCASE("Stationary: Stays Walking") {
        MovementStanceSystem::Update(registry, 1.0f);
        auto& stance = registry.get<MovementStanceComponent>(player);
        CHECK(stance.stance == MovementStance::Walking);
        CHECK(stance.movingTimer == 0.0f);
    }

    SUBCASE("Moving: Transition to Sword Riding") {
        auto& vel = registry.get<Velocity>(player);
        vel.vx = 300.0f; // Moving!

        // Move for 1s
        MovementStanceSystem::Update(registry, 1.0f);
        auto& stance = registry.get<MovementStanceComponent>(player);
        CHECK(stance.stance == MovementStance::Walking);
        CHECK(stance.movingTimer == 1.0f);

        // Move for another 1.1s (Total 2.1s > 2.0s)
        MovementStanceSystem::Update(registry, 1.1f);
        CHECK(stance.stance == MovementStance::SwordRiding);
        
        // Verify stats are marked dirty
        CHECK(registry.all_of<StatsDirty>(player));
        
        // Recalculate stats
        StatsSystem::update(registry);
        auto& combat = registry.get<CombatStats>(player);
        // Base 300 + 100% = 600
        CHECK(combat.move_speed == doctest::Approx(600.0f));
    }

    SUBCASE("Interruption: Stopped Moving") {
        auto& vel = registry.get<Velocity>(player);
        vel.vx = 300.0f;
        MovementStanceSystem::Update(registry, 2.1f); // Enter stance
        
        auto& stance = registry.get<MovementStanceComponent>(player);
        CHECK(stance.stance == MovementStance::SwordRiding);

        // Stop moving
        vel.vx = 0.0f;
        MovementStanceSystem::Update(registry, 0.1f);
        CHECK(stance.stance == MovementStance::Walking);
        CHECK(stance.movingTimer == 0.0f);
        
        StatsSystem::update(registry);
        auto& combat = registry.get<CombatStats>(player);
        CHECK(combat.move_speed == 300.0f);
    }

    SUBCASE("Interruption: Taking Damage") {
        auto& vel = registry.get<Velocity>(player);
        vel.vx = 300.0f;
        MovementStanceSystem::Update(registry, 2.1f);
        
        auto& stance = registry.get<MovementStanceComponent>(player);
        CHECK(stance.stance == MovementStance::SwordRiding);

        // Take damage
        MovementStanceSystem::OnTakeDamage(registry, player);
        CHECK(stance.stance == MovementStance::Walking);
        CHECK(stance.movingTimer == 0.0f);
    }
}
