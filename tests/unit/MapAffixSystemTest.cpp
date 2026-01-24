#include "doctest.h"
#include "game/systems/world/MapAffixCalculator.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"
#include <cmath>
#include <vector>

using namespace NoMoreDay;

TEST_CASE("MapAffixCalculator::DifficultyScore") {
    std::vector<MapAffix> affixes;
    
    // Test empty
    CHECK(MapAffixCalculator::CalculateDifficultyScore(affixes) == 0);
    
    // Add typical debuffs
    MapAffix a1;
    a1.type = MapAffixType::Enemy_ExtraHealth;
    a1.tier = 1; 
    a1.category = MapAffixCategory::Debuff;
    
    MapAffix a2;
    a2.type = MapAffixType::Enemy_ExtraDamage;
    a2.tier = 5; 
    a2.category = MapAffixCategory::Debuff; // Must set category explicitly
    
    affixes.push_back(a1);
    affixes.push_back(a2);
    
    // Expect: T1 Health (1*10*1.0 = 10) + T5 Damage (5*10*2.0 = 100) = 110
    int ds = MapAffixCalculator::CalculateDifficultyScore(affixes);
    CHECK(ds == 110);

    // Buffs should not affect DS
    MapAffix b1;
    b1.category = MapAffixCategory::Buff;
    affixes.push_back(b1);
    CHECK(MapAffixCalculator::CalculateDifficultyScore(affixes) == 110);
}

TEST_CASE("MapAffixCalculator::RewardScaling") {
    // DS = 0
    auto r0 = MapAffixCalculator::CalculateRewards(0);
    CHECK(r0.rarityBonus == 0.0f);
    CHECK(r0.quantityBonus == 0.0f);
    
    // DS = 40 (Milestone)
    // Rarity: DS * 1.5% = 40 * 0.015 = 0.6 (60%)
    // Quantity: 0.5 * log2(1 + 40/40) = 0.5 * log2(2) = 0.5 (50%)
    auto r40 = MapAffixCalculator::CalculateRewards(40);
    CHECK(r40.rarityBonus == doctest::Approx(0.6f));
    CHECK(r40.quantityBonus == doctest::Approx(0.5f));
    
    // DS = 120 (Extreme)
    // Rarity: 120 * 1.5% = 1.8 (180%)
    // Quantity: 0.5 * log2(1 + 120/40) = 0.5 * log2(4) = 0.5 * 2 = 1.0 (100%)
    auto r120 = MapAffixCalculator::CalculateRewards(120);
    CHECK(r120.rarityBonus == doctest::Approx(1.8f));
    CHECK(r120.quantityBonus == doctest::Approx(1.0f));
}

TEST_CASE("MapAffixCalculator::LPProbabilityMultiplier") {
    // Current formula: 1.0f + rarityBonus
    // Rarity Bonus 1.0 (100%) -> lpMult = 2.0
    CHECK(MapAffixCalculator::CalculateLPProbabilityMultiplier(1.0f) == doctest::Approx(2.0f));
    
    // Rarity Bonus 5.0 (500%) -> lpMult = 6.0
    CHECK(MapAffixCalculator::CalculateLPProbabilityMultiplier(5.0f) == doctest::Approx(6.0f));
}
