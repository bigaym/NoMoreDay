#include <entt/entt.hpp>
#include "../src/components/Common.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/core/ItemFactory.hpp"
#include "../src/systems/DropSystem.hpp"
#include "../src/tools/Logger.hpp"
#include <iostream>
#include <chrono>
#include <vector>

using namespace NoMoreDay;

int main() {
    try {
        tools::Logger::Init(); 
        ItemFactory::initialize();
    } catch (const std::exception& e) {
        std::cerr << "Init failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Starting benchmark..." << std::endl;
    entt::registry registry;
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<PlayerLevel>(player, 10);

    const int ENTITY_COUNT = 1000;
    std::vector<entt::entity> entities;
    
    // Setup test pool
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
        entities.push_back(e);
    }

    std::cout << "Benchmarking DropSystem with " << ENTITY_COUNT << " entity deaths..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    
    DropSystem::update(registry);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Duration: " << duration << " us" << std::endl;
    std::cout << "Average per death: " << (double)duration / ENTITY_COUNT << " us" << std::endl;

    // Count results
    auto items = registry.view<ItemComponent>().size();
    auto gold = registry.view<GoldComponent>().size();
    
    std::cout << "Total items dropped: " << items << std::endl;
    std::cout << "Total gold stacks dropped: " << gold << std::endl;

    return 0;
}
