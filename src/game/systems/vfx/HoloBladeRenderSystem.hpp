#pragma once
#include "game/foundation/SharedContext.hpp"
#include <entt/entt.hpp>
#include "raylib.h"


namespace NoMoreDay::systems {

class HoloBladeRenderSystem {
public:
  static void Render(entt::registry &registry,
                     const NoMoreDay::SharedContext &context,
                     const Camera2D &camera);
  static void Shutdown();
};

} // namespace NoMoreDay::systems
