#include "game/application/states/GameplayStateInternal.hpp"

#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillPointAccess.hpp" // 统一技能节点读点 helper

#include "game/systems/skill/ElementPathSystem.hpp" // 技能8 元素路径

namespace NoMoreDay {


GameplayState::GameplayState(StateManager &stateManager, SharedContext &context,
                             RenderContext &renderContext)
    : IState(stateManager, context), m_renderContext(&renderContext) {}

void GameplayState::OnEnter() {
  LOG_INFO("Entering GameplayState...");
  
  const auto sceneExtent = RenderSystem::GetRenderTargetExtent(
      GetScreenWidth(), GetScreenHeight());
  m_sceneRT = LoadRenderTexture(sceneExtent.width, sceneExtent.height);

  // Clear any residual particles from previous states (e.g. Main Menu)
  systems::GPUParticleSystem::Get().Clear();

  // [CRITICAL] Initialize Static Registries to prevent Async Race Conditions
  NoMoreDay::MapAffixRegistry::Initialize();

  // [CRITICAL] Reset Static Safety Queues to prevent Cross-Session ID
  // Collisions
  NoMoreDay::XPAwardingSystem::Reset();

  // Initialize Spatial Grid
  using namespace NoMoreDay::Constants::World;
  m_spatialGrid =
      systems::SpatialHashGrid(GRID_COLS, GRID_ROWS, GRID_CELL_SIZE);
  m_context->spatialGrid = &m_spatialGrid;

  // Initialize Visual FX (Events)
  systems::VisualFXSystem::Initialize(*m_context->registry);
  m_vfxHotReloadAccumulator = 0.0f;
  vfx::VFXSequenceManager::Get().Initialize();
  if (std::filesystem::is_directory("assets/vfx")) {
    vfx::VFXSequenceManager::Get().LoadFromJson("assets/vfx");
  } else {
    LOG_WARN("GameplayState: assets/vfx not found, sequencer assets skipped");
  }

  // Initialize Elite Modifiers (SoulLink, Avenger)
  // Initialize Elite Modifiers (SoulLink, Avenger)
  EliteModifierSystem::Init();
  MonsterAffixSystem::Init();
  CombatHistorySystem::Init();
  FragmentDropSystem::Init();
  FragmentDropSystem::SetLevelManager(m_context->levelManager);

  // GPU Skill Effect System is initialized in Game.cpp (Global)

  // 1. Initialize Managers/Resources (if not already)
  // Note: Resources are managed by SharedContext->resources (owned by Game)
  // But we might need to load specific assets here if not preloaded.
  // Assuming Game preloads or we do it here.
  // Game::init logic for loading textures:
  auto &resourceManager = *m_context->resources;

  // Ensure UI System is ready
  // UISystem::Initialize(resourceManager); // Already called in Game::init?
  // Let's assume Game calls it once.

  // Load Game Textures
  const auto &playerAsset = assets::textures::Player_Warrior;
  resourceManager.loadTexture(playerAsset.id, std::string(playerAsset.path));
  resourceManager.loadTexture(assets::textures::Weapon_Sword.id,
                              std::string(assets::textures::Weapon_Sword.path));
  resourceManager.loadTexture(assets::textures::Skeleton.id,
                              std::string(assets::textures::Skeleton.path));
  resourceManager.loadTexture(assets::textures::Cultist.id,
                              std::string(assets::textures::Cultist.path));
  resourceManager.loadTexture(assets::textures::Demon.id,
                              std::string(assets::textures::Demon.path));
  resourceManager.loadTexture(
      assets::textures::Corrupted_Beast.id,
      std::string(assets::textures::Corrupted_Beast.path));

  // 2. Initialize Level if needed
  if (!m_context->levelManager->isInitialized()) {
    m_context->levelManager->initialize(resourceManager, *m_context->registry);
    using namespace NoMoreDay::Constants::World;
    // Start in Town by default (as per user request: "将初始地图设置为平安镇")
    m_context->levelManager->loadNewLevel(NoMoreDay::BiomeID::Town,
                                          LevelManager::DEFAULT_MAP_WIDTH,
                                          LevelManager::DEFAULT_MAP_HEIGHT);
  }

  // 3. Initialize Entities (Player)
  auto playerTagView = m_context->registry->view<PlayerTag>();
  if (playerTagView.empty()) {
    InitializeEntities();
  } else {
    auto pEntity = playerTagView.front();
    if (!m_context->registry->any_of<SpriteComponent>(pEntity)) {
      Texture2D pTex = resourceManager.getTexture(playerAsset.id);
      if (pTex.id > 0) {
        m_context->registry->emplace<SpriteComponent>(pEntity, pTex, 0.4f);
      }
    }
  }

  // 4. Initialize Camera
  m_camera.zoom = m_context->settings ? m_context->settings->cameraZoom : 1.5f;
  m_camera.offset = {(float)GetScreenWidth() / 2.0f,
                     (float)GetScreenHeight() / 2.0f};
  m_camera.rotation = 0.0f;
  m_camera.target = {(float)GetScreenWidth() / 2.0f,
                     (float)GetScreenHeight() / 2.0f};
  auto playerView = m_context->registry->view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end()) {
    const auto &pos = playerView.get<Position>(playerView.front());
    m_camera.target = {pos.x, pos.y};
  }

