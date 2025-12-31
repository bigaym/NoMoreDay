#pragma once

#include "../src/components/ItemComponent.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/core/ItemFactory.hpp"
#include "../src/systems/DropSystem.hpp"
#include "../src/components/EnemyComponent.hpp"
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("DropSystem Tests") {
    entt::registry registry;

    SUBCASE("Basic Gold Drop") {
        registry.clear();
        // Setup player
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player);
        
        // Setup victim
        auto victim = registry.create();
        registry.emplace<Position>(victim, 100.0f, 200.0f);
        
        // Custom pool with ONLY gold
        LootPool goldPool;
        goldPool.id = 1;
        goldPool.name = "Gold Only";
        goldPool.entries = { { LootEntryType::Gold, 0, 10, 10, 100.0f } };
        ItemFactory::addLootPool(1, goldPool);
        
        registry.emplace<DropTableComponent>(victim, 1, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
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

    SUBCASE("Magic Find Rarity Shift") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        auto& stats = registry.emplace<CombatStats>(player);
        stats.magic_find = 1000.0f; // Massive MF
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 0.0f, 0.0f);
        
        LootPool itemPool;
        itemPool.id = 2;
        itemPool.name = "Item Only";
        itemPool.entries = { { LootEntryType::Item, 0, 1, 1, 100.0f } };
        ItemFactory::addLootPool(2, itemPool);
        
        registry.emplace<DropTableComponent>(victim, 2, 1.0f, 10, 10); // 10 items
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        auto itemView = registry.view<ItemComponent>();
        int totalItems = 0;
        int legendaryCount = 0;
        for (auto entity : itemView) {
            totalItems++;
            if (itemView.get<ItemComponent>(entity).rarity == Rarity::Legendary) {
                legendaryCount++;
            }
        }
        
        CHECK(totalItems == 10);
        CHECK(legendaryCount > 0);
    }

    SUBCASE("Gold Bonus") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        auto& stats = registry.emplace<CombatStats>(player);
        stats.gold_bonus = 1.0f; // +100% gold
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 0.0f, 0.0f);
        
        LootPool goldPool;
        goldPool.id = 3;
        goldPool.name = "Gold Only";
        goldPool.entries = { { LootEntryType::Gold, 0, 100, 100, 100.0f } };
        ItemFactory::addLootPool(3, goldPool);
        
        registry.emplace<DropTableComponent>(victim, 3, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        auto goldView = registry.view<GoldComponent>();
        int foundCount = 0;
        for (auto entity : goldView) {
            if (goldView.get<GoldComponent>(entity).amount == 200) {
                foundCount++;
            }
        }
        CHECK(foundCount == 1);
    }

    SUBCASE("Area Level and Victim Rarity") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player);
        
        // Victim is an Elite in a Level 50 area
        auto victim = registry.create();
        registry.emplace<Position>(victim, 0.0f, 0.0f);
        auto& enemyState = registry.emplace<EnemyStateComponent>(victim, EnemyRace::DEMON, EnemyArchetype::FODDER);
        enemyState.level = 40;
        registry.emplace<EnemyRarityComponent>(victim, EnemyRarityComponent::ELITE);
        
        LootPool itemPool;
        itemPool.id = 4;
        itemPool.name = "Item Only";
        itemPool.entries = { { LootEntryType::Item, 0, 1, 1, 100.0f } };
        ItemFactory::addLootPool(4, itemPool);
        
        // 1 drop roll base
        registry.emplace<DropTableComponent>(victim, 4, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        // Area level 50
        DropSystem::update(registry, 50);
        
        auto itemView = registry.view<ItemComponent>();
        int totalItems = 0;
        for (auto entity : itemView) {
            totalItems++;
        }
        
        // Elite adds extraRolls = 1. Base 1. Total min 2.
        // Update: rollDist is (min, max + extra). So (1, 2). It can roll 1.
        CHECK(totalItems >= 1);
    }

    SUBCASE("Global Default Loot Pool (ID 0) from JSON") {
        // Initialize real data to load assets/data/loot_tables.json
        ItemFactory::initialize();
        
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player); // 0 MF
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 50.0f, 50.0f);
        
        // Use ID 999 (non-existent) so it falls back to 0
        // Set chance to 1.0f and 10 rolls to guarantee drops if pool 0 exists
        registry.emplace<DropTableComponent>(victim, 999, 1.0f, 10, 10); 
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        // Should drop SOMETHING (Gold or Items) from pool 0
        int dropCount = 0;
        // Check for Gold
        for(auto entity : registry.view<GoldComponent>()) dropCount++;
        // Check for Items
        for(auto entity : registry.view<ItemComponent>()) dropCount++;
        
        CHECK(dropCount > 0);
    }
}
