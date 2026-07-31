#pragma once
#include "doctest.h"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "game/components/Common.hpp"
#include <vector>
#include <entt/entt.hpp>

TEST_SUITE("SIMDSpatialGrid") {
    using namespace NoMoreDay::systems;
    // using namespace NoMoreDay::components; // Removed
    // Position is global

    TEST_CASE("[Unit] SIMDSpatialGrid - Basic Rebuild and Query") {
        // Grid: 10x10, CellSize 10.0
        SIMDSpatialGrid grid(10, 10, 10.0f);
        entt::registry reg;

        // Create some entities
        // Entity A at (5, 5) -> Cell (0, 0)
        auto e1 = reg.create();
        reg.emplace<Position>(e1, 5.0f, 5.0f);

        // Entity B at (15, 5) -> Cell (1, 0)
        auto e2 = reg.create();
        reg.emplace<Position>(e2, 15.0f, 5.0f);

        // Entity C at (5, 15) -> Cell (0, 1)
        auto e3 = reg.create();
        reg.emplace<Position>(e3, 5.0f, 15.0f);

        auto view = reg.view<Position>();
        grid.rebuild<Position>(view, reg);

        // Query near (5, 5) with radius 2.0 -> Should find e1 only
        int count = 0;
        grid.query({5.0f, 5.0f}, 2.0f, [&](entt::entity e, const Position& p) -> bool {
            count++;
            CHECK(e == e1);
            CHECK(p.x == 5.0f);
            CHECK(p.y == 5.0f);
            return true;
        });
        CHECK(count == 1);

        // Query near (5, 5) with radius 12.0 -> Should find e1, e2, e3
        // e2 dist = 10, e3 dist = 10. Both <= 12
        count = 0;
        std::vector<entt::entity> hits;
        grid.query({5.0f, 5.0f}, 12.0f, [&](entt::entity e, const Position& p) -> bool {
            hits.push_back(e);
            return true;
        });
        CHECK(hits.size() == 3);
        
        // Verify contains e1, e2, e3
        bool hasE1 = false, hasE2 = false, hasE3 = false;
        for(auto e : hits) {
            if(e == e1) hasE1 = true;
            if(e == e2) hasE2 = true;
            if(e == e3) hasE3 = true;
        }
        CHECK(hasE1);
        CHECK(hasE2);
        CHECK(hasE3);
    }
    
    TEST_CASE("[Unit] SIMDSpatialGrid - Boundary and Alignment") {
        // Create enough entities to trigger SIMD boundary logic (e.g. > 8)
        SIMDSpatialGrid grid(10, 10, 10.0f);
        entt::registry reg;
        
        // 20 entities at same location
        for(int i=0; i<20; ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, 5.0f, 5.0f);
        }
        
        auto view = reg.view<Position>();
        grid.rebuild<Position>(view, reg);
        
        int count = 0;
        grid.query({5.0f, 5.0f}, 1.0f, [&](entt::entity e, const Position& p) -> bool {
            count++;
            return true;
        });
        CHECK(count == 20);
    }

    TEST_CASE("[Unit] SIMDSpatialGrid - Empty Grid") {
        SIMDSpatialGrid grid(10, 10, 10.0f);
        entt::registry reg;
        auto view = reg.view<Position>();
        grid.rebuild<Position>(view, reg); // Empty view
        
        int count = 0;
        grid.query({0.0f, 0.0f}, 100.0f, [&](entt::entity e, const Position& p) -> bool {
            count++;
            return true;
        });
        CHECK(count == 0);
    }
}