#pragma once
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include "raylib.h"
#include "ResourceManager.hpp"
#include "StateManager.hpp"
#include "SharedContext.hpp"
#include "LevelManager.hpp"
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

    // Core Systems owned by Game
    NoMoreDay::SharedContext m_context;
    std::unique_ptr<NoMoreDay::StateManager> m_stateManager;

    // Resources owned by Game (referenced by Context)
    entt::registry m_registry;
    ResourceManager m_resourceManager;
    tf::Executor m_executor;
    std::unique_ptr<LevelManager> m_levelManager;
};