  // Pre-update Fog of War around player position so initial frame is clear
  if (m_context->levelManager->isInitialized() && playerView.begin() != playerView.end()) {
    auto &fogSystem = m_context->levelManager->getFogSystem();
    const auto &pPos = playerView.get<Position>(playerView.front());
    float viewRadius = 300.0f;
    if (const auto *vis = m_context->registry->try_get<VisionComponent>(playerView.front())) {
      if (vis->radius > 0.0f) {
        viewRadius = vis->radius;
      }
    }
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(
        m_context->levelManager->getCurrentBiomeID());
    if (biome.hasFeature(NoMoreDay::BiomeFeature::LimitedVision) &&
        biome.visionRadius > 0.0f) {
      viewRadius = std::min(viewRadius, biome.visionRadius);
    }
    fogSystem.updateVisibility(pPos, viewRadius);
  }

  // 5. Initialize Portal System
  if (m_context->sceneManager) {
    m_portalSystem = std::make_unique<PortalSystem>(*m_context->sceneManager);
    if (auto *adapter = dynamic_cast<GameplayRenderAdapter*>(m_context->gameplayRenderHooks)) {
      adapter->SetPortalSystem(m_portalSystem.get());
    }
  }

  // 6. Borrow the UI composition root and scope the gameplay UI session.
  m_uiHost = m_context->uiHost;
  if (m_uiHost) {
    m_uiHost->EnterGameplay();
  }
}



void GameplayState::OnExit() {
  LOG_INFO("Exiting GameplayState...");

  if (m_sceneRT.id != 0) {
    UnloadRenderTexture(m_sceneRT);
    m_sceneRT = {0};
  }
  if (m_activeFilterShader.id != 0) {
    UnloadShader(m_activeFilterShader);
    m_activeFilterShader = {0};
    m_lastFilterPath.clear();
    m_filterLocTime = -1;
    m_filterLocCam = -1;
    m_filterLocZoom = -1;
    m_filterLocScreen = -1;
    m_filterLocPlayer = -1;
    m_filterLocVision = -1;
  }

  NoMoreDay::XPAwardingSystem::Reset(); // Cleanup static GC queue
  EliteModifierSystem::Shutdown();

  CombatHistorySystem::Shutdown();
  FragmentDropSystem::Shutdown();
  vfx::VFXSequenceManager::Get().Shutdown();

  // End the gameplay-scoped UI session: panels/drag/tooltip state is reset so
  // nothing leaks into the next run.
  if (m_uiHost) {
    m_uiHost->LeaveGameplay();
  }
  m_uiHost = nullptr;

  // Cleanup logic if needed.
  // Note: Game::cleanup will handle the global registry clear.
}

GameplayState::~GameplayState() {
  // Destructor defined here where PortalSystem is complete
}

