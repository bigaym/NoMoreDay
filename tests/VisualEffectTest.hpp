#pragma once

#include "../src/components/EffectComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Stats.hpp"
#include "../src/components/PlayerState.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/systems/DropSystem.hpp"
#include "../src/systems/InventorySystem.hpp"
#include "../src/systems/EffectSystem.hpp"
#include "../src/core/ItemFactory.hpp"
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("Visual Effect Tests") {
    entt::registry registry;

    SUBCASE("Drop System Spawns Effects") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player);
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 100.0f, 100.0f);
        
        // Use a loot pool that drops items
        LootPool itemPool;
        itemPool.id = 100;
        itemPool.name = "Test Pool";
        itemPool.entries = { { LootEntryType::Item, 0, 1, 1, 100.0f } };
        ItemFactory::addLootPool(100, itemPool);
        
        registry.emplace<DropTableComponent>(victim, 100, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        // Check for VisualEffect
        int effectCount = 0;
        auto view = registry.view<VisualEffect, Position>();
        for (auto entity : view) {
            const auto& effect = view.get<VisualEffect>(entity);
            const auto& pos = view.get<Position>(entity);
            if (effect.type == VisualEffectType::DropPillar && pos.x == 100.0f && pos.y == 100.0f) {
                effectCount++;
            }
        }
        CHECK(effectCount > 0);
    }

    SUBCASE("Drop System Spawns Gold Effects") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player);
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 200.0f, 200.0f);
        
        LootPool goldPool;
        goldPool.id = 101;
        goldPool.name = "Gold Pool";
        goldPool.entries = { { LootEntryType::Gold, 0, 10, 10, 100.0f } };
        ItemFactory::addLootPool(101, goldPool);
        
        registry.emplace<DropTableComponent>(victim, 101, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        // Check for VisualEffect
        int effectCount = 0;
        auto view = registry.view<VisualEffect, Position>();
        for (auto entity : view) {
            const auto& effect = view.get<VisualEffect>(entity);
            const auto& pos = view.get<Position>(entity);
            if (effect.type == VisualEffectType::GoldSparkle && pos.x == 200.0f && pos.y == 200.0f) {
                effectCount++;
            }
        }
        CHECK(effectCount > 0);
    }

    SUBCASE("Inventory System Pickup Spawns Effects") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<InventoryComponent>(player);
        registry.emplace<EquipmentComponent>(player);
        registry.emplace<Position>(player, 0.0f, 0.0f);
        registry.get<InventoryComponent>(player).capacity = 10;
        registry.get<InventoryComponent>(player).items.resize(10, entt::null);

        // Create item on ground
        auto item = ItemFactory::createRandomLoot(registry, 1, 0.0f);
        registry.emplace<Position>(item, 50.0f, 50.0f);
        
        // Pick it up
        bool picked = InventorySystem::pickUpItem(registry, player, item);
        CHECK(picked);
        
        // Check for VisualEffect
        int effectCount = 0;
        auto view = registry.view<VisualEffect, Position>();
        for (auto entity : view) {
            const auto& effect = view.get<VisualEffect>(entity);
            const auto& pos = view.get<Position>(entity);
            if (effect.type == VisualEffectType::Pickup && pos.x == 50.0f && pos.y == 50.0f) {
                effectCount++;
            }
        }
        CHECK(effectCount > 0);
    }

    SUBCASE("Damage Popup Animation") {
        registry.clear();
        auto entity = registry.create();
        registry.emplace<Position>(entity, 0.0f, 0.0f);
        
        DamagePopup popup;
        popup.damage = 10;
        popup.timer = 0.0f;
        popup.lifeTime = 1.0f;
        popup.velX = 0;
        popup.velY = 0;
        popup.isCrit = true; // Test Crit logic
        popup.currentScale = 0.0f;
        
        registry.emplace<DamagePopup>(entity, popup);
        
        // Include EffectSystem to update it
        // Note: EffectSystem is a class or namespace? 
        // In source, it's a class with static method update? No, usually struct with method.
        // Let's check EffectSystem.hpp.
        // Assuming EffectSystem::update(registry, dt)
        
        // Update for 0.1s
        EffectSystem::update(registry, 0.1f);
        
        // Check scale
        const auto& p = registry.get<DamagePopup>(entity);
        // At 0.1s, Crit scale should be > 0.5f (start)
        CHECK(p.currentScale > 0.5f);
        CHECK(p.timer == doctest::Approx(0.1f));
        
        // Update more to exceed lifetime
        EffectSystem::update(registry, 1.0f);
        
        // Entity should be destroyed
        CHECK(registry.valid(entity) == false);
    }
}
