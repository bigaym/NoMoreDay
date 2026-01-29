#pragma once

#include "TestCommon.hpp"
#include <entt/entt.hpp>
#include "game/components/Common.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/DropSystem.hpp"
#include <iostream>
#include <chrono>
#include <vector>

namespace NoMoreDay {

TEST_CASE("[Performance] DropSystem - Mass Drop Generation" * doctest::skip(true)) {
    TestSetupScope scope;
    entt::registry registry;
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<PlayerLevel>(player, 10);

    const int ENTITY_COUNT = 1000;
    
    LootPool benchmarkPool;
    benchmarkPool.name = "Benchmark Pool";
    benchmarkPool.entries = {
        { LootEntryType::Item, 0, 1, 1, 50.0f },
        { LootEntryType::Gold, 0, 10, 50, 50.0f }
    };
    ItemFactory::addLootPool(100, benchmarkPool);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = registry.create();
        registry.emplace<Position>(e, (float)i, (float)i);
        registry.emplace<DropTableComponent>(e, 100, 1.0f, 1, 1);
        registry.emplace<KilledTag>(e, player);
    }

    auto start = std::chrono::high_resolution_clock::now();
    DropSystem::update(registry);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[Benchmark] DropSystem Duration for " << ENTITY_COUNT << " deaths: " << duration << " us" << std::endl;
}

} // namespace NoMoreDay
