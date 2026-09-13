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
#include "engine/render/RenderSystem.hpp"
#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/resource/MSDFAtlasLoader.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"
#include "engine/render/CoordSystem.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/foundation/components/AstrolabeUIComponent.hpp"
#include "game/foundation/components/WorldState.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
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
    dst.uvMinX = src.uvRect[0];  // Space::FboTexel (图集 UV 直通)
    dst.uvMinY = src.uvRect[1];
    dst.uvMaxX = src.uvRect[2];
    dst.uvMaxY = src.uvRect[3];
    // 空间边界: metrics.bin 数据为 Space::MsdfMetric (em 单位, 基线原点方位;
    // 参见 MSDFAtlasRegistry::kV4AtlasEmSize)。GPU 文本管线通过 text_layout.compute
    // 将度量数据作为 Space::World (y 向下) 消费，因此导入时通过 MSDF 标签路径使用的相同辅助函数进行归一化。
    // 当前 GPU 文本在 fontSize == emSize (1:1，即图集 em 大小) 下渲染；
    // 修改 kGpuTextFontSize 即可缩放而无需修改布局着色器。
    constexpr float kGpuTextFontSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    const float emSize = MSDFAtlasRegistry::kV4AtlasEmSize;
    dst.offsetX = NoMoreDay::render::coord::MsdfBearingToWorldOffset(
        src.bearing[0], emSize, kGpuTextFontSize);  // Space::World
    dst.offsetY = NoMoreDay::render::coord::MsdfBearingToWorldOffset(
        src.bearing[1], emSize, kGpuTextFontSize);  // Space::World
    dst.sizeX = src.size[0] * (kGpuTextFontSize / emSize);  // Space::World
    dst.sizeY = src.size[1] * (kGpuTextFontSize / emSize);  // Space::World
    dst.advance = src.advance * (kGpuTextFontSize / emSize);  // Space::World
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

  // 发布 CPU 侧字形度量数据以供运行时查询 (掉落标签系统)。
  // Register() 会复制度量向量，因此下面可以卸载 atlasData。
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

  // 在 InitWindow 前配置窗口标志
  // FLAG_WINDOW_RESIZABLE: 允许用户调整大小
  // FLAG_MSAA_4X_HINT: 抗锯齿
  // 注意: 已移除 HighDPI 标志以匹配先前的 GCC 行为。
  // 确保禁用垂直同步以支持无上限帧率
  // ClearConfigFlags(FLAG_VSYNC_HINT); // 当前 Raylib 版本不支持
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

  InitWindow(m_screenWidth, m_screenHeight, m_title);

  // 智能窗口定位与默认无边框窗口化
  int monitor = GetCurrentMonitor();
  int monitorW = GetMonitorWidth(monitor);
  int monitorH = GetMonitorHeight(monitor);

  LOG_INFO("Initializing display mode: Borderless Windowed (Monitor: {}x{})", monitorW, monitorH);
  SetWindowState(FLAG_WINDOW_UNDECORATED);
  SetWindowSize(monitorW, monitorH);
  SetWindowPosition(0, 0);
  m_screenWidth = monitorW;
  m_screenHeight = monitorH;
  m_isBorderlessFullscreen = true;

  // 保存回退窗口化状态以供 Alt+Enter 切换
  m_windowedWidth = (width > 0 && width < monitorW) ? width : static_cast<int>(monitorW * 0.75f);
  m_windowedHeight = (height > 0 && height < monitorH) ? height : static_cast<int>(monitorH * 0.75f);
  m_windowedPosX = (monitorW - m_windowedWidth) / 2;
  m_windowedPosY = (monitorH - m_windowedHeight) / 2;

  InitAudioDevice();

  // 初始化 GPU 能力检测并加载扩展
  m_gpuInfo = NoMoreDay::utils::GPUUtils::Initialize();

  // 提前注册 EnTT Groups (在添加任何组件之前)
  // 这对防止注册表损坏至关重要。
  NoMoreDay::groups::RegisterGroups(m_registry);

  SetExitKey(0);

  // 先加载设置，以确保 targetFPS 可用
  m_settings.Load();
  SetTargetFPS(m_settings.targetFPS); // 使用设置中的目标 FPS (默认: 180)
  LOG_INFO("Settings loaded: tier={}, targetFPS={}, cameraZoom={:.1f}, shake={:.1f}",
           std::string(NoMoreDay::GameSettings::RenderQualityTierToStringView(
               m_settings.renderQualityTier)),
           m_settings.targetFPS, m_settings.cameraZoom, m_settings.shakeIntensity);
  LOG_INFO("Target FPS set to: {}", m_settings.targetFPS);

  // 填充上下文
  m_levelManager = std::make_unique<LevelManager>();
  m_levelManager->initialize(m_resourceManager, m_registry);
  m_context.registry = &m_registry;
  m_context.resources = &m_resourceManager;
  m_context.levelManager = m_levelManager.get();
  m_context.executor = &m_executor;
  m_context.settings = &m_settings;
  m_context.uiHost = &m_uiHost;
  m_context.itemStorage = &m_itemStorage;
  m_context.saveManager = &m_saveManager;
  m_context.sharedStash = &m_sharedStash;
  m_context.heirloomVault = &m_heirloomVault;

  // 渲染上下文设置
  m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
  m_renderContext.mdiRenderer = &m_mdiRenderer;
  m_renderContext.resources = &m_resourceManager;
  m_context.renderContext = &m_renderContext;

  // 初始化 SceneManager
  m_sceneManager =
      std::make_unique<NoMoreDay::SceneManager>(*m_levelManager, m_registry);
  m_context.sceneManager = m_sceneManager.get();

  // 初始化 StateManager
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

  // 全局静态初始化
  NoMoreDay::CombatEventDispatcher::Init();
  NoMoreDay::AstrolabeRegistry::Get().Load();
  NoMoreDay::MaterialRegistry::Get().LoadMaterials(
      "assets/data/materials.json");
  NoMoreDay::SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // 机制表是技能数值唯一来源，加载失败必须阻断启动，禁止运行期静默回退默认值
  if (!NoMoreDay::data::SkillMechanicsRegistry::Get().LoadFromFile(
          "assets/data/skill_mechanics.json")) {
    LOG_CRITICAL("SkillMechanicsRegistry load failed");
    throw std::runtime_error("SkillMechanicsRegistry load failed");
  }
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

  // 初始化地图词缀注册表
  NoMoreDay::MapAffixRegistry::Initialize();

  // 初始化持久化管理器与共享存储
  m_sharedStash.initialize();
  m_saveManager.Initialize(&m_executor, &m_itemStorage);
  m_saveManager.loadGlobal(m_registry);

  // 在 Context 中初始化 ActiveDimensionalState
  // 确保该状态对所有系统全局可用
  if (!m_registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
    m_registry.ctx().emplace<NoMoreDay::ActiveDimensionalState>();
  }

  // 通过注册表上下文暴露 SharedContext，供仅基于注册表接口操作的系统使用（如 CombatSystem 静态调用）。
  if (!m_registry.ctx().contains<NoMoreDay::SharedContext *>()) {
    m_registry.ctx().emplace<NoMoreDay::SharedContext *>(&m_context);
  } else {
    m_registry.ctx().get<NoMoreDay::SharedContext *>() = &m_context;
  }

  // 绑定自动存档请求回调 (切图等世界系统触发)
  m_context.requestSave = [this](int slot) {
    m_saveManager.saveCharacterAsync(m_registry, slot);
  };

  // 初始化属性系统 (清理缓存)
  NoMoreDay::StatsSystem::Initialize(m_registry);

  // 初始化 UI 系统 (加载字体)。所有权已移至 GameUiHost (U4)；
  // 旧版外观通过 host 进行初始化。
  m_uiHost.Initialize(m_resourceManager);

  // U8: 绑定世界空间 UI 帧，以便渲染写入端与主机读取端通过它交换可见物品/悬停数据，
  // 而不再依赖 UiShared 静态槽位。
  m_uiHost.BindWorldFrame(&m_worldFrame);

  // U7 第3组: 跨层锻造入口通过这些回调路由 (参见 SharedContext)，
  // 从而使 UI 层之下的系统永远不需要触碰静态 UICrafting 面板。
  m_context.openCraftingMergePanel = [this]() { m_uiHost.CraftingOpenMergePanel(); };
  // R10 (收尾): craftingSetTargetItem 回调已移除 (无调用方)；
  // 覆盖层右键菜单 Craft 动作直接调用 GameUiHost::CraftingSetTargetItem，
  // 且 InventorySystem 仅需要上述 merge-panel 回调。
  // R8: 旧版 closeAstrolabe 回调已移除 (技能树控制器直接通过 host 通道路由星盘关闭)。
  // U8: 游戏层消息框通知 (InventorySystem 等) 通过 host 拥有的 OverlayController 路由，
  // 而非旧版静态 State.showMessageBox (参见 SharedContext)。
  m_context.showMessageBox = [this](const char* text) {
    m_uiHost.ShowMessageBox(text);
  };

  // 初始化 GPU 系统
  if (m_gpuInfo.computeShaderSupported) {
    // 1. 预加载实体纹理数组
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

    // 2. GPU 粒子系统 (间接绘制)
    NoMoreDay::systems::GPUParticleSystem::Get().Init(
        NoMoreDay::RenderConstants::GPU::MAX_PARTICLES);

    m_gpuEntitySystem.Init(m_resourceManager, 30000);
    m_gpuEntityAdapter.Init(30000, &m_registry, m_gpuEntitySystem);
    m_gpuEntityAdapter.SetLevelManager(m_context.levelManager);

    // Gameplay 渲染适配器：接收 RenderSystem gameplay 钩子。
    // 在此处锁存上下文指针以便钩子能够访问 Game 状态。
    m_gameplayRenderAdapter.SetContext(&m_context);
    m_gameplayRenderAdapter.Init();
    // U8: 将 UI world pass 路由至 Game 拥有的帧对象中。
  m_gameplayRenderAdapter.BindWorldUiFrame(&m_worldFrame);
  m_context.gameplayRenderHooks = &m_gameplayRenderAdapter;

  // U8 最终版: 渲染适配器不再读取 UiShared 全局字体 (已移除)；
  // 组合根在 host 和 adapter 就绪后注入 GameUiHost::Initialize 期间加载的字体 (UISystem 私有静态)。
  m_gameplayRenderAdapter.SetFont(UISystem::GetFont());

    m_mdiRenderer.Init(m_resourceManager, 30000);
    NoMoreDay::systems::GPUFlowFieldSystem::Get().Init(m_resourceManager, 256,
                                                       256);
    // 初始化 GPU 技能特效系统 (全局)
    NoMoreDay::systems::GPUSkillEffectSystem::Get().Init(
        m_resourceManager, NoMoreDay::RenderConstants::GPU::MAX_SKILL_EFFECTS);

    // 初始化 GPU 伤害跳字系统
    NoMoreDay::render::PopupRenderer::Get().Init();
    InitializeGPUTextBootstrap(m_resourceManager);

    // 初始化实例化标签渲染器
    if (!RenderSystem::Initialize()) {
      LOG_CRITICAL("Game::Init: RenderSystem initialization failed. Aborting game startup.");
      throw std::runtime_error("Game::init: RenderSystem initialization failed. Aborting game startup.");
    }

    // 关联上下文
    m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
    m_renderContext.mdiRenderer = &m_mdiRenderer;
    m_renderContext.gpuFlowFieldSystem =
        &NoMoreDay::systems::GPUFlowFieldSystem::Get();
    m_renderContext.resources = &m_resourceManager;
    m_context.renderContext = &m_renderContext;
  }

  // 压入初始状态
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
              // 1. 基于帧率更新游戏状态 (可变 DT)
              // 确保在高刷新率下的输入响应性 (ESC, 点击)。
              NoMoreDay::utils::ScopedTimer timer("1. Update State", 2000); // 2ms 阈值
              m_stateManager->Update(frameTime);
            }        if (m_stateManager->IsEmpty()) {
          LOG_INFO("State stack empty, exiting game loop");
          break;
        }
  
            // 2. 连续模拟与物理 (固定 DT)
            accumulator += frameTime;
            bool logicRan = false;
            while (accumulator >= fixedDt) {
              if (m_gpuInfo.computeShaderSupported) {
                // 1. CPU -> Shadow 同步 (仅逻辑更新，此处无 GPU 映射)
                m_gpuEntityAdapter.Update(m_registry, m_gpuEntitySystem,
                                          fixedDt, (float)GetTime());
              }
        
              accumulator -= fixedDt;
              logicRan = true;
            }
        
            if (logicRan && m_gpuInfo.computeShaderSupported) {
              // 提交新脉冲至 GPU
              m_gpuEntitySystem.UploadGPU(
                  {m_context.resources, &m_context.renderContext->MDI(),
                   m_context.renderAlpha});
            }  

            // 更新粒子系统 (始终运行以处理菜单粒子和发射器)
            if (m_gpuInfo.computeShaderSupported) {
                NoMoreDay::systems::GPUParticleSystem::Get().Update(frameTime);
            }
        // 更新用于渲染的插值 Alpha
        // alpha = accumulator / fixedDt, 范围 [0, 1)
        // 允许物理帧之间的平滑插值
        m_context.renderAlpha = accumulator / fixedDt;
  
        static float fpsLogTimer = 0.0f;
        fpsLogTimer += frameTime;
        // 每 1.0 秒以 INFO 级别记录 FPS
            LOG_LIMITED_INFO(1.0f, ">>> [PERF] FPS: {} | FrameTime: {:.3f} ms", GetFPS(),
                              frameTime * 1000.0f);
        
            {
              NoMoreDay::utils::ScopedTimer timer("Frame Render", 5000); 
              BeginDrawing();
              ClearBackground(BLACK);
              m_stateManager->Render();
              EndDrawing();
            }
            
            // 检查全屏切换按键 (Alt + Enter)
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

  // W6 (M0-C): 驱动程序借用真实游戏成员 - 真实 registry、
  // 真实 SharedContext/render context 以及由 Game::init() 安装的真实 gameplay 渲染钩子 -
  // 并持有 RGBA16F 复合目标。
  GpuGateDriver driver(&m_registry, &m_context);
  const GateReport report =
      GPUHardwareValidationGate::RunGate(revision, sampleFramesPerFixture,
                                         stressTest1Min, toggleLoops, &driver);

  const std::string statusStr =
      (report.status == GateStatus::Go)
          ? "GO"
          : (report.status == GateStatus::NoGo) ? "NO_GO" : "NOT_RUN";

  // W6.4 (M0-C): 在 BEGIN/END 标记之间输出唯一的 status 标记和唯一的版本化 JSON 工件。
  // 缺失必填字段在运行器处判定为 NOT_RUN (从不填充默认值 - 故障闭锁)。
  std::cout << "GPU_HARDWARE_GATE_RESULT status=" << statusStr << "\n";
  std::cout << "GPU_HARDWARE_GATE_REPORT_BEGIN\n";
  std::cout << report.ToJsonString() << "\n";
  std::cout << "GPU_HARDWARE_GATE_REPORT_END\n" << std::flush;

  LOG_INFO("GPU hardware validation gate completed: status={}", statusStr);
  // 进程退出码与裁决结果解耦: 运行器根据工件决定通过/失败
  // (return_code==0 且 schema 有效且 status=="GO")；NO_GO/NOT_RUN 无论退出码如何均视为失败。
  return 0;
}

