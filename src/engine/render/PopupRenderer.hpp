#pragma once

#include <vector>
#include <raylib.h>
#include "engine/render/GPUData.hpp"
#include "engine/render/PersistentBuffer.hpp"

namespace NoMoreDay::render {

/**
 * @brief GPU-instanced renderer for damage popups and combat text.
 * 
 * Performance:
 * - Uses a PersistentBuffer (SSBO) for zero-copy data upload.
 * - Single DrawArraysInstanced call for thousands of popups.
 * - Supports billboard rendering and basic animations (scaling, fading).
 */
class PopupRenderer {
public:
    static PopupRenderer& Get();

    void Init();
    void Shutdown();

    /**
     * @brief Emit a new damage popup.
     * @param position World position.
     * @param amount Damage value (will be converted to string).
     * @param isCrit Whether it's a critical hit (affects color/animation).
     * @param color Base color.
     */
    void Emit(Vector2 position, int amount, bool isCrit, Color color = WHITE);

    /**
     * @brief Emit custom status text (limited to pre-rendered glyphs).
     */
    void EmitStatus(Vector2 position, const char* text, Color color = WHITE);

    void Update(float dt);
    void Render(const Matrix& viewProj);

private:
    PopupRenderer() = default;
    
    struct PopupState {
        Vector2 position;
        Vector2 velocity;
        float timer;
        float lifeTime;
        int amount;
        char amountText[16];
        bool isCrit;
        Color color;
        bool active = false;
    };

    static constexpr int MAX_POPUPS = 2048;
    
    bool m_initialized = false;
    std::vector<PopupState> m_popups;
    int m_activeCount = 0;

    // GPU Resources
    PersistentBuffer m_instanceBuffer;
    Shader m_shader = { 0 };
    Texture2D m_atlas = { 0 };
    
    // Uniform locations
    int m_uViewProj = -1;
    int m_uAtlas = -1;
    int m_uTime = -1;

    // Quad VAO (shared or internal)
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    void CreateResources();
    void LoadGlyphAtlas();
};

} // namespace NoMoreDay::render
