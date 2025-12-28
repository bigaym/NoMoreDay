#include "Game.hpp"
#include "../components/Common.hpp"
#include "../components/AIComponent.hpp"
#include "../systems/PhysicsSystem.hpp"
#include "../systems/RenderSystem.hpp"
#include "../systems/InputSystem.hpp"
#include "../systems/CombatSystem.hpp"
#include "../systems/StatsSystem.hpp"
#include "../systems/AISystem.hpp"
#include "../systems/EffectSystem.hpp"
#include "../systems/UISystem.hpp"
#include "../systems/DropSystem.hpp"
#include "../components/PlayerState.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/Stats.hpp"
#include "../utils/Parallel.hpp"
#include "../systems/FogOfWarSystem.hpp"
#include "../tools/Logger.hpp"
#include "AssetRegistry.hpp"
#include "LevelManager.hpp"
#include "ItemFactory.hpp"
#include <random>

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title),
      m_spatialGrid(WorldConstants::GRID_COLS, WorldConstants::GRID_ROWS, WorldConstants::GRID_CELL_SIZE),
      m_levelManager(std::make_unique<LevelManager>()) {
    
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    // Raylib Init
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    InitAudioDevice(); // Initialize Audio System
    SetTargetFPS(60);
    
    LOG_DEBUG("Game window initialized");
    init();
}

Game::~Game() {
    LOG_INFO("Shutting down Game...");
    cleanup();
    CloseAudioDevice(); // Close Audio System
    CloseWindow();
    LOG_INFO("Game shutdown completed");
}

void Game::init() {
    LOG_INFO("Initializing Game systems...");
    // 1. Initialize Managers
    m_levelManager->initialize();
    m_levelManager->loadNewLevel("cave", WorldConstants::WORLD_WIDTH / 10, WorldConstants::WORLD_HEIGHT / 10);
    
    NoMoreDay::ItemFactory::initialize();
    UISystem::Initialize(m_resourceManager);
    
    LOG_DEBUG("Managers initialized, loading player texture...");
    // 2. Load Resources via Manager (Compile-time IDs)
    const auto& playerAsset = assets::textures::Player_Warrior;
    Texture2D playerTexture = m_resourceManager.loadTexture(playerAsset.id, std::string(playerAsset.path));
    
    // 3. Spawn Player
    auto player = m_registry.create();
    LOG_DEBUG("Created player entity with ID: {}", (uint32_t)player);
    m_registry.emplace<Position>(player, (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f);
    m_registry.emplace<Velocity>(player, 0.0f, 0.0f);
    m_registry.emplace<PlayerTag>(player);
    m_registry.emplace<InputComponent>(player);
    m_registry.emplace<PlayerLevel>(player); // 初始化等级
    m_registry.emplace<PlayerStats>(player); // 初始化统计数据
    m_registry.emplace<NoMoreDay::PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f); // 初始基础属性 (Str, Dex, Int, Vit)
    m_registry.emplace<NoMoreDay::CombatStats>(player); // 初始战斗属性 (使用默认值)
    m_registry.emplace<VisionComponent>(player, 300.0f); // 初始视野半径
    m_registry.emplace<DashComponent>(player); // 冲刺组件
    m_registry.emplace<NoMoreDay::InventoryComponent>(player); // Init Inventory (default 40 slots)
    m_registry.emplace<NoMoreDay::EquipmentComponent>(player); // Init Equipment slots
    
    // Legacy Weapon Component (Kept for compatibility, but StatsSystem prefers Equipment)
    m_registry.emplace<WeaponComponent>(player, 10.0f, 60.0f, 0.15f, 1500.0f, 0.0f);
    m_registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

    LOG_DEBUG("Creating test equipment...");
    // --- TEST EQUIPMENT (Generated via Factory) ---
    // Create a Legendary Weapon for testing
    auto sword = NoMoreDay::ItemFactory::createWeapon(m_registry, 10, NoMoreDay::Rarity::Legendary);
    LOG_DEBUG("Created test weapon with entity ID: {}", (uint32_t)sword);
    
    // Equip it
    auto& equip = m_registry.get<NoMoreDay::EquipmentComponent>(player);
    equip.set(NoMoreDay::EquipmentSlot::MainHand, sword);
    LOG_DEBUG("Equipped weapon to player");
    // ----------------------

    if (playerTexture.id > 0) {
        // Character is big (1024x1024), scale down to ~100px
        m_registry.emplace<SpriteComponent>(player, playerTexture, 0.1f);
        LOG_DEBUG("Assigned sprite component to player");
    } else {
        LOG_ERROR("Failed to load player texture, ID: {}", playerTexture.id);
    }

    LOG_INFO("Taskflow initialized with {} workers", std::thread::hardware_concurrency());

    // 4. Init Camera
    m_camera = { 0 };
    m_camera.zoom = 1.0f;
    m_camera.offset = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
    m_camera.rotation = 0.0f;
    m_camera.target = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
    LOG_INFO("Game initialization completed");
}

