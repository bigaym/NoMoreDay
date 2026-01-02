#pragma once
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include "raylib.h"
#include "ResourceManager.hpp"
#include "StateManager.hpp"
#include "SharedContext.hpp"
#include "LevelManager.hpp"
#include "SceneManager.hpp"
#include <memory>

class Game {
public:
    Game(int width, int height, const char* title);
    ~Game();

    void run();

private:
    void init(); 
    void cleanup();

    // Window settings
    int m_screenWidth;
    int m_screenHeight;
    const char* m_title;

    // 1. 基础资源 (最后析构)
    entt::registry m_registry;
    ResourceManager m_resourceManager;
    tf::Executor m_executor;

    // 2. 共享上下文 (依赖资源)
    NoMoreDay::SharedContext m_context;

    // 3. 逻辑管理器 (最先析构)
    std::unique_ptr<LevelManager> m_levelManager;
    std::unique_ptr<NoMoreDay::SceneManager> m_sceneManager;
    std::unique_ptr<NoMoreDay::StateManager> m_stateManager;
};