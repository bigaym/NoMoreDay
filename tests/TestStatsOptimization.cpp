#include "TestCommon.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("StatsSystem Optimization") {
    entt::registry registry;
    auto entity = registry.create();
    
    SUBCASE("EnemyRaceData Application") {
        // Create an enemy (e.g. UNDEAD with POISON/BLEED resistance)
        registry.emplace<EnemyStateComponent>(entity, EnemyRace::UNDEAD, EnemyArchetype::FODDER);
        registry.emplace<CombatStats>(entity);
        
        StatsSystem::Recalculate(registry, entity);
        
        const auto& stats = registry.get<CombatStats>(entity);
        
        // Initial stats check (Base UNDEAD stats)
        // HP: 30, Speed: 40, Armor: 100
        CHECK(stats.max_health == doctest::Approx(30.0f));
        CHECK(stats.move_speed == doctest::Approx(40.0f));
        CHECK(stats.armor == doctest::Approx(100.0f));
    }

    SUBCASE("Resistance Tag Logic") {
        // DEMON has Fire and Shadow resistance (50%)
        registry.emplace<EnemyStateComponent>(entity, EnemyRace::DEMON, EnemyArchetype::TANK);
        registry.emplace<CombatStats>(entity);
        
        StatsSystem::Recalculate(registry, entity);
         const auto& stats = registry.get<CombatStats>(entity);
        
        // Fire Resistance should be ~0.50f (50%) - hardcoded NATIVE_RESISTANCE_VALUE
        CHECK(stats.resistances[static_cast<size_t>(DamageType::Fire)] == doctest::Approx(0.5f));
        CHECK(stats.resistances[static_cast<size_t>(DamageType::Shadow)] == doctest::Approx(0.5f));
        
        // Cold should be near 0
        CHECK(stats.resistances[static_cast<size_t>(DamageType::Cold)] == doctest::Approx(0.0f));
    }

    SUBCASE("Performance Validation") {
        // Just verify it doesn't crash on repeated calls
        registry.emplace<EnemyStateComponent>(entity, EnemyRace::GOBLIN, EnemyArchetype::FODDER);
        registry.emplace<CombatStats>(entity);
        
        for(int i=0; i<1000; ++i) {
            StatsSystem::Recalculate(registry, entity);
        }
        CHECK(true);
    }
}

} // namespace NoMoreDay
