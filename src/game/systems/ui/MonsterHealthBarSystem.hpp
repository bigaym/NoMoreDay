#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay::systems {

class MonsterHealthBarSystem {
public:
    static void Render(entt::registry& registry, const Camera2D& camera);
    static void RenderUI(entt::registry& registry);

private:
    static void DrawTargetWidget(entt::registry& registry, entt::entity entity);
    static entt::entity s_hoveredEntity;
};

} // namespace NoMoreDay::systems
