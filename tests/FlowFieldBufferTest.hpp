#pragma once
#include "TestCommon.hpp"
#include "../src/systems/GPUFlowFieldSystem.hpp"
#include "../src/core/ResourceManager.hpp"
#include <vector>

using namespace NoMoreDay;

TEST_CASE("GPUFlowFieldSystem: Buffer Logic") {
    // Requires OpenGL context (provided by main.cpp)
    ResourceManager rm;
    auto& flowSystem = systems::GPUFlowFieldSystem::Get();
    
    // Grid Size 10x10 for testing
    int width = 10;
    int height = 10;
    
    flowSystem.Init(rm, width, height);
    
    // Create a simple U-shape obstacle
    // Target at (5, 5)
    // Wall at x=5, y=2..4
    std::vector<unsigned char> costMap(width * height, 1); // 1 = Walkable
    
    // Set target pos
    Vector2 target = {5.0f, 5.0f};
    Vector2 origin = {0.0f, 0.0f};
    
    // Walls (Cost 255)
    // x=4, y=4 (Obstacle)
    costMap[4 * width + 4] = 255;
    
    flowSystem.Update(costMap, target, origin);
    
    // Read back Flow Buffer
    std::vector<Vector2> flow(width * height);
    flowSystem.GetFlowBuffer().Read(flow.data(), flow.size() * sizeof(Vector2));
    
    // Verify Target Cell Flow (Should be 0,0 or towards center? Shader sets dir to neighbor)
    // Actually flow at target is tricky. Reset shader sets dist=0.
    // Neighbors point to target.
    // Target itself: if neighbors have higher dist, it points nowhere?
    // flow_vector.compute: Check 8 neighbors. If neighbor dist < current dist.
    // Target dist is 0. Neighbors dist > 0.
    // So target cell flow remains 0?
    // Let's check (5,5)
    Vector2 tFlow = flow[5 * width + 5];
    // CHECK(tFlow.x == 0.0f);
    // CHECK(tFlow.y == 0.0f);
    
    // Check neighbor (6,5). Should point to (5,5) => (-1, 0)
    Vector2 nFlow = flow[5 * width + 6];
    CHECK(nFlow.x == doctest::Approx(-1.0f));
    CHECK(nFlow.y == doctest::Approx(0.0f));
    
    // Check neighbor (5,6). Should point to (5,5) => (0, -1)
    Vector2 nFlow2 = flow[6 * width + 5];
    CHECK(nFlow2.x == doctest::Approx(0.0f));
    CHECK(nFlow2.y == doctest::Approx(-1.0f));
    
    // Check behind wall (4,4) is wall.
    // (3,3) should flow AROUND (4,4) towards (5,5).
    // Simple relaxation might pass through corners if diagonal allowed.
    // (4,4) cost is 255.
    
    // Verify Wall Flow is Zero
    Vector2 wFlow = flow[4 * width + 4];
    CHECK(wFlow.x == 0.0f);
    CHECK(wFlow.y == 0.0f);

    flowSystem.Shutdown();
}
