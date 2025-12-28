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
