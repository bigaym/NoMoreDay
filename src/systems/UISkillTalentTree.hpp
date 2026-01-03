#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class UISkillTalentTree {
public:
    static void Draw(entt::registry& registry, entt::entity player, uint32_t skillId);
};

} // namespace NoMoreDay
