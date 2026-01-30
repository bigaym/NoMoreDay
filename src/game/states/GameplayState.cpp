#include "game/states/GameplayState.hpp"
#include "app/SharedContext.hpp"
#include "engine/render/RenderContext.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/MaterialBankComponent.hpp" // Added
#include "game/components/PlayerState.hpp"
#include "game/components/StashComponent.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/registry/GroupRegistry.hpp"
#include "game/states/InventoryState.hpp"
#include "game/states/MosaicEditorState.hpp"
#include "game/states/PauseState.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/MapSystem.hpp"    // Explicit include
#include "game/systems/world/PortalSystem.hpp" // Moved up
#include "game/systems/world/TilemapCollisionSystem.hpp"

// Utilities (formerly in PCH)
#include "core/utils/ScopedTimer.hpp"
#include "core/math/PhysicsUtils.hpp"
#include "game/components/EnemyComponent.hpp"

// Systems
#include "engine/input/InputSystem.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "game/components/WorldState.hpp"
#include "game/components/vfx/MotionTrailComponent.hpp"
#include "game/systems/ai/AISystem.hpp"
#include "game/systems/combat/CombatHistorySystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "game/systems/combat/EliteModifierSystem.hpp"
#include "game/systems/combat/HazardSystem.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/combat/RegenerationSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/VisualFXSystem.hpp"
#include "game/systems/combat/XPAwardingSystem.hpp"
#include "game/systems/item/DropSystem.hpp"
#include "game/systems/item/FragmentDropSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/ShadowSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/SummonSystem.hpp"
#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include "game/systems/ui/UICharacter.hpp"
#include "game/systems/ui/UIMinimap.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/vfx/GhostSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/MapAffixCalculator.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"
#include "game/systems/world/MovementStanceSystem.hpp"
#include "systems/SerializationSystem.hpp"

