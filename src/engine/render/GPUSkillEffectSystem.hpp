#pragma once
#include <vector>
#include <raylib.h>
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/SkillVfxEvent.hpp"

#include <array>

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

    void SubmitSkillEvent(const SkillVfxEvent& event);

    struct DistortionRequest {
        float worldX = 0.0f;
        float worldY = 0.0f;
        float radius = 0.0f;
        float strength = 0.0f;
    };
    void DrainDistortionRequests(std::vector<DistortionRequest>& out);
    
    // Uploads data to GPU and renders
    void Render(const Camera2D& camera);

    void Shutdown();

private:
    GPUSkillEffectSystem() = default;

    int m_maxEffects = 0;
    std::vector<components::GPUSkillEffect> m_hostBuffer; // CPU side buffer
    int m_currentCount = 0;
    std::vector<SkillVfxEvent> m_pendingEvents;
    std::vector<DistortionRequest> m_pendingDistortion;
    std::array<int, 10> m_skillFrameCounts = {};

    core::ComputeBuffer m_gpuBuffer; // SSBO
    
    Shader m_shader = { 0 };
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    void InitRender();
    void StageSkillEvents();
    void EmitSkillEventVisual(const SkillVfxEvent& event);
    bool TrySubmitCapped(uint32_t skillId, int cap, const components::GPUSkillEffect& effect);
    int ResolveSkillCap(uint32_t skillId, uint8_t tier) const;
    bool QueueDistortion(float worldX, float worldY, float radius, float strength);
};

} // namespace NoMoreDay::systems
