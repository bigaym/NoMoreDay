#include "app/Game.hpp"
#include "core/logging/Logger.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/data/BuffRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/states/GameplayState.hpp"
#include "game/states/MainMenuState.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include "rlgl.h"

Game::Game(int width, int height, const char *title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {

  system("chcp 65001 > nul");
  LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height,
           title);

  InitWindow(m_screenWidth, m_screenHeight, m_title);

  // After InitWindow, Raylib (especially when used as a DLL) has already
  // initialized the OpenGL context and internal function pointers.
  // We utilize rlgl abstraction to stay "Unified" with Raylib's state.
  LOG_INFO("OpenGL Context initialized via Raylib (rlgl).");

  InitAudioDevice();

  // Check GPU Support via our Unified Utility
  m_gpuInfo = NoMoreDay::utils::GPUUtils::CheckSupport();

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

  // Initialize Persistence
  NoMoreDay::SaveManager::Get().Initialize(&m_executor);

  // Initialize Stats System (Cache cleanup)
  NoMoreDay::StatsSystem::Initialize(m_registry);

  // Initialize UI System (Loads Fonts)
  UISystem::Initialize(m_resourceManager);

  // Initialize GPU Systems
  if (m_gpuInfo.computeShaderSupported) {
    // GPU Particle System (Indirect Drawing)
    NoMoreDay::systems::GPUParticleSystem::Get().Init(NoMoreDay::Constants::Render::MAX_PARTICLES_DEFAULT);

    NoMoreDay::systems::GPUEntitySystem::Get().Init(m_resourceManager);
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager, 256,
                                                       256);
    // Initialize GPU Skill Effect System (Global)
    NoMoreDay::systems::GPUSkillEffectSystem::Get().Init(m_resourceManager, NoMoreDay::Constants::Render::MAX_SKILL_EFFECTS);
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

    // 1. Update Game State based on Frame Rate (Variable DT)
    // This ensures input responsiveness (ESC, Clicks) at high refresh rates.
    m_stateManager->Update(frameTime);

    if (m_stateManager->IsEmpty()) {
      LOG_INFO("State stack empty, exiting game loop");
      break;
    }

    // 2. Continuous Simulation & Physics (Fixed DT)
    accumulator += frameTime;
    while (accumulator >= fixedDt) {
      // Note: GameplayState::UpdatePhysics is called inside its OnUpdate,
      // which now runs at variable rate. If we truly want fixed physics,
      // we should separate it from the State and call it here.
      // But for now, moving the whole Update to variable DT is the safest
      // way to fix the input issues reported by the user.

      if (m_gpuInfo.computeShaderSupported) {
        // 1. CPU -> GPU Sync & Compute Physics
        NoMoreDay::systems::GPUEntitySystem::Get().Update(m_registry, fixedDt);
        // 2. GPU -> CPU Sync Back
        NoMoreDay::systems::GPUEntitySystem::Get().SyncBack(m_registry);

        // Update particle system
        NoMoreDay::systems::GPUParticleSystem::Get().Update(fixedDt);
      }

      accumulator -= fixedDt;
    }

    // Update Accumulator for Rendering (Interpolation/Extrapolation)
    m_context.renderAccumulator = accumulator;

    BeginDrawing();
    ClearBackground(BLACK);
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
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Shutdown();
  NoMoreDay::systems::GPUFlowFieldSystem::Get().Shutdown();
  NoMoreDay::StatsSystem::Shutdown(m_registry);
  NoMoreDay::BuffRegistry::Shutdown();

  m_resourceManager.unloadAll();

  LOG_INFO("Cleanup finished successfully.");
}