bool GameplayState::OnUpdate(float dt) {
  auto &registry = *m_context->registry;
  NoMoreDay::ProcBudgetManager::Get().BeginFrame(dt);
  NoMoreDay::CombatTelemetry::Get().BeginFrame(dt);
  NoMoreDay::render::GPUTextSystem::Get().BeginFrame();

  UpdateSceneRT();

  // Load/Update Filter Shader based on Biome
  const auto &biome =
      NoMoreDay::BiomeRegistry::Get().GetBiome(m_context->levelManager->getCurrentBiomeID());
  if (biome.hasFeature(NoMoreDay::BiomeFeature::VisualFilter) &&
      !biome.visualFilterShader.empty()) {
    if (m_lastFilterPath != biome.visualFilterShader) {
      if (m_activeFilterShader.id != 0) {
        UnloadShader(m_activeFilterShader);
      }
      m_activeFilterShader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
          0, biome.visualFilterShader.c_str());
      m_lastFilterPath = biome.visualFilterShader;
      m_filterLocTime = GetShaderLocation(m_activeFilterShader, "time");
      m_filterLocCam = GetShaderLocation(m_activeFilterShader, "cameraOffset");
      m_filterLocZoom = GetShaderLocation(m_activeFilterShader, "zoom");
      m_filterLocScreen = GetShaderLocation(m_activeFilterShader, "screenSize");
      m_filterLocPlayer = GetShaderLocation(m_activeFilterShader, "playerPos");
      m_filterLocVision = GetShaderLocation(m_activeFilterShader, "visionRadius");
      LOG_INFO("Loaded visual filter shader: {}", m_lastFilterPath);
    }
  } else if (m_activeFilterShader.id != 0) {
    UnloadShader(m_activeFilterShader);
    m_activeFilterShader = {0};
    m_lastFilterPath.clear();
    m_filterLocTime = -1;
    m_filterLocCam = -1;
    m_filterLocZoom = -1;
    m_filterLocScreen = -1;
    m_filterLocPlayer = -1;
    m_filterLocVision = -1;
  }

  // 0. Update SceneManager (Transitions)
  if (m_context->sceneManager) {
    m_context->sceneManager->Update(dt);
    if (m_context->sceneManager->IsTransitioning()) {
      return false; // Skip logic during transition (loading/fade)
    }
  }

  // 0. State Transition Input
  // U8 inventory takeover: KEY_I toggles the hosted inventory controller
  // inside GameUiHost::Update (runs later this frame, after this check), so
  // the game state no longer pushes InventoryState.

  // Town Portal (KEY_T) - only when no major panel is open.
  // U8 host read-side migration: the anyPanelOpen check aggregates the hosted
  // controller instances (inventory/skill tree/character/stash/crafting/
  // astrolabe) via the host channel; the legacy State reads are gone.
  const bool anyPanelOpen = m_uiHost->IsAnyPanelOpen();
  if (IsKeyPressed(KEY_T) && !anyPanelOpen) {
    auto playerViewT = registry.view<PlayerTag>();
    if (playerViewT.begin() != playerViewT.end()) {
      PortalSystem::StartTownPortalCast(registry, playerViewT.front());
    }
  }

  // Get Player Pos
  Position playerPos{0, 0};
  auto playerView = registry.view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end()) {
    playerPos = playerView.get<Position>(playerView.front());
  }

  // 1. Level & Systems
  {
    NoMoreDay::utils::ScopedTimer timer("Level Update", 200);
    m_context->levelManager->update(dt, registry, playerPos);
  }

  // Spatial Grid Rebuild (Exclude items/gold/dormant to keep AI/physics search
  // fast) Move rebuild here so systems use fresh data this frame
  {
    NoMoreDay::utils::ScopedTimer timer("Spatial Rebuild", 200);
    auto gridView = registry.view<Position>(
        entt::exclude<NoMoreDay::ItemComponent, GoldComponent, DormantTag>);
    m_spatialGrid.rebuild(gridView, registry);
  }

  // Update Dormant Entities (Spec 2.3)
  const auto &map = m_context->levelManager->getMapSystem();
  m_context->levelManager->getEnemySpawnSystem().updateDormantEntities(
      registry, playerPos, map.getWidth(), map.getHeight());
  // m_context->levelManager->getMapSystem().updateFlowField(playerPos);

  // GPU Flow Field
  {
    NoMoreDay::utils::ScopedTimer timer("FlowField Update", 200);
    // Use RenderContext for FlowFieldSystem
    auto &flowSystem = m_renderContext->Flow();
    // Map already declared above
    if (map.getWidth() > 0) {
      using namespace NoMoreDay::Constants::World;
      float cellSize = GRID_TILE_SIZE; // Tile size
      int gw = flowSystem.GetWidth();
      int gh = flowSystem.GetHeight();

      // Center grid on player and snap to tile size
      float originX =
          floor((playerPos.x - (gw * cellSize * 0.5f)) / cellSize) * cellSize;
      float originY =
          floor((playerPos.y - (gh * cellSize * 0.5f)) / cellSize) * cellSize;

      auto &gpuEntitySystem = *m_renderContext->gpuEntitySystem;
      flowSystem.Update(map.getCostMap(), map.getWidth(), map.getHeight(),
                        {playerPos.x, playerPos.y}, {originX, originY},
                        &gpuEntitySystem.GetEntityBuffer(),
                        gpuEntitySystem.GetMaxEntities());
    }

    if (m_context->sceneManager) {
      m_portalSystem->Update(registry, dt);
    }

    // Check if PortalSystem requested MosaicEditorState
    auto pendingView = registry.view<PendingMosaicEditorTag, PlayerTag>();
    for (auto entity : pendingView) {
      registry.remove<PendingMosaicEditorTag>(entity);
      m_stateManager->PushState<MosaicEditorState>();
      LOG_INFO("Pushed MosaicEditorState");
    }

    // Handle Dimensional Gate (Town Hub)
    auto pendingGateView = registry.view<PendingDimensionalGateTag, PlayerTag>();
    for (auto entity : pendingGateView) {
        registry.remove<PendingDimensionalGateTag>(entity);

        if (!registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
            OpenDimensionalLevelSelect(registry, entity);
            continue;
        }

        const auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
        if (NoMoreDay::HasInProgressRift(state)) {
            m_gateDialogPlayer = entity;
            m_showGateResumeOrNewDialog = true;
            m_showGateStartNewConfirmDialog = false;
            LOG_INFO("Dimensional Gate blocked by active rift. Showing Resume/New dialog.");
        } else {
            OpenDimensionalLevelSelect(registry, entity);
        }
    }

    m_showRiftCompletedDialog = registry.ctx().contains<RiftCompletionPromptState>() &&
                                registry.ctx().get<RiftCompletionPromptState>().isPending;

    if (HandleRiftDialogs(registry)) {
      return false;
    }
  }
  {
    NoMoreDay::utils::ScopedTimer timer("Systems Update", 200);
    MovementStanceSystem::Update(registry, dt);
    StatsSystem::UpdateBuffs(registry, dt);
    StatsSystem::update(registry);
    RegenerationSystem::update(registry, dt);
    EliteModifierSystem::Update(registry, dt);
    MonsterAffixSystem::Update(registry, dt, m_spatialGrid);
    CombatHistorySystem::Update(registry, dt);
    NoMoreDay::HazardSystem::Update(registry, dt, m_spatialGrid);
    DropSystem::update(registry, m_context->levelManager->getCurrentLevel());
    NoMoreDay::systems::LootGridSystem::update(registry); // Phase 4: Loot Spatial Grid
    FragmentDropSystem::Update(registry); // 处理碎片的延迟创建请求
    XPAwardingSystem::update(registry);
    InventorySystem::update(registry, dt);
    // Update Skill System
    ShadowSystem::Update(registry, dt);
    SkillSystem::Update(registry, m_spatialGrid, dt, m_context->executor);
    NoMoreDay::systems::SummonSystem::Update(registry, dt, m_spatialGrid);
    MovementStanceSystem::Update(registry, dt);
    ProjectileSystem::Update(registry, m_spatialGrid, dt);
    // 技能8 元素路径：衰减、拖尾与雷路径电弧（在飞剑交付之后结算）
    NoMoreDay::element_path::Update(registry, dt);
    NoMoreDay::systems::GhostSystem::Update(registry, dt);
    NoMoreDay::render::PopupRenderer::Get().Update(dt);
    const auto &renderConfig =
        NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
    if (renderConfig.gpuTextEnabled) {
      const float animDuration =
          renderConfig.gpuTextAdvancedAnimation ? 1.0f : 0.6f;
      NoMoreDay::render::GPUTextSystem::Get().DispatchLayout(
          static_cast<float>(GetTime()), animDuration);
    }
  }

  // 2. Input
  // U5: gameplay input runs after the UI update below so it can consume the
  // aggregated UiInputCapture. The U8 typing gate aggregates the controllers'
  // instance text-input focus (GameUiHost::IsTyping); the legacy State field
  // is gone. All global keys ('C'/'K'/'S'/'N'/'E'/'F'/ESC) are handled inside
  // GameUiHost::Update (U8 final); the null-host fallback path is removed.
  // R1 (remediation): intent execution moves BEFORE the snapshot build. The
  // stable frame order is: execute the intents queued by the previous phase
  // (success/failure flows into the next snapshot through GameUiResult), then
  // build the read-only snapshot from the final gameplay state, then run the
  // UI update against it (design §3.1, remediation design §3.3).
  for (const auto &intent : m_uiHost->DrainUpdateIntents()) {
    const auto result = m_commandHandler.Execute(registry, intent);
    m_uiHost->Publish(result);
  }
  const auto snapshot =
      m_snapshotBuilder.Build(registry, m_uiHost->SnapshotOptions());
  m_uiHost->Update(registry, *m_context->levelManager, snapshot);

  // R3 (remediation, design §3.6): the pause check runs AFTER the UI update
  // and only on the Escape edge when the host did NOT consume the key (no UI
  // surface was closed by this press). The old IsInventoryVisible() proxy is
  // gone: the host owns the full UI Escape close policy and reports
  // consumption through EscapeConsumedThisFrame, so one press is never
  // consumed twice (once by the UI, once by PauseState — H-02).
  if (IsKeyPressed(KEY_ESCAPE) &&
      !m_uiHost->EscapeConsumedThisFrame()) {
    m_stateManager->PushState<PauseState>();
  }

  // Gameplay input consumes the UI capture aggregated after the UI update
  // (U5), so InputSystem reads the capture produced by the composition root
  // (GameUiHost::InputCapture), never UI static state.
