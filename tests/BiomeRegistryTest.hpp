#pragma once
#include "doctest.h"
#include "TestCommon.hpp"
#include "game/data/BiomeRegistry.hpp"
#include <filesystem>

TEST_CASE("BiomeRegistry - Loading from JSON") {
    LoggerScope scope;
    using namespace NoMoreDay;
    
    // Ensure we are in the right directory to find assets
    // (Assuming tests run from project root or build directory with assets copied)
    
    BiomeRegistry& registry = BiomeRegistry::Get();
    printf("DEBUG: Before LoadFromJSON\n");
    registry.LoadFromJSON("assets/data/biomes.json");
    printf("DEBUG: After LoadFromJSON\n");
    
    SUBCASE("Town biome exists and has correct data") {
        printf("DEBUG: Inside Town SUBCASE\n");
        CHECK(registry.HasBiome("town"));
        printf("DEBUG: HasBiome(town) passed\n");
        const auto& town = registry.GetBiome("town");
        printf("DEBUG: GetBiome(town) passed. ID: %s\n", town.id.c_str());
        CHECK(town.id == "town");
        CHECK(town.name == "平安镇 (Town)");
        CHECK(town.isSafeZone == true);
        CHECK(town.maxEnemies == 0);
        printf("DEBUG: End Town SUBCASE\n");
    }
    
    SUBCASE("Cave biome exists and has correct data") {
        printf("DEBUG: Inside Cave SUBCASE\n");
        CHECK(registry.HasBiome("cave"));
        const auto& cave = registry.GetBiome("cave");
        CHECK(cave.id == "cave");
        CHECK(cave.name == "幽暗洞穴 (Dark Cave)");
        CHECK(cave.isSafeZone == false);
        CHECK(cave.maxEnemies == 500);
        CHECK(cave.enemyPool.size() >= 2);
        printf("DEBUG: End Cave SUBCASE\n");
    }
    
    SUBCASE("Unknown biome returns default") {
        printf("DEBUG: Inside Unknown SUBCASE\n");
        CHECK_FALSE(registry.HasBiome("non_existent"));
        const auto& def = registry.GetBiome("non_existent");
        CHECK(def.id == ""); // Default config has empty id
        printf("DEBUG: End Unknown SUBCASE\n");
    }
    printf("DEBUG: End TEST_CASE body\n");
}
