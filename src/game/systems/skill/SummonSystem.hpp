#pragma once
#include <entt/entt.hpp>
#include "game/systems/physics/SpatialGrid.hpp"

namespace NoMoreDay::systems {

class SummonSystem {
public:
    static void Update(entt::registry& registry, float dt, const SpatialHashGrid& grid);
};

} // namespace NoMoreDay::systems
