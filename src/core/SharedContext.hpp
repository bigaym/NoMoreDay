#pragma once

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>

class ResourceManager;
class LevelManager;
namespace NoMoreDay { class SceneManager; }

namespace NoMoreDay {

    struct SharedContext {
        entt::registry* registry = nullptr;
        ResourceManager* resources = nullptr;
        LevelManager* levelManager = nullptr;
        SceneManager* sceneManager = nullptr;
        tf::Executor* executor = nullptr;
        // Window* window; // Raylib uses global state mostly, add if wrapper exists
    };

}
