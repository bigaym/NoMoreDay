#pragma once

#include "../src/components/PlayerState.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/Common.hpp" // For KilledTag
#include "../src/systems/ProgressionSystem.hpp"
#include "../src/systems/XPAwardingSystem.hpp" // For XPAwardingSystem
#include <entt/entt.hpp>

TEST_CASE("ProgressionSystem - XP Scaling") {
    // Level 1 should be 100 (baseline)
    CHECK(ProgressionSystem::CalculateRequiredXP(1) == doctest::Approx(100.0f));
    
    // Level 2 should be higher
    CHECK(ProgressionSystem::CalculateRequiredXP(2) > 100.0f);
    
    // Test a few levels to ensure it's increasing
    float xp1 = ProgressionSystem::CalculateRequiredXP(1);
    float xp2 = ProgressionSystem::CalculateRequiredXP(2);
    float xp3 = ProgressionSystem::CalculateRequiredXP(3);
    
    CHECK(xp2 > xp1);
    CHECK(xp3 > xp2);
}

TEST_CASE("ProgressionSystem - Experience Gain and Level Up") {
    entt::registry registry;
    auto player = registry.create();
    
    registry.emplace<PlayerLevel>(player, 1);
    registry.emplace<PlayerStats>(player, 0ULL, 0ULL, 1, 0.0f, 100.0f, 0, 0, 10, 10, 10, 10);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    
    // 1. Gain some XP (not enough to level up)
    ProgressionSystem::AddExperience(registry, player, 50.0f);
    
    const auto& stats = registry.get<PlayerStats>(player);
    CHECK(stats.current_xp == doctest::Approx(50.0f));
    CHECK(stats.level == 1);
    
    // 2. Gain enough XP to level up
    ProgressionSystem::AddExperience(registry, player, 60.0f); // Total 110, requirement 100
    
    const auto& levelComp = registry.get<PlayerLevel>(player);
    const auto& updatedStats = registry.get<PlayerStats>(player);
    const auto& primStats = registry.get<PrimaryStats>(player);
    
    CHECK(updatedStats.level == 2);
    CHECK(levelComp.value == 2);
    
    // Carry over XP: 110 - 100 = 10
    CHECK(updatedStats.current_xp == doctest::Approx(10.0f));
    
    // Points awarded
    CHECK(updatedStats.available_attribute_points == 5);
    CHECK(updatedStats.available_skill_points == 1);
    
    // Auto stat growth was removed in favor of manual allocation
    // CHECK(primStats.strength > 10.0f);
    // CHECK(primStats.vitality > 10.0f);
    
    // Required XP for level 2 should be updated
    CHECK(updatedStats.required_xp == doctest::Approx(ProgressionSystem::CalculateRequiredXP(2)));
}

TEST_CASE("ProgressionSystem - Multi-Level Up") {
    entt::registry registry;
    auto player = registry.create();
    
    registry.emplace<PlayerLevel>(player, 1);
    registry.emplace<PlayerStats>(player, 0ULL, 0ULL, 1, 0.0f, 100.0f, 0, 0);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    
    // Add massive XP to jump multiple levels
    // Level 1->2: 100 XP
    // Level 2->3: 282 XP
    // Level 3->4: 519 XP
    // Adding 1000 XP should reach level 3 easily
    ProgressionSystem::AddExperience(registry, player, 1000.0f);
    
    const auto& stats = registry.get<PlayerStats>(player);
    CHECK(stats.level >= 3);
    CHECK(stats.available_attribute_points >= 10);
    CHECK(stats.available_skill_points >= 2);
}

TEST_CASE("ProgressionSystem - Point Allocation") {
    entt::registry registry;
    auto player = registry.create();
    
    registry.emplace<PlayerStats>(player, 0ULL, 0ULL, 1, 0.0f, 100.0f, 5, 1);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    
    // Allocate 2 points to Strength
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength) == true);
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength) == true);
    
    // Allocate 1 point to Vitality
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Vitality) == true);
    
    const auto& stats = registry.get<PlayerStats>(player);
    const auto& primStats = registry.get<PrimaryStats>(player);
    
    CHECK(stats.available_attribute_points == 2);
    CHECK(primStats.strength == doctest::Approx(12.0f));
    CHECK(primStats.vitality == doctest::Approx(11.0f));
    CHECK(registry.all_of<StatsDirty>(player));
    
    // Allocate skill point
    CHECK(ProgressionSystem::AllocateSkillPoint(registry, player) == true);
    CHECK(stats.available_skill_points == 0);
    
    // Try to allocate more than available
    CHECK(ProgressionSystem::AllocateSkillPoint(registry, player) == false);
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength) == true);
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength) == true);
    CHECK(ProgressionSystem::AllocateAttribute(registry, player, StatType::Strength) == false); // Out of points
}

TEST_CASE("ProgressionSystem - CalculateAwardedXP") {
    float baseXP = 100.0f;

    // Player same level as monster
    CHECK(ProgressionSystem::CalculateAwardedXP(1, 1, baseXP) == doctest::Approx(100.0f));

    // Player 1 level higher than monster (10% reduction)
    CHECK(ProgressionSystem::CalculateAwardedXP(2, 1, baseXP) == doctest::Approx(90.0f));

    // Player 5 levels higher than monster (50% reduction)
    CHECK(ProgressionSystem::CalculateAwardedXP(6, 1, baseXP) == doctest::Approx(50.0f));

    // Player 9 levels higher than monster (10% reduction)
    CHECK(ProgressionSystem::CalculateAwardedXP(10, 1, baseXP) == doctest::Approx(10.0f));

    // Player 10 levels higher than monster (still 10% reduction, min 10%)
    CHECK(ProgressionSystem::CalculateAwardedXP(11, 1, baseXP) == doctest::Approx(10.0f));

    // Player 10 levels lower than monster (no bonus, 100%)
    CHECK(ProgressionSystem::CalculateAwardedXP(1, 11, baseXP) == doctest::Approx(100.0f));
}

TEST_CASE("XPAwardingSystem - Award XP on Kill") {
    entt::registry registry;
    
    // Setup player
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<PlayerLevel>(player, 1);
    registry.emplace<PlayerStats>(player, 0ULL, 0ULL, 1, 0.0f, 100.0f, 0, 0);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<CombatStats>(player); // No bonus mult
    
    // Setup enemy
    auto enemy = registry.create();
    registry.emplace<PlayerLevel>(enemy, 1); // Enemy level 1
    
    // Kill enemy
    registry.emplace<KilledTag>(enemy, player);
    
    // Run system
    XPAwardingSystem::update(registry);
    
    // Verify player gained XP (base 50 for same level)
    const auto& stats = registry.get<PlayerStats>(player);
    CHECK(stats.current_xp == doctest::Approx(50.0f));
    
    // Verify enemy is destroyed
    CHECK(!registry.valid(enemy));
}
