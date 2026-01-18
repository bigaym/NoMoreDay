#include "game/states/GameplayState.hpp"
#include "game/systems/world/PortalSystem.hpp" // Moved up
#include "app/SharedContext.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/scene/StateManager.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/MaterialBankComponent.hpp" // Added
#include "game/data/SkillRegistry.hpp"
#include "game/states/InventoryState.hpp"
#include "game/states/MosaicEditorState.hpp"
#include "game/states/PauseState.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/world/LevelManager.hpp"

// Systems
#include "engine/input/InputSystem.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "game/components/vfx/MotionTrailComponent.hpp"
#include "game/systems/ai/AISystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "game/systems/combat/EliteModifierSystem.hpp"
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
#include "game/systems/vfx/GhostSystem.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include "game/systems/ui/UICharacter.hpp"
#include "game/systems/ui/UIMinimap.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/MovementStanceSystem.hpp"
#include "systems/SerializationSystem.hpp"

namespace NoMoreDay {

void GameplayState::OnEnter() {
  LOG_INFO("Entering GameplayState...");

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
  FragmentDropSystem::Init();

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
  m_context->levelManager->loadNewLevel("cave", WORLD_WIDTH / 10,
                                        WORLD_HEIGHT / 10);

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
  registry.emplace<EquipmentComponent>(player);
  registry.emplace<AttackState>(player);
  using namespace NoMoreDay::Constants::Combat;
  registry.emplace<HealthComponent>(player, DEFAULT_MAX_HEALTH,
                                    DEFAULT_MAX_HEALTH);
  registry.emplace<TextureIDComponent>(player, playerAsset.id);
  registry.emplace<MovementStanceComponent>(player);
  registry.emplace<MovementAccumulator>(player);

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
    registry.emplace<SpriteComponent>(player, playerTexture, 0.2f);
  }
}

void GameplayState::OnExit() {
  LOG_INFO("Exiting GameplayState...");
  EliteModifierSystem::Shutdown();
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
  if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
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
  m_context->levelManager->update(dt, registry, playerPos);

  // Update Dormant Entities (Spec 2.3)
  const auto &map = m_context->levelManager->getMapSystem();
  m_context->levelManager->getEnemySpawnSystem().updateDormantEntities(
      registry, playerPos, map.getWidth(), map.getHeight());
  // m_context->levelManager->getMapSystem().updateFlowField(playerPos);

  // GPU Flow Field
  auto &flowSystem = NoMoreDay::systems::GPUFlowFieldSystem::Get();
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

    flowSystem.Update(map.getCostMap(), map.getWidth(), map.getHeight(),
                      {playerPos.x, playerPos.y}, {originX, originY});
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

  MovementStanceSystem::Update(registry, dt);
  StatsSystem::UpdateBuffs(registry, dt);
  StatsSystem::update(registry);
  RegenerationSystem::update(registry, dt);
  EliteModifierSystem::Update(registry, dt);
  MonsterAffixSystem::Update(registry, dt);
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

  // 2. Input
  InputSystem::update(registry, m_camera);
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
      registry.emplace_or_replace<SpriteComponent>(entity, tex, 0.2f);
    }
  }

  // 3. Player Movement & Dash (Moved from Game.cpp)
  auto playerView2 =
      registry
          .view<PlayerTag, InputComponent, Velocity, Position, DashComponent>();
  for (auto entity : playerView2) {
    auto &input = playerView2.get<InputComponent>(entity);
    auto &vel = playerView2.get<Velocity>(entity);
    const auto &pos = playerView2.get<Position>(entity);
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

  // Spatial Grid Rebuild (Exclude items/gold to keep AI/physics search fast)
  auto gridView = registry.view<Position>(entt::exclude<NoMoreDay::ItemComponent, GoldComponent>);
  m_spatialGrid.rebuild(gridView, registry);

  // 4. AI
  AISystem::update(registry, m_spatialGrid,
                   m_context->levelManager->getMapSystem(), playerPos, dt);

  // 5. Combat
  CombatSystem::update(registry, m_spatialGrid, m_camera, dt);

  // 6. Effects
  systems::EffectSystem::update(registry, dt);
  systems::VisualFXSystem::Update(registry, dt);
  systems::TrailSystem::Update(registry, dt);
  systems::SwordIntentVisualSystem::Update(registry, dt);

  // 7. Physics (Taskflow)
  UpdatePhysics(dt);

  return true;
}

