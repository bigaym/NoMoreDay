#pragma once

#include "doctest.h"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Buff.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include <chrono>
#include <vector>
#include <cstdio>

namespace NoMoreDay {

TEST_CASE("[Performance] StatsSystem - Calculation Benchmark") {
    entt::registry registry;
    const int num_entities = 1000;
    std::vector<entt::entity> entities;

    // Load necessary data
    AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");

    for (int i = 0; i < num_entities; ++i) {
        auto e = registry.create();
        registry.emplace<PrimaryStats>(e);
        registry.emplace<CombatStats>(e);
        registry.emplace<AstrolabeComponent>(e);
        registry.emplace<EquipmentComponent>(e);
        registry.emplace<ActiveEffectsComponent>(e);
        registry.emplace<StatsDirty>(e);
        entities.push_back(e);
    }

    // Benchmark Recalculate (Cold)
    auto start = std::chrono::high_resolution_clock::now();
    for (auto e : entities) {
        StatsSystem::Recalculate(registry, e);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    printf("\n[Benchmark] Recalculate 1000 entities: %lld us (avg %.2f us/entity)\n", 
        (long long)duration.count(), (float)duration.count() / num_entities);

    // Benchmark GetStatWithTags (Repeated queries to test cache)
    const int queries_per_entity = 10;
    start = std::chrono::high_resolution_clock::now();
    for (auto e : entities) {
        for (int q = 0; q < queries_per_entity; ++q) {
            StatsSystem::GetStatWithTags(registry, e, StatType::Dexterity, Tag::Melee | Tag::Physical);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    printf("[Benchmark] GetStatWithTags 10000 queries (Cached): %lld us (avg %.2f us/query)\n", 
        (long long)duration.count(), (float)duration.count() / (num_entities * queries_per_entity));
}

} // namespace NoMoreDay