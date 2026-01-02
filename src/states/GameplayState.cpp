#include "GameplayState.hpp"
#include "InventoryState.hpp"
#include "PauseState.hpp"
#include "../core/StateManager.hpp"
#include "../core/SharedContext.hpp"
#include "../core/ResourceManager.hpp"
#include "../core/LevelManager.hpp"
#include "../core/ItemFactory.hpp"
#include "../core/AssetRegistry.hpp"

// Systems
#include "../systems/PhysicsSystem.hpp"
#include "../systems/RenderSystem.hpp"
#include "../systems/InputSystem.hpp"
#include "../systems/CombatSystem.hpp"
#include "../systems/StatsSystem.hpp"
#include "../systems/AISystem.hpp"
#include "../systems/EffectSystem.hpp"
#include "../systems/UISystem.hpp"
#include "../systems/UIMinimap.hpp"
#include "../systems/UICharacter.hpp"
#include "../systems/DropSystem.hpp"
#include "../systems/XPAwardingSystem.hpp"
#include "../systems/InventorySystem.hpp"
#include "../systems/SkillSystem.hpp"
#include "../systems/ProjectileSystem.hpp"
#include "../systems/SerializationSystem.hpp"
#include "../systems/FogOfWarSystem.hpp"
#include "../systems/RegenerationSystem.hpp"

