#pragma once

#include <entt/entt.hpp>

namespace NoMoreDay::systems {

class SummonLifecycleSystem {
public:
  static void Update(entt::registry &registry, float dt);
};

} // namespace NoMoreDay::systems
