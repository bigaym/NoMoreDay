#pragma once
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include "raylib.h"
#include "ResourceManager.hpp"
#include "../systems/SpatialGrid.hpp"
#include "../components/AIComponent.hpp"
#include "LevelManager.hpp"

class Game {
public:
    Game(int width, int height, const char* title);
    ~Game();

    void run();

private:
    void init();
    void update(float dt);
    void render();
    void cleanup();

    // Window settings
    int m_screenWidth;
    int m_screenHeight;
    const char* m_title;
    
    // Camera
    Camera2D m_camera;

    // ECS
    entt::registry m_registry;

    // Spatial Grid (Physics & AI)
    systems::SpatialHashGrid m_spatialGrid;

    // Taskflow
    tf::Executor m_executor;
    tf::Taskflow m_taskflow;

    // Cache for physics entities to avoid per-frame allocation
    std::vector<entt::entity> m_physicsEntities;

    // Resources
    ResourceManager m_resourceManager;
    
    // Level Management
    std::unique_ptr<LevelManager> m_levelManager;
};
