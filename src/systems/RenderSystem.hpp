#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

class RenderSystem {
public:
    static void render(entt::registry& registry);
};
