#pragma once
#include <entt/entt.hpp>
#include "../components/Common.hpp"
#include "SpatialGrid.hpp"
#include "raylib.h"

class CombatSystem {
public:
    // Processes attack inputs, manages cooldowns, and resolves hits
    static void update(entt::registry& registry, systems::SpatialHashGrid& grid, const Camera2D& camera, float dt);
};
