#include "app/Game.hpp"
#include "core/logging/Logger.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/RenderSystem.hpp" // ADDED
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/WorldState.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/data/BuffRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "game/states/GameplayState.hpp"
#include "game/states/MainMenuState.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include "engine/render/GPUUtils.hpp"

Game::Game(int width, int height, const char *title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {

  // Configure Window Flags BEFORE InitWindow
  // FLAG_WINDOW_RESIZABLE: Allows user resizing
  // FLAG_MSAA_4X_HINT: Anti-aliasing
  // NOTE: HighDPI flag removed to match previous GCC behavior.
  // NOTE: VSYNC flag removed to allow uncapped FPS for benchmarking.
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

  InitWindow(m_screenWidth, m_screenHeight, m_title);

  // Smart Window Positioning
  int monitor = GetCurrentMonitor();
  int monitorW = GetMonitorWidth(monitor);
  int monitorH = GetMonitorHeight(monitor);

  // If the requested size matches the monitor resolution, we force a borderless fullscreen-like state.
  if (m_screenWidth == monitorW && m_screenHeight == monitorH) {
      LOG_INFO("Window size matches monitor resolution. Switching to Borderless Fullscreen.");
      
      // Method 1: Remove decorations and force resize/reposition
      // We MUST reset size because Windows might have clamped the window height (Client + Border > Screen) 
      // during InitWindow, making the client area smaller than requested.
      SetWindowState(FLAG_WINDOW_UNDECORATED);
      SetWindowSize(m_screenWidth, m_screenHeight); // Force restore full client size
      SetWindowPosition(0, 0);
      m_isBorderlessFullscreen = true;
  } else {
      // Center the window manually
      SetWindowPosition((monitorW - m_screenWidth) / 2, (monitorH - m_screenHeight) / 2);
      m_isBorderlessFullscreen = false;
      
      // Save initial windowed state
      m_windowedWidth = m_screenWidth;
      m_windowedHeight = m_screenHeight;
      m_windowedPosX = (monitorW - m_screenWidth) / 2;
      m_windowedPosY = (monitorH - m_screenHeight) / 2;
  }

  InitAudioDevice();

  // Initialize GPU Capability Detection and Load Extensions
  m_gpuInfo = NoMoreDay::utils::GPUUtils::Initialize();

  // Register EnTT Groups EARLY (before any components are added)
  // This is critical to prevent registry corruption.
  NoMoreDay::groups::RegisterGroups(m_registry);

  SetExitKey(0);

  // Load settings first so targetFPS is available
  m_settings.Load();
  SetTargetFPS(m_settings.targetFPS); // Use FPS from settings (default: 180)
  LOG_INFO("Target FPS set to: {}", m_settings.targetFPS);

  // Fill Context
  m_levelManager = std::make_unique<LevelManager>();
  m_context.registry = &m_registry;
  m_context.resources = &m_resourceManager;
  m_context.levelManager = m_levelManager.get();
  m_context.executor = &m_executor;
  m_context.settings = &m_settings;

  // Render Context Setup
  m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
  m_renderContext.mdiRenderer = &m_mdiRenderer;
  m_renderContext.resources = &m_resourceManager;
  m_context.renderContext = &m_renderContext;

  // Init SceneManager
  m_sceneManager =
      std::make_unique<NoMoreDay::SceneManager>(*m_levelManager, m_registry);
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
  std::system("chcp 65001 >2&1");

  // Global Static Inits
  NoMoreDay::CombatEventDispatcher::Init();
  NoMoreDay::AstrolabeRegistry::Get().Load("assets/data/astrolabe.json");
  NoMoreDay::MaterialRegistry::Get().LoadMaterials(
      "assets/data/materials.json");
  NoMoreDay::SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  NoMoreDay::SkillSystem::InitHooks();
  NoMoreDay::BuffRegistry::Initialize();
  NoMoreDay::BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");

  NoMoreDay::ItemFactory::initialize();
  NoMoreDay::ItemFactory::loadAffixDefinitions("assets/data/affixes.json");

  // Initialize Map Affix Registry
  NoMoreDay::MapAffixRegistry::Initialize();

  // Initialize Persistence
  NoMoreDay::SaveManager::Get().Initialize(&m_executor);
  NoMoreDay::SaveManager::Get().loadGlobal(m_registry);

  // Initialize ActiveDimensionalState in Context
  // This ensures the state is available globally for all systems
  if (!m_registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
    m_registry.ctx().emplace<NoMoreDay::ActiveDimensionalState>();
  }

  // Initialize Stats System (Cache cleanup)
  NoMoreDay::StatsSystem::Initialize(m_registry);

  // Initialize UI System (Loads Fonts)
  UISystem::Initialize(m_resourceManager);

  // Initialize GPU Systems
  if (m_gpuInfo.computeShaderSupported) {
    // 1. Pre-load Entity Texture Array
    std::vector<std::string> entityPaths;
    static const std::vector<std::string> races = {
        "skeleton", "demon",  "warcraft", "cultist",  "elf",
        "beast",    "goblin", "mech",     "elemental"};
    for (const auto &race : races) {
      for (int i = 0; i < 5; ++i) {
        entityPaths.push_back("assets/textures/monster/" + race + "_" +
                              std::to_string(i) + ".png");
      }
    }
    m_resourceManager.loadTextureArray(entityPaths);

    // 2. GPU Particle System (Indirect Drawing)
    NoMoreDay::systems::GPUParticleSystem::Get().Init(
        NoMoreDay::Constants::Render::MAX_PARTICLES_DEFAULT);

    m_gpuEntitySystem.Init(m_resourceManager, 30000, &m_registry);
    m_mdiRenderer.Init(m_resourceManager, 30000);
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager, 256,
                                                       256);
    // Initialize GPU Skill Effect System (Global)
    NoMoreDay::systems::GPUSkillEffectSystem::Get().Init(
        m_resourceManager, NoMoreDay::Constants::Render::MAX_SKILL_EFFECTS);

    // Initialize GPU Damage Popup System
    NoMoreDay::render::PopupRenderer::Get().Init();

    // Initialize Instanced Label Renderer
    RenderSystem::Initialize();

    // Link context
    m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
    m_renderContext.mdiRenderer = &m_mdiRenderer;
    m_renderContext.gpuFlowFieldSystem =
        &NoMoreDay::systems::GPUFlowFieldSystem::Get();
    m_renderContext.resources = &m_resourceManager;
    m_context.renderContext = &m_renderContext;
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
            if (frameTime > 0.25f)
              frameTime = 0.25f;
      
            {
              // 1. Update Game State based on Frame Rate (Variable DT)
              // This ensures input responsiveness (ESC, Clicks) at high refresh rates.
              NoMoreDay::utils::ScopedTimer timer("1. Update State", 2000); // 2ms threshold
              m_stateManager->Update(frameTime);
            }        if (m_stateManager->IsEmpty()) {
          LOG_INFO("State stack empty, exiting game loop");
          break;
        }
  
            // 2. Continuous Simulation & Physics (Fixed DT)
            accumulator += frameTime;
            bool logicRan = false;
            while (accumulator >= fixedDt) {
              if (m_gpuInfo.computeShaderSupported) {
                // 1. CPU -> Shadow Sync (Logic update only, no GPU mapping here)
                m_gpuEntitySystem.UpdateLogic(m_context, fixedDt);
              }
        
              accumulator -= fixedDt;
              logicRan = true;
            }
        
            if (logicRan && m_gpuInfo.computeShaderSupported) {
              // Submit new pulse to GPU
              m_gpuEntitySystem.UploadGPU(m_context);
              
              // Update particle system
              NoMoreDay::systems::GPUParticleSystem::Get().Update(frameTime);
            }  
        // Update Interpolation Alpha for Rendering
        // alpha = accumulator / fixedDt, range [0, 1)
        // This allows smooth interpolation between physics frames
        m_context.renderAlpha = accumulator / fixedDt;
  
        static float fpsLogTimer = 0.0f;
        fpsLogTimer += frameTime;
        // Log FPS every 1.0 second at INFO level
            LOG_LIMITED_INFO(1.0f, ">>> [PERF] FPS: {} | FrameTime: {:.3f} ms", GetFPS(),
                              frameTime * 1000.0f);
        
            {
              NoMoreDay::utils::ScopedTimer timer("Frame Render", 500); 
              BeginDrawing();
              ClearBackground(BLACK);
              m_stateManager->Render();
              EndDrawing();
            }
            
            // Check for Fullscreen Toggle (Alt + Enter)
            if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)) {
                toggleFullScreen();
            }
      }}

