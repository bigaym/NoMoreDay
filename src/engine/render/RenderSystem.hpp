#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "app/SharedContext.hpp"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include <vector>

// Forward declaration for GPU buffer
namespace NoMoreDay::core {
    class ComputeBuffer;
}

class RenderSystem {
public:
    static void Initialize(); // New init method
    static void Shutdown();   // New shutdown method
    static void render(entt::registry& registry, const NoMoreDay::SharedContext& context, const Camera2D& camera);

    // Screen Shake API
    static void AddScreenShake(float intensity);
    static void UpdateShake(float dt);
    static Vector2 GetShakeOffset();

private:
    static float s_trauma;
    
    // --- Instanced Label Rendering ---
    struct TextRenderCmd {
        Vector2 position;
        const char* text; // Pointer to existing component string (unsafe if component deleted, but safe within frame)
        float fontSize;
        Color color;
        bool centered;
    };

    static Shader s_labelShader;
    static int s_labelMvpLoc;
    static std::unique_ptr<NoMoreDay::core::ComputeBuffer> s_labelInstanceBuffer;
    
    // Rendering Queues
    static std::vector<NoMoreDay::components::GPULabelInstance> s_labelBuffer;
    static std::vector<TextRenderCmd> s_textQueue;
};