// Components
#include "../components/Common.hpp"
#include "../components/PlayerState.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/EquipmentComponent.hpp" // ADDED THIS LINE
#include "../components/Progression.hpp" // For AstrolabeComponent
#include "../components/Stats.hpp"
#include "../utils/UUID.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay {

    void GameplayState::OnEnter() {
        LOG_INFO("Entering GameplayState...");
        
        // Initialize Spatial Grid
        m_spatialGrid = systems::SpatialHashGrid(WorldConstants::GRID_COLS, WorldConstants::GRID_ROWS, WorldConstants::GRID_CELL_SIZE);

        // 1. Initialize Managers/Resources (if not already)
        // Note: Resources are managed by SharedContext->resources (owned by Game)
        // But we might need to load specific assets here if not preloaded.
        // Assuming Game preloads or we do it here.
        // Game::init logic for loading textures:
        auto& resourceManager = *m_context->resources;
        
        // Ensure UI System is ready
        // UISystem::Initialize(resourceManager); // Already called in Game::init? Let's assume Game calls it once.

        // Load Game Textures
        const auto& playerAsset = assets::textures::Player_Warrior;
        resourceManager.loadTexture(playerAsset.id, std::string(playerAsset.path));
        resourceManager.loadTexture(assets::textures::Weapon_Sword.id, std::string(assets::textures::Weapon_Sword.path));
        resourceManager.loadTexture(assets::textures::Skeleton.id, std::string(assets::textures::Skeleton.path));
        resourceManager.loadTexture(assets::textures::Cultist.id, std::string(assets::textures::Cultist.path));
        resourceManager.loadTexture(assets::textures::Demon.id, std::string(assets::textures::Demon.path));
        resourceManager.loadTexture(assets::textures::Corrupted_Beast.id, std::string(assets::textures::Corrupted_Beast.path));

        // 2. Initialize Level
        m_context->levelManager->initialize();
        m_context->levelManager->loadNewLevel("cave", WorldConstants::WORLD_WIDTH / 10, WorldConstants::WORLD_HEIGHT / 10);

        // 3. Initialize Entities (Player)
        InitializeEntities();

        // 4. Initialize Camera
        m_camera.zoom = 1.0f;
        m_camera.offset = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f };
        m_camera.rotation = 0.0f;
        m_camera.target = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f }; // Will be updated
    }

    void GameplayState::InitializeEntities() {
        auto& registry = *m_context->registry;
        auto& resourceManager = *m_context->resources;
        const auto& playerAsset = assets::textures::Player_Warrior;

        auto player = registry.create();
        LOG_DEBUG("Created player entity with ID: {}", (uint32_t)player);
        
        float startX = (float)GetScreenWidth() / 2.0f;
        float startY = (float)GetScreenHeight() / 2.0f;

        registry.emplace<Position>(player, startX, startY);
        registry.emplace<IDComponent>(player, Utils::UUID::from("Player"));
        registry.emplace<Velocity>(player, 0.0f, 0.0f);
        registry.emplace<PlayerTag>(player);
        registry.emplace<InputComponent>(player);
        registry.emplace<PlayerLevel>(player);
        registry.emplace<PlayerStats>(player);
        registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
        registry.emplace<CombatStats>(player);
        registry.emplace<VisionComponent>(player, 300.0f);
        registry.emplace<StatsDirty>(player);
        registry.emplace<DashComponent>(player);
        registry.emplace<InventoryComponent>(player);
        registry.emplace<EquipmentComponent>(player);
        registry.emplace<AttackState>(player);
        registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
        registry.emplace<TextureIDComponent>(player, playerAsset.id);
        
        // Astrolabe
        auto& astro = registry.emplace<AstrolabeComponent>(player);
        astro.available_points = 5; // Start with 5 points for testing

        // Test Equipment
        auto sword = ItemFactory::createWeapon(registry, 10, Rarity::Legendary);
        registry.emplace_or_replace<IDComponent>(sword, Utils::UUID::generate());
        registry.emplace<TextureIDComponent>(sword, assets::textures::Weapon_Sword.id);
        registry.get<EquipmentComponent>(player).set(EquipmentSlot::MainHand, sword);

        // Skill Setup
        auto& active = registry.emplace<ActiveSkillsComponent>(player);
        active.slots[0].id = 1; // Q -> Flowing Thrust
        active.slots[4].id = 2; // RMB -> Rending Wave
        
        // Ensure some mana
        auto& stats = registry.get<CombatStats>(player);
        stats.mana = 100.0f;
        stats.max_mana = 100.0f;

        // Test Potions
        auto& inv = registry.get<InventoryComponent>(player);
        auto redPot = ItemFactory::createPotion(registry, 0, 10);
        registry.emplace_or_replace<IDComponent>(redPot, Utils::UUID::generate());
        inv.items.push_back(redPot);
        auto bluePot = ItemFactory::createPotion(registry, 1, 10);
        registry.emplace_or_replace<IDComponent>(bluePot, Utils::UUID::generate());
        inv.items.push_back(bluePot);

        // Texture
        Texture2D playerTexture = resourceManager.getTexture(playerAsset.id); // Should be loaded
        if (playerTexture.id > 0) {
            registry.emplace<SpriteComponent>(player, playerTexture, 0.1f);
        }
    }

    void GameplayState::OnExit() {
        LOG_INFO("Exiting GameplayState...");
        // Cleanup logic if needed. 
        // Note: We might not want to clear registry if we just Pop to MainMenu but want to resume?
        // But typically Gameplay exit means Game Over or Return to Menu.
        // For now, Game cleanup handles global registry clear. 
        // Here we could clear entities if we want to ensure fresh state on re-enter.
        m_context->registry->clear();
    }

    bool GameplayState::OnUpdate(float dt) {
        auto& registry = *m_context->registry;
        
        // 0. State Transition Input
        if (IsKeyPressed(KEY_I)) {
            m_stateManager->PushState<InventoryState>();
        }
        
        if (IsKeyReleased(KEY_ESCAPE)) {
            m_stateManager->PushState<PauseState>();
        }

        // Get Player Pos
        Position playerPos{0, 0};
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            playerPos = playerView.get<Position>(playerView.front());
        }

        // 1. Level & Systems
        m_context->levelManager->update(dt, registry, playerPos);
        m_context->levelManager->getMapSystem().updateFlowField(playerPos);
        StatsSystem::update(registry);
        RegenerationSystem::update(registry, dt);
        DropSystem::update(registry, m_context->levelManager->getCurrentLevel());
        XPAwardingSystem::update(registry);
        InventorySystem::update(registry, dt);
        SkillSystem::Update(registry, dt);
        ProjectileSystem::Update(registry, m_spatialGrid, dt);
        
        // 2. Input
        InputSystem::update(registry, m_camera);
        // Note: UISystem::Update was here in Game.cpp. 
        // Now UISystem::Update handles "Global Keys" like 'C' for char panel.
        // We should probably keep it, but it might conflict with InventoryState if logic is duplicated.
        // InventoryState handles 'I'/'ESC' when it is active.
        // When GameplayState is active, we still want 'C' (Char Panel) to work (which is an overlay in Gameplay?).
        // If Char Panel is a state, we'd PushState<CharPanelState>.
        // Currently it's a bool flag in UISystem.
        // Let's call UISystem::Update here to handle those global flags.
        // BUT we need to ensure it doesn't handle 'I' if we already handled it.
        // UISystem::Update checks 'I'.
        // We should refactor 'I' out of UISystem::Update or just let UISystem::Update handle it?
        // If UISystem::Update handles 'I' by toggling a bool, it doesn't push a State.
        // We want to Push State.
        // So we should NOT call UISystem::Update for 'I'.
        // We should call `UIInventory::Update`? No.
        // We need to migrate the 'C' key logic too eventually.
        // For now, let's Call UISystem::Update but be aware it might try to toggle flags.
        // Wait, if UISystem::Update toggles `State.showInventory`, our `InventoryState` syncs with it in `OnEnter`.
        // So:
        // 1. User presses 'I'.
        // 2. UISystem::Update sees 'I', toggles `showInventory = true`.
        // 3. We check `State.showInventory`? 
        // Better: We handle 'I' here and PushState. 
        // We should probably BLOCK UISystem from handling 'I' or remove that logic from UISystem.
        // Since I haven't removed it from UISystem.cpp yet, it will toggle the flag.
        // If I PushState, InventoryState::OnEnter sets `showInventory = true`.
        // So it matches.
        // BUT, if I press 'I' to CLOSE, InventoryState handles it.
        // So in GameplayState, 'I' means OPEN.
        
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
        
        // Check if UISystem opened inventory (via its own logic, e.g. if I didn't intercept 'I' above or if Update ran first)
        if (UISystem::State.showInventory) {
             // If the flag is set but we are in GameplayState, it means we should be in InventoryState.
             // But wait, if we are here, we are GameplayState.
             // If we rely on UISystem::Update to toggle flag, we should detect flag change and PushState?
             // Or just disable UISystem's 'I' handling.
             // For this step, I will rely on my explicit check above `IsKeyPressed(KEY_I)` and PushState.
             // And ignore the duplicate handling for a moment, or hope `UISystem::State` sync makes it visual only.
             // Actually, if I PushState, the next frame `GameplayState::OnUpdate` is NOT called (blocked).
             // So it's fine.
        }

        // Serialization
        if (SerializationSystem::Update(registry)) {
             // Reload sprites logic
             auto view = registry.view<TextureIDComponent>();
             for (auto entity : view) {
                 const auto& texComp = view.get<TextureIDComponent>(entity);
                 Texture2D tex = m_context->resources->loadTexture(texComp.id, ""); 
                 registry.emplace_or_replace<SpriteComponent>(entity, tex, 0.1f);
             }
        }

        // 3. Player Movement & Dash (Moved from Game.cpp)
        auto playerView2 = registry.view<PlayerTag, InputComponent, Velocity, Position, DashComponent>();
        for (auto entity : playerView2) {
            auto& input = playerView2.get<InputComponent>(entity);
            auto& vel = playerView2.get<Velocity>(entity);
            const auto& pos = playerView2.get<Position>(entity);
            auto& dash = playerView2.get<DashComponent>(entity);
            
            if (IsKeyPressed(KEY_SPACE)) input.dash = true;

            float effectiveCooldown = dash.cooldownDuration;
            if (registry.all_of<CombatStats>(entity)) {
                float cdr = registry.get<CombatStats>(entity).cooldown_reduction;
                if (cdr > 0.75f) cdr = 0.75f;
                effectiveCooldown *= (1.0f - cdr);
            }

            if (dash.charges < dash.maxCharges) {
                dash.cooldownTimer -= dt;
                if (dash.cooldownTimer <= 0.0f) {
                    dash.charges++;
                    dash.uiFlash = true;
                    dash.uiFlashTimer = 0.2f;
                    if (dash.charges < dash.maxCharges) dash.cooldownTimer = effectiveCooldown;
                }
            }
            if (dash.uiFlash) {
                dash.uiFlashTimer -= dt;
                if (dash.uiFlashTimer <= 0.0f) dash.uiFlash = false;
            }

            if (input.dash && dash.charges > 0 && !dash.isDashing) {
                dash.charges--;
                dash.isDashing = true;
                dash.dashTimer = dash.dashDuration;
                
                float len = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
                if (len > 0.1f) {
                    dash.dirX = input.moveX / len;
                    dash.dirY = input.moveY / len;
                } else {
                    Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), m_camera);
                    float dx = mousePos.x - pos.x;
                    float dy = mousePos.y - pos.y;
                    float mLen = std::sqrt(dx*dx + dy*dy);
                    if (mLen > 0.1f) {
                        dash.dirX = dx / mLen;
                        dash.dirY = dy / mLen;
                    } else {
                        dash.dirX = 1.0f; dash.dirY = 0.0f;
                    }
                }
                
                if (dash.charges == dash.maxCharges - 1 && dash.cooldownTimer <= 0.0f) {
                    dash.cooldownTimer = effectiveCooldown;
                }
            }

            if (dash.isDashing) {
                dash.dashTimer -= dt;
                vel.vx = dash.dirX * dash.dashSpeed;
                vel.vy = dash.dirY * dash.dashSpeed;
                if (dash.dashTimer <= 0.0f) {
                    dash.isDashing = false;
                    vel.vx = 0; vel.vy = 0;
                }
            } else {
                float speed = 300.0f;
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
        }

        // Spatial Grid Rebuild
        auto gridView = registry.view<Position, Velocity>();
        m_spatialGrid.rebuild(gridView, registry);

        // 4. AI
        AISystem::update(registry, m_spatialGrid, m_context->levelManager->getMapSystem(), playerPos, dt);

        // 5. Combat
        CombatSystem::update(registry, m_spatialGrid, m_camera, dt);

        // 6. Effects
        EffectSystem::update(registry, dt);

        // 7. Physics (Taskflow)
        UpdatePhysics(dt);

        return true;
    }

    void GameplayState::UpdatePhysics(float dt) {
        auto& registry = *m_context->registry;
        auto view = registry.view<Position, Velocity>();
        
        m_taskflow.clear();
        m_physicsEntities.clear();
        m_physicsEntities.reserve(view.size_hint());
        for(auto entity : view) m_physicsEntities.push_back(entity);

        int worldSizeW = WorldConstants::WORLD_WIDTH;
        int worldSizeH = WorldConstants::WORLD_HEIGHT;

        // Phase 1: Resolve Collisions
        auto resolveTask = m_taskflow.for_each(m_physicsEntities.begin(), m_physicsEntities.end(),
            [this, dt, &registry](entt::entity entity) {
                const auto& pos = registry.get<Position>(entity);
                auto& vel = registry.get<Velocity>(entity);

                // Only resolve collisions for solid game entities (Players and Enemies)
                if (registry.any_of<PlayerTag, EnemyTag>(entity)) {
                    PhysicsSystem::resolveCollisions(entity, pos, vel, m_spatialGrid, registry, dt);
                }
                
                // Map Collision
                const auto& map = m_context->levelManager->getMapSystem();
                const float TILE_SIZE = 10.0f;
                const float RADIUS = 4.0f; 
                float nextX = pos.x + vel.vx * dt;
                int tileX_left = static_cast<int>((nextX - RADIUS) / TILE_SIZE);
                int tileX_right = static_cast<int>((nextX + RADIUS) / TILE_SIZE);
                int tileY_top = static_cast<int>((pos.y - RADIUS) / TILE_SIZE);
                int tileY_bottom = static_cast<int>((pos.y + RADIUS) / TILE_SIZE);

                if (!map.isWalkable(tileX_left, tileY_top) || !map.isWalkable(tileX_left, tileY_bottom) ||
                    !map.isWalkable(tileX_right, tileY_top) || !map.isWalkable(tileX_right, tileY_bottom)) {
                    vel.vx = 0; 
                }

                float nextY = pos.y + vel.vy * dt;
                int tileY_next_top = static_cast<int>((nextY - RADIUS) / TILE_SIZE);
                int tileY_next_bottom = static_cast<int>((nextY + RADIUS) / TILE_SIZE);
                int tileX_left_curr = static_cast<int>((pos.x - RADIUS) / TILE_SIZE);
                int tileX_right_curr = static_cast<int>((pos.x + RADIUS) / TILE_SIZE);

                if (!map.isWalkable(tileX_left_curr, tileY_next_top) || !map.isWalkable(tileX_right_curr, tileY_next_top) ||
                    !map.isWalkable(tileX_left_curr, tileY_next_bottom) || !map.isWalkable(tileX_right_curr, tileY_next_bottom)) {
                    vel.vy = 0; 
                }
            });

        // Phase 2: Update Positions
        auto updateTask = m_taskflow.for_each(m_physicsEntities.begin(), m_physicsEntities.end(),
            [this, dt, worldSizeW, worldSizeH, &registry](entt::entity entity) {
                auto& pos = registry.get<Position>(entity);
                auto& vel = registry.get<Velocity>(entity);
                PhysicsSystem::updatePosition(entity, pos, vel, dt, worldSizeW, worldSizeH);
            });

        resolveTask.precede(updateTask);

        m_context->executor->run(m_taskflow).wait();
        return;
    }

    void GameplayState::OnRender() {
        auto& registry = *m_context->registry;

        BeginMode2D(m_camera);
            // Grid
            const int gridSize = 100;
            const int worldWidth = WorldConstants::WORLD_WIDTH;
            const int worldHeight = WorldConstants::WORLD_HEIGHT;
            for(int x = 0; x <= worldWidth; x += gridSize) DrawLine(x, 0, x, worldHeight, LIGHTGRAY);
            for(int y = 0; y <= worldHeight; y += gridSize) DrawLine(0, y, worldWidth, y, LIGHTGRAY);

            // Level
            m_context->levelManager->render(m_camera);

            // Entities
            RenderSystem::render(registry);
            
            // Fog
            m_context->levelManager->getFogSystem().renderFog();
        EndMode2D();
        
        // UI
        // Note: UISystem::Draw draws EVERYTHING (Inventory, CharPanel, Minimap, Tooltip).
        // Since we are moving to State-based UI, we should be careful.
        // If InventoryState is Active (on top), it will draw Inventory.
        // GameplayState should draw HUD (Minimap, CharPanel if it's not a state yet, MessageBox).
        // But UISystem::Draw currently draws `if (State.showInventory) ...`.
        // If we split InventoryState, we should ideally NOT call UISystem::Draw for inventory.
        // But `UISystem::Draw` is all-or-nothing currently.
        
        // Strategy:
        // GameplayState calls `UISystem::Draw`.
        // If `State.showInventory` is false (because we are in GameplayState), it won't draw inventory.
        // Wait, if `InventoryState` is active, `GameplayState::OnRender` is called FIRST (Background), then `InventoryState::OnRender` (Overlay).
        // `GameplayState::OnRender` calls `UISystem::Draw`.
        // `InventoryState::OnRender` calls `UIInventory::Draw`.
        // If `UISystem::Draw` checks `showInventory`, and it's true... it draws Inventory.
        // So we get double draw?
        // YES.
        
        // Fix:
        // We need to modify `UISystem::Draw` to NOT draw Inventory, or ensure `showInventory` is false when GameplayState draws?
        // But `showInventory` IS true when InventoryState is active.
        // So `GameplayState` calling `UISystem::Draw` will draw inventory.
        
        // Temporary Fix:
        // In `GameplayState::OnRender`, we can assume we only want HUD.
        // But `UISystem::Draw` mixes HUD and Window logic.
        // We should call `UIMinimap::Draw` and `UICharacter::Draw` manually here?
        // And let `InventoryState` handle Inventory.
        
        // `UISystem::Draw` implementation:
        // 1. Draw Subsystems (Inv, Map, Char)
        // 2. Ground Interaction
        // 3. Global Overlays
        
        // If we are in GameplayState, we want Ground Interaction, Minimap, CharPanel (overlay), Tooltips.
        // If Inventory is open:
        // GameplayState draws (Background + Ground Interaction + Minimap).
        // InventoryState draws (Inventory + Tooltips).
        
        // Problem: Ground Interaction (hoveredItem) might conflict.
        // And Tooltips might be drawn twice.
        
        // Ideally, `GameplayState` should NOT draw `UISystem::Draw` blindly.
        // It should draw `GameplayHUD`.
        
        // For now, to avoid double draw issues, I will rely on `UISystem::Draw` checking flags.
        // But the flags are global.
        
        // Let's modify `UISystem::Draw` in `src/systems/UISystem.cpp` later to be more modular?
        // Or just call specific parts here.
        
        // Manual Draw:
        UIMinimap::Draw(registry, *m_context->levelManager);
        if (UISystem::State.showCharacterPanel) UICharacter::Draw(registry);
        
        // Ground Interaction
        // Copied logic from UISystem::Draw or call a helper?
        // I can leave `UISystem::Draw` to handle "Gameplay UI".
        // BUT `UISystem::Draw` checks `showInventory`.
        // I can temporarily hack: `bool prev = State.showInventory; State.showInventory = false; UISystem::Draw(...); State.showInventory = prev;`
        // This is ugly but works for the transition.
        
        bool wasInv = UISystem::State.showInventory;
        UISystem::State.showInventory = false;
        UISystem::Draw(registry, *m_context->levelManager, m_camera);
        UISystem::State.showInventory = wasInv;
        
        // Serialization UI
        SerializationSystem::DrawUI();
        
        DrawFPS(10, 10);
    }

}
