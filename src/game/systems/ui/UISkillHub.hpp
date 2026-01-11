#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class UISkillHub {
public:
    static void Draw(entt::registry& registry, entt::entity player);
};

} // namespace NoMoreDay
