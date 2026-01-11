#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "app/SharedContext.hpp"

class RenderSystem {
public:
    static void render(entt::registry& registry, const NoMoreDay::SharedContext& context, const Camera2D& camera);

    // Screen Shake API
    static void AddScreenShake(float intensity);
    static void UpdateShake(float dt);
    static Vector2 GetShakeOffset();

private:
    static float s_trauma;
};
