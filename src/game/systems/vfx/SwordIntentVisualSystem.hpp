#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay::systems {

class SwordIntentVisualSystem {
public:
  static void Update(entt::registry &registry, float dt);
  static void Render(entt::registry &registry);
};

} // namespace NoMoreDay::systems
