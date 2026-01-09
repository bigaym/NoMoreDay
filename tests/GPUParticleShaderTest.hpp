#pragma once
#include "TestCommon.hpp"
#include "../src/systems/GPUParticleSystem.hpp"
#include "../src/core/ResourceManager.hpp"

TEST_CASE("GPUParticleSystem Shader Compilation") {
    ResourceManager rm;
    // GPUParticleSystem is a singleton with private constructor
    auto& particleSystem = NoMoreDay::systems::GPUParticleSystem::Get();
    
    // Init should compile shaders
    particleSystem.Init(rm, 100);
    
    // Emit a particle using InkEffectHelper
    auto p = NoMoreDay::systems::InkEffectHelper::CreateInkTrail({ 100, 100 }, { 10, 10 }, 1.0f, 1.0f);
    
    particleSystem.Emit(p);
    
    // Test Splash
    auto splash = NoMoreDay::systems::InkEffectHelper::CreateInkSplash({ 200, 200 }, 10, 50.0f, 100.0f);
    particleSystem.EmitBatch(splash);
    
    // Run update to trigger Compute Shader
    particleSystem.Update(0.016f); 
    
    // Run render to trigger Vertex/Frag Shader
    BeginDrawing();
    ClearBackground(RAYWHITE);
    particleSystem.Render();
    EndDrawing();

    CHECK(true);
    
    particleSystem.Shutdown();
}
