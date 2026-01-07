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
#include "GLFW/glfw3.h"

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {
    
    system("chcp 65001 > nul"); 
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    
    InitWindow(m_screenWidth, m_screenHeight, m_title);

    // Initialize OpenGL function pointers via GLAD using a robust loader
    LOG_INFO("Loading OpenGL 4.3 extensions via GLAD...");
    
    auto glad_loader = [](const char* name) -> void* {
        void* p = (void*)glfwGetProcAddress(name);
#ifdef _WIN32
        if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
            static HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
            if (opengl32 == nullptr) opengl32 = LoadLibraryA("opengl32.dll");
            p = (void*)GetProcAddress(opengl32, name);
        }
#endif
        return p;
    };

    // We don't strictly check the return value here because gladLoadGL 2.0 might return 0
    // if a single obscure function from the requested version is missing.
    // We will rely on our own GPUUtils::CheckSupport to verify if what we NEED is there.
    int loaded_version = gladLoadGL((GLADloadfunc)+glad_loader);
    
    const char* gl_version_str = (const char*)glGetString(GL_VERSION);
    if (gl_version_str) {
        LOG_INFO("GLAD loader finished. OpenGL Version: {}. Glad reported version: {}", gl_version_str, loaded_version);
    } else {
        LOG_ERROR("GLAD loader failed critically: glGetString(GL_VERSION) is NULL!");
    }

    InitAudioDevice(); 
    
    // Check GPU Support
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
    NoMoreDay::BuffRegistry::Initialize(); // BuffRegistry is static and uses Initialize
    NoMoreDay::BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    NoMoreDay::ItemFactory::initialize();
    NoMoreDay::ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    
    // Initialize UI System (Loads Fonts)
    UISystem::Initialize(m_resourceManager);

    // Initialize GPU Particle System
    if (m_gpuInfo.computeShaderSupported) {
        NoMoreDay::systems::GPUParticleSystem::Get().Init(m_resourceManager);
        NoMoreDay::systems::GPUEntitySystem::Get().Init(m_resourceManager);
        NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager);
    }

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
            
            if (m_gpuInfo.computeShaderSupported) {
                // 1. CPU -> GPU Sync & Compute Physics
                NoMoreDay::systems::GPUEntitySystem::Get().Update(m_registry, fixedDt);
                // 2. GPU -> CPU Sync Back (To allow CPU systems like AI/Render to see the new positions)
                NoMoreDay::systems::GPUEntitySystem::Get().SyncBack(m_registry);
                
                NoMoreDay::systems::GPUParticleSystem::Get().Update(fixedDt);
            }

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

    // 3. 清理注册表实体。
    // 如果此处依然崩溃，请检查 SwordHeartComponent 是否有手动连接的 on_destroy 信号未断开。
    m_registry.on_destroy<NoMoreDay::AstrolabeUIComponent>().disconnect(); // 安全起见，断开相关信号
    LOG_DEBUG("Final registry clear...");    
    m_registry.clear(); 

    // 4. 关闭静态系统。
    // 必须在 Registry 清理之后关闭，因为销毁组件时可能依赖这些系统的资源 (如字体、纹理 ID 等)
    UISystem::Shutdown();
    NoMoreDay::systems::GPUParticleSystem::Get().Shutdown();
    NoMoreDay::systems::GPUEntitySystem::Get().Shutdown();
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Shutdown();
    NoMoreDay::BuffRegistry::Shutdown();
    
    // 5. 卸载资源。
    m_resourceManager.unloadAll();
    
    LOG_INFO("Cleanup finished successfully.");
}
