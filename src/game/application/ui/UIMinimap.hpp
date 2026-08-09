#pragma once
#include <entt/entity/registry.hpp>
#include "raylib.h"
#include <vector>

class LevelManager;
namespace NoMoreDay { namespace systems { class SpatialHashGrid; } }

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

    static void Draw(entt::registry& registry, const LevelManager& levelManager, const NoMoreDay::systems::SpatialHashGrid* grid = nullptr);
    static void Cleanup();
    static void ToggleDebugReveal();

private:
    static inline Texture2D s_minimapTexture = {0};
    static inline int s_minimapW = 0;
    static inline int s_minimapH = 0;
    static inline std::vector<Color> s_minimapPixels;
    static inline bool s_debugRevealMap = false;
    static inline bool s_minimapDirty = true;
    static inline std::vector<Color> s_partialBuffer;
};