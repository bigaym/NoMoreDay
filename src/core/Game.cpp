#include "Game.hpp"
#include "../states/MainMenuState.hpp"
#include "../states/GameplayState.hpp"
#include "../tools/Logger.hpp"
#include "../systems/UISystem.hpp"
#include "../core/ItemFactory.hpp"

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
    
    // Push Initial State
    LOG_INFO("Pushing MainMenuState...");
    m_stateManager->PushState<NoMoreDay::MainMenuState>();
    
    LOG_INFO("Game initialization completed");
}

void Game::run() {
    LOG_INFO("Starting Game Loop...");
    
    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        if (frameTime > 0.25f) frameTime = 0.25f;

        accumulator += frameTime;

        while (accumulator >= fixedDt) {
            m_stateManager->Update(fixedDt);
            accumulator -= fixedDt;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
            m_stateManager->Render();
        EndDrawing();
    }
    LOG_INFO("Game loop ended, window closed");
}

void Game::cleanup() {
    // Clear States first
    if (m_stateManager) {
        // StateManager destructor handles stack clearing
        m_stateManager.reset(); 
    }
    
    UISystem::Shutdown();
    if (m_levelManager) m_levelManager->cleanup();
    m_resourceManager.unloadAll();
    m_registry.clear();
}
