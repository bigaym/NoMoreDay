#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

class UIAstrolabe {
public:
    static void Update(entt::registry& registry);
    static void Draw(entt::registry& registry);

    // Helper to toggle UI
    static void Toggle(entt::registry& registry, entt::entity player);
    static bool IsVisible(entt::registry& registry, entt::entity player);
};

}
