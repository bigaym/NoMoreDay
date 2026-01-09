#pragma once
#include <vector>
#include <entt/entt.hpp>
#include "../core/ComputeBuffer.hpp"
#include "../components/GPUData.hpp"
#include "../core/ResourceManager.hpp"
#include "raylib.h"

namespace NoMoreDay::systems {

class GPUParticleSystem {
public:
    static GPUParticleSystem& Get() {
        static GPUParticleSystem instance;
        return instance;
    }

    void Init(ResourceManager& rm, int maxParticles = 100000);
    void Update(float dt);
    void Render();
    
    // Emit a single particle
    void Emit(const components::GPUParticle& p);

    // Emit a batch of particles
    void EmitBatch(const std::vector<components::GPUParticle>& particles);

    void Shutdown();

private:
    GPUParticleSystem() = default;
    
    int m_maxParticles = 0;
    int m_poolIndex = 0;
    
    core::ComputeBuffer m_particleBuffer;
    Shader m_computeShader;
    Shader m_renderShader;
    
    // Optional: local buffer for batch updates if needed
    // But for particles, we can often just update the SSBO directly for emissions
    
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
};

class InkEffectHelper {
public:
    static constexpr Color COLOR_INK_LIGHT = { 40, 40, 45, 120 };
    static constexpr Color COLOR_INK_DARK = { 20, 20, 25, 200 };
    static constexpr Color COLOR_GOLD_CORE = { 255, 215, 0, 255 };
    static constexpr Color COLOR_GOLD_GLOW = { 255, 180, 50, 150 };

    // Create a generic ink particle (for trails/ambient)
    static components::GPUParticle CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life);

    // Create a burst of ink particles
    static std::vector<components::GPUParticle> CreateInkSplash(Vector2 pos, int count, float radius, float force);

    // Create a gold stream particle (for empowered effects)
    static components::GPUParticle CreateGoldParticle(Vector2 pos, Vector2 vel, float scale);
};

} // namespace NoMoreDay::systems