const NoMoreDay::ui::UiInputCapture uiCapture =
m_uiHost->InputCapture();
  InputSystem::update(registry, m_camera, uiCapture);

  // U8 inventory takeover: the legacy InventoryState block that observed
  // UISystem::State.showInventory (and its comment about pushing the state)
  // is removed — the inventory panel is a hosted overlay now, toggled by
  // GameUiHost on KEY_I/ESC/TAB.

  // Serialization (F5/F8 快捷键重定向到 SaveManager)
  if (IsKeyPressed(KEY_F5)) {
    if (m_context && m_context->saveManager) {
      m_context->saveManager->saveCharacterAsync(registry, 0);
    } else {
      LOG_ERROR("GameplayState: SaveManager not available in SharedContext!");
    }
  }
  if (IsKeyPressed(KEY_F8)) {
    bool loaded = false;
    if (m_context && m_context->saveManager) {
      loaded = m_context->saveManager->loadCharacter(registry, 0);
    } else {
      LOG_ERROR("GameplayState: SaveManager not available in SharedContext!");
    }
    if (loaded) {
      // Reload sprites logic
      auto view = registry.view<TextureIDComponent>();
      for (auto entity : view) {
        const auto &texComp = view.get<TextureIDComponent>(entity);
        Texture2D tex = m_context->resources->loadTexture(texComp.id, "");
        registry.emplace_or_replace<SpriteComponent>(entity, tex, 0.4f);
      }
    }
  }

  // 3. Player Movement & Dash (Moved from Game.cpp)
  auto playerView2 =
      registry
          .view<PlayerTag, InputComponent, Velocity, Position, DashComponent>();
  for (auto entity : playerView2) {
    auto &input = playerView2.get<InputComponent>(entity);
    auto &vel = playerView2.get<Velocity>(entity);
    auto &pos = playerView2.get<Position>(entity);
    auto &dash = playerView2.get<DashComponent>(entity);

    using namespace NoMoreDay::Constants::Combat; // Cap::CDR etc.

    // 550 御剑行: 引导万剑归宗 (技能5) 时的移动/位移约束。
    // 基底引导不可移动; 点出 550 后可保持移动但移速固定降低, 且无法使用位移技能;
    // 503 灵动引导降低惩罚; 551 御剑风雷在御剑步状态下免除惩罚。
    bool channelingSkill5 = false;
    bool walkThePath = false;       // 550
    float movePenalty = 0.0f;
    if (auto *chan = registry.try_get<BeamChannelComponent>(entity);
        chan && chan->skill_id == 5) {
      channelingSkill5 = true;
      if (const auto *profile = SkillSystem::GetBakedSkillProfile(registry, entity, 5)) {
        constexpr uint32_t kWalkThePathFlag = 32768;      // 550 御剑行
        constexpr uint32_t kSwordStepChannelFlag = 65536; // 551 御剑风雷
        if ((profile->delivery.feature_flags & kWalkThePathFlag) != 0) {
          walkThePath = true;
          movePenalty = data::SkillMechanicsRegistry::Get().GetFloat(
              5, 550, "move_speed_penalty_pct", 0.40f);
          // 503 灵动引导: 每点降低 10% 移速惩罚
          int pts_503 = 0;
          if (const auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
            for (const auto &spec : active->specialized_slots) {
              if (spec.skill_id == 5) {
                pts_503 = skills::ReadPoints(spec, 503);
                break;
              }
            }
          }
          if (pts_503 > 0) {
            movePenalty *=
                (1.0f - data::SkillMechanicsRegistry::Get().GetFloat(
                            5, 503, "move_penalty_reduction_pct_per_point", 0.10f) *
                            static_cast<float>(pts_503));
          }
          // 551 御剑风雷: 御剑步状态引导期间免除移速惩罚
          if ((profile->delivery.feature_flags & kSwordStepChannelFlag) != 0) {
            bool inSwordStep = false;
            if (const auto *effects = registry.try_get<ActiveEffectsComponent>(entity)) {
              inSwordStep = (effects->Get(BuffId::SwordStep) != nullptr);
            }
            if (!inSwordStep) inSwordStep = registry.any_of<PhaseTag>(entity);
            if (inSwordStep) movePenalty = 0.0f;
          }
        }
      }
    }

    if (IsKeyPressed(KEY_SPACE))
      input.dash = true;

    float effectiveCooldown = dash.cooldownDuration;
    if (registry.all_of<CombatStats>(entity)) {
      float cdr = registry.get<CombatStats>(entity).cooldown_reduction;
      if (cdr > Cap::CDR)
        cdr = Cap::CDR;
      effectiveCooldown *= (1.0f - cdr);
    }

    if (dash.charges < dash.maxCharges) {
      dash.cooldownTimer -= dt;
      if (dash.cooldownTimer <= 0.0f) {
        dash.charges++;
        dash.uiFlash = true;
        dash.uiFlashTimer = 0.2f;
        if (dash.charges < dash.maxCharges)
          dash.cooldownTimer = effectiveCooldown;
      }
    }
    if (dash.uiFlash) {
      dash.uiFlashTimer -= dt;
      if (dash.uiFlashTimer <= 0.0f)
        dash.uiFlash = false;
    }

    // 引导万剑归宗期间禁用位移技能 (冲刺; 技能槽位位移需 SkillSystem 侧拦截)
    if (input.dash && dash.charges > 0 && !dash.isDashing && !channelingSkill5) {
      dash.charges--;
      dash.isDashing = true;
      dash.dashTimer = dash.dashDuration;

      float len =
          std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
      if (len > 0.1f) {
        dash.dirX = input.moveX / len;
        dash.dirY = input.moveY / len;
      } else {
        Vector2 mousePos = NoMoreDay::render::coord::ScenePixelToWorld(
            NoMoreDay::render::coord::Camera2DTransform::From(m_camera),
            GetMousePosition());
        float dx = mousePos.x - pos.x;
        float dy = mousePos.y - pos.y;
        float mLen = std::sqrt(dx * dx + dy * dy);
        if (mLen > 0.1f) {
          dash.dirX = dx / mLen;
          dash.dirY = dy / mLen;
        } else {
          dash.dirX = 1.0f;
          dash.dirY = 0.0f;
        }
      }

      if (dash.charges == dash.maxCharges - 1 && dash.cooldownTimer <= 0.0f) {
        dash.cooldownTimer = effectiveCooldown;
      }

      // Start Trail
      auto &trail = registry.get_or_emplace<components::MotionTrail>(entity);
      trail.isActive = true;
      trail.maxWidth = 15.0f;
      trail.lifetime = 0.3f;
      trail.color = {200, 200, 255, 150}; // Light blue for normal dash
    }

    if (dash.isDashing) {
      dash.dashTimer -= dt;
      vel.vx = dash.dirX * dash.dashSpeed;
      vel.vy = dash.dirY * dash.dashSpeed;

      // Predictive CPU position update for DASH with Collision Check
      const auto &mapSystem = m_context->levelManager->getMapSystem();
      PhysicsSystem::performDashStep(registry, entity, dash, pos, vel, dt,
                                     m_spatialGrid, &mapSystem);

      if (!dash.isDashing || dash.dashTimer <= 0.0f) {
        dash.isDashing = false;
        vel.vx = 0;
        vel.vy = 0;
        registry.remove<PhaseTag>(entity);

        // Stop Trail
        if (auto *trail = registry.try_get<components::MotionTrail>(entity)) {
          trail->isActive = false;
        }
      }
    } else {
      using namespace NoMoreDay::Constants::Combat;
      float speed = DEFAULT_MOVE_SPEED;
      if (registry.all_of<CombatStats>(entity)) {
        speed = registry.get<CombatStats>(entity).move_speed;
      }
      // 550 御剑行: 点出后引导时可保持移动但移速降低; 未点出时引导期间不可移动
      if (channelingSkill5) {
        if (walkThePath) {
          speed *= (1.0f - movePenalty);
        } else {
          speed = 0.0f;
        }
      }
      speed *= m_context->levelManager->getMapSystem().getSpeedMultiplierAtWorld(
          pos.x, pos.y);
      vel.vx = input.moveX * speed;
      vel.vy = input.moveY * speed;

      // Predictive CPU position update for immediate camera/UI response
      // This bypasses the 2-frame GPUEntitySystem SyncBack latency for the
      // player. [FIX] Apply Map Collision Check BEFORE position update
      float radius =
          NoMoreDay::Constants::Physics::DEFAULT_ENTITY_RADIUS * 0.8f;
      if (registry.all_of<Radius>(entity))
        radius = registry.get<Radius>(entity).value * 0.8f;

      const auto &map = m_context->levelManager->getMapSystem();
      TilemapCollisionSystem::ResolveCollision(map, pos, vel, dt, radius);

      pos.x += vel.vx * dt;
      pos.y += vel.vy * dt;

      // [SAFETY] Position Sanity Check
      static Position lastValidPos = pos;
      LOG_LIMITED_DEBUG(
          1.0f,
          "[PLAYER_POS] Current: ({:.1f}, {:.1f}), Velocity: ({:.1f}, {:.1f})",
          pos.x, pos.y, vel.vx, vel.vy);

      if (std::isnan(pos.x) || std::isnan(pos.y) || pos.x < -2000.0f ||
          pos.x > 15000.0f || pos.y < -2000.0f || pos.y > 15000.0f) {
        LOG_ERROR("[CRITICAL] Player teleported to invalid position: ({:.1f}, "
                  "{:.1f}). Resetting to last valid pos.",
                  pos.x, pos.y);
        pos = lastValidPos;
        vel.vx = 0;
        vel.vy = 0;
      } else {
        lastValidPos = pos;
      }
    }

    // Camera Follow
    float lerpSpeed = 5.0f;
    m_camera.target.x += (pos.x - m_camera.target.x) * lerpSpeed * dt;
    m_camera.target.y += (pos.y - m_camera.target.y) * lerpSpeed * dt;

    // Sync Zoom from settings
    if (m_context->settings) {
      m_camera.zoom = m_context->settings->cameraZoom;
      RenderSystem::SetShakeMultiplier(m_context->settings->shakeIntensity);
    }

    // Screen Shake
    RenderSystem::UpdateShake(dt);
    Vector2 shake = RenderSystem::GetShakeOffset();

    // Reset offset to center then apply shake
    m_camera.offset = { floorf((float)GetScreenWidth() / 2.0f),
                       floorf((float)GetScreenHeight() / 2.0f) };
    m_camera.offset.x += roundf(shake.x);
    m_camera.offset.y += roundf(shake.y);
  }

  // 更新战争迷雾 (基于玩家位置和视野半径)
  if (m_context->levelManager->isInitialized() && playerView.begin() != playerView.end()) {
    playerPos = registry.get<Position>(playerView.front());
    auto &fogSystem = m_context->levelManager->getFogSystem();
    float viewRadius = 300.0f;
    if (const auto *vis = registry.try_get<VisionComponent>(playerView.front())) {
      if (vis->radius > 0.0f) {
        viewRadius = vis->radius;
      }
    }
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(
        m_context->levelManager->getCurrentBiomeID());
    if (biome.hasFeature(NoMoreDay::BiomeFeature::LimitedVision) &&
        biome.visionRadius > 0.0f) {
      viewRadius = std::min(viewRadius, biome.visionRadius);
    }
    fogSystem.updateVisibility(playerPos, viewRadius);
  }

  // 4. AI
  {
    NoMoreDay::utils::ScopedTimer timer("AI Update", 200);
    AISystem::update(registry, m_spatialGrid,
                     m_context->levelManager->getMapSystem(), playerPos, dt);
  }

  // 5. Combat
  {
   // NoMoreDay::utils::ScopedTimer timer("1.6 Combat Update", 500); // Combat is usually fast?
    NoMoreDay::systems::BossFrameworkSystem::Update(registry, dt);
    CombatSystem::update(registry, m_spatialGrid, m_camera, dt);
  }

  // 6. Effects
  m_vfxHotReloadAccumulator += dt;
  if (m_vfxHotReloadAccumulator >= 0.5f) {
    // Hot-reload path touches filesystem metadata; polling at 2 Hz is enough in gameplay.
    vfx::VFXSequenceManager::Get().TryHotReload();
    m_vfxHotReloadAccumulator = 0.0f;
  }
  vfx::VFXSequencerSystem::Update(registry, dt);
  systems::EffectSystem::update(registry, dt);
  systems::VisualFXSystem::Update(registry, dt);
  systems::TrailSystem::Update(registry, dt);
  systems::SwordIntentVisualSystem::Update(registry, dt);

  // 7. Physics (Taskflow)
  {
    NoMoreDay::utils::ScopedTimer timer("Physics Total", 200);
    UpdatePhysics(dt);
  }
  
  return true;

  return true;
}