namespace NoMoreDay {

GameplayState::GameplayState(StateManager &stateManager, SharedContext &context,
                             RenderContext &renderContext)
    : IState(stateManager, context), m_renderContext(&renderContext) {}

void GameplayState::OnEnter() {
  LOG_INFO("Entering GameplayState...");

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

  // 2. Initialize Level
  m_context->levelManager->initialize(resourceManager, *m_context->registry);
  using namespace NoMoreDay::Constants::World;
  // Start in Town by default (as per user request: "将初始地图设置为平安镇")
  m_context->levelManager->loadNewLevel(NoMoreDay::BiomeID::Town,
                                        WORLD_WIDTH / 10, WORLD_HEIGHT / 10);

  // 3. Initialize Entities (Player)
  InitializeEntities();

  // 4. Initialize Camera
  m_camera.zoom = m_context->settings ? m_context->settings->cameraZoom : 1.5f;
  m_camera.offset = {(float)GetScreenWidth() / 2.0f,
                     (float)GetScreenHeight() / 2.0f};
  m_camera.rotation = 0.0f;
  m_camera.target = {(float)GetScreenWidth() / 2.0f,
                     (float)GetScreenHeight() / 2.0f}; // Will be updated

  // 5. Initialize Portal System
  if (m_context->sceneManager) {
    m_portalSystem = std::make_unique<PortalSystem>(*m_context->sceneManager);
  }
}

void GameplayState::InitializeEntities() {
  auto &registry = *m_context->registry;
  auto &resourceManager = *m_context->resources;
  const auto &playerAsset = assets::textures::Player_Warrior;

  auto player = registry.create();
  LOG_DEBUG("Created player entity with ID: {}", (uint32_t)player);

  using namespace NoMoreDay::Constants::World;
  float startX = (float)WORLD_WIDTH / 2.0f;
  float startY = (float)WORLD_HEIGHT / 2.0f;

  // Use Map System to find a safe spawn (STAIRS_UP or nearest walkable)
  const auto &map = m_context->levelManager->getMapSystem();
  bool spawnFound = false;
  for (int y = 0; y < map.getHeight(); y++) {
    for (int x = 0; x < map.getWidth(); x++) {
      if (map.getTileType(x, y) == Tile::Type::STAIRS_UP) {
        using namespace NoMoreDay::Constants::World;
        startX = x * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
        startY = y * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
        spawnFound = true;
        break;
      }
    }
    if (spawnFound)
      break;
  }

  if (!spawnFound) {
    int cx = (int)(startX / 10.0f);
    int cy = (int)(startY / 10.0f);
    for (int r = 0; r < 50 && !spawnFound; r++) {
      for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
          if (map.isWalkable(cx + dx, cy + dy)) {
            using namespace NoMoreDay::Constants::World;
            startX = (cx + dx) * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
            startY = (cy + dy) * GRID_TILE_SIZE + (GRID_TILE_SIZE * 0.5f);
            spawnFound = true;
            break;
          }
        }
        if (spawnFound)
          break;
      }
    }
  }

  registry.emplace<Position>(player, startX, startY);
  registry.emplace<IDComponent>(player, Utils::UUID::from("Player"));
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<Radius>(player, 5.0f);
  registry.emplace<GPUIndex>(player, -1);
  registry.emplace<PlayerTag>(player);
  registry.emplace<PersistentTag>(player);
  registry.emplace<InputComponent>(player);
  registry.emplace<PlayerLevel>(player);
  registry.emplace<PlayerStats>(player);
  registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
  registry.emplace<CombatStats>(player);
  registry.emplace<VisionComponent>(player, 600.0f);
  registry.emplace<StatsDirty>(player);
  registry.emplace<DashComponent>(player);
  registry.emplace<InventoryComponent>(player);
  registry.emplace<MaterialBankComponent>(player);
  {
    auto &stash = registry.emplace<PersonalStashComponent>(player);
    stash.unlockedTabs = 1;
    stash.tabs.resize(1);
    stash.tabs[0].name = "Stash 1";
  }
  registry.emplace<EquipmentComponent>(player);
  registry.emplace<AttackState>(player);
  using namespace NoMoreDay::Constants::Combat;
  registry.emplace<HealthComponent>(player, DEFAULT_MAX_HEALTH,
                                    DEFAULT_MAX_HEALTH);
  registry.emplace<TextureIDComponent>(player, playerAsset.id);
  registry.emplace<MovementStanceComponent>(player);
  registry.emplace<MovementAccumulator>(player);
  registry.emplace<PlayerCombatHistory>(player);

  // Set up Inventory

  // Astrolabe
  auto &astro = registry.emplace<AstrolabeComponent>(player);
  astro.available_points = 5; // Start with 5 points for testing

  // Test Equipment
  auto sword = ItemFactory::createWeapon(registry, 10, Rarity::Legendary);
  registry.emplace_or_replace<IDComponent>(sword, Utils::UUID::generate());
  registry.emplace<TextureIDComponent>(sword,
                                       assets::textures::Weapon_Sword.id);
  registry.emplace<PersistentTag>(sword); // Persist across scene transitions
  registry.get<EquipmentComponent>(player).set(EquipmentSlot::MainHand, sword);

  // Skill Setup
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = 1; // Q -> Flowing Thrust
  active.slots[4].id = 2; // RMB -> Rending Wave

  // Initialize charges
  for (auto &slot : active.slots) {
    if (slot.id != 0) {
      if (const auto *data = SkillRegistry::Get().GetSkill(slot.id)) {
        slot.current_charges = data->max_charges;
      }
    }
  }

  // IMPORTANT: Link skills to specialized slots so talents can be tracked
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[1].skill_id = 2;
  active.available_talent_points = 49; // Give 49 points for testing

  // Ensure some mana
  auto &stats = registry.get<CombatStats>(player);
  using namespace NoMoreDay::Constants::Combat;
  stats.mana = DEFAULT_MAX_MANA;
  stats.max_mana = DEFAULT_MAX_MANA;

  // Test Potions
  auto &inv = registry.get<InventoryComponent>(player);
  auto redPot = ItemFactory::createPotion(registry, 0, 10);
  registry.emplace_or_replace<IDComponent>(redPot, Utils::UUID::generate());
  registry.emplace<PersistentTag>(redPot); // Persist across scene transitions
  inv.items.push_back(redPot);
  auto bluePot = ItemFactory::createPotion(registry, 1, 10);
  registry.emplace_or_replace<IDComponent>(bluePot, Utils::UUID::generate());
  registry.emplace<PersistentTag>(bluePot); // Persist across scene transitions
  inv.items.push_back(bluePot);

  // --- Legendary Merging Test Items ---
  // 1. Catalyst
  auto core = ItemFactory::createMaterial(registry, 10001, 5);
  registry.emplace_or_replace<IDComponent>(core, Utils::UUID::generate());
  registry.emplace<PersistentTag>(core);
  inv.items.push_back(core);

  // 2. Base Item (Unique/Mythic with LP)
  auto baseFunc = ItemFactory::createWeapon(registry, 10, Rarity::Mythic);
  auto &baseItem = registry.get<ItemComponent>(baseFunc);
  baseItem.legendaryPotential = 2; // LP 2
  baseItem.name = "Blade of Testing (LP2)";
  registry.emplace_or_replace<IDComponent>(baseFunc, Utils::UUID::generate());
  registry.emplace<PersistentTag>(baseFunc);
  inv.items.push_back(baseFunc);

  // 3. Fodder Item (Exalted with 4 T6/T7)
  auto fodderFunc = ItemFactory::createWeapon(
      registry, 10,
      Rarity::Uncommon); // Exalted typically Uncommon base? Or Rare?
  auto &fodderItem = registry.get<ItemComponent>(fodderFunc);
  fodderItem.name = "Exalted Fodder";
  fodderItem.affixes.clear(); // Clear default
  // Add 4 high tier affixes
  for (int i = 0; i < 4; ++i) {
    Affix aff;
    aff.type = (AffixType)(i % 5); // Str, Dex, Int, Vit, FlatPhys
    aff.tier = (i < 2) ? 7 : 6;    // Two T7, Two T6
    aff.value = 50.0f;
    fodderItem.affixes.push_back(aff);
  }
  registry.emplace_or_replace<IDComponent>(fodderFunc, Utils::UUID::generate());
  registry.emplace<PersistentTag>(fodderFunc);
  inv.items.push_back(fodderFunc);

  // Texture
  Texture2D playerTexture =
      resourceManager.getTexture(playerAsset.id); // Should be loaded
  if (playerTexture.id > 0) {
    registry.emplace<SpriteComponent>(player, playerTexture, 0.4f);
  }
}

