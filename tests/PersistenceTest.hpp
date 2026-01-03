#pragma once
#include "doctest.h"
#include "../src/core/SceneManager.hpp"
#include "../src/core/LevelManager.hpp"
#include "../src/core/BiomeRegistry.hpp"
#include "../src/components/Common.hpp"
#include "../src/components/Buff.hpp"
#include "../src/components/InventoryComponent.hpp"
#include <chrono>
#include <thread>

TEST_CASE("Persistence - Player State survival across transitions") {
    using namespace NoMoreDay;
    
    entt::registry registry;
    LevelManager levelManager;
    SceneManager sceneManager(levelManager, registry);
    
    // 1. Setup Biomes
    BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    // 2. Create Player (Persistent)
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<PersistentTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    auto& buffs = registry.emplace<ActiveEffectsComponent>(player);
    BuffEffect testBuff;
    testBuff.id = "persistence_test_buff";
    testBuff.name = "Test Buff";
    testBuff.remaining = 100.0f;
    buffs.effects.push_back(testBuff);
    
    auto& inv = registry.emplace<InventoryComponent>(player);
    // Add a dummy item if needed, for now just checking component exists
    
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    active.available_talent_points = 10;
    active.specialized_slots[0].skill_id = 1;
    active.specialized_slots[0].allocated_points[101] = 3;
    
    // 3. Initial Load (Cave)
    sceneManager.RequestTransition("cave", 1);
    
    // Fast forward transition logic (Manual update to avoid actual time wait)
    // FADE_OUT
    sceneManager.Update(1.0f); 
    // LOADING -> WAIT_FOR_FUTURE
    sceneManager.Update(0.1f);
    
    // Wait for async load to finish
    int attempts = 0;
    while (sceneManager.IsTransitioning() && attempts < 100) {
        sceneManager.Update(0.1f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    // 4. Verify Cave State
    CHECK(levelManager.getCurrentBiome() == "cave");
    CHECK(registry.valid(player));
    CHECK(registry.all_of<ActiveEffectsComponent>(player));
    CHECK(registry.get<ActiveEffectsComponent>(player).effects.size() == 1);
    CHECK(registry.get<ActiveEffectsComponent>(player).effects[0].id == "persistence_test_buff");
    
    // 5. Transition to Town
    sceneManager.RequestTransition("town", 1);
    sceneManager.Update(1.0f); // FADE_OUT
    sceneManager.Update(0.1f); // LOADING
    
    attempts = 0;
    while (sceneManager.IsTransitioning() && attempts < 100) {
        sceneManager.Update(0.1f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    // 6. Verify Town State & Persistence
    CHECK(levelManager.getCurrentBiome() == "town");
    CHECK(registry.valid(player));
    CHECK(registry.all_of<ActiveEffectsComponent>(player));
    CHECK(registry.get<ActiveEffectsComponent>(player).effects.size() == 1);
    
    // 7. Verify Local Clearing
    // Spawn a local entity in Town (should not happen normally but for test...)
    auto localEnt = registry.create();
    registry.emplace<LocalLevelTag>(localEnt);
    CHECK(registry.valid(localEnt));
    
    // Transition back to Cave
    sceneManager.RequestTransition("cave", 2);
    sceneManager.Update(1.0f);
    sceneManager.Update(0.1f);
    
    attempts = 0;
    while (sceneManager.IsTransitioning() && attempts < 100) {
        sceneManager.Update(0.1f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    CHECK_FALSE(registry.valid(localEnt)); // Local entity should be destroyed
    CHECK(registry.valid(player));         // Player should still be there
    CHECK(registry.get<ActiveEffectsComponent>(player).effects.size() == 1);
    
    auto& pActive = registry.get<ActiveSkillsComponent>(player);
    CHECK(pActive.available_talent_points == 10);
    CHECK(pActive.specialized_slots[0].skill_id == 1);
    CHECK(pActive.specialized_slots[0].allocated_points.at(101) == 3);
}
