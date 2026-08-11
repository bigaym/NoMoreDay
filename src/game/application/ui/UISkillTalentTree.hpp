#pragma once
#include <cstddef>
#include <cstdint>

#include "entt/entt.hpp"

// Forward declarations
struct Vector2; // If Raylib is included? No. Just use void* or manual struct.

namespace NoMoreDay {

// Talent tree panel widget.
//
// U7 cleanup: the legacy static mutable state (s_viewOffset, s_viewZoom,
// s_lastSkillId, s_lastMouseLogicPos, s_layoutEditMode, s_draggingNodeId,
// s_dragNodeOffset) was migrated to instance members so the UI no longer
// keeps any static mutable rendering state (design invariant 4). The
// ComputeTooltipLayoutMetrics pure helper stays static: it is stateless and
// side-effect free.
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

    void Draw(entt::registry& registry, entt::entity player, uint32_t skillId);
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
};

} // namespace NoMoreDay
