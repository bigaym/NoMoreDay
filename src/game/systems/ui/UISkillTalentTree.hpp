#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class UISkillTalentTree {
public:
    static void Draw(entt::registry& registry, entt::entity player, uint32_t skillId);

private:
    static inline Vector2 s_viewOffset = { 0, 0 };
    static inline float s_viewZoom = 1.0f;
    static inline uint32_t s_lastSkillId = 0;
    static inline Vector2 s_lastMouseLogicPos = { 0, 0 };
};

} // namespace NoMoreDay