void Game::toggleFullScreen() {
    int monitor = GetCurrentMonitor();
    int monitorW = GetMonitorWidth(monitor);
    int monitorH = GetMonitorHeight(monitor);

    if (m_isBorderlessFullscreen) {
        // 切换到窗口化模式
        LOG_INFO("Switching to Windowed Mode...");
        SetWindowState(FLAG_WINDOW_UNDECORATED); // 临时确保状态以便平滑过渡
        ClearWindowState(FLAG_WINDOW_UNDECORATED); // 恢复窗口边框装饰
        
        // 恢复保存的窗口大小或默认为安全大小
        if (m_windowedWidth == 0 || m_windowedHeight == 0) {
            // 无保存状态时默认为显示器尺寸的 75%
            m_windowedWidth = (int)(monitorW * 0.75f);
            m_windowedHeight = (int)(monitorH * 0.75f);
            m_windowedPosX = (monitorW - m_windowedWidth) / 2;
            m_windowedPosY = (monitorH - m_windowedHeight) / 2;
        }

        SetWindowSize(m_windowedWidth, m_windowedHeight);
        SetWindowPosition(m_windowedPosX, m_windowedPosY);
        m_screenWidth = m_windowedWidth;
        m_screenHeight = m_windowedHeight;
        m_isBorderlessFullscreen = false;
    } else {
        // 切换到无边框全屏模式
        LOG_INFO("Switching to Borderless Fullscreen...");
        
        // 保存当前窗口化状态
        m_windowedWidth = GetScreenWidth();
        m_windowedHeight = GetScreenHeight();
        Vector2 pos = GetWindowPosition();
        m_windowedPosX = (int)pos.x;
        m_windowedPosY = (int)pos.y;

        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(monitorW, monitorH);
        SetWindowPosition(0, 0);
        m_screenWidth = monitorW;
        m_screenHeight = monitorH;
        m_isBorderlessFullscreen = true;
    }
}

void Game::cleanup() {
  LOG_INFO("Cleaning up game systems...");

  // 保存全局状态 (共享仓库)
  m_saveManager.saveGlobalAsync(m_registry);

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

  RenderSystem::Shutdown();
  m_gameplayRenderAdapter.Shutdown();
  m_context.gameplayRenderHooks = nullptr;
  // 在持有的服务超出作用域之前释放上下文借用。
  m_context.itemStorage = nullptr;
  m_context.saveManager = nullptr;
  m_context.sharedStash = nullptr;
  m_context.heirloomVault = nullptr;
  m_context.requestSave = nullptr;
  // UI host 关闭先于资源卸载 / 窗口关闭，以便后端在 GL 上下文依然有效时释放已注册的 raylib 资源。
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
