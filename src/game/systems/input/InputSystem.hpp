#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class InputSystem {
public:
    static void update(entt::registry& registry, const Camera2D& camera);

private:
    static inline bool s_hasMovementTarget = false;
    static inline Vector2 s_movementTarget = {0.0f, 0.0f};
};

} // namespace NoMoreDay