void GameplayState::UpdatePhysics(float dt) {
  auto &registry = *m_context->registry;

  auto view = registry.view<Position, Velocity>();

  // [RECOVERY] Restore Force Fields logic (Phase 0 - Serial Pre-task)
  // This calculates skills like Singularity/Vortex that modify Velocity.
  PhysicsSystem::applyForceFields(registry, dt, m_spatialGrid);

  m_taskflow.clear();
  m_physicsEntities.clear();
  m_physicsEntities.reserve(view.size_hint());
  for (auto entity : view)
    m_physicsEntities.push_back(entity);

  const auto &map = m_context->levelManager->getMapSystem();
  using namespace NoMoreDay::Constants::World;
  int worldSizeW = map.getWidth() * (int)GRID_TILE_SIZE;
  int worldSizeH = map.getHeight() * (int)GRID_TILE_SIZE;
  // Fallback if map not ready
  if (worldSizeW == 0)
    worldSizeW = WORLD_WIDTH;
  if (worldSizeH == 0)
    worldSizeH = WORLD_HEIGHT;

  // Phase 1: Resolve Collisions
  auto resolveTask = m_taskflow.for_each(
      m_physicsEntities.begin(), m_physicsEntities.end(),
      [this, dt, &registry](entt::entity entity) {
        if (registry.any_of<DormantTag>(entity))
          return;

        const auto &pos = registry.get<Position>(entity);
        auto &vel = registry.get<Velocity>(entity);

        // CPU is now the authority for both Player and Enemies.
        bool isGpuManaged = registry.all_of<GPUIndex>(entity);
        bool isPlayer = registry.all_of<PlayerTag>(entity);
        bool isEnemy = registry.all_of<EnemyTag>(entity);

        // Only skip if it's neither player nor enemy (e.g. some other GPU
        // managed VFX)
        if (!isPlayer && !isEnemy)
          return;

        // Only resolve collisions for solid game entities (Players and Enemies)
        if (isPlayer || isEnemy) {
          PhysicsSystem::resolveCollisions(entity, pos, vel, m_spatialGrid,
                                           registry, dt);
        }

        // Map collision
        const auto &map = m_context->levelManager->getMapSystem();
        using namespace NoMoreDay::Constants::World;
        using namespace NoMoreDay::Constants::Physics;
        float radius = DEFAULT_ENTITY_RADIUS * MAP_COLLISION_RADIUS_FACTOR;
        if (registry.all_of<Radius>(entity))
          radius =
              registry.get<Radius>(entity).value * MAP_COLLISION_RADIUS_FACTOR;

        TilemapCollisionSystem::ResolveCollision(map, pos, vel, dt, radius);
      });

  // Phase 2: Update Positions
  auto updateTask = m_taskflow.for_each(
      m_physicsEntities.begin(), m_physicsEntities.end(),
      [dt, worldSizeW, worldSizeH, &registry, &map](entt::entity entity) {
        if (registry.any_of<DormantTag>(entity))
          return;

        // Player is handled in OnUpdate (Predictive).
        bool isPlayer = registry.all_of<PlayerTag>(entity);
        if (isPlayer)
          return;

        // Enemies AND Projectiles are now handled here on CPU.
        // We integrate anything with Velocity that isn't the player or dormant.

        auto &pos = registry.get<Position>(entity);
        auto &vel = registry.get<Velocity>(entity);
        const float speedZoneMul =
            map.getSpeedMultiplierAtWorld(pos.x, pos.y);
        if (speedZoneMul > 1.0f) {
          vel.vx *= speedZoneMul;
          vel.vy *= speedZoneMul;
        }

        PhysicsSystem::updatePosition(registry, entity, pos, vel, dt,
                                      worldSizeW, worldSizeH);
      });

  resolveTask.precede(updateTask);

  {
    m_context->executor->run(m_taskflow).wait();
  }
  return;
}

