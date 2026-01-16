#pragma once
#include "app/SharedContext.hpp"
#include <entt/entt.hpp>


namespace NoMoreDay::systems {

class HoloBladeRenderSystem {
public:
  static void Render(entt::registry &registry,
                     const NoMoreDay::SharedContext &context);
};

} // namespace NoMoreDay::systems
