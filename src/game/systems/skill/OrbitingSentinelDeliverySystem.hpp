#pragma once

#include <entt/entt.hpp>
#include "game/systems/physics/SpatialGrid.hpp"

namespace NoMoreDay {

class OrbitingSentinelDeliverySystem {
public:
  static void Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt);
};

} // namespace NoMoreDay