void GameplayState::UpdatePhysics(float dt) {
  auto &registry = *m_context->registry;

  auto view = registry.view<Position, Velocity>();

  m_taskflow.clear();
  m_physicsEntities.clear();
  m_physicsEntities.reserve(view.size_hint());
  for (auto entity : view)
    m_physicsEntities.push_back(entity);

  using namespace NoMoreDay::Constants::World;
  int worldSizeW = WORLD_WIDTH;
  int worldSizeH = WORLD_HEIGHT;

  // Phase 1: Resolve Collisions
  auto resolveTask = m_taskflow.for_each(
      m_physicsEntities.begin(), m_physicsEntities.end(),
      [this, dt, &registry](entt::entity entity) {
        const auto &pos = registry.get<Position>(entity);
        auto &vel = registry.get<Velocity>(entity);

        // Only resolve collisions for solid game entities (Players and Enemies)
        if (registry.any_of<PlayerTag, EnemyTag>(entity)) {
          PhysicsSystem::resolveCollisions(entity, pos, vel, m_spatialGrid,
                                           registry, dt);
        }

        // Map collision
        const auto &map = m_context->levelManager->getMapSystem();
        using namespace NoMoreDay::Constants::World;
        using namespace NoMoreDay::Constants::Physics;
        const float TILE_SIZE = GRID_TILE_SIZE;
        const float RADIUS =
            DEFAULT_ENTITY_RADIUS *
            0.8f; // Using slightly smaller radius for map collision buffer

        // Horizontal collision
        if (std::abs(vel.vx) > EPSILON_VELOCITY) {
          float nextX = pos.x + vel.vx * dt;
          int tileX = static_cast<int>(
              (vel.vx > 0 ? nextX + RADIUS : nextX - RADIUS) / TILE_SIZE);
          int tileY_top = static_cast<int>((pos.y - RADIUS + 0.5f) / TILE_SIZE);
          int tileY_bottom =
              static_cast<int>((pos.y + RADIUS - 0.5f) / TILE_SIZE);

          if (!map.isWalkable(tileX, tileY_top) ||
              !map.isWalkable(tileX, tileY_bottom)) {
            vel.vx = 0;
          }
        }

        // Vertical collision
        if (std::abs(vel.vy) > EPSILON_VELOCITY) {
          float nextY = pos.y + vel.vy * dt;
          int tileY = static_cast<int>(
              (vel.vy > 0 ? nextY + RADIUS : nextY - RADIUS) / TILE_SIZE);
          int tileX_left =
              static_cast<int>((pos.x - RADIUS + 0.5f) / TILE_SIZE);
          int tileX_right =
              static_cast<int>((pos.x + RADIUS - 0.5f) / TILE_SIZE);

          if (!map.isWalkable(tileX_left, tileY) ||
              !map.isWalkable(tileX_right, tileY)) {
            vel.vy = 0;
          }
        }
      });

  // Phase 2: Update Positions
  auto updateTask = m_taskflow.for_each(
      m_physicsEntities.begin(), m_physicsEntities.end(),
      [dt, worldSizeW, worldSizeH, &registry](entt::entity entity) {
        auto &pos = registry.get<Position>(entity);
        auto &vel = registry.get<Velocity>(entity);

        // We update position here on CPU.
        // If GPUEntitySystem is active, it will SYNC BACK and overwrite these
        // values in the Game loop. This provides a safe fallback and consistent
        // state.
        PhysicsSystem::updatePosition(entity, pos, vel, dt, worldSizeW,
                                      worldSizeH);
      });

  resolveTask.precede(updateTask);

  m_context->executor->run(m_taskflow).wait();
  return;
}

