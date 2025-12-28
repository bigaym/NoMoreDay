#pragma once
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include "raylib.h"
#include "ResourceManager.hpp"
#include "../systems/SpatialGrid.hpp"
#include "../components/AIComponent.hpp"
#include "LevelManager.hpp"
#include "../systems/XPAwardingSystem.hpp"

class Game {
public:
    Game(int width, int height, const char* title);
    ~Game();

    void run();

private:
    void init(); // 初始化游戏
    void update(float dt);
    void render();
    void cleanup();

    // Window settings
    int m_screenWidth;
    int m_screenHeight;
    const char* m_title;

    // 摄像机
    Camera2D m_camera;

    // 实体组件系统 (ECS)
    entt::registry m_registry;

    // 空间哈希网格 (用于物理和AI)
    systems::SpatialHashGrid m_spatialGrid;

    // Taskflow (并行任务库)
    tf::Executor m_executor;
    tf::Taskflow m_taskflow;

    // 物理实体缓存，避免每帧重新分配
    std::vector<entt::entity> m_physicsEntities;

    // 资源管理
    ResourceManager m_resourceManager;
    
    // 等级管理
    std::unique_ptr<LevelManager> m_levelManager;
};
