#pragma once
#include <cstdint>

// Forward declarations
struct Vector2; // If Raylib is included? No. Just use void* or manual struct.

namespace NoMoreDay {

class SkillTreeUI {
public:
    struct Vec2 { float x, y; };
    static void Draw(void* registry, int player, uint32_t skillId);

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
