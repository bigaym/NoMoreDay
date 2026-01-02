#include "Game.hpp"
#include "../states/MainMenuState.hpp"
#include "../states/GameplayState.hpp"
#include "../tools/Logger.hpp"
#include "../systems/UISystem.hpp"
#include "../systems/SkillSystem.hpp"
#include "../core/ItemFactory.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/AstrolabeRegistry.hpp"
#include "../core/SkillRegistry.hpp"
#include "../core/BuffRegistry.hpp"
#include "../core/BiomeRegistry.hpp"
#include "../components/AstrolabeUIComponent.hpp"

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {
    
    system("chcp 65001 > nul"); 
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    InitAudioDevice(); 
    SetExitKey(0); 
    SetTargetFPS(60);
    
    // Fill Context
    m_levelManager = std::make_unique<LevelManager>();
    m_context.registry = &m_registry;
    m_context.resources = &m_resourceManager;
    m_context.levelManager = m_levelManager.get();
    m_context.executor = &m_executor;

    // Init StateManager
    m_stateManager = std::make_unique<NoMoreDay::StateManager>(m_context);

    LOG_DEBUG("Game window and core systems initialized");
    init();
}

Game::~Game() {
    LOG_INFO("Shutting down Game...");
    cleanup();
    CloseAudioDevice();
    CloseWindow();
    LOG_INFO("Game shutdown completed");
}

void Game::init() {
    LOG_INFO("Initializing Game systems...");
    
    // Global Static Inits
    NoMoreDay::AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");
    NoMoreDay::SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    NoMoreDay::BuffRegistry::Initialize(); // BuffRegistry is static and uses Initialize
    NoMoreDay::BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    NoMoreDay::ItemFactory::initialize();
    NoMoreDay::ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    
    // Push Initial State
    LOG_INFO("Pushing MainMenuState...");
    m_stateManager->PushState<NoMoreDay::MainMenuState>();
    
    // 关键修复：立即处理待处理的状态更改，确保状态栈不为空
    m_stateManager->Update(0.0f); 
    
    LOG_INFO("Game initialization completed");
}

void Game::run() {
    LOG_INFO("Starting Game Loop...");
    
    // // 手动创建一个崩溃点以测试崩溃处理
    // int* p = nullptr;
    // *p = 10;

    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    // 初始状态已经在 init() 中处理，此时 IsEmpty() 应为 false
    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        if (frameTime > 0.25f) frameTime = 0.25f;

        accumulator += frameTime;

        while (accumulator >= fixedDt) {
            m_stateManager->Update(fixedDt);
            accumulator -= fixedDt;
        }

        // 如果在更新后状态栈为空，说明所有状态都已退出（例如主菜单点击退出），此时跳出循环
        if (m_stateManager->IsEmpty()) {
            LOG_INFO("State stack empty, exiting game loop");
            break;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
            m_stateManager->Render();
        EndDrawing();
    }
    LOG_INFO("Game loop ended, window closed or stack empty");
}

void Game::cleanup() {
    LOG_INFO("Cleaning up game systems...");

    // 0. 强制等待所有后台异步任务完成
    // 这是防止内存损坏的关键。必须确保没有任何 Taskflow 任务在后台修改 Registry。
    m_executor.wait_for_all();

    // 1. 彻底销毁状态管理器。
    // 这会触发当前所有 State 的 OnExit() 和析构函数。
    // 必须在清理注册表之前完成，因为 State 可能持有对 Registry 的信号连接。
    if (m_stateManager) {
        m_stateManager.reset(); 
    }

    // 2. 彻底销毁关卡管理器。
    if (m_levelManager) {
        m_levelManager->cleanup();
        m_levelManager.reset();
    }

    // 3. 关闭静态系统。
    UISystem::Shutdown();
    NoMoreDay::BuffRegistry::Shutdown();

    // 4. 清理注册表实体。
    // 如果此处依然崩溃，请检查 SwordHeartComponent 是否有手动连接的 on_destroy 信号未断开。
    m_registry.on_destroy<NoMoreDay::AstrolabeUIComponent>().disconnect(); // 安全起见，断开相关信号
    LOG_DEBUG("Final registry clear...");    

    m_registry.clear(); 
    
    // 5. 卸载资源。
    m_resourceManager.unloadAll();
    
    LOG_INFO("Cleanup finished successfully.");
}
