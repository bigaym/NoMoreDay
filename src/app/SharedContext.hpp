#pragma once

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include "app/Settings.hpp"

class ResourceManager;
class LevelManager;
namespace NoMoreDay { class SceneManager; }

namespace NoMoreDay {
    namespace systems { class SpatialHashGrid; }

    struct SharedContext {
        entt::registry* registry = nullptr;
        ResourceManager* resources = nullptr;
        LevelManager* levelManager = nullptr;
        SceneManager* sceneManager = nullptr;
        tf::Executor* executor = nullptr;
        GameSettings* settings = nullptr;
        systems::SpatialHashGrid* spatialGrid = nullptr;
        float renderAccumulator = 0.0f; // For interpolation/extrapolation in rendering
        // Window* window; // Raylib uses global state mostly, add if wrapper exists
    };

}
