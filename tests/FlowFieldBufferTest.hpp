#pragma once
#include "TestCommon.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
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
    
    // Set target pos (Center of tile 5,5)
    Vector2 target = {55.0f, 55.0f};
    Vector2 origin = {0.0f, 0.0f};
    
    // Walls (Cost 255)
    // x=4, y=4 (Obstacle)
    costMap[4 * width + 4] = 255;
    
    flowSystem.Update(costMap, width, height, target, origin);
    
    // Read back Flow Buffer
    std::vector<Vector2> flow(width * height);
    flowSystem.GetFlowBuffer().Read(flow.data(), flow.size() * sizeof(Vector2));
    
    // Check neighbor (6,5). Should point to (5,5) => (-1, 0)
    Vector2 nFlow = flow[5 * width + 6];
    INFO("Flow at (6, 5): " << nFlow.x << ", " << nFlow.y);
    CHECK(nFlow.x == doctest::Approx(-1.0f));
    CHECK(nFlow.y == doctest::Approx(0.0f));
    
    // Check neighbor (5,6). Should point to (5,5) => (0, -1)
    Vector2 nFlow2 = flow[6 * width + 5];
    INFO("Flow at (5, 6): " << nFlow2.x << ", " << nFlow2.y);
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
