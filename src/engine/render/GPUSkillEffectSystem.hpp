#pragma once
#include <vector>
#include <raylib.h>
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/SkillVfxEvent.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>

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

    struct ResistOverlayRequest {
        Vector2 worldPos = {0.0f, 0.0f};
        uint8_t resistDebuffType = 0u;
        float intensity = 1.0f;
    };
    void DrainResistOverlayRequests(std::vector<ResistOverlayRequest>& out);
    
    // Uploads data to GPU and renders
    void Render(const Camera2D& camera);

    void Shutdown();

    enum class RecipeActionKind : uint8_t {
        Overlay = 0,
        ParticleBurst = 1,
        TrailStroke = 2,
        DistortionPulse = 3,
        ResistOverlay = 4,
    };

    struct SkillVfxRecipeSelector {
        int skillId = -1;            // -1 wildcard
        int eventType = -1;          // -1 wildcard
        int elementType = -1;        // -1 wildcard
        int resistDebuffType = -1;   // -1 wildcard
        uint32_t requiredNodeRoleMask = SkillVfxNodeRoleMask::None;
    };

    struct SkillVfxRecipeAction {
        RecipeActionKind kind = RecipeActionKind::Overlay;
        int count = 1;
        float radius = 16.0f;
        float angle = 360.0f;
        float softness = 0.35f;
        float type = 1.0f;
        float speed = 140.0f;
        float alpha = 1.0f;
        float spread = 0.0f;
        float distortionStrength = 0.12f;
        float width = 8.0f;
        float lifetime = 0.2f;
        float trailLength = 0.25f;
    };

    struct SkillVfxRecipe {
        std::string name;
        int priority = 0;
        SkillVfxRecipeSelector selector = {};
        std::vector<SkillVfxRecipeAction> actions;
    };

private:
    GPUSkillEffectSystem() = default;

    int m_maxEffects = 0;
    std::vector<components::GPUSkillEffect> m_hostBuffer; // CPU side buffer
    int m_currentCount = 0;
    std::vector<SkillVfxEvent> m_pendingEvents;
    std::vector<DistortionRequest> m_pendingDistortion;
    std::vector<ResistOverlayRequest> m_pendingResistOverlay;
    std::array<int, 10> m_skillFrameCounts = {};
    std::array<int, 10> m_triggerFrameCounts = {};
    std::array<float, 10> m_triggerCarryBlend = {};
    std::unordered_set<size_t> m_triggerDedupKeys;
    std::vector<SkillVfxRecipe> m_recipes;

    core::ComputeBuffer m_gpuBuffer; // SSBO
    
    Shader m_shader = { 0 };
    int m_timeLoc = -1;
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    void InitRender();
    void StageSkillEvents();
    void EmitSkillEventVisual(const SkillVfxEvent& event);
    void EmitLegacySkillEventVisual(const SkillVfxEvent& event);
    bool EmitRecipeDrivenVisual(const SkillVfxEvent& event);
    void LoadSkillVfxRecipes(const std::string& path);
    void LoadBuiltinRecipes();
    bool TrySubmitCapped(uint32_t skillId, int cap, const components::GPUSkillEffect& effect);
    int ResolveSkillCap(uint32_t skillId, uint8_t tier) const;
    int ResolveTriggerCap(uint32_t skillId, uint8_t tier) const;
    bool QueueDistortion(float worldX, float worldY, float radius, float strength);
    bool ConsumeTriggerBudget(const SkillVfxEvent& event, uint8_t tier, float& actionScale, float& intensityScale);
    bool ShouldCullDuplicateTrigger(const SkillVfxEvent& event);
};

} // namespace NoMoreDay::systems
