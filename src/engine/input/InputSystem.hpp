#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class InputSystem {
public:
    static void update(entt::registry& registry, const Camera2D& camera);
};

} // namespace NoMoreDay