void GameplayState::OnExit() {
  LOG_INFO("Exiting GameplayState...");
  NoMoreDay::XPAwardingSystem::Reset(); // Cleanup static GC queue
  EliteModifierSystem::Shutdown();

  CombatHistorySystem::Shutdown();
  FragmentDropSystem::Shutdown();
  // Cleanup logic if needed.
  // Note: Game::cleanup will handle the global registry clear.
}

GameplayState::~GameplayState() {
  // Destructor defined here where PortalSystem is complete
}

bool GameplayState::OnUpdate(float dt) {
  auto &registry = *m_context->registry;

  // 0. Update SceneManager (Transitions)
  if (m_context->sceneManager) {
    m_context->sceneManager->Update(dt);
    if (m_context->sceneManager->IsTransitioning()) {
      return false; // Skip logic during transition (loading/fade)
    }
  }

  // 0. State Transition Input
  if (IsKeyPressed(KEY_I)) {
    m_stateManager->PushState<InventoryState>();
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    m_stateManager->PushState<PauseState>();
  }

  // Town Portal (KEY_T) - only when no major panel is open
  bool anyPanelOpen = UISystem::State.showSkillTree ||
                      UISystem::State.showInventory ||
                      UISystem::State.showCharacterPanel;
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
    FragmentDropSystem::Update(registry); // 处理碎片的延迟创建请求
    XPAwardingSystem::update(registry);
    InventorySystem::update(registry, dt);
    // Update Skill System
    ShadowSystem::Update(registry, dt);
    SkillSystem::Update(registry, m_spatialGrid, dt, m_context->executor);
    NoMoreDay::systems::SummonSystem::Update(registry, dt, m_spatialGrid);
    MovementStanceSystem::Update(registry, dt);
    ProjectileSystem::Update(registry, m_spatialGrid, dt);
    NoMoreDay::systems::GhostSystem::Update(registry, dt);
  }

  // 2. Input
  InputSystem::update(registry, m_camera);
  UISystem::State.isTyping =
      false; // Reset blocking flag for next frame's UI Render pass
  // Note: UISystem::Update was here in Game.cpp.
  // Now UISystem::Update handles "Global Keys" like 'C' for char panel.
  // We should probably keep it, but it might conflict with InventoryState if
  // logic is duplicated. InventoryState handles 'I'/'ESC' when it is active.
  // When GameplayState is active, we still want 'C' (Char Panel) to work (which
  // is an overlay in Gameplay?). If Char Panel is a state, we'd
  // PushState<CharPanelState>. Currently it's a bool flag in UISystem. Let's
  // call UISystem::Update here to handle those global flags. BUT we need to
  // ensure it doesn't handle 'I' if we already handled it. UISystem::Update
  // checks 'I'. We should refactor 'I' out of UISystem::Update or just let
  // UISystem::Update handle it? If UISystem::Update handles 'I' by toggling a
  // bool, it doesn't push a State. We want to Push State. So we should NOT call
  // UISystem::Update for 'I'. We should call `UIInventory::Update`? No. We need
  // to migrate the 'C' key logic too eventually. For now, let's Call
  // UISystem::Update but be aware it might try to toggle flags. Wait, if
  // UISystem::Update toggles `State.showInventory`, our `InventoryState` syncs
  // with it in `OnEnter`. So:
  // 1. User presses 'I'.
  // 2. UISystem::Update sees 'I', toggles `showInventory = true`.
  // 3. We check `State.showInventory`?
  // Better: We handle 'I' here and PushState.
  // We should probably BLOCK UISystem from handling 'I' or remove that logic
  // from UISystem. Since I haven't removed it from UISystem.cpp yet, it will
  // toggle the flag. If I PushState, InventoryState::OnEnter sets
  // `showInventory = true`. So it matches. BUT, if I press 'I' to CLOSE,
  // InventoryState handles it. So in GameplayState, 'I' means OPEN.

  // Logic conflict risk:
  // GameplayState calls UISystem::Update.
  // UISystem::Update: `if (Key_I) Toggle()`.
  // If I press I, UISystem sets show=true.
  // Then GameplayState sees Key_I (Input is processed once per frame usually).
  // If I PushState, then next frame InventoryState is active.
  // InventoryState::OnEnter ensures show=true.
  // Seems okay.

  // However, we want to transition fully to StateManager.
  // Let's keep UISystem::Update for 'C' and Debug keys for now.
  UISystem::Update(registry, *m_context->levelManager);

  // Check if UISystem opened inventory (via its own logic, e.g. if I didn't
  // intercept 'I' above or if Update ran first)
  if (UISystem::State.showInventory) {
    // If the flag is set but we are in GameplayState, it means we should be in
    // InventoryState. But wait, if we are here, we are GameplayState. If we
    // rely on UISystem::Update to toggle flag, we should detect flag change and
    // PushState? Or just disable UISystem's 'I' handling. For this step, I will
    // rely on my explicit check above `IsKeyPressed(KEY_I)` and PushState. And
    // ignore the duplicate handling for a moment, or hope `UISystem::State`
    // sync makes it visual only. Actually, if I PushState, the next frame
    // `GameplayState::OnUpdate` is NOT called (blocked). So it's fine.
  }

  // Serialization
  if (SerializationSystem::Update(registry)) {
    // Reload sprites logic
    auto view = registry.view<TextureIDComponent>();
    for (auto entity : view) {
      const auto &texComp = view.get<TextureIDComponent>(entity);
      Texture2D tex = m_context->resources->loadTexture(texComp.id, "");
      registry.emplace_or_replace<SpriteComponent>(entity, tex, 0.4f);
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

    if (input.dash && dash.charges > 0 && !dash.isDashing) {
      dash.charges--;
      dash.isDashing = true;
      dash.dashTimer = dash.dashDuration;

      float len =
          std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
      if (len > 0.1f) {
        dash.dirX = input.moveX / len;
        dash.dirY = input.moveY / len;
      } else {
        Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), m_camera);
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

      if (dash.dashTimer <= 0.0f) {
        dash.isDashing = false;
        vel.vx = 0;
        vel.vy = 0;

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
    }

    // Screen Shake
    RenderSystem::UpdateShake(dt);
    Vector2 shake = RenderSystem::GetShakeOffset();

    // Reset offset to center then apply shake
    m_camera.offset = {(float)GetScreenWidth() / 2.0f,
                       (float)GetScreenHeight() / 2.0f};
    m_camera.offset.x += shake.x;
    m_camera.offset.y += shake.y;
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
    CombatSystem::update(registry, m_spatialGrid, m_camera, dt);
  }

  // 6. Effects
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
      [dt, worldSizeW, worldSizeH, &registry](entt::entity entity) {
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
  auto &registry = *m_context->registry;

  BeginMode2D(m_camera);
  // Grid - REMOVED per user request (Dark background for void area)
  // Level
  {
    NoMoreDay::utils::ScopedTimer timer("Render Level", 20);
    m_context->levelManager->render(m_camera);
  }

  // Entities
  {
    NoMoreDay::utils::ScopedTimer timer("Render Entities", 20);
    RenderSystem::render(*m_context->registry, *m_context, m_camera);
  }

  // Monster Health Bars
  {
      NoMoreDay::utils::ScopedTimer timer("Render HealthBars", 10);
      systems::MonsterHealthBarSystem::Render(registry, m_camera);
  }

  // Skill Range Indicators
  {
      NoMoreDay::utils::ScopedTimer timer("4.4 Render Indicators", 100);
      auto view_chan = registry.view<ChannelingComponent, Position>();
      for (auto entity : view_chan) {
        auto &chan = view_chan.get<ChannelingComponent>(entity);
        if (chan.skill_id == 7) { // Heart Sword: Shadowless
          auto &pos = view_chan.get<Position>(entity);
          DrawCircleLines((int)pos.x, (int)pos.y, 350.0f, ColorAlpha(GOLD, 0.2f));
          DrawCircleLines((int)pos.x, (int)pos.y, 352.0f,
                          ColorAlpha(ORANGE, 0.15f)); // Thicker rim
        }
      }
  }

  // Portals
  if (m_portalSystem) {
    m_portalSystem->Render(registry, m_camera);
  }

  // Fog
  {
      NoMoreDay::utils::ScopedTimer timer("Render Fog", 10);
      m_context->levelManager->getFogSystem().renderFog();
  }

  // Ghost Snapshots
  NoMoreDay::systems::GhostSystem::Render(registry);

  EndMode2D();

  // Manual Draw:
  {
      NoMoreDay::utils::ScopedTimer timer("Render Minimap", 10);
      UIMinimap::Draw(registry, *m_context->levelManager, &m_spatialGrid);
  }
  
  if (UISystem::State.showCharacterPanel ||
      UISystem::State.characterPanelAlpha > 0.0f) {
    UICharacter::Draw(registry);
  }

  // Ground Interaction
  {
    NoMoreDay::utils::ScopedTimer timer("Render UISystem", 10);
    UISystem::Draw(registry, *m_context->levelManager, m_camera, &m_spatialGrid);
  }

  // Monster Target Widget (Top Center)
  systems::MonsterHealthBarSystem::RenderUI(registry);

  // Player HUD (Resource Bars)
  systems::PlayerHUD::Draw(registry);

  // Global UI Overlay (Dragging Phantom)
  UISystem::DrawDraggingPhantom(registry);

  // Cleanup Dragging if mouse released (Fallback if no other state active)
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    UISystem::State.draggedItem = entt::null;
    UISystem::State.isDraggingSkill = false;
    UISystem::State.draggedSkillId = 0;
  }

  // Scene Transition Overlay
  if (m_context->sceneManager) {
    m_context->sceneManager->RenderOverlay();
  }

  // --- Map Affix Overlay (Tab Menu) ---
  if (IsKeyDown(KEY_TAB)) {
    RenderMapAffixOverlay();
  }
}

