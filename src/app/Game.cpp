#include "app/Game.hpp"
#include "app/GpuGateDriver.hpp"
#include "core/logging/Logger.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/RenderSystem.hpp" // ADDED
#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/resource/MSDFAtlasLoader.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/foundation/components/AstrolabeUIComponent.hpp"
#include "game/foundation/components/WorldState.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/registry/GroupRegistry.hpp"
#include "game/application/states/GameplayState.hpp"
#include "game/application/states/MainMenuState.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include "engine/render/GPUUtils.hpp"
#include "core/utils/Time.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace {

using NoMoreDay::render::GPUTextStringMeta;

void AppendAsciiString(const std::unordered_map<uint32_t, uint32_t> &codepointToMetric,
                       const std::string_view text, const uint8_t animStyle,
                       std::vector<uint32_t> &glyphIndices,
                       std::vector<GPUTextStringMeta> &meta) {
  GPUTextStringMeta entry = {};
  entry.glyphOffset = static_cast<uint32_t>(glyphIndices.size());
  entry.animStyle = animStyle;

  for (const char ch : text) {
    const uint32_t cp = static_cast<uint8_t>(ch);
    const auto it = codepointToMetric.find(cp);
    if (it == codepointToMetric.end()) {
      continue;
    }
    glyphIndices.push_back(it->second);
  }

  entry.glyphCount = static_cast<uint16_t>(
      std::min<size_t>(glyphIndices.size() - entry.glyphOffset, 0xFFFFu));
  meta.push_back(entry);
}

void InitializeGPUTextBootstrap(ResourceManager &resourceManager) {
  using namespace NoMoreDay::render;
  using namespace NoMoreDay::components;

  auto &textSystem = GPUTextSystem::Get();
  textSystem.Init(resourceManager, 4096, 16384);

  MSDFAtlasData atlasData;
  constexpr const char *kAtlasPath =
      "assets/textures/fonts/msdf/v4_msdf_gb2312_4096.png";
  constexpr const char *kMetricsPath =
      "assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin";
  if (!MSDFAtlasLoader::Load(kAtlasPath, kMetricsPath, MSDFAtlasCompression::None,
                             atlasData)) {
    return;
  }

  std::vector<GPUGlyphMetrics> gpuMetrics;
  gpuMetrics.reserve(atlasData.glyphs.size());
  std::unordered_map<uint32_t, uint32_t> codepointToMetric;
  codepointToMetric.reserve(atlasData.glyphs.size());

  for (size_t i = 0; i < atlasData.glyphs.size(); ++i) {
    const auto &src = atlasData.glyphs[i];
    GPUGlyphMetrics dst = {};
    dst.uvMinX = src.uvRect[0];
    dst.uvMinY = src.uvRect[1];
    dst.uvMaxX = src.uvRect[2];
    dst.uvMaxY = src.uvRect[3];
    dst.offsetX = src.bearing[0];
    dst.offsetY = src.bearing[1];
    dst.sizeX = src.size[0];
    dst.sizeY = src.size[1];
    dst.advance = src.advance;
    gpuMetrics.push_back(dst);
    codepointToMetric.emplace(src.codepoint, static_cast<uint32_t>(i));
  }
  textSystem.UploadGlyphMetrics(gpuMetrics);

  std::vector<uint32_t> glyphIndices;
  std::vector<GPUTextStringMeta> meta;
  glyphIndices.reserve(256);
  meta.reserve(16);

  for (uint32_t cp = '0'; cp <= '9'; ++cp) {
    const auto it = codepointToMetric.find(cp);
    GPUTextStringMeta entry = {};
    entry.glyphOffset = static_cast<uint32_t>(glyphIndices.size());
    entry.glyphCount = (it != codepointToMetric.end()) ? 1u : 0u;
    entry.animStyle = 0u;
    if (it != codepointToMetric.end()) {
      glyphIndices.push_back(it->second);
    }
    meta.push_back(entry);
  }

  AppendAsciiString(codepointToMetric, "CRIT", 4u, glyphIndices, meta);
  AppendAsciiString(codepointToMetric, "STATUS", 2u, glyphIndices, meta);

  textSystem.UploadStringTable(glyphIndices, meta);

  // Publish CPU-side glyph metrics for runtime lookups (loot label system).
  // Register() copies the metrics vector, so atlasData can be unloaded below.
  MSDFAtlasRegistry::Get().Register(atlasData.texture, atlasData.glyphs,
                                    atlasData.distanceRange,
                                    MSDFAtlasRegistry::kV4AtlasEmSize);
  textSystem.SetAtlasTexture(atlasData.texture, true);
  atlasData.texture = {};
  MSDFAtlasLoader::Unload(atlasData);
}

} // namespace

