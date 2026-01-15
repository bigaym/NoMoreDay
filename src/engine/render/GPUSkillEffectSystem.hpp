#pragma once
#include <vector>
#include <raylib.h>
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/resource/ResourceManager.hpp"

namespace NoMoreDay::systems {

class GPUSkillEffectSystem {
public:
    static GPUSkillEffectSystem& Get() {
        static GPUSkillEffectSystem instance;
        return instance;
    }

    void Init(ResourceManager& rm, int maxEffects = 10000);
    
    // Call this every frame to add an effect to be rendered
    void Submit(const components::GPUSkillEffect& effect);
    
    // Uploads data to GPU and renders
    void Render(const Camera2D& camera);

    void Shutdown();

private:
    GPUSkillEffectSystem() = default;

    int m_maxEffects = 0;
    std::vector<components::GPUSkillEffect> m_hostBuffer; // CPU side buffer
    int m_currentCount = 0;

    core::ComputeBuffer m_gpuBuffer; // SSBO
    
    Shader m_shader = { 0 };
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    void InitRender();
};

} // namespace NoMoreDay::systems
