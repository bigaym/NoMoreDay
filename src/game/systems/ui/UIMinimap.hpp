#pragma once
#include <entt/entt.hpp>

class LevelManager;

class UIMinimap {
public:
    static void Draw(entt::registry& registry, const LevelManager& levelManager);
    static void Cleanup();
    static void ToggleDebugReveal();
};