void GameplayState::OnRender() {
  auto &registry = *m_context->registry;

  BeginMode2D(m_camera);
  // Grid
  const int gridSize = 100;
  using namespace NoMoreDay::Constants::World;
  const int worldWidth = WORLD_WIDTH;
  const int worldHeight = WORLD_HEIGHT;
  for (int x = 0; x <= worldWidth; x += gridSize)
    DrawLine(x, 0, x, worldHeight, LIGHTGRAY);
  for (int y = 0; y <= worldHeight; y += gridSize)
    DrawLine(0, y, worldWidth, y, LIGHTGRAY);

  // Level
  m_context->levelManager->render(m_camera);

  // Entities
  RenderSystem::render(*m_context->registry, *m_context, m_camera);

  // Monster Health Bars
  systems::MonsterHealthBarSystem::Render(registry, m_camera);

  // Skill Range Indicators
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

  // Portals
  if (m_portalSystem) {
    m_portalSystem->Render(registry, m_camera);
  }

  // Fog
  m_context->levelManager->getFogSystem().renderFog();
  
  // Ghost Snapshots
  NoMoreDay::systems::GhostSystem::Render(registry);
  
  EndMode2D();

  // UI
  // Note: UISystem::Draw draws EVERYTHING (Inventory, CharPanel, Minimap,
  // Tooltip). Since we are moving to State-based UI, we should be careful. If
  // InventoryState is Active (on top), it will draw Inventory. GameplayState
  // should draw HUD (Minimap, CharPanel if it's not a state yet, MessageBox).
  // But UISystem::Draw currently draws `if (State.showInventory) ...`.
  // If we split InventoryState, we should ideally NOT call UISystem::Draw for
  // inventory. But `UISystem::Draw` is all-or-nothing currently.

  // Strategy:
  // GameplayState calls `UISystem::Draw`.
  // If `State.showInventory` is false (because we are in GameplayState), it
  // won't draw inventory. Wait, if `InventoryState` is active,
  // `GameplayState::OnRender` is called FIRST (Background), then
  // `InventoryState::OnRender` (Overlay). `GameplayState::OnRender` calls
  // `UISystem::Draw`. `InventoryState::OnRender` calls `UIInventory::Draw`. If
  // `UISystem::Draw` checks `showInventory`, and it's true... it draws
  // Inventory. So we get double draw? YES.

  // Fix:
  // We need to modify `UISystem::Draw` to NOT draw Inventory, or ensure
  // `showInventory` is false when GameplayState draws? But `showInventory` IS
  // true when InventoryState is active. So `GameplayState` calling
  // `UISystem::Draw` will draw inventory.

  // Temporary Fix:
  // In `GameplayState::OnRender`, we can assume we only want HUD.
  // But `UISystem::Draw` mixes HUD and Window logic.
  // We should call `UIMinimap::Draw` and `UICharacter::Draw` manually here?
  // And let `InventoryState` handle Inventory.

  // `UISystem::Draw` implementation:
  // 1. Draw Subsystems (Inv, Map, Char)
  // 2. Ground Interaction
  // 3. Global Overlays

  // If we are in GameplayState, we want Ground Interaction, Minimap, CharPanel
  // (overlay), Tooltips. If Inventory is open: GameplayState draws (Background
  // + Ground Interaction + Minimap). InventoryState draws (Inventory +
  // Tooltips).

  // Problem: Ground Interaction (hoveredItem) might conflict.
  // And Tooltips might be drawn twice.

  // Ideally, `GameplayState` should NOT draw `UISystem::Draw` blindly.
  // It should draw `GameplayHUD`.

  // For now, to avoid double draw issues, I will rely on `UISystem::Draw`
  // checking flags. But the flags are global.

  // Let's modify `UISystem::Draw` in `src/systems/UISystem.cpp` later to be
  // more modular? Or just call specific parts here.

  // Manual Draw:
  UIMinimap::Draw(registry, *m_context->levelManager);
  if (UISystem::State.showCharacterPanel ||
      UISystem::State.characterPanelAlpha > 0.0f) {
    UICharacter::Draw(registry);
  }

  // Ground Interaction
  // Copied logic from UISystem::Draw or call a helper?
  // I can leave `UISystem::Draw` to handle "Gameplay UI".

  UISystem::Draw(registry, *m_context->levelManager, m_camera, &m_spatialGrid);

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
}

} // namespace NoMoreDay
