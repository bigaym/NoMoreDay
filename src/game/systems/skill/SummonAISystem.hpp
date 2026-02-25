#pragma once

#include "engine/physics/SpatialGrid.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::systems {

class SummonAISystem {
public:
  static void Update(entt::registry &registry, float dt,
                     const SpatialHashGrid &grid);
};

} // namespace NoMoreDay::systems