Game::Game(int width, int height, const char *title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title) {

  // Configure Window Flags BEFORE InitWindow
  // FLAG_WINDOW_RESIZABLE: Allows user resizing
  // FLAG_MSAA_4X_HINT: Anti-aliasing
  // NOTE: HighDPI flag removed to match previous GCC behavior.
  // Ensure VSync is disabled to allow uncapped FPS
  // ClearConfigFlags(FLAG_VSYNC_HINT); // Not supported in this Raylib version
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
  LOG_INFO("Settings loaded: tier={}, targetFPS={}, cameraZoom={:.1f}, shake={:.1f}",
           std::string(NoMoreDay::GameSettings::RenderQualityTierToStringView(
               m_settings.renderQualityTier)),
           m_settings.targetFPS, m_settings.cameraZoom, m_settings.shakeIntensity);
  LOG_INFO("Target FPS set to: {}", m_settings.targetFPS);

  // Fill Context
  m_levelManager = std::make_unique<LevelManager>();
  m_context.registry = &m_registry;
  m_context.resources = &m_resourceManager;
  m_context.levelManager = m_levelManager.get();
  m_context.executor = &m_executor;
  m_context.settings = &m_settings;
  m_context.uiHost = &m_uiHost;

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
  std::system("chcp 65001 >nul 2>&1");

  // Global Static Inits
  NoMoreDay::CombatEventDispatcher::Init();
  NoMoreDay::AstrolabeRegistry::Get().Load();
  NoMoreDay::MaterialRegistry::Get().LoadMaterials(
      "assets/data/materials.json");
  NoMoreDay::SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  NoMoreDay::data::BladeMasteryRegistry::Get().Load();
  NoMoreDay::SkillSystem::InitHooks();
  NoMoreDay::BuffRegistry::Initialize();
  NoMoreDay::BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");

  NoMoreDay::ItemFactory::initialize();
  NoMoreDay::ItemFactory::loadAffixDefinitions("assets/data/affixes.json");

  if (!NoMoreDay::ModifierRuntimeRegistry::Get().EnsureLoaded(
          "assets/generated/modifier_runtime_v2.bin")) {
    LOG_CRITICAL("ModifierRuntimeV2 load failed");
    throw std::runtime_error("ModifierRuntimeV2 load failed");
  }

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

  // Expose SharedContext through registry context for systems that operate on
  // registry-only interfaces (e.g. CombatSystem static calls).
  if (!m_registry.ctx().contains<NoMoreDay::SharedContext *>()) {
    m_registry.ctx().emplace<NoMoreDay::SharedContext *>(&m_context);
  } else {
    m_registry.ctx().get<NoMoreDay::SharedContext *>() = &m_context;
  }

  // Initialize Stats System (Cache cleanup)
  NoMoreDay::StatsSystem::Initialize(m_registry);

  // Initialize UI System (Loads Fonts). Ownership moved to GameUiHost (U4);
  // the legacy facade is initialized through the host.
  m_uiHost.Initialize(m_resourceManager);

  // U8: bind the world-space UI frame so the render write side and the host
  // read side exchange visible-item/hover data through it instead of the
  // UiShared static slots.
  m_uiHost.BindWorldFrame(&m_worldFrame);

  // U7 group 3: cross-layer crafting entry points route through these
  // callbacks (see SharedContext) so systems below the UI layer never touch
  // the static UICrafting panel.
  m_context.openCraftingMergePanel = [this]() { m_uiHost.CraftingOpenMergePanel(); };
  // R10 (收尾): the craftingSetTargetItem callback is gone (no callers); the
  // overlay context-menu Craft action calls GameUiHost::CraftingSetTargetItem
  // directly, and InventorySystem only needs the merge-panel callback above.
  // R8: the legacy closeAstrolabe callback is gone (the skill-tree controller
  // routes the astrolabe close through the host channel directly).
  // U8: gameplay-layer message box notifications (InventorySystem etc.) route
  // through the host-owned OverlayController instead of the legacy static
  // State.showMessageBox (see SharedContext).
  m_context.showMessageBox = [this](const char* text) {
    m_uiHost.ShowMessageBox(text);
  };

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
        NoMoreDay::RenderConstants::GPU::MAX_PARTICLES);

    m_gpuEntitySystem.Init(m_resourceManager, 30000);
    m_gpuEntityAdapter.Init(30000, &m_registry, m_gpuEntitySystem);
    m_gpuEntityAdapter.SetLevelManager(m_context.levelManager);

    // Gameplay render adapter: receives RenderSystem gameplay hooks. The
    // context pointer is latched here so the hooks can reach Game state.
    m_gameplayRenderAdapter.SetContext(&m_context);
    m_gameplayRenderAdapter.Init();
    // U8: route the UI world pass into the frame object owned by Game.
  m_gameplayRenderAdapter.BindWorldUiFrame(&m_worldFrame);
  m_context.gameplayRenderHooks = &m_gameplayRenderAdapter;

  // U8 final: the render adapter no longer reads the UiShared global font
  // (removed); the composition root injects the font loaded during
  // GameUiHost::Initialize (UISystem private static) after both the host and
  // the adapter are ready.
  m_gameplayRenderAdapter.SetFont(UISystem::GetFont());

    m_mdiRenderer.Init(m_resourceManager, 30000);
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager, 256,
                                                       256);
    // Initialize GPU Skill Effect System (Global)
    NoMoreDay::systems::GPUSkillEffectSystem::Get().Init(
        m_resourceManager, NoMoreDay::RenderConstants::GPU::MAX_SKILL_EFFECTS);

    // Initialize GPU Damage Popup System
    NoMoreDay::render::PopupRenderer::Get().Init();
    InitializeGPUTextBootstrap(m_resourceManager);

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
            NoMoreDay::utils::Time::Update();
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
                m_gpuEntityAdapter.Update(m_registry, m_gpuEntitySystem,
                                          fixedDt, (float)GetTime());
              }
        
              accumulator -= fixedDt;
              logicRan = true;
            }
        
            if (logicRan && m_gpuInfo.computeShaderSupported) {
              // Submit new pulse to GPU
              m_gpuEntitySystem.UploadGPU(
                  {m_context.resources, &m_context.renderContext->MDI(),
                   m_context.renderAlpha});
            }  

            // Update particle system (Always run to process menu particles and emissions)
            if (m_gpuInfo.computeShaderSupported) {
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
              NoMoreDay::utils::ScopedTimer timer("Frame Render", 5000); 
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

int Game::runGpuGate(const std::string &revision, int sampleFramesPerFixture,
                     bool stressTest1Min, int toggleLoops) {
  using namespace NoMoreDay::render::validation;
  LOG_INFO("Running GPU hardware validation gate (revision={}, samples={}, "
           "stress={}, toggle_loops={})",
           revision, sampleFramesPerFixture, stressTest1Min, toggleLoops);

  // W6 (M0-C): the driver borrows the real game members - real registry,
  // real SharedContext/render context and the real gameplay render hooks
  // installed by Game::init() - and owns the RGBA16F composite target.
  GpuGateDriver driver(&m_registry, &m_context);
  const GateReport report =
      GPUHardwareValidationGate::RunGate(revision, sampleFramesPerFixture,
                                         stressTest1Min, toggleLoops, &driver);

  const std::string statusStr =
      (report.status == GateStatus::Go)
          ? "GO"
          : (report.status == GateStatus::NoGo) ? "NO_GO" : "NOT_RUN";

  // W6.4 (M0-C): exactly one status marker and exactly one versioned JSON
  // artifact between the BEGIN/END markers. Missing required fields are
  // NOT_RUN at the runner (never filled with defaults - fail-closed).
  std::cout << "GPU_HARDWARE_GATE_RESULT status=" << statusStr << "\n";
  std::cout << "GPU_HARDWARE_GATE_REPORT_BEGIN\n";
  std::cout << report.ToJsonString() << "\n";
  std::cout << "GPU_HARDWARE_GATE_REPORT_END\n" << std::flush;

  LOG_INFO("GPU hardware validation gate completed: status={}", statusStr);
  // Process exit code is decoupled from the verdict: the runner decides
  // pass/fail from the artifact (return_code==0 AND schema valid AND
  // status=="GO"); NO_GO/NOT_RUN are failures regardless of exit code.
  return 0;
}

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
  m_gameplayRenderAdapter.Shutdown();
  m_context.gameplayRenderHooks = nullptr;
  // UI host shutdown precedes resource unload / window close so the backend
  // releases registered raylib resources while the GL context is still alive.
  m_uiHost.Shutdown();
  NoMoreDay::render::GPUTextSystem::Get().Shutdown();
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
