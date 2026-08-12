#pragma once
#include <cstddef>
#include <cstdint>

#include "entt/entt.hpp"
#include "game/foundation/components/SkillDefs.hpp"

// Forward declarations
struct Vector2; // If Raylib is included? No. Just use void* or manual struct.

namespace NoMoreDay {

namespace ui {
class TooltipController; // fwd: hovered-skill channel (U8).
}

// Talent tree panel widget.
//
// U7 cleanup: the legacy static mutable state (s_viewOffset, s_viewZoom,
// s_lastSkillId, s_lastMouseLogicPos, s_layoutEditMode, s_draggingNodeId,
// s_dragNodeOffset) was migrated to instance members so the UI no longer
// keeps any static mutable rendering state (design invariant 4). The
// ComputeTooltipLayoutMetrics pure helper stays static: it is stateless and
// side-effect free. U8 final narrowing: the panel reads its alpha from the
// caller (was State.skillTreeAlpha), uses UISystem scale helpers (was
// State.scaleFactor) and routes the hovered node through the bound tooltip
// controller (was State.hoveredSkillId).
class SkillTreeUI {
public:
    struct Vec2 { float x, y; };
    struct TooltipLayoutMetrics {
        float tooltipHeight = 0.0f;
        float descriptionTop = 0.0f;
        float descriptionHeight = 0.0f;
        float descriptionBottom = 0.0f;
        float quantitativeTop = 0.0f;
        float quantitativeHeight = 0.0f;
        float footerTop = 0.0f;
        float footerHeight = 0.0f;
        float footerGap = 0.0f;
    };

    SkillTreeUI() = default;
    SkillTreeUI(const SkillTreeUI&) = delete;
    SkillTreeUI& operator=(const SkillTreeUI&) = delete;

    // U8: bind the tooltip controller for the hovered-node channel (may be
    // null in headless tests). Called by SkillTreeController.
    void SetTooltip(ui::TooltipController* tooltip) noexcept { m_tooltip = tooltip; }

    // `alpha` is the animated panel alpha supplied by the owning controller
    // (was State.skillTreeAlpha).
    void Draw(entt::registry& registry, entt::entity player, uint32_t skillId,
              float alpha = 1.0f);
    static TooltipLayoutMetrics ComputeTooltipLayoutMetrics(float tooltipHeight,
                                                            std::size_t quantitativeLineCount,
                                                            std::size_t footerLineCount);

private:
    Vec2 m_viewOffset = { 0, 0 };
    float m_viewZoom = 1.0f;
    uint32_t m_lastSkillId = 0;
    Vec2 m_lastMouseLogicPos = { 0, 0 };
    bool m_layoutEditMode = false;
    uint32_t m_draggingNodeId = 0;
    Vec2 m_dragNodeOffset = { 0, 0 };
    ui::TooltipController* m_tooltip = nullptr; // Hover channel (U8).
    uint32_t m_selectedSkillId = NoMoreDay::INVALID_SKILL_ID; // View reset (U8).
};

} // namespace NoMoreDay
