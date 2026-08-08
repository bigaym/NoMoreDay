#pragma once
#include "doctest.h"
#include "game/systems/item/StashSystem.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/StashComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/item/SharedStash.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] StashSystem - Basic Transfer") {
    entt::registry registry;
    
    // Setup Player with Personal Stash
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stash = registry.emplace<PersonalStashComponent>(player);
    
    // Add Item
    auto item = registry.create();
    auto& itemComp = registry.emplace<ItemComponent>(item);
    itemComp.name = "Test Item";
    itemComp.type = ItemType::Weapon;
    
    stash.tabs[0].items[0] = item;
    
    // Transfer from Slot 0 to Slot 1 in same tab
    bool result = StashSystem::transferItem(registry, 
        StashType::Personal, 0, 0, 
        StashType::Personal, 0, 1);
        
    CHECK(result == true);
    CHECK(stash.tabs[0].items[0] == entt::entity{entt::null});
    CHECK(stash.tabs[0].items[1] == item);
}

TEST_CASE("[Unit] StashSystem - Cross Stash Transfer") {
    entt::registry registry;
    SharedStash::Get().initialize();
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& pStash = registry.emplace<PersonalStashComponent>(player);
    
    auto item = registry.create();
    auto& itemComp = registry.emplace<ItemComponent>(item);
    itemComp.type = ItemType::Armor;
    
    pStash.tabs[0].items[0] = item;
    
    // Transfer Personal -> Shared
    bool result = StashSystem::transferItem(registry, 
        StashType::Personal, 0, 0, 
        StashType::Shared, 0, 0);
        
    CHECK(result == true);
    CHECK(pStash.tabs[0].items[0] == entt::entity{entt::null});
    CHECK(SharedStash::Get().getItem(0, 0) == item);
}

TEST_CASE("[Unit] StashSystem - Material Acceptance") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stash = registry.emplace<PersonalStashComponent>(player);
    
    auto mat = registry.create();
    auto& matComp = registry.emplace<ItemComponent>(mat);
    matComp.type = ItemType::Material;
    
    stash.tabs[0].items[0] = mat; // Force put (bypass system)
    
    // Try to move it (should succeed now as constraint was removed)
    bool result = StashSystem::transferItem(registry,
        StashType::Personal, 0, 0,
        StashType::Personal, 0, 1);
        
    CHECK(result == true);
}

TEST_CASE("[Unit] StashSystem - Unlock Tab") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& inv = registry.emplace<InventoryComponent>(player);
    auto& stash = registry.emplace<PersonalStashComponent>(player);
    
    inv.gold = 0;
    
    // Fail due to gold
    CHECK(StashSystem::unlockTab(registry, StashType::Personal) == false);
    
    inv.gold = 1000000;
    int initialTabs = stash.unlockedTabs;
    
    CHECK(StashSystem::unlockTab(registry, StashType::Personal) == true);
    CHECK(stash.unlockedTabs == initialTabs + 1);
}
