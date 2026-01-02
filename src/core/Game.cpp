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

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title),
      m_levelManager(std::make_unique<LevelManager>()) {
    
    system("chcp 65001 > nul"); 
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    InitAudioDevice(); 
    SetExitKey(0); 
    SetTargetFPS(60);
    
    // Fill Context
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
    NoMoreDay::ItemFactory::initialize();
    UISystem::Initialize(m_resourceManager);
    NoMoreDay::AssetLoadingSystem::LoadAllEquipment();
    NoMoreDay::AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");
    NoMoreDay::SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    NoMoreDay::BuffRegistry::Initialize();
    NoMoreDay::SkillSystem::InitHooks();
    
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

    // 1. 先清理状态管理器，这会触发各状态的 OnExit，确保系统在注册表还在时安全关闭
    if (m_stateManager) {
        m_stateManager.reset(); 
    }

    // 2. 清理注册表中的所有实体
    m_registry.clear();
    
    // 3. 关闭各单例和资源管理器
    UISystem::Shutdown();
    NoMoreDay::BuffRegistry::Shutdown();

    if (m_levelManager) m_levelManager->cleanup();
    m_resourceManager.unloadAll();
}
