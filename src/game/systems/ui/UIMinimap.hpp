#pragma once
#include <entt/entt.hpp>

class LevelManager;

class UIMinimap {
public:
    struct Constants {
        static constexpr float MAP_SIZE = 180.0f;
        static constexpr float MARGIN = 30.0f;
        static constexpr int VIEW_RADIUS = 30;
        static constexpr float REFRESH_INTERVAL = 0.166f; // ~6 Hz
        static constexpr float ENEMY_MARKER_SIZE = 2.5f;
        static constexpr float PLAYER_MARKER_SIZE = 4.0f;
    };

    static void Draw(entt::registry& registry, const LevelManager& levelManager);
    static void Cleanup();
    static void ToggleDebugReveal();
};