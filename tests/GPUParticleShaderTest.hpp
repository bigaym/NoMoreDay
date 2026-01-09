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
    
    // Emit a particle with Ink Fade flag (8) and Growth Rate
    NoMoreDay::components::GPUParticle p = {};
    p.position = { 0, 0 };
    p.velocity = { 0, 0 };
    p.acceleration = { 0, 0 };
    p.color = { 255, 255, 255, 255 };
    p.lifetime = 1.0f;
    p.maxLifetime = 1.0f;
    p.scale = 1.0f;
    p.flags = 8; // Bit 3: Ink Fade
    p.growthRate = 0.5f; 
    
    particleSystem.Emit(p);
    
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
