#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/RenderFrameInput.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <string>
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

    // W6 (M0-C): hardware-gate evidence accessors. The gate must derive its
    // pass trace and SDF sign probe from the REAL execution path, never from a
    // synthetic graph/proxy. These expose the pass order of the actual graph
    // compiled inside the last render() call and the real GI distance field
    // (JFAPass) produced by the real render.
    struct GiDistanceFieldInfo {
        uint32_t texture = 0;
        int width = 0;
        int height = 0;
    };
    struct JfaDiagnostics {
        std::string mode;
        uint32_t dispatchTexelCount = 0;
        int dirtyRectArea = 0;
        int expandedRectArea = 0;
        bool plus2Recovery = false;
        bool verificationAttempted = false;
        bool verificationPassed = false;
        bool verificationRecovery = false;
        std::string verificationResult;
    };
    // M0-A R3 occupancy history evidence (GICompositePass R8 ping-pong). The
    // history is updated every real Execute; `historyValid` mirrors
    // HasOccupancyHistory() and `historyResetCount` proves temporal rejection
    // actually occurred (extent/camera/zoom/light/occluder/emissive resets).
    struct GiOccupancyInfo {
        uint32_t texture = 0;
        int width = 0;
        int height = 0;
        bool historyValid = false;
        uint64_t historyResetCount = 0;
        std::string lastResetReason;
    };
    [[nodiscard]] static const std::vector<std::string> &GetLastExecutedPassOrder();
    [[nodiscard]] static GiDistanceFieldInfo GetGiDistanceField();
    [[nodiscard]] static JfaDiagnostics GetJfaDiagnostics();
    [[nodiscard]] static GiOccupancyInfo GetGiOccupancy();

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

    // B4: MSDF glyph variant (glyph.vert + glyph_msdf.frag). Distinct program,
    // so mvp/uFontAtlas/uniform locations are queried against its own id. When
    // the shader fails to load (id == 0), the glyph draw falls back to the
    // bitmap path; the adapter additionally skips emitting MSDF instances when
    // the MSDF atlas registry is unavailable.
    static Shader s_glyphMsdfShader;
    static int s_glyphMsdfMvpLoc;
    static int s_glyphMsdfTexLoc;
    static int s_glyphMsdfPxRangeLoc;
    
public:
    // Phase 4: Loot Label Spatial Optimization
    // (moved to Game-side GameplayRenderAdapter: s_itemGrid / s_itemGridDirty)

private:
    // Rendering Queues
    static std::vector<NoMoreDay::components::GPULabelInstance> s_labelBuffer;
    static std::vector<NoMoreDay::components::GPUGlyphInstance> s_glyphBuffer; // New
    static NoMoreDay::render::resources::FramebufferHandle s_hdrSceneBuffer;
};
