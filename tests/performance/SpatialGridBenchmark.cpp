#pragma once
#include "doctest.h"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "game/components/Common.hpp"
#include <vector>
#include <chrono>
#include <random>

using namespace NoMoreDay;
using namespace NoMoreDay::systems;

    TEST_CASE("[Performance] SIMDSpatialGrid - Rebuild and Query Benchmark") {
        // Setup: 5000x5000 world, 10000 entities
        int worldW = 5000;
        int worldH = 5000;
        float cellSize = 32.0f;
        int cols = static_cast<int>(worldW / cellSize) + 1;
        int rows = static_cast<int>(worldH / cellSize) + 1;

        SIMDSpatialGrid grid(cols, rows, cellSize);
        entt::registry reg;
        
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> distX(0.0f, (float)worldW);
        std::uniform_real_distribution<float> distY(0.0f, (float)worldH);

        for(int i=0; i<10000; ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, distX(rng), distY(rng));
        }

        // Measure Rebuild
        auto startRebuild = std::chrono::high_resolution_clock::now();
        auto view = reg.view<Position>();
        grid.rebuild<Position>(view, reg);
        auto endRebuild = std::chrono::high_resolution_clock::now();
        
        auto rebuildTime = std::chrono::duration_cast<std::chrono::microseconds>(endRebuild - startRebuild).count();
        MESSAGE("Rebuild 10k entities: ", rebuildTime, " us");

        // Measure Query (1000 queries)
        int queryCount = 1000;
        long long hitCountTotal = 0;
        
        std::vector<Position> queryPoints;
        queryPoints.reserve(queryCount);
        for(int i=0; i<queryCount; ++i) {
            queryPoints.push_back({distX(rng), distY(rng)});
        }
        
        auto startQuery = std::chrono::high_resolution_clock::now();
        for(const auto& p : queryPoints) {
        grid.query(p, 100.0f, [&](entt::entity, const Position&) -> bool {
            hitCountTotal++;
            return true;
        });
        }
        auto endQuery = std::chrono::high_resolution_clock::now();
        
        auto queryTime = std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery).count();
        double avgQuery = (double)queryTime / queryCount;
        
        MESSAGE("Query 1000 times (R=100): ", queryTime, " us (Avg ", avgQuery, " us/query)");
        MESSAGE("Total Hits: ", hitCountTotal);
    }

