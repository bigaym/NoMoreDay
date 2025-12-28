#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#include "../third_party/doctest/doctest.h"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/core/ItemFactory.hpp"
#include "../src/systems/DropSystem.hpp"
#include "../src/tools/Logger.hpp"
#include <entt/entity/registry.hpp>

using namespace NoMoreDay;

int main(int argc, char** argv) {
    try {
        tools::Logger::Init();
        ItemFactory::initialize();
    } catch (const std::exception& e) {
        printf("Init failed: %s\n", e.what());
        return 1;
    }

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();
    tools::Logger::Shutdown();
    return res;
}

TEST_CASE("DropSystem - Basic Gold Drop") {
    entt::registry registry;
    
    // Setup player
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<CombatStats>(player);
    
    // Setup victim
    auto victim = registry.create();
    registry.emplace<Position>(victim, 100.0f, 200.0f);
    
    // Custom pool with ONLY gold
    LootPool goldPool;
    goldPool.name = "Gold Only";
    goldPool.entries = { { LootEntryType::Gold, 0, 10, 10, 100.0f } };
    ItemFactory::addLootPool(1, goldPool);
    
    registry.emplace<DropTableComponent>(victim, 1, 1.0f, 1, 1);
    registry.emplace<KilledTag>(victim, player);
    
    DropSystem::update(registry);
    
    // Verify gold exists at position
    auto goldView = registry.view<GoldComponent, Position>();
    bool found = false;
    for (auto entity : goldView) {
        const auto& gold = goldView.get<GoldComponent>(entity);
        const auto& pos = goldView.get<Position>(entity);
        if (gold.amount == 10 && pos.x == 100.0f && pos.y == 200.0f) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("DropSystem - Magic Find Rarity Shift") {
    entt::registry registry;
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.magic_find = 1000.0f; // Massive MF
    
    auto victim = registry.create();
    registry.emplace<Position>(victim, 0.0f, 0.0f);
    
    LootPool itemPool;
    itemPool.name = "Item Only";
    itemPool.entries = { { LootEntryType::Item, 0, 1, 1, 100.0f } };
    ItemFactory::addLootPool(2, itemPool);
    
    registry.emplace<DropTableComponent>(victim, 2, 1.0f, 10, 10); // 10 items
    registry.emplace<KilledTag>(victim, player);
    
    DropSystem::update(registry);
    
    auto itemView = registry.view<ItemComponent>();
    int legendaryCount = 0;
    int totalItems = 0;
    for (auto entity : itemView) {
        totalItems++;
        if (itemView.get<ItemComponent>(entity).rarity == Rarity::Legendary) {
            legendaryCount++;
        }
    }
    
    CHECK(totalItems == 10);
    // With 1000 MF, Legendary should be much more likely than base (which is very low)
    // Base legendary is > 9500 in 10000 roll. MF boost is MF * 10.
    // 1000 MF * 10 = 10000 boost. So it should always be legendary?
    // Let's check ItemFactory::rollRarity: 9500 - mfBoost. 
    // If mfBoost is 10000, 9500 - 10000 = -500. So any roll > -500 is legendary.
    CHECK(legendaryCount > 0);
}

TEST_CASE("DropSystem - Gold Bonus") {
    entt::registry registry;
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.gold_bonus = 1.0f; // +100% gold
    
    auto victim = registry.create();
    registry.emplace<Position>(victim, 0.0f, 0.0f);
    
    LootPool goldPool;
    goldPool.name = "Gold Only";
    goldPool.entries = { { LootEntryType::Gold, 0, 100, 100, 100.0f } };
    ItemFactory::addLootPool(3, goldPool);
    
    registry.emplace<DropTableComponent>(victim, 3, 1.0f, 1, 1);
    registry.emplace<KilledTag>(victim, player);
    
    DropSystem::update(registry);
    
    auto goldView = registry.view<GoldComponent>();
    for (auto entity : goldView) {
        CHECK(goldView.get<GoldComponent>(entity).amount == 200); // 100 * (1 + 1.0)
    }
}
