#include "Game.hpp"
#include "../states/MainMenuState.hpp"
#include "../states/GameplayState.hpp"
#include "../tools/Logger.hpp"
#include "../systems/UISystem.hpp"
#include "../systems/SkillSystem.hpp"
#include "../systems/GPUParticleSystem.hpp"
#include "../systems/GPUEntitySystem.hpp"
#include "../systems/GPUFlowFieldSystem.hpp"
#include "../core/ItemFactory.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/AstrolabeRegistry.hpp"
#include "../core/SkillRegistry.hpp"
#include "../core/BuffRegistry.hpp"
#include "../core/BiomeRegistry.hpp"
#include "../components/AstrolabeUIComponent.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include "rlgl.h"

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {
    
    system("chcp 65001 > nul"); 
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    
    InitWindow(m_screenWidth, m_screenHeight, m_title);

    // After InitWindow, Raylib (especially when used as a DLL) has already 
    // initialized the OpenGL context and internal function pointers.
    // We utilize rlgl abstraction to stay "Unified" with Raylib's state.
    LOG_INFO("OpenGL Context initialized via Raylib (rlgl).");

    InitAudioDevice(); 
    
    // Check GPU Support via our Unified Utility
    m_gpuInfo = NoMoreDay::utils::GPUUtils::CheckSupport();

    SetExitKey(0); 
    SetTargetFPS(60);
    
    // Fill Context
    m_levelManager = std::make_unique<LevelManager>();
    m_context.registry = &m_registry;
    m_context.resources = &m_resourceManager;
    m_context.levelManager = m_levelManager.get();
    m_context.executor = &m_executor;

    // Init SceneManager
    m_sceneManager = std::make_unique<NoMoreDay::SceneManager>(*m_levelManager, m_registry);
    m_context.sceneManager = m_sceneManager.get();

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
    NoMoreDay::SkillSystem::InitHooks();
    NoMoreDay::BuffRegistry::Initialize(); 
    NoMoreDay::BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    NoMoreDay::ItemFactory::initialize();
    NoMoreDay::ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    
    // Initialize UI System (Loads Fonts)
    UISystem::Initialize(m_resourceManager);

    // Initialize GPU Particle System
    if (m_gpuInfo.computeShaderSupported) {
        NoMoreDay::systems::GPUParticleSystem::Get().Init(m_resourceManager);
        NoMoreDay::systems::GPUEntitySystem::Get().Init(m_resourceManager);
        NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager, 500, 500);
    }

    // Push Initial State
    LOG_INFO("Pushing MainMenuState...");
    m_stateManager->PushState<NoMoreDay::MainMenuState>();
    
    m_stateManager->Update(0.0f); 
    
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
            
            if (m_gpuInfo.computeShaderSupported) {
                // 1. CPU -> GPU Sync & Compute Physics
                NoMoreDay::systems::GPUEntitySystem::Get().Update(m_registry, fixedDt);
                // 2. GPU -> CPU Sync Back
                NoMoreDay::systems::GPUEntitySystem::Get().SyncBack(m_registry);
                
                NoMoreDay::systems::GPUParticleSystem::Get().Update(fixedDt);
            }

            accumulator -= fixedDt;
        }

        if (m_stateManager->IsEmpty()) {
            LOG_INFO("State stack empty, exiting game loop");
            break;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
            m_stateManager->Render();
        EndDrawing();
    }
}

void Game::cleanup() {
    LOG_INFO("Cleaning up game systems...");

    m_executor.wait_for_all();

    if (m_stateManager) {
        m_stateManager.reset(); 
    }

    if (m_levelManager) {
        m_levelManager->cleanup();
        m_levelManager.reset();
    }

    m_registry.on_destroy<NoMoreDay::AstrolabeUIComponent>().disconnect(); 
    m_registry.clear(); 

    UISystem::Shutdown();
    NoMoreDay::systems::GPUParticleSystem::Get().Shutdown();
    NoMoreDay::systems::GPUEntitySystem::Get().Shutdown();
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Shutdown();
    NoMoreDay::BuffRegistry::Shutdown();
    
    m_resourceManager.unloadAll();
    
    LOG_INFO("Cleanup finished successfully.");
}