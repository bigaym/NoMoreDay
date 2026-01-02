#pragma once
#include "doctest.h"
#include "../src/core/BiomeRegistry.hpp"
#include <filesystem>

TEST_CASE("BiomeRegistry - Loading from JSON") {
    using namespace NoMoreDay;
    
    // Ensure we are in the right directory to find assets
    // (Assuming tests run from project root or build directory with assets copied)
    
    BiomeRegistry& registry = BiomeRegistry::Get();
    registry.LoadFromJSON("assets/data/biomes.json");
    
    SUBCASE("Town biome exists and has correct data") {
        CHECK(registry.HasBiome("town"));
        const auto& town = registry.GetBiome("town");
        CHECK(town.id == "town");
        CHECK(town.name == "平安镇 (Town)");
        CHECK(town.isSafeZone == true);
        CHECK(town.maxEnemies == 0);
    }
    
    SUBCASE("Cave biome exists and has correct data") {
        CHECK(registry.HasBiome("cave"));
        const auto& cave = registry.GetBiome("cave");
        CHECK(cave.id == "cave");
        CHECK(cave.name == "幽暗洞穴 (Dark Cave)");
        CHECK(cave.isSafeZone == false);
        CHECK(cave.maxEnemies == 50);
        CHECK(cave.enemyPool.size() >= 2);
    }
    
    SUBCASE("Unknown biome returns default") {
        CHECK_FALSE(registry.HasBiome("non_existent"));
        const auto& def = registry.GetBiome("non_existent");
        CHECK(def.id == ""); // Default config has empty id
    }
}
