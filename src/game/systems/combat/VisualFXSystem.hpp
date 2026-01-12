#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay::systems {

class VisualFXSystem {
public:
    static void Initialize(entt::registry& registry);
    static void Update(entt::registry& registry, float dt);
};

} // namespace NoMoreDay::systems
