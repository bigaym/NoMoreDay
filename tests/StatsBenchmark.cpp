#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/components/Stats.hpp"
#include "../src/systems/StatsSystem.hpp"
#include <chrono>
#include <iostream>

using namespace NoMoreDay;

TEST_CASE("StatsSystem Benchmark - 10,000 Entities") {
    entt::registry registry;
    const int entityCount = 10000;

    std::cout << "Creating " << entityCount << " entities..." << std::endl;

    for (int i = 0; i < entityCount; ++i) {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, PrimaryStats{
            .strength = static_cast<float>(i % 100),
            .dexterity = static_cast<float>(i % 100),
            .intelligence = static_cast<float>(i % 100),
            .vitality = static_cast<float>(i % 100)
        });
        registry.emplace<StatsDirty>(entity);
    }

    std::cout << "Starting benchmark..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    
    StatsSystem::update(registry);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "StatsSystem::update for " << entityCount << " entities took: " 
              << elapsed.count() << " ms" << std::endl;

    // Target: < 1ms for 10,000 entities.
    CHECK(elapsed.count() < 1.0); 
}

#include "../src/components/Buff.hpp"

TEST_CASE("Buff System Benchmark - 10,000 Entities") {
    entt::registry registry;
    const int entityCount = 10000;

    std::cout << "Creating " << entityCount << " entities with buffs..." << std::endl;

    for (int i = 0; i < entityCount; ++i) {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, 10.0f, 10.0f, 10.0f, 10.0f);
        
        auto& effects = registry.emplace<ActiveEffectsComponent>(entity);
        for (int j = 0; j < 5; ++j) {
            BuffEffect b;
            b.id = "buff_" + std::to_string(j);
            b.duration = 10.0f;
            b.remaining = 10.0f;
            b.stacks = 1;
            b.max_stacks = 5;
            b.modifiers.push_back({ StatType::Strength, ModifierMode::Flat, 1.0f });
            effects.effects.push_back(b);
        }
    }

    std::cout << "Starting Buff Update benchmark..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    StatsSystem::UpdateBuffs(registry, 0.016f);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "StatsSystem::UpdateBuffs for " << entityCount << " entities (5 buffs each) took: " 
              << elapsed.count() << " ms" << std::endl;

    std::cout << "Starting Recalculate benchmark (with Buffs)..." << std::endl;
    registry.view<CombatStats>().each([&](auto entity, auto&){
        registry.emplace_or_replace<StatsDirty>(entity);
    });

    start = std::chrono::high_resolution_clock::now();
    StatsSystem::update(registry);
    end = std::chrono::high_resolution_clock::now();
    
    elapsed = end - start;
    std::cout << "StatsSystem::update (recalc) for " << entityCount << " entities with buffs took: " 
              << elapsed.count() << " ms" << std::endl;

    CHECK(elapsed.count() < 10.0); // Recalculation is heavy, 10ms for 10k entities is reasonable
}
