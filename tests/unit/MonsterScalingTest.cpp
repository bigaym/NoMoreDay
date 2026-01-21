#include "doctest.h"
#include "game/utils/MonsterScaling.hpp"
#include "game/components/Common.hpp"
#include <cmath>

using namespace NoMoreDay;
using namespace NoMoreDay::Constants::Combat::Scaling::Monster;

TEST_CASE("MonsterScaling::PowerCurve") {
    // Accessing private method via Friend? No, MonsterScaling usually doesn't expose PowerCurve. 
    // Testing Public methods implicitly tests PowerCurve.
    
    // Manual check of growth rate
    // HP Growth = 0.10
    
    MonsterScalingResult r1 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 1, EnemyRarityComponent::NORMAL);
    MonsterScalingResult r2 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 2, EnemyRarityComponent::NORMAL);
    
    // Level 1 should match base HP (assuming UNDEAD has base HP)
    // We just check ratio
    CHECK(r2.maxHealth / r1.maxHealth == doctest::Approx(1.10f).epsilon(0.01f));
    
    MonsterScalingResult r11 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 11, EnemyRarityComponent::NORMAL);
    // 1.1^10 = 2.5937
    CHECK(r11.maxHealth / r1.maxHealth == doctest::Approx(2.5937f).epsilon(0.001f));
}

TEST_CASE("MonsterScaling::SyncLevel") {
    CHECK(MonsterScaling::SyncLevel(50, 30) == 50); // Area Higher
    CHECK(MonsterScaling::SyncLevel(20, 60) == 55); // Player Higher (60-5)
    CHECK(MonsterScaling::SyncLevel(1, 3) == 1); // Low levels
    CHECK(MonsterScaling::SyncLevel(0, 10) == 5); // Area 0 corrected to 1, then max(1, 5)
}

TEST_CASE("MonsterScaling::GetXPMultiplier") {
    // Diff <= 5 -> 1.0
    CHECK(MonsterScaling::GetXPMultiplier(50, 50) == 1.0f);
    CHECK(MonsterScaling::GetXPMultiplier(50, 55) == 1.0f);
    CHECK(MonsterScaling::GetXPMultiplier(50, 45) == 1.0f);
    
    // Diff 6 -> 0.9
    CHECK(MonsterScaling::GetXPMultiplier(50, 56) == doctest::Approx(0.9f));
    CHECK(MonsterScaling::GetXPMultiplier(50, 44) == doctest::Approx(0.9f));
    
    // Diff 15 -> 0.1 (Max penalty)
    // 15 - 5 = 10. 10 * 0.1 = 1.0. 1.0 - 1.0 = 0.0. Max(0.1, 0.0) = 0.1.
    CHECK(MonsterScaling::GetXPMultiplier(50, 65) == doctest::Approx(0.1f));
}

TEST_CASE("MonsterScaling::ResistanceBonus") {
    // Under 100
    MonsterScalingResult r90 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 90, EnemyRarityComponent::NORMAL);
    CHECK(r90.resistanceBonus == 0.0f);
    
    // Over 100
    MonsterScalingResult r110 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 110, EnemyRarityComponent::NORMAL);
    // 10 levels over. 0.002 per level. Total 0.02.
    CHECK(r110.resistanceBonus == doctest::Approx(0.02f));
    
    // Boss Scaling
    MonsterScalingResult b110 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 110, EnemyRarityComponent::BOSS);
    // 10 levels. 0.008 per level. Total 0.08.
    CHECK(b110.resistanceBonus == doctest::Approx(0.08f));
}

TEST_CASE("MonsterScaling::ArmorTargetDR") {
    // Level 1 -> DR 0
    MonsterScalingResult r1 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 1, EnemyRarityComponent::NORMAL);
    CHECK(r1.armor == 0.0f);
    
    // Level 100 -> DR 0.20
    // Armor formula is complex, but result.armor should yield DR 0.20 when plugged back into formula.
    MonsterScalingResult r100 = MonsterScaling::Calculate(EnemyRace::UNDEAD, 100, EnemyRarityComponent::NORMAL);
    
    // Reverse check: DR = Armor / (Armor + LevelFactor * ARMOR_BASE)
    // LevelFactor(100)
    using namespace NoMoreDay::Constants::Combat::Scaling;
    using namespace NoMoreDay::Constants::Combat::Pipeline;
    float lv = 100.0f;
    float lf = LEVEL_BASE + lv * LEVEL_LINEAR + lv * lv * LEVEL_QUADRATIC;
    float dr = r100.armor / (r100.armor + lf * ARMOR_BASE);
    CHECK(dr == doctest::Approx(0.198f).epsilon(0.001f));
}