void Game::update(float dt) {
    // LOG_TRACE("Game update started with dt: {}", dt);
    // Get player position for AI system
    Position playerPos{0, 0};
    auto playerView = m_registry.view<PlayerTag, Position>();
    auto playerBegin = playerView.begin();
    if (playerBegin != playerView.end()) {
        auto playerEntity = *playerBegin;
        const auto& pos = playerView.get<Position>(playerEntity);
        playerPos = pos;
    } else {
        LOG_WARN("No player entity found in registry during update");
    }

    // 1. Update Level Manager (Map, Enemies, Fog of War)
    m_levelManager->update(dt, m_registry, playerPos);
    
    // 更新流场 (Black Magic for Pathfinding)
    m_levelManager->getMapSystem().updateFlowField(playerPos);

    // 2. Stats System (Bake Attributes)
    // Must run before CombatSystem so we have fresh stats
    NoMoreDay::StatsSystem::update(m_registry);

    // DropSystem processes killed entities and generates loot BEFORE they are destroyed or XP awarded
    NoMoreDay::DropSystem::update(m_registry);

    // XPAwardingSystem processes killed entities and awards XP
    NoMoreDay::XPAwardingSystem::update(m_registry);

    // 3. Process Input
    InputSystem::update(m_registry);
    UISystem::Update(m_registry);
    
    // 3. Apply Player Input to Velocity & Update Camera
    auto playerView2 = m_registry.view<PlayerTag, InputComponent, Velocity, Position, DashComponent>();
    if (playerView2.begin() == playerView2.end()) {
        LOG_WARN("No player entity with required components found during update");
    }
    for (auto entity : playerView2) {
        auto& input = playerView2.get<InputComponent>(entity);
        auto& vel = playerView2.get<Velocity>(entity);
        const auto& pos = playerView2.get<Position>(entity);
        auto& dash = playerView2.get<DashComponent>(entity);
        
        // 确保 Space 键能触发冲刺 (如果 InputSystem 未处理)
        if (IsKeyPressed(KEY_SPACE)) input.dash = true;

        // --- Dash Cooldown & Charges ---
        if (dash.charges < dash.maxCharges) {
            dash.cooldownTimer -= dt;
            if (dash.cooldownTimer <= 0.0f) {
                dash.charges++;
                dash.uiFlash = true; // 触发UI闪烁
                dash.uiFlashTimer = 0.2f;
                LOG_DEBUG("Dash charge restored, current charges: {}", dash.charges);
                // 如果还没满，重置计时器
                if (dash.charges < dash.maxCharges) {
                    dash.cooldownTimer = dash.cooldownDuration;
                }
            }
        }
        // UI Flash Timer
        if (dash.uiFlash) {
            dash.uiFlashTimer -= dt;
            if (dash.uiFlashTimer <= 0.0f) dash.uiFlash = false;
        }

        // --- Dash Activation ---
        if (input.dash && dash.charges > 0 && !dash.isDashing) {
            dash.charges--;
            dash.isDashing = true;
            dash.dashTimer = dash.dashDuration;
            LOG_DEBUG("Dash activated, remaining charges: {}", dash.charges);
            
            // 确定冲刺方向：如果有输入则按输入方向，否则按鼠标方向
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
                    dash.dirX = 1.0f; dash.dirY = 0.0f; // 默认向右
                }
            }
            
            // 如果刚开始冷却（满充能使用了一次），启动计时器
            if (dash.charges == dash.maxCharges - 1 && dash.cooldownTimer <= 0.0f) {
                dash.cooldownTimer = dash.cooldownDuration;
            }
        }

        // --- Apply Velocity ---
        if (dash.isDashing) {
            dash.dashTimer -= dt;
            vel.vx = dash.dirX * dash.dashSpeed;
            vel.vy = dash.dirY * dash.dashSpeed;
            
            if (dash.dashTimer <= 0.0f) {
                dash.isDashing = false;
                vel.vx = 0; vel.vy = 0;
                LOG_DEBUG("Dash ended");
            }
        } else {
            // Normal Movement
            float speed = 300.0f;
            vel.vx = input.moveX * speed;
            vel.vy = input.moveY * speed;
        }

        // Camera Follow (Smooth Lerp)
        float lerpSpeed = 5.0f;
        m_camera.target.x += (pos.x - m_camera.target.x) * lerpSpeed * dt;
        m_camera.target.y += (pos.y - m_camera.target.y) * lerpSpeed * dt;
    }

    // 4. AI System (Update enemy behaviors)
    AISystem::update(m_registry, m_spatialGrid, m_levelManager->getMapSystem(), playerPos, dt);

    // 5. Combat System (Process Attacks)
    // Note: We run this BEFORE rebuilding the grid, because if entities die, we want them gone.
    // However, Combat needs the Grid to find targets. So it uses the Grid from LAST frame.
    // This is acceptable latency (16ms).
    CombatSystem::update(m_registry, m_spatialGrid, m_camera, dt);

    // 6. Effect System (Update visual effects)
    EffectSystem::update(m_registry, dt);

    // 7. Rebuild Spatial Grid
    auto view = m_registry.view<Position, Velocity>();
    m_spatialGrid.rebuild(view, m_registry);

    // 8. Parallel Physics Execution
    m_taskflow.clear();
    
    // Collect entities (reuse vector)
    m_physicsEntities.clear();
    m_physicsEntities.reserve(view.size_hint());
    for(auto entity : view) m_physicsEntities.push_back(entity);

    // Use a larger world boundary (e.g., 5000x5000) so entities don't bounce at screen edges
    int worldSizeW = WorldConstants::WORLD_WIDTH;
    int worldSizeH = WorldConstants::WORLD_HEIGHT;

    // Phase 1: Resolve Collisions (Read Pos, Write Vel)
    auto resolveTask = m_taskflow.for_each(m_physicsEntities.begin(), m_physicsEntities.end(),
        [this, dt](entt::entity entity) {
            const auto& pos = m_registry.get<Position>(entity);
            auto& vel = m_registry.get<Velocity>(entity);
            PhysicsSystem::resolveCollisions(entity, pos, vel, m_spatialGrid, m_registry, dt);
            
            // Map Collision (Obstacles) - Absorb kinetic energy (no bounce)
            if (m_levelManager) {
                const auto& map = m_levelManager->getMapSystem();
                const float TILE_SIZE = 10.0f;
                const float RADIUS = 4.0f; // Small collision radius for navigation

                // 1. X-Axis Collision Prediction
                float nextX = pos.x + vel.vx * dt;
                int tileX_left = static_cast<int>((nextX - RADIUS) / TILE_SIZE);
                int tileX_right = static_cast<int>((nextX + RADIUS) / TILE_SIZE);
                int tileY_top = static_cast<int>((pos.y - RADIUS) / TILE_SIZE);
                int tileY_bottom = static_cast<int>((pos.y + RADIUS) / TILE_SIZE);

                if (!map.isWalkable(tileX_left, tileY_top) || !map.isWalkable(tileX_left, tileY_bottom) ||
                    !map.isWalkable(tileX_right, tileY_top) || !map.isWalkable(tileX_right, tileY_bottom)) {
                    vel.vx = 0; // Stop X movement
                    // LOG_TRACE("Collision detected, stopping X movement for entity: {}", (uint32_t)entity); // 频率过高
                }

                // 2. Y-Axis Collision Prediction
                float nextY = pos.y + vel.vy * dt;
                int tileY_next_top = static_cast<int>((nextY - RADIUS) / TILE_SIZE);
                int tileY_next_bottom = static_cast<int>((nextY + RADIUS) / TILE_SIZE);
                int tileX_left_curr = static_cast<int>((pos.x - RADIUS) / TILE_SIZE);
                int tileX_right_curr = static_cast<int>((pos.x + RADIUS) / TILE_SIZE);

                if (!map.isWalkable(tileX_left_curr, tileY_next_top) || !map.isWalkable(tileX_right_curr, tileY_next_top) ||
                    !map.isWalkable(tileX_left_curr, tileY_next_bottom) || !map.isWalkable(tileX_right_curr, tileY_next_bottom)) {
                    vel.vy = 0; // Stop Y movement
                    // LOG_TRACE("Collision detected, stopping Y movement for entity: {}", (uint32_t)entity);  // 频率过高
                }
            }
        });

    // Phase 2: Update Positions (Read Vel, Write Pos)
    auto updateTask = m_taskflow.for_each(m_physicsEntities.begin(), m_physicsEntities.end(),
        [this, dt, worldSizeW, worldSizeH](entt::entity entity) {
            auto& pos = m_registry.get<Position>(entity);
            auto& vel = m_registry.get<Velocity>(entity);
            PhysicsSystem::updatePosition(entity, pos, vel, dt, worldSizeW, worldSizeH);
        });

    // Ensure Phase 1 completes before Phase 2
    resolveTask.precede(updateTask);

    m_executor.run(m_taskflow).wait();
    // LOG_TRACE("Game update completed");
}

