#include <doctest/doctest.h>
#include "game/systems/world/TilemapCollisionSystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "game/components/Common.hpp"

TEST_CASE("[Unit] TilemapCollisionSystem - Walkability Logic") {
    NoMoreDay::MapSystem mapSystem;
    mapSystem.generateTownMap(10, 10);
    
    SUBCASE("IsAreaWalkable") {
        NoMoreDay::Vector2 centerPos = {50.0f, 50.0f};
        CHECK(NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, centerPos, 4.0f) == true);
        
        NoMoreDay::Vector2 wallPos = {5.0f, 5.0f}; // Tile 0,0 Wall
        // Radius 4 extends 1 to 9. Inside Tile 0.
        CHECK(NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, wallPos, 4.0f) == false);
        
        NoMoreDay::Vector2 nearWall = {14.0f, 50.0f}; // Tile 1,5.
        // Radius 4. 10 to 18.
        // Tile 1 is Floor. Tile 0 is Wall.
        // Min X = 10. Max X = 18.
        // Tile Min = 1. Tile Max = 1.
        // Should be True.
        CHECK(NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, nearWall, 4.0f) == true);
        
        NoMoreDay::Vector2 touchingWall = {13.0f, 50.0f}; 
        // Radius 4. 9 to 17.
        // Tile Min = 0 (Wall). Tile Max = 1 (Floor).
        // Should be False.
        CHECK(NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, touchingWall, 4.0f) == false);
    }
}
