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
#include "../systems/SerializationSystem.hpp"
#include "../systems/InventorySystem.hpp"
#include "../components/Stats.hpp"
#include "../utils/Parallel.hpp"
#include "../systems/FogOfWarSystem.hpp"
#include "../tools/Logger.hpp"
#include "AssetRegistry.hpp"
#include "LevelManager.hpp"
#include "ItemFactory.hpp"
#include "../utils/UUID.hpp"
#include <random>

Game::Game(int width, int height, const char* title)
    : m_screenWidth(width), m_screenHeight(height), m_title(title),
      m_spatialGrid(WorldConstants::GRID_COLS, WorldConstants::GRID_ROWS, WorldConstants::GRID_CELL_SIZE),
      m_levelManager(std::make_unique<LevelManager>()) {
    
    system("chcp 65001 > nul"); // 设置控制台为 UTF-8 编码
    // 游戏初始化，设置窗口尺寸和标题
    LOG_INFO("Initializing Game with dimensions: {}x{}, title: {}", width, height, title);
    // Raylib Init
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    InitAudioDevice(); // Initialize Audio System
    SetExitKey(0); // 禁用 ESC 键退出游戏，由 InputSystem/UISystem 处理
    SetTargetFPS(60);
    
    LOG_DEBUG("Game window initialized");
    init();
}

Game::~Game() {
    LOG_INFO("Shutting down Game...");
    cleanup(); // 清理游戏资源
    CloseAudioDevice(); // Close Audio System
    CloseWindow();
    LOG_INFO("Game shutdown completed");
}

