#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

class RenderSystem {
public:
    static void render(entt::registry& registry);

    // Screen Shake API
    static void AddScreenShake(float intensity);
    static void UpdateShake(float dt);
    static Vector2 GetShakeOffset();

private:
    static float s_trauma;
};
