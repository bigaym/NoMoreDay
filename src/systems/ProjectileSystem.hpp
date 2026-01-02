#pragma once
#include <entt/entt.hpp>
#include "SpatialGrid.hpp"

namespace NoMoreDay {

class ProjectileSystem {
public:
    static void Update(entt::registry& registry, systems::SpatialHashGrid& grid, float dt);
};

} // namespace NoMoreDay