void GameplayState::RenderMapAffixOverlay() {
  auto &registry = *m_context->registry;
  if (!registry.ctx().contains<NoMoreDay::ActiveDimensionalState>())
    return;

  const auto &state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
  if (!state.isActive)
    return;

  // Draw Background
  float width = 350.0f;
  float height = 450.0f;
  float x = (GetScreenWidth() - width) / 2.0f;
  float y = (GetScreenHeight() - height) / 2.0f;

  DrawRectangleRounded(Rectangle{x, y, width, height}, 0.05f, 8,
                       ColorAlpha(BLACK, 0.85f));
  DrawRectangleRoundedLinesEx(Rectangle{x, y, width, height}, 0.05f, 8, 2.0f,
                              DARKGRAY);

  Font font = UISystem::GetFont();
  UIRenderer::DrawTextUI(font, "当前维度概览 (Map Modifiers)", x + 20, y + 20,
                         20, GOLD, 1.0f);

  float ly = y + 60;
  char buf[128];

  // 0. Depth Info
  snprintf(buf, sizeof(buf), "当前深度 (Layer): %d / %d", state.currentDepth,
           state.maxDepth);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 18, SKYBLUE, 1.0f);
  ly += 25;

  // 1. Difficulty
  snprintf(buf, sizeof(buf), "难度系数 (DS): %d", state.difficultyScore);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 18, RED, 1.0f);
  ly += 25;

  // 1.1 Kill Counter
  snprintf(buf, sizeof(buf), "击杀计数 (Kills): %d", state.killCounter);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16, WHITE, 1.0f);
  ly += 25;

  // 2. Rewards
  snprintf(buf, sizeof(buf), "物品寻宝率 (Rarity): +%.0f%%",
           state.calculatedRarity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16,
                         components::Colors::RARITY_LEGENDARY, 1.0f);
  ly += 20;

  snprintf(buf, sizeof(buf), "物品数量 (Quantity): +%.0f%%",
           state.calculatedQuantity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16,
                         components::Colors::RARITY_EPIC, 1.0f);
  ly += 25;

  DrawLine(x + 20, ly, x + width - 20, ly, GRAY);
  ly += 15;

  UIRenderer::DrawTextUI(font, "鎸戞垬璇嶇紑 (Active Challenges):", x + 25, ly,
                         16, LIGHTGRAY, 1.0f);
  ly += 25;

  // 3. Cached Aggregated Affixes
  for (const auto &agg : state.aggregatedAffixes) {
    std::string desc =
        MapAffixRegistry::FormatDescription(agg.type, agg.totalValue);

    Color color = WHITE;
    if (agg.category == MapAffixCategory::Debuff)
      color = RED;
    else if (agg.category == MapAffixCategory::Buff)
      color = GREEN;

    UIRenderer::DrawTextUI(font, desc.c_str(), x + 30, ly, 14, color, 1.0f);
    ly += 18;

    if (ly > y + height - 30) {
      UIRenderer::DrawTextUI(font, "...", x + 30, ly, 14, GRAY, 1.0f);
      break;
    }
  }
}

} // namespace NoMoreDay