void GameplayState::OnRender() {
  NoMoreDay::utils::ScopedTimer totalTimer("Gameplay OnRender", 5000);
  auto &registry = *m_context->registry;
  const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(
      m_context->levelManager->getCurrentBiomeID());

  // 1. Render World to Texture (Level, Entities, Lighting, VFX via RenderSystem)
  BeginTextureMode(m_sceneRT);
  ClearBackground(BLACK);

  // Entities & Level & VFX via RenderSystem
  {
    NoMoreDay::utils::ScopedTimer timer("RenderSystem Total", 50);
    RenderSystem::render(*m_context->registry,
                       render::RenderFrameInput{m_context->resources,
                                                m_context->renderAlpha,
                                                m_context->renderContext,
                                                (m_context->settings != nullptr)
                                                    ? m_context->settings->cameraZoom
                                                    : 1.5f},
                       m_camera, m_context->gameplayRenderHooks);
  }

  // World Space Overlays (Health Bars, Indicators, Fog)
  {
    BeginMode2D(m_camera);

    // Monster Health Bars
    {
      NoMoreDay::utils::ScopedTimer timer("Render HealthBars", 10);
      m_uiHost->RenderMonsterHealthBars(registry, m_camera);
    }

    // Skill Range Indicators
    {
      NoMoreDay::utils::ScopedTimer timer("4.4 Render Indicators", 100);
      auto view_chan = registry.view<BeamChannelComponent, Position>();
      for (auto entity : view_chan) {
        auto &chan = view_chan.get<BeamChannelComponent>(entity);
        if (chan.skill_id == 7) { // Heart Sword: Shadowless
          auto &pos = view_chan.get<Position>(entity);
          // 射程基准与 Baker 共用机制表键 base_range，避免双源漂移；
          // TODO: 703 的 SKILL_RANGE_MULT 专精放大未接入，渲染圈未随节点放大。
          const float range = skills::GetMech(7, 0, "base_range", 350.0f);
          DrawCircleLines((int)pos.x, (int)pos.y, range, ColorAlpha(GOLD, 0.2f));
          DrawCircleLines((int)pos.x, (int)pos.y, range + 2.0f,
                          ColorAlpha(ORANGE, 0.15f)); // Thicker rim
        }
      }
    }

    // Fog
    {
      NoMoreDay::utils::ScopedTimer timer("Render Fog", 10);
      if (!biome.isSafeZone) {
        m_context->levelManager->getFogSystem().renderFog();
      }
    }

    // Ghost Snapshots
    NoMoreDay::systems::GhostSystem::Render(registry);

    EndMode2D();
  }

  EndTextureMode();

  // 2. Draw Scene Texture to Screen with Filter
  if (m_activeFilterShader.id != 0) {
    BeginShaderMode(m_activeFilterShader);
    
    // Set Uniforms
    float time = static_cast<float>(GetTime());
    Vector2 camOffset = m_camera.target;
    float zoom = m_camera.zoom;
    Vector2 screenSize = { static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) };
    
    if (m_filterLocTime != -1) SetShaderValue(m_activeFilterShader, m_filterLocTime, &time, SHADER_UNIFORM_FLOAT);
    if (m_filterLocCam != -1) SetShaderValue(m_activeFilterShader, m_filterLocCam, &camOffset, SHADER_UNIFORM_VEC2);
    if (m_filterLocZoom != -1) SetShaderValue(m_activeFilterShader, m_filterLocZoom, &zoom, SHADER_UNIFORM_FLOAT);
    if (m_filterLocScreen != -1) SetShaderValue(m_activeFilterShader, m_filterLocScreen, &screenSize, SHADER_UNIFORM_VEC2);

    if (m_filterLocPlayer != -1 || m_filterLocVision != -1) {
      auto pView = registry.view<PlayerTag, Position>();
      if (pView.begin() != pView.end()) {
        const auto &pos = pView.get<Position>(pView.front());
        Vector2 screenPlayer = NoMoreDay::render::coord::WorldToScenePixel(
            NoMoreDay::render::coord::Camera2DTransform::From(m_camera),
            {pos.x, pos.y});
        // Flip Y for screenPlayer because GL_FRAGCOORD is y-up
        screenPlayer.y = NoMoreDay::render::coord::NativeYToGl(
            screenPlayer.y, static_cast<float>(GetScreenHeight()));
        
        if (m_filterLocPlayer != -1) SetShaderValue(m_activeFilterShader, m_filterLocPlayer, &screenPlayer, SHADER_UNIFORM_VEC2);
        
        if (m_filterLocVision != -1) {
          float vRad = biome.visionRadius;
          if (vRad <= 0.0f) {
            auto *vis = registry.try_get<VisionComponent>(pView.front());
            vRad = vis ? vis->radius : 600.0f;
          }
          // Convert world radius to screen radius
          vRad *= zoom;
          SetShaderValue(m_activeFilterShader, m_filterLocVision, &vRad, SHADER_UNIFORM_FLOAT);
        }
      }
    }

    // Keep the world target scaled while presenting it at native resolution.
    // The scene RT blits to the window (flipY = false): the y-up texel
    // storage needs the negative-height source rect.
    const Rectangle sceneSource =
        NoMoreDay::render::coord::BlitSourceRect(
            false, static_cast<float>(m_sceneRT.texture.width),
            static_cast<float>(m_sceneRT.texture.height));
    const Rectangle sceneTarget = {0.0f, 0.0f,
                                   static_cast<float>(GetScreenWidth()),
                                   static_cast<float>(GetScreenHeight())};
    DrawTexturePro(m_sceneRT.texture, sceneSource, sceneTarget, {0.0f, 0.0f},
                   0.0f, WHITE);
    EndShaderMode();
  } else {
    // The scene RT blits to the window (flipY = false): the y-up texel
    // storage needs the negative-height source rect.
    const Rectangle sceneSource =
        NoMoreDay::render::coord::BlitSourceRect(
            false, static_cast<float>(m_sceneRT.texture.width),
            static_cast<float>(m_sceneRT.texture.height));
    const Rectangle sceneTarget = {0.0f, 0.0f,
                                   static_cast<float>(GetScreenWidth()),
                                   static_cast<float>(GetScreenHeight())};
    DrawTexturePro(m_sceneRT.texture, sceneSource, sceneTarget, {0.0f, 0.0f},
                   0.0f, WHITE);
  }

  // 3. Render UI (Directly to screen)
  // Manual Draw:
  {
      NoMoreDay::utils::ScopedTimer timer("Render Minimap", 10);
      // U7 group 1: minimap routes through the host controller (U8 final:
      // the legacy static panel and its null-host fallback are removed).
      m_uiHost->DrawMinimap(*m_context->levelManager, &m_spatialGrid);
  }
  
  // U8 final: the character panel render gate reads the hosted controller
  // instance visibility directly (the legacy State flag is gone).
  const bool characterVisible = m_uiHost->IsCharacterPanelVisible();
  if (characterVisible) {
    // U7 group 2: character panel routes through the host controller.
    m_uiHost->DrawCharacter(registry);
  }

  // Ground Interaction
  {
    NoMoreDay::utils::ScopedTimer timer("Render UISystem", 10);
    // U4/U8: the UI render path runs through the composition root: re-fit
    // the native viewport, run the hosted controller draw pass, then submit
    // the new draw list through the raylib backend. Frame position is
    // unchanged (after the scene composite, before EndDrawing).
    m_uiHost->PrepareRender();
    // R8: the registry parameter is gone — every panel is a
    // snapshot/intent/draw-list surface.
    m_uiHost->Draw(*m_context->levelManager, m_camera, &m_spatialGrid);
  }

  // Monster Target Widget (Top Center)
  // U7 group 2: routes through the host controller (U8 final).
  m_uiHost->RenderMonsterHealthBarsUI(registry);

  // Player HUD (Resource Bars)
  // U7 group 1: HUD routes through the host controller (U8 final).
  m_uiHost->DrawHud(registry);

  // Global UI Overlay (Dragging Phantom)
  // U7 group 6-B: the drag phantom + top-most tooltip pass routes through the
  // host controller. R8: the phantom and the tooltip paint from the drag
  // session + frame snapshot inside Draw (draw list), so this legacy call
  // position is a no-op kept for the call order.
  m_uiHost->DrawDraggingPhantom();

  // Cleanup Dragging if mouse released (Fallback if no inventory overlay is active)
  // U8 final: the cleanup clears the host-owned drag session (the panels own
  // their drag state through it). The guard string is locked by a tech test
  // (UITests.cpp [Tech] InventoryUI gameplay fallback).
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) &&
      !m_uiHost->IsInventoryVisible()) {
    m_uiHost->ClearDragSession();
  }

  // Scene Transition Overlay
  if (m_context->sceneManager) {
    NoMoreDay::utils::ScopedTimer timer("Render SceneOverlay", 50);
    m_context->sceneManager->RenderOverlay();
  }

  RenderRiftDialogs();

  // --- Map Affix Overlay (Tab Menu) ---
  if (IsKeyDown(KEY_TAB)) {
    NoMoreDay::utils::ScopedTimer timer("Render AffixOverlay", 50);
    RenderMapAffixOverlay();
  }
}



void GameplayState::UpdateSceneRT() {
  const auto sceneExtent = RenderSystem::GetRenderTargetExtent(
      GetScreenWidth(), GetScreenHeight());
  if (m_sceneRT.texture.width != sceneExtent.width ||
      m_sceneRT.texture.height != sceneExtent.height) {
    if (m_sceneRT.id != 0) {
      UnloadRenderTexture(m_sceneRT);
    }
    m_sceneRT = LoadRenderTexture(sceneExtent.width, sceneExtent.height);
    RenderSystem::NotifyRenderTargetResize();
    LOG_INFO("GameplayState: Resized Scene RenderTexture to {}x{} (scale={:.3f})",
             sceneExtent.width, sceneExtent.height, sceneExtent.scale);
  }
}

} // namespace NoMoreDay
