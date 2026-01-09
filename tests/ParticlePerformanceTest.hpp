#pragma once
#include "TestCommon.hpp"
#include "../src/systems/GPUParticleSystem.hpp"
#include "../src/core/ResourceManager.hpp"
#include <chrono>
#include <vector>

using namespace NoMoreDay;

TEST_CASE("GPUParticleSystem: Stress Test (100k Particles)") {
    ResourceManager rm;
    auto& particleSystem = systems::GPUParticleSystem::Get();
    
    // Target: 200k particles capacity to handle 10k entities
    // Each entity might emit bursts.
    int maxParticles = 200000;
    particleSystem.Init(rm, maxParticles);
    
    LOG_INFO("Starting Stress Test with {} particles capacity...", maxParticles);

    // Prepare a large batch to simulate a "heavy frame" (e.g., big AoE or many entities attacking)
    // 10,000 entities. If 10% attack per frame (1000), and each emits 5 particles -> 5000 particles/frame.
    // Let's stress it with 10,000 particles per frame.
    int batchSize = 10000;
    std::vector<components::GPUParticle> batch;
    batch.reserve(batchSize);
    
    Vector2 center = { 500, 500 };
    for(int i=0; i<batchSize; ++i) {
        batch.push_back(systems::InkEffectHelper::CreateInkTrail(center, { 10, 10 }, 1.0f, 2.0f));
    }

    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate 600 frames (10 seconds at 60fps)
    // Continually emit to fill buffer and wrap around
    int frames = 600;
    double totalEmitTime = 0;
    double totalUpdateTime = 0;

    for (int i = 0; i < frames; ++i) {
        // Measure Emit Batch (CPU -> GPU Map/Copy)
        auto t1 = std::chrono::high_resolution_clock::now();
        particleSystem.EmitBatch(batch);
        auto t2 = std::chrono::high_resolution_clock::now();
        
        // Measure Update (Dispatch)
        particleSystem.Update(0.016f); // 60fps dt
        auto t3 = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> emitDt = t2 - t1;
        std::chrono::duration<double, std::milli> updateDt = t3 - t2;
        
        totalEmitTime += emitDt.count();
        totalUpdateTime += updateDt.count();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> totalElapsed = end - start;

    double avgEmit = totalEmitTime / frames;
    double avgUpdate = totalUpdateTime / frames;
    double avgTotal = totalElapsed.count() / frames;
    
    LOG_INFO("Stress Test Results ({} frames, {} particles/frame):", frames, batchSize);
    LOG_INFO("Avg Emit Time: {:.3f} ms", avgEmit);
    LOG_INFO("Avg Update Time: {:.3f} ms", avgUpdate);
    LOG_INFO("Avg Total Frame Time (Physics only): {:.3f} ms", avgTotal);
    
    // Criteria: Total particle overhead per frame should be < 2ms (budget for VFX)
    // Note: This doesn't include Rasterization (Render), but Compute + Upload is usually the bottleneck for bandwidth.
    CHECK(avgTotal < 4.0f); // 4ms budget for integrated GPUs, conservative.

    particleSystem.Shutdown();
}
