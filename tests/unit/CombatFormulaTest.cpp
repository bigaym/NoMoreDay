#pragma once

#include "TestCommon.hpp"
#include "game/contracts/CombatFormula.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] CombatFormula - Level Scaling") {
    SUBCASE("Level Factor calculation") {
        // Level 1: 10 + 0.5*1 + 0.05*1 = 10.55
        CHECK(CombatFormula::LevelFactor(1) == doctest::Approx(10.55f));
        
        // Level 100: 10 + 0.5*100 + 0.05*10000 = 10 + 50 + 500 = 560
        CHECK(CombatFormula::LevelFactor(100) == doctest::Approx(560.0f));
    }
}

TEST_CASE("[Unit] CombatFormula - Armor") {
    int level = 100; // LF = 560
    
    SUBCASE("Positive Armor") {
        // 0 Armor -> 1.0 Multiplier (0% DR)
        CHECK(CombatFormula::CalculateArmorMultiplier(0.0f, level) == doctest::Approx(1.0f));
        
        // 560 Armor -> 0.5 Multiplier (50% DR)
        // LF / (Armor + LF) = 560 / (560 + 560) = 0.5
        CHECK(CombatFormula::CalculateArmorMultiplier(560.0f, level) == doctest::Approx(0.5f));
        
        // 5040 Armor -> 0.1 Multiplier (90% DR)
        // 560 / (5040 + 560) = 560 / 5600 = 0.1
        CHECK(CombatFormula::CalculateArmorMultiplier(5040.0f, level) == doctest::Approx(0.1f));
    }
    
    SUBCASE("Negative Armor") {
        // -560 Armor -> 1.5 Multiplier (50% Increased Damage)
        // 1 + |Armor| / (|Armor| + LF) = 1 + 560 / (560 + 560) = 1.5
        CHECK(CombatFormula::CalculateArmorMultiplier(-560.0f, level) == doctest::Approx(1.5f));
        
        // Very negative armor approaches 2.0x damage
        CHECK(CombatFormula::CalculateArmorMultiplier(-1000000.0f, level) == doctest::Approx(2.0f).epsilon(0.01));
    }
}

TEST_CASE("[Unit] CombatFormula - Dodge") {
    int level = 100; // LF = 560
    
    SUBCASE("Dodge Chance Scaling") {
        CHECK(CombatFormula::CalculateDodgeChance(0.0f, level) == 0.0f);
        
        // 1000 Rating @ Level 100
        // num = 0.1 * 1000 + 0.001 * 1000000 = 100 + 1000 = 1100
        // raw = 1 - 1 / (1100 / 560 + 1) = 1 - 1 / 2.964 = 1 - 0.337 = 0.663
        // chance = 0.663 * 0.9 = 0.596 (approx 60%)
        float chance = CombatFormula::CalculateDodgeChance(1000.0f, level);
        CHECK(chance > 0.55f);
        CHECK(chance < 0.65f);
        
        // High rating should approach 90%
        CHECK(CombatFormula::CalculateDodgeChance(1000000.0f, level) == doctest::Approx(0.90f).epsilon(0.001f));
    }
}

TEST_CASE("[Unit] CombatFormula - Block") {
    int level = 100; // LF = 560
    
    SUBCASE("Block Effectiveness") {
        CHECK(CombatFormula::CalculateBlockEffectiveness(0.0f, level) == 0.0f);
        
        // 560 Rating -> 50% DR on Block
        CHECK(CombatFormula::CalculateBlockEffectiveness(560.0f, level) == doctest::Approx(0.5f));
    }
}

} // namespace NoMoreDay
