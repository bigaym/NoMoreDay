#pragma once

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>

class ResourceManager;
class LevelManager;

namespace NoMoreDay {

    struct SharedContext {
        entt::registry* registry = nullptr;
        ResourceManager* resources = nullptr;
        LevelManager* levelManager = nullptr;
        tf::Executor* executor = nullptr;
        // Window* window; // Raylib uses global state mostly, add if wrapper exists
    };

}
