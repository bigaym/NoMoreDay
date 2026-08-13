#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <raylib.h>
#include "engine/render/GPUData.hpp"
#include "engine/render/PersistentBuffer.hpp"

namespace NoMoreDay::render {

/**
 * @brief Status popup kinds rendered by the popup glyph protocol.
 *
 * Glyph atlas Row 1 pairs (2 glyphs per kind, 2 CJK chars each):
 *   Crit 16/17, Dodge 18/19, Block 20/21, Immune 22/23, Absorb 24/25.
 * Kinds without an active caller (Crit/Immune/Absorb) are kept so the
 * protocol stays complete and future call sites remain type-safe.
 */
enum class StatusPopupKind : uint8_t { Crit, Dodge, Block, Immune, Absorb };

/**
 * @brief Single source of truth for the display text of each status kind.
 *
 * These are user-facing display strings (localized through data); the
 * Chinese names are preserved verbatim and never used as comparison keys.
 */
[[nodiscard]] constexpr std::string_view DisplayName(StatusPopupKind kind) {
  switch (kind) {
    case StatusPopupKind::Crit:
      return "暴击";
    case StatusPopupKind::Dodge:
      return "闪避";
    case StatusPopupKind::Block:
      return "格挡";
    case StatusPopupKind::Immune:
      return "免疫";
    case StatusPopupKind::Absorb:
      return "吸收";
  }
  return "?";
}

/**
 * @brief GPU-instanced renderer for damage popups and combat text.
 * 
 * Performance:
 * - Uses a PersistentBuffer (SSBO) for zero-copy data upload.
 * - Single DrawArraysInstanced call for thousands of popups.
 * - Supports billboard rendering and basic animations (scaling, fading).
 */
// DEPRECATED(v4_gpu_text_rendering): CPU popup renderer fallback path.
// Primary text route should use GPUTextSystem when enabled by tier/feature flag.
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
     * @brief Emit a status popup (limited to pre-rendered glyph pairs).
     * @param position World position.
     * @param kind Status kind; selects the glyph pair (see StatusPopupKind).
     * @param color Base color.
     */
    void EmitStatus(Vector2 position, StatusPopupKind kind, Color color = WHITE);

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
        uint8_t glyphs[16];
        uint8_t glyphCount;
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
    int m_nextSlot = 0; // Unified slot counter
    
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    void CreateResources();
    void LoadGlyphAtlas();
};

} // namespace NoMoreDay::render