void Game::init() {
    LOG_INFO("Initializing Game systems...");
    // 1. 初始化管理器
    m_levelManager->initialize();
    m_levelManager->loadNewLevel("cave", WorldConstants::WORLD_WIDTH / 10, WorldConstants::WORLD_HEIGHT / 10);
    // 初始化物品工厂和UI系统
    NoMoreDay::ItemFactory::initialize();
    UISystem::Initialize(m_resourceManager);
    
    LOG_DEBUG("Managers initialized, loading player texture...");
    // 2. Load Resources via Manager (Compile-time IDs)
    const auto& playerAsset = assets::textures::Player_Warrior;
    Texture2D playerTexture = m_resourceManager.loadTexture(playerAsset.id, std::string(playerAsset.path));
    // 加载其他必要纹理
    // Load other essential textures
    m_resourceManager.loadTexture(assets::textures::Weapon_Sword.id, std::string(assets::textures::Weapon_Sword.path));
    m_resourceManager.loadTexture(assets::textures::Skeleton.id, std::string(assets::textures::Skeleton.path));
    m_resourceManager.loadTexture(assets::textures::Cultist.id, std::string(assets::textures::Cultist.path));
    m_resourceManager.loadTexture(assets::textures::Demon.id, std::string(assets::textures::Demon.path));
    m_resourceManager.loadTexture(assets::textures::Corrupted_Beast.id, std::string(assets::textures::Corrupted_Beast.path));

    // 3. 生成玩家
    auto player = m_registry.create();
    LOG_DEBUG("Created player entity with ID: {}", (uint32_t)player);
    m_registry.emplace<Position>(player, (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f);
    m_registry.emplace<IDComponent>(player, NoMoreDay::Utils::UUID::from("Player")); // 使用确定性 UUID
    m_registry.emplace<Velocity>(player, 0.0f, 0.0f);
    m_registry.emplace<PlayerTag>(player);
    m_registry.emplace<InputComponent>(player);
    m_registry.emplace<PlayerLevel>(player); // 初始化等级
    m_registry.emplace<PlayerStats>(player); // 初始化统计数据
    m_registry.emplace<NoMoreDay::PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f); // 初始基础属性 (Str, Dex, Int, Vit)
    m_registry.emplace<NoMoreDay::CombatStats>(player); // 初始战斗属性 (使用默认值)
    m_registry.emplace<VisionComponent>(player, 300.0f); // 初始视野半径
    m_registry.emplace<NoMoreDay::StatsDirty>(player); // 初始标记为脏，强制第一帧烘焙
    m_registry.emplace<DashComponent>(player); // 冲刺组件
    m_registry.emplace<NoMoreDay::InventoryComponent>(player); // 初始化背包 (默认40格)
    m_registry.emplace<NoMoreDay::EquipmentComponent>(player); // 初始化装备槽
    m_registry.emplace<NoMoreDay::AttackState>(player); // 新的攻击状态组件

    m_registry.emplace<HealthComponent>(player, 100.0f, 100.0f);
    m_registry.emplace<TextureIDComponent>(player, playerAsset.id); // 记录纹理ID以便存档恢复

    LOG_DEBUG("Creating test equipment...");
    // --- TEST EQUIPMENT (Generated via Factory) ---
    // Create a Legendary Weapon for testing
    auto sword = NoMoreDay::ItemFactory::createWeapon(m_registry, 10, NoMoreDay::Rarity::Legendary);
    m_registry.emplace_or_replace<IDComponent>(sword, NoMoreDay::Utils::UUID::generate()); // 随机 UUID
    m_registry.emplace<TextureIDComponent>(sword, assets::textures::Weapon_Sword.id); // 添加纹理ID以支持存档恢复
    LOG_DEBUG("Created test weapon with entity ID: {}", (uint32_t)sword);
    // 装备它
    auto& equip = m_registry.get<NoMoreDay::EquipmentComponent>(player);
    equip.set(NoMoreDay::EquipmentSlot::MainHand, sword);
    LOG_DEBUG("Equipped weapon to player");
    // ----------------------

    // --- 初始药水验证 ---
    auto& inv = m_registry.get<NoMoreDay::InventoryComponent>(player);
    auto redPot = NoMoreDay::ItemFactory::createPotion(m_registry, 0, 10); // 10个红药水
    m_registry.emplace_or_replace<IDComponent>(redPot, NoMoreDay::Utils::UUID::generate()); // 随机 UUID
    // 注意：这里假设药水也有对应的 TextureID，如果 ItemFactory 没有自动添加，需要在此处添加
    // m_registry.emplace<TextureIDComponent>(redPot, assets::textures::Potion_Red.id); 
    inv.items.push_back(redPot);
    
    auto bluePot = NoMoreDay::ItemFactory::createPotion(m_registry, 1, 10); // 10个蓝药水
    m_registry.emplace_or_replace<IDComponent>(bluePot, NoMoreDay::Utils::UUID::generate()); // 随机 UUID
    // m_registry.emplace<TextureIDComponent>(bluePot, assets::textures::Potion_Blue.id);
    inv.items.push_back(bluePot);
    LOG_DEBUG("Added 10 Red Potions and 10 Blue Potions to inventory");

    if (playerTexture.id > 0) {
        // 角色纹理较大 (1024x1024)，缩小到约100像素
        m_registry.emplace<SpriteComponent>(player, playerTexture, 0.1f);
        LOG_DEBUG("Assigned sprite component to player");
    } else {
        LOG_ERROR("Failed to load player texture, ID: {}", playerTexture.id);
    }

    LOG_INFO("Taskflow initialized with {} workers", std::thread::hardware_concurrency());
    // 4. 初始化相机
    m_camera = { 0 };
    m_camera.zoom = 1.0f;
    m_camera.offset = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
    m_camera.rotation = 0.0f;
    m_camera.target = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
    LOG_INFO("Game initialization completed");
}

