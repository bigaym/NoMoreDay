#pragma once

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

class AreaFieldDeliverySystem {
public:
  static void Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt);
};

} // namespace NoMoreDay
