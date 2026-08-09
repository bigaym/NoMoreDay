#pragma once
#include <cstddef>
#include <cstdint>

// Forward declarations
struct Vector2; // If Raylib is included? No. Just use void* or manual struct.

namespace NoMoreDay {

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

    static void Draw(void* registry, int player, uint32_t skillId);
    static TooltipLayoutMetrics ComputeTooltipLayoutMetrics(float tooltipHeight,
                                                            std::size_t quantitativeLineCount,
                                                            std::size_t footerLineCount);

public:
    static Vec2 s_viewOffset;
    static float s_viewZoom;
    static uint32_t s_lastSkillId;
    static Vec2 s_lastMouseLogicPos;
    static bool s_layoutEditMode;
    static uint32_t s_draggingNodeId;
    static Vec2 s_dragNodeOffset;
};

} // namespace NoMoreDay