void Game::update(float dt) {
    // LOG_TRACE("Game update started with dt: {}", dt);
    // 获取玩家位置供AI系统使用
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

    // 1. 更新关卡管理器 (地图, 敌人, 战争迷雾)
    m_levelManager->update(dt, m_registry, playerPos);
    
    // 更新流场 (寻路黑科技)
    m_levelManager->getMapSystem().updateFlowField(playerPos);

    // 2. 属性系统 (烘焙属性)
    // 必须在战斗系统之前运行，以确保属性是最新的
    NoMoreDay::StatsSystem::update(m_registry);

    // 掉落系统在实体被销毁或获得经验之前处理被击杀的实体并生成掉落物
    NoMoreDay::DropSystem::update(m_registry);

    // 经验奖励系统处理被击杀的实体并奖励经验
    NoMoreDay::XPAwardingSystem::update(m_registry);

    // 物品/金币吸附系统
    InventorySystem::update(m_registry, dt);

    // 3. Process Input
    InputSystem::update(m_registry);
    UISystem::Update(m_registry, *m_levelManager);
    
    // 处理存档/读档，并检查是否刚刚完成了读档
    if (SerializationSystem::Update(m_registry)) {
        LOG_INFO("Save loaded, restoring visual assets...");
        // 读档后修复：遍历所有带有 TextureID 的实体，重新加载 SpriteComponent
        auto view = m_registry.view<TextureIDComponent>();
        for (auto entity : view) {
            const auto& texComp = view.get<TextureIDComponent>(entity);
            // 假设 ResourceManager 缓存了纹理，可以直接通过 ID 获取
            // 注意：这里假设 loadTexture 对于已存在的 ID 会直接返回缓存的纹理
            // 如果 ResourceManager 不支持空路径重载，你可能需要扩展它或使用 getTexture(id)
            Texture2D tex = m_resourceManager.loadTexture(texComp.id, ""); 
            m_registry.emplace_or_replace<SpriteComponent>(entity, tex, 0.1f); // 注意：缩放比例可能需要保存，这里暂时硬编码
        }
        // 强制刷新一次空间网格
    }
    
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
        if (IsKeyPressed(KEY_SPACE)) input.dash = true; // 如果按下空格键，则冲刺

        // 计算有效冷却时间 (应用冷却缩减)
        float effectiveCooldown = dash.cooldownDuration;
        if (m_registry.all_of<NoMoreDay::CombatStats>(entity)) {
            float cdr = m_registry.get<NoMoreDay::CombatStats>(entity).cooldown_reduction;
            if (cdr > 0.75f) cdr = 0.75f; // Cap at 75%
            effectiveCooldown *= (1.0f - cdr);
        }

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
                    dash.cooldownTimer = effectiveCooldown;
                }
            }
        }
        // UI闪烁计时器
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
            
            // 确定冲刺方向：如果有输入则按输入方向，否则按鼠标方向。
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
            
            // 如果刚开始冷却（满充能使用了一次），则启动计时器
            if (dash.charges == dash.maxCharges - 1 && dash.cooldownTimer <= 0.0f) {
                dash.cooldownTimer = effectiveCooldown;
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
            // 普通移动
            float speed = 300.0f;
            if (m_registry.all_of<NoMoreDay::CombatStats>(entity)) {
                speed = m_registry.get<NoMoreDay::CombatStats>(entity).move_speed;
            }
            vel.vx = input.moveX * speed;
            vel.vy = input.moveY * speed;
        }

        // 摄像机跟随 (平滑插值)
        float lerpSpeed = 5.0f;
        m_camera.target.x += (pos.x - m_camera.target.x) * lerpSpeed * dt;
        m_camera.target.y += (pos.y - m_camera.target.y) * lerpSpeed * dt;
    }

    // 提前重建空间网格，确保 AI 和战斗系统使用当前帧的位置
    auto gridView = m_registry.view<Position, Velocity>();
    m_spatialGrid.rebuild(gridView, m_registry);

    // 4. AI系统 (更新敌人行为)
    AISystem::update(m_registry, m_spatialGrid, m_levelManager->getMapSystem(), playerPos, dt);

    // 5. 战斗系统 (处理攻击)
    // 注意：我们在此之前重建网格，因为如果实体死亡，我们希望它们消失。
    // 然而，战斗系统需要网格来寻找目标。因此它使用上一帧的网格。
    // 这种延迟 (16ms) 是可以接受的。
    CombatSystem::update(m_registry, m_spatialGrid, m_camera, dt);

    // 6. 特效系统 (更新视觉效果)
    EffectSystem::update(m_registry, dt);

    // 8. Parallel Physics Execution
    auto view = m_registry.view<Position, Velocity>();
    m_taskflow.clear(); // 清除任务流
    
    // 收集实体 (复用向量)
    m_physicsEntities.clear();
    m_physicsEntities.reserve(view.size_hint());
    for(auto entity : view) m_physicsEntities.push_back(entity);

    // 使用更大的世界边界 (例如，5000x5000)，这样实体就不会在屏幕边缘反弹
    int worldSizeW = WorldConstants::WORLD_WIDTH;
    int worldSizeH = WorldConstants::WORLD_HEIGHT;

    // 阶段1: 解决碰撞 (读取位置，写入速度)
    auto resolveTask = m_taskflow.for_each(m_physicsEntities.begin(), m_physicsEntities.end(),
        [this, dt](entt::entity entity) {
            const auto& pos = m_registry.get<Position>(entity);
            auto& vel = m_registry.get<Velocity>(entity);
            PhysicsSystem::resolveCollisions(entity, pos, vel, m_spatialGrid, m_registry, dt);
            
            // Map Collision (Obstacles) - Absorb kinetic energy (no bounce)
            // 地图碰撞 (障碍物) - 吸收动能 (无反弹)
            if (m_levelManager) {
                const auto& map = m_levelManager->getMapSystem();
                const float TILE_SIZE = 10.0f;
                const float RADIUS = 4.0f; // 用于导航的小碰撞半径
                // 1. X-Axis Collision Prediction
                float nextX = pos.x + vel.vx * dt;
                int tileX_left = static_cast<int>((nextX - RADIUS) / TILE_SIZE);
                int tileX_right = static_cast<int>((nextX + RADIUS) / TILE_SIZE);
                int tileY_top = static_cast<int>((pos.y - RADIUS) / TILE_SIZE);
                int tileY_bottom = static_cast<int>((pos.y + RADIUS) / TILE_SIZE);

                if (!map.isWalkable(tileX_left, tileY_top) || !map.isWalkable(tileX_left, tileY_bottom) ||
                    !map.isWalkable(tileX_right, tileY_top) || !map.isWalkable(tileX_right, tileY_bottom)) {
                    vel.vx = 0; // 停止X轴移动
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
                    vel.vy = 0; // 停止Y轴移动
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

    // 确保阶段1在阶段2之前完成
    resolveTask.precede(updateTask);

    m_executor.run(m_taskflow).wait();
    // LOG_TRACE("Game update completed");
}

void Game::render() { // 渲染
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    BeginMode2D(m_camera);
        // 绘制世界网格作为视觉参考
        const int gridSize = 100;
        const int worldWidth = WorldConstants::WORLD_WIDTH;
        const int worldHeight = WorldConstants::WORLD_HEIGHT;
        for(int x = 0; x <= worldWidth; x += gridSize) DrawLine(x, 0, x, worldHeight, LIGHTGRAY);
        for(int y = 0; y <= worldHeight; y += gridSize) DrawLine(0, y, worldWidth, y, LIGHTGRAY);

        // 1. 渲染地图 (地板/墙壁)
        m_levelManager->render(m_camera);

        // 2. Render Entities (Player, Enemies)
        RenderSystem::render(m_registry);
        
        // 3. Render Fog of War (Overlay on top of everything)
        m_levelManager->getFogSystem().renderFog();
    EndMode2D();
    
    // 渲染UI (屏幕空间)
    // Draw Character Panel (C key)
    UISystem::Draw(m_registry, *m_levelManager, m_camera);
    SerializationSystem::DrawUI();
    
    DrawFPS(10, 10);
    // DrawText("WASD to Move", 10, 40, 20, DARKGRAY);
    
    EndDrawing();
}

void Game::run() {
    LOG_INFO("Starting Game Loop...");
    
    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;
    // 游戏主循环
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
    LOG_INFO("开始游戏清理...");
    UISystem::Shutdown();
    m_levelManager->cleanup();
    m_resourceManager.unloadAll();
    m_registry.clear();
    LOG_INFO("Game cleanup completed");
}