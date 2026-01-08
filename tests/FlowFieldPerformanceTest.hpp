#pragma once
#include "TestCommon.hpp"
#include "../src/systems/GPUFlowFieldSystem.hpp"
#include "../src/core/ResourceManager.hpp"
#include <chrono>
#include <vector>

using namespace NoMoreDay;

TEST_CASE("GPUFlowFieldSystem: Performance Stress Test") {
    ResourceManager rm;
    auto& flowSystem = systems::GPUFlowFieldSystem::Get();
    
    // Spec target grid size: 256x256
    int width = 256;
    int height = 256;
    
    flowSystem.Init(rm, width, height);
    
    std::vector<unsigned char> costMap(width * height, 1);
    // Add some random walls
    for (int i = 0; i < (width * height) / 10; ++i) {
        costMap[rand() % (width * height)] = 255;
    }
    
    Vector2 target = {128.0f, 128.0f};
    Vector2 origin = {0.0f, 0.0f};
    
    // Warm up
    flowSystem.Update(costMap, width, height, target, origin);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        flowSystem.Update(costMap, width, height, target, origin);
    }
    
    // Final sync to ensure all work is done
    auto result = flowSystem.DownloadFlowField();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    double avgTime = elapsed.count() / iterations;
    
    LOG_INFO("GPU Flow Field Performance ({}x{}): avg {:.3f}ms per update", width, height, avgTime);
    
    // Performance Requirement: < 10ms for integrated graphics, < 2ms for discrete
    CHECK(avgTime < 10.0f); 

    flowSystem.Shutdown();
}