void Game::toggleFullScreen() {
    int monitor = GetCurrentMonitor();
    int monitorW = GetMonitorWidth(monitor);
    int monitorH = GetMonitorHeight(monitor);

    if (m_isBorderlessFullscreen) {
        // Switch to Windowed
        LOG_INFO("Switching to Windowed Mode...");
        SetWindowState(FLAG_WINDOW_UNDECORATED); // Temporarily ensure state for cleaner transition
        ClearWindowState(FLAG_WINDOW_UNDECORATED); // Add decorations back
        
        // Restore saved windowed size or default to safe size
        if (m_windowedWidth == 0 || m_windowedHeight == 0) {
            // Default to 75% of monitor size if no saved state
            m_windowedWidth = (int)(monitorW * 0.75f);
            m_windowedHeight = (int)(monitorH * 0.75f);
            m_windowedPosX = (monitorW - m_windowedWidth) / 2;
            m_windowedPosY = (monitorH - m_windowedHeight) / 2;
        }

        SetWindowSize(m_windowedWidth, m_windowedHeight);
        SetWindowPosition(m_windowedPosX, m_windowedPosY);
        m_isBorderlessFullscreen = false;
    } else {
        // Switch to Borderless Fullscreen
        LOG_INFO("Switching to Borderless Fullscreen...");
        
        // Save current windowed state
        m_windowedWidth = GetScreenWidth();
        m_windowedHeight = GetScreenHeight();
        Vector2 pos = GetWindowPosition();
        m_windowedPosX = (int)pos.x;
        m_windowedPosY = (int)pos.y;

        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(monitorW, monitorH);
        SetWindowPosition(0, 0);
        m_isBorderlessFullscreen = true;
    }
}

void Game::cleanup() {
  LOG_INFO("Cleaning up game systems...");

  // Save Global State (Shared Stash)
  NoMoreDay::SaveManager::Get().saveGlobalAsync(m_registry);

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

  RenderSystem::Shutdown(); // ADDED
  UISystem::Shutdown();
  NoMoreDay::render::PopupRenderer::Get().Shutdown();
  NoMoreDay::systems::GPUParticleSystem::Get().Shutdown();
  m_gpuEntitySystem.Shutdown();
  m_mdiRenderer.Shutdown();
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Shutdown();
  NoMoreDay::systems::GPUFlowFieldSystem::Get().Shutdown();
  NoMoreDay::StatsSystem::Shutdown(m_registry);
  NoMoreDay::BuffRegistry::Shutdown();

  m_resourceManager.unloadAll();

  LOG_INFO("Cleanup finished successfully.");
}