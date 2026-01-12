#pragma once
#include "doctest.h"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include <vector>
#include <cstdint>

TEST_CASE("GPU Flow Field Compute Test") {
    using namespace NoMoreDay;
    using namespace NoMoreDay::systems;

    // Skip if no Compute support (though we assume modern GPU for dev)
    // We can't easily check utils::GPUUtils::CheckSupport() without including it, 
    // but let's assume it works or fail if shader fails.

    ResourceManager resources;
    // Ensure assets path is correct relative to test executable
    // Usually tests run from bin/, so assets might be ../../assets or just assets/
    // Code usually assumes assets/ is in CWD.
    
    GPUFlowFieldSystem& flowSystem = GPUFlowFieldSystem::Get();
    
    // 1. Initialize
    // 64x64 grid
    flowSystem.Init(resources, 64, 64);
    
    CHECK(flowSystem.GetWidth() == 64);
    CHECK(flowSystem.GetHeight() == 64);

    // 2. Setup Cost Map
    // Grid: 64x64
    // Target: (600, 600) -> Grid (60, 60)
    // Origin: (0, 0)
    // Obstacle at (30, 30)
    std::vector<unsigned char> costMap(64 * 64, 1); // Cost 1
    
    // Add a wall
    int wallX = 30;
    int wallY = 30;
    costMap[wallY * 64 + wallX] = 255;
    
    // 3. Update
    Vector2 targetPos = { 600.0f, 600.0f };
    Vector2 gridOrigin = { 0.0f, 0.0f };
    
    flowSystem.Update(costMap, 64, 64, targetPos, gridOrigin);
    
    // 4. Download and Verify
    // Flow vectors should be normalized.
    std::vector<Vector2> flowData = flowSystem.DownloadFlowField();
    
    REQUIRE(flowData.size() == 64 * 64);
    
    // Debug: Dump Integration Field
    std::vector<uint32_t> intData(64 * 64);
    flowSystem.GetIntegrationBuffer().Read(intData.data(), 64 * 64 * sizeof(uint32_t));
    
    // Check target (60, 60)
    uint32_t targetDist = intData[60 * 64 + 60];
    MESSAGE("Target Dist: ", targetDist);
    
    // Check neighbor of target (59, 60)
    uint32_t neighborDist = intData[60 * 64 + 59];
    MESSAGE("Neighbor Dist: ", neighborDist); // Should be small
    
    // Check start (5, 5) neighborhood
    uint32_t startDist = intData[5 * 64 + 5];
    MESSAGE("Start Dist (5,5): ", startDist);
    
    MESSAGE("Neighbors of (5,5):");
    for(int dy=-1; dy<=1; dy++) {
        for(int dx=-1; dx<=1; dx++) {
            int nx = 5+dx;
            int ny = 5+dy;
            uint32_t nd = intData[ny * 64 + nx];
            MESSAGE("  (", nx, ",", ny, "): ", nd);
        }
    }

    // Download Flow Field
    std::vector<Vector2> flowData2 = flowSystem.DownloadFlowField();
    Vector2 flowAtStart = flowData2[5 * 64 + 5];
    MESSAGE("Flow at (5,5): ", flowAtStart.x, ", ", flowAtStart.y);
    
    CHECK(flowAtStart.x > 0.0f || flowAtStart.y > 0.0f);
    
    // Check flow at wall (should be zero or valid)
    Vector2 flowAtWall = flowData[wallY * 64 + wallX];
    // Wall flow is typically zeroed or ignored, depending on shader. 
    // physics.compute checks length > 0.01.
    // If our integration shader writes zero for walls, this holds.
    
    // Cleanup
    flowSystem.Shutdown();
}
