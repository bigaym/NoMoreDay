#pragma once
#include "raylib.h"
#include <entt/entt.hpp>


namespace NoMoreDay::systems {

class TrailSystem {
public:
  static void Update(entt::registry &registry, float dt);
  static void Render(entt::registry &registry, Shader trailShader);
};

} // namespace NoMoreDay::systems
