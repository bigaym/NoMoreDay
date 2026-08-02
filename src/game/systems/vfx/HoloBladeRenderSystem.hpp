#pragma once
#include "game/SharedContext.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay::systems {

class HoloBladeRenderSystem {
public:
  static void Render(entt::registry &registry,
                     const NoMoreDay::SharedContext &context);
  static void Shutdown();
};

} // namespace NoMoreDay::systems
