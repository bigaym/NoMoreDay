#pragma once
#include "TestCommon.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include <vector>

using namespace NoMoreDay;

TEST_CASE("GPUFlowFieldSystem: Crowd Density Flanking") {
    ResourceManager rm;
    auto& flowSystem = systems::GPUFlowFieldSystem::Get();
    
    int width = 20;
    int height = 20;
    flowSystem.Init(rm, width, height);
    
    // 1. Target at (10, 0) TILES = (100, 0) PX
    Vector2 target = {105.0f, 5.0f}; // Center of tile (10, 0)
    Vector2 origin = {0.0f, 0.0f};
    
    // 2. Obstacle in middle (y=5..7, x=3..16)
    // Exits at x=0..2 and x=17..19
    std::vector<unsigned char> costMap(width * height, 1);
    for (int x = 3; x <= 16; x++) {
        for (int y = 5; y <= 7; y++) {
            costMap[y * width + x] = 255;
        }
    }
    
    // 3. Entry point at (10, 15) TILES = (105, 155) PX
    // From (10, 15), both paths (Left/Right) are equidistant.
    
    flowSystem.Update(costMap, width, height, target, origin);
    
    // Check cost at (10, 6) (Should be 255)
    std::vector<uint32_t> costData(width * height);
    flowSystem.GetCostBuffer().Read(costData.data(), costData.size() * sizeof(uint32_t));
    INFO("Cost at (10, 6): " << costData[6 * width + 10]);
    CHECK(costData[6 * width + 10] == 255);

    std::vector<uint32_t> baseIntegration(width * height);
    flowSystem.GetIntegrationBuffer().Read(baseIntegration.data(), baseIntegration.size() * sizeof(uint32_t));
    INFO("Base Target Dist: " << baseIntegration[0 * width + 10]);
    INFO("Base Integration at (2, 5) (Left): " << baseIntegration[5 * width + 2]);
    INFO("Base Integration at (17, 5) (Right): " << baseIntegration[5 * width + 17]);
    INFO("Base Integration at (10, 15): " << baseIntegration[15 * width + 10]);

    std::vector<Vector2> flowBase = flowSystem.DownloadFlowField();

    
    int entryIdx = 15 * width + 10;
    Vector2 vBase = flowBase[entryIdx];
    INFO("Base Flow at (10, 15): " << vBase.x << ", " << vBase.y);
    
    // 4. Add "Enemies" to the RIGHT path (x=17, y=0..19)
    struct MockEntity {
        Vector2 position;
        Vector2 velocity;
        float radius;
        int type;
        int id;
        float padding;
    };
    
    int entityCount = 2000;
    std::vector<MockEntity> entities(entityCount);
    for (int i = 0; i < entityCount; i++) {
        // Place enemies all along the RIGHT exit (x=17, y=0..19)
        int ey = i % 20;
        entities[i].position = {175.0f, (float)ey * 10.0f + 5.0f}; 
        entities[i].radius = 5.0f;
        entities[i].type = 1; // Enemy
    }
    
    // 5. Update density using ComputeBuffer
    core::ComputeBuffer entityBuffer;
    entityBuffer.Create(entities.size() * sizeof(MockEntity), entities.data(), RL_STATIC_DRAW);
    
    flowSystem.UpdateCrowdDensity(entityBuffer.GetId(), entityCount, 10.0f);
    
    // Check if density buffer is non-zero
    std::vector<uint32_t> densityData(width * height);
    flowSystem.GetDensityBuffer().Read(densityData.data(), densityData.size() * sizeof(uint32_t));
    uint32_t totalDensity = 0;
    for (auto d : densityData) totalDensity += d;
    INFO("Total Density in buffer: " << totalDensity);
    CHECK(totalDensity == entityCount);
    CHECK(densityData[5 * width + 17] == 100); 
    
    // 6. Update Flow with density
    flowSystem.Update(costMap, width, height, target, origin);
    
    // Check Integration field
    std::vector<uint32_t> integrationData(width * height);
    flowSystem.GetIntegrationBuffer().Read(integrationData.data(), integrationData.size() * sizeof(uint32_t));
    INFO("Integration at (2, 5) (Left, Clear): " << integrationData[5 * width + 2]);
    INFO("Integration at (17, 5) (Right, Dense): " << integrationData[5 * width + 17]);
    
    // Right should be MUCH higher than Left due to density
    CHECK(integrationData[5 * width + 17] > integrationData[5 * width + 2]);

    std::vector<Vector2> flowWithDensity = flowSystem.DownloadFlowField();

    
    Vector2 vDensity = flowWithDensity[entryIdx];
    INFO("Density-aware Flow at (10, 15): " << vDensity.x << ", " << vDensity.y);
    
    // With 2000 enemies on Right, it SHOULD prefer Left (-x).
    CHECK(vDensity.x < 0.0f);
    
    entityBuffer.Release();
    flowSystem.Shutdown();
}
