#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/components/Stats.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../third_party/entt/src/entt/entity/registry.hpp"

using namespace NoMoreDay;

TEST_CASE("Stats recalculation") {
    entt::registry registry;
    auto entity = registry.create();
    
    auto& primary = registry.emplace<PrimaryStats>(entity);
    primary.strength = 10.0f;
    primary.vitality = 10.0f;
    
    auto& combat = registry.emplace<CombatStats>(entity);
    
    // Initial stats
    CHECK(combat.max_health == 100.0f);
    
    // Trigger recalculation
    StatsSystem::Recalculate(registry, entity);
    
    // Expect health to increase based on vitality (e.g., 1 vit = 10 health)
    // 100 (base) + 10 * 10 = 200
    CHECK(combat.max_health == 200.0f);
    
    // Test modifiers
    auto& mod_list = registry.emplace<ModifierList>(entity);
    mod_list.modifiers.push_back({StatType::MaxHealth, ModifierMode::Flat, 50.0f});
    
    StatsSystem::Recalculate(registry, entity);
    
    // 200 (from vitality) + 50 (flat mod) = 250
    CHECK(combat.max_health == 250.0f);

    // Test PercentAdd mod
    mod_list.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentAdd, 10.0f}); // +10%
    StatsSystem::Recalculate(registry, entity);
    
    // Formula: (Base + Flat) * (1 + PercentAdd)
    // (100 base + 100 from vit + 50 flat) * (1 + 0.1) = 250 * 1.1 = 275
    CHECK(combat.max_health == 275.0f);

    // Test PercentMult mod
    mod_list.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentMult, 20.0f}); // x1.2
    StatsSystem::Recalculate(registry, entity);
    
    // Formula: (Base + Flat) * (1 + PercentAdd) * (1 + PercentMult)
    // 275 * 1.2 = 330
    CHECK(combat.max_health == 330.0f);
}
