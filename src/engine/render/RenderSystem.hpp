#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "app/SharedContext.hpp"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <vector>

// Forward declaration for GPU buffer
namespace NoMoreDay::core {
    class ComputeBuffer;
}

namespace NoMoreDay::systems {
    class SIMDSpatialGrid;
}

class RenderSystem {
public:
    // --- Phase 1 Optimization: Shared Visibility Cache ---
    struct VisibleItemCache {
        struct ItemData {
            entt::entity entity;
            Rectangle worldRect; // World Space Bounds for Label
            // Vector2 screenPos; // Removed: Use World Space check
            // float radius;      // Removed: Use Rect check
        };
        static std::vector<ItemData> visibleItems;
        static void Clear() { visibleItems.clear(); }
    };

    static void render(entt::registry &registry,
                       const NoMoreDay::SharedContext &context,
                       const Camera2D &camera);
    
    static void Initialize();
    static void Shutdown();

    // Screen Shake API
    static void AddScreenShake(float intensity);
    static void AddDistortionSource(float worldX, float worldY, float radius,
                                    float strength);
    static void UpdateShake(float dt);
    static Vector2 GetShakeOffset();
    static void SetShakeMultiplier(float multiplier) { s_shakeMultiplier = multiplier; }

private:
    static float s_trauma;
    static float s_shakeMultiplier;
    
    // --- Instanced Label Rendering ---
    static Shader s_labelShader;
    static int s_labelMvpLoc;
    static std::unique_ptr<NoMoreDay::core::ComputeBuffer> s_labelInstanceBuffer;

    // Task 1.4: GPU Glyph Rendering
    static Shader s_glyphShader;
    static int s_glyphMvpLoc;
    static int s_glyphTexLoc;
    static std::unique_ptr<NoMoreDay::core::ComputeBuffer> s_glyphInstanceBuffer;
    
public:
    // Phase 4: Loot Label Spatial Optimization
    static std::unique_ptr<NoMoreDay::systems::SIMDSpatialGrid> s_itemGrid;
    static bool s_itemGridDirty;

private:
    // Rendering Queues
    static std::vector<NoMoreDay::components::GPULabelInstance> s_labelBuffer;
    static std::vector<NoMoreDay::components::GPUGlyphInstance> s_glyphBuffer; // New
    static NoMoreDay::render::resources::FramebufferHandle s_hdrSceneBuffer;
};
