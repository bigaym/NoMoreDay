#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay::systems {

class MonsterHealthBarSystem {
public:
    static void Render(entt::registry& registry, const Camera2D& camera);
};

} // namespace NoMoreDay::systems