void Game::render() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    BeginMode2D(m_camera);
        // Draw World Grid for visual reference
        const int gridSize = 100;
        const int worldWidth = WorldConstants::WORLD_WIDTH;
        const int worldHeight = WorldConstants::WORLD_HEIGHT;
        for(int x = 0; x <= worldWidth; x += gridSize) DrawLine(x, 0, x, worldHeight, LIGHTGRAY);
        for(int y = 0; y <= worldHeight; y += gridSize) DrawLine(0, y, worldWidth, y, LIGHTGRAY);

        // 1. Render Map (Floor/Walls)
        m_levelManager->render(m_camera);

        // 2. Render Entities (Player, Enemies)
        RenderSystem::render(m_registry);
        
        // 3. Render Fog of War (Overlay on top of everything)
        m_levelManager->getFogSystem().renderFog();
    EndMode2D();
    
    // Render UI (Screen Space)
    // Draw Character Panel (C key)
    UISystem::Draw(m_registry, *m_levelManager);
    
    DrawFPS(10, 10);
    // DrawText("WASD to Move", 10, 40, 20, DARKGRAY);
    
    EndDrawing();
}

void Game::run() {
    LOG_INFO("Starting Game Loop...");
    
    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        // LOG_TRACE("Frame time: {}", frameTime);
        // Prevent "Spiral of Death" if frame time is too long (e.g. debugging break)
        if (frameTime > 0.25f) {
            frameTime = 0.25f;
            LOG_WARN("Frame time exceeded 250ms, clamping to 0.25f");
        }

        accumulator += frameTime;

        while (accumulator >= fixedDt) {
            update(fixedDt);
            accumulator -= fixedDt;
        }

        render();
    }
    LOG_INFO("Game loop ended, window closed");
}

void Game::cleanup() {
    LOG_INFO("Starting Game cleanup...");
    UISystem::Shutdown();
    m_levelManager->cleanup();
    m_resourceManager.unloadAll();
    m_registry.clear();
    LOG_INFO("Game cleanup completed");
}