#pragma once
#include "doctest.h"
#include "game/systems/item/StashSystem.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/StashComponent.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include <iostream>
#include <chrono>

using namespace NoMoreDay;

TEST_CASE("[Performance] StashSystem - Stress Test") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stash = registry.emplace<PersonalStashComponent>(player);
    
    // 1. Fill 3 tabs with random items (432 items total) - Reduced from 10 to speed up
    int tabCount = 3;
    stash.unlockedTabs = tabCount;
    stash.tabs.resize(tabCount);
    
    std::cout << "[Benchmark] Filling " << tabCount << " stash tabs with " << (tabCount * 144) << " items..." << std::endl;
    for (int t = 0; t < tabCount; ++t) {
        stash.tabs[t].name = "Tab " + std::to_string(t);
        for (int s = 0; s < 144; ++s) {
            auto item = ItemFactory::createWeapon(registry, 50, Rarity::Rare);
            stash.tabs[t].items[s] = item;
        }
    }

    // 2. Benchmark Sorting
    {
        auto start = std::chrono::high_resolution_clock::now();
        StashSystem::sortTab(registry, StashType::Personal, 0, StashSortMode::RarityDesc);
        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  - Sort 144 items: " << diff << " us" << std::endl;
    }

    // 3. Benchmark Searching (String search across items)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto results = StashSystem::search(registry, StashType::Personal, "远古");
        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  - Search " << (tabCount * 144) << " items: " << diff << " us (Found " << results.size() << ")" << std::endl;
    }

    // 4. Benchmark Auto-Deposit (Simulate full inventory deposit)
    auto& inv = registry.emplace<InventoryComponent>(player);
    for (int i = 0; i < 40; ++i) {
        inv.items[i] = ItemFactory::createArmor(registry, 50, Rarity::Magic, EquipmentSlot::Chest);
    }
    // Make space in stash tab 0
    for(int i=0; i<40; ++i) stash.tabs[0].items[i] = entt::null;

    {
        auto start = std::chrono::high_resolution_clock::now();
        int count = StashSystem::autoDeposit(registry, StashType::Personal);
        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "  - Auto-Deposit 40 items: " << diff << " us (Moved " << count << ")" << std::endl;
    }
}
