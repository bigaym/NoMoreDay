#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/RenderFrameInput.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <vector>

// Forward declaration for GPU buffer
namespace NoMoreDay::core {
    class ComputeBuffer;
}

namespace NoMoreDay::render {
    class GameplayRenderHooks;
}

struct OffscreenTargetDescriptor {
    uint32_t framebuffer = 0;
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    bool scissorEnabled = false;
    int scissorX = 0;
    int scissorY = 0;
    int scissorWidth = 0;
    int scissorHeight = 0;
    int renderExtentWidth = 0;
    int renderExtentHeight = 0;
    bool flipY = false;
    uint32_t internalFormat = 0;
    bool ownsFramebuffer = false; // RenderSystem does not own/free external target
};

struct RenderTargetExtent {
    int width = 0;
    int height = 0;
    float scale = 1.0f;
};

class RenderSystem {
public:
    struct ScopedTargetStateGuard {
        OffscreenTargetDescriptor target;
        ScopedTargetStateGuard();
        ~ScopedTargetStateGuard();
    };

    // --- Phase 1 Optimization: Shared Visibility Cache ---
    // (moved to Game-side GameplayRenderAdapter)

    static void render(entt::registry &registry,
                       const NoMoreDay::render::RenderFrameInput &context,
                       const Camera2D &camera,
                       NoMoreDay::render::GameplayRenderHooks *gameplayHooks = nullptr);
    
    static void Initialize();
    static void Shutdown();

    // World/HDR targets may be reduced while screen-space UI remains native.
    [[nodiscard]] static RenderTargetExtent GetRenderTargetExtent(int nativeWidth,
                                                                  int nativeHeight);
    [[nodiscard]] static float GetRenderScale();
    static void NotifyRenderTargetResize();

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
    // (moved to Game-side GameplayRenderAdapter: s_itemGrid / s_itemGridDirty)

private:
    // Rendering Queues
    static std::vector<NoMoreDay::components::GPULabelInstance> s_labelBuffer;
    static std::vector<NoMoreDay::components::GPUGlyphInstance> s_glyphBuffer; // New
    static NoMoreDay::render::resources::FramebufferHandle s_hdrSceneBuffer;
};
