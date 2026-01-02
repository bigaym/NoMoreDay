#pragma once
#include "doctest.h"
#include "../src/core/LevelManager.hpp"
#include "../src/core/BiomeRegistry.hpp"

TEST_CASE("LevelManager - Biome loading") {
    using namespace NoMoreDay;
    
    // Ensure biomes are loaded
    BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    LevelManager levelManager;
    
    SUBCASE("Load Town Biome (Safe Zone)") {
        levelManager.loadNewLevel("town", 50, 50);
        CHECK(levelManager.getCurrentBiome() == "town");
        
        // Town should have no enemies
        const auto& enemySystem = levelManager.getEnemySystem();
        // We can't easily check private m_spawnData but we can check the effect
        // Actually, initData logs the count.
    }
    
    SUBCASE("Load Cave Biome (Hostile)") {
        levelManager.loadNewLevel("cave", 50, 50);
        CHECK(levelManager.getCurrentBiome() == "cave");
    }
}
