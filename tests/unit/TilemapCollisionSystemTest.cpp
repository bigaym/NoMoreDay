#include "doctest.h"
#include "game/systems/world/TilemapCollisionSystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include "game/foundation/components/Common.hpp"

TEST_CASE("[Unit] TilemapCollisionSystem - Basic Logic") {
    MapSystem mapSystem;
    mapSystem.generateTownMap(10, 10);
    
    SUBCASE("IsAreaWalkable Check") {
        // Tile 0 at x=0..10 is Wall.
        // Tile 1 at x=10..20 is Floor.
        
        bool r1 = NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, {50.0f, 50.0f}, 4.0f);
        CHECK(r1 == true);
        
        bool r2 = NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, {5.0f, 5.0f}, 4.0f);
        CHECK(r2 == false);
        
        bool r3 = NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, {14.0f, 50.0f}, 4.0f);
        CHECK(r3 == true);
        
        bool r4 = NoMoreDay::TilemapCollisionSystem::IsAreaWalkable(mapSystem, {14.0f, 50.0f}, 4.1f);
        CHECK(r4 == false);
    }
}
