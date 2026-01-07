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

} // namespace NoMoreDay::systems
