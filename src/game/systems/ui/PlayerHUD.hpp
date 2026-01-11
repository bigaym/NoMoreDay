#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay::systems {

class PlayerHUD {
public:
    static void Draw(entt::registry& registry);
};

} // namespace NoMoreDay::systems
