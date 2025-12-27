#include "Game.hpp"
#include "../components/Common.hpp"
#include "../systems/PhysicsSystem.hpp"
#include "../systems/RenderSystem.hpp"
#include "../systems/InputSystem.hpp"
#include "../systems/CombatSystem.hpp"
#include "../utils/Parallel.hpp"
#include "../tools/Logger.hpp"
#include "AssetRegistry.hpp"
#include <random>

Game::Game(int width, int height, const char* title) 
    : m_screenWidth(width), m_screenHeight(height), m_title(title), 
      m_spatialGrid(200, 200, 32.0f) { // Optimized CellSize (approx 3x diameter)
    
    // Raylib Init
    InitWindow(m_screenWidth, m_screenHeight, m_title);
    SetTargetFPS(60);
    
    init();
}

Game::~Game() {
    cleanup();
    CloseWindow();
}

void Game::init() {
    // 1. Load Resources via Manager (Compile-time IDs)
    const auto& playerAsset = assets::textures::Player_Warrior;
    Texture2D playerTexture = m_resourceManager.loadTexture(playerAsset.id, std::string(playerAsset.path));
    
    // 2. Spawn Player
    auto player = m_registry.create();
    m_registry.emplace<Position>(player, (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f);
    m_registry.emplace<Velocity>(player, 0.0f, 0.0f);
    m_registry.emplace<PlayerTag>(player);
    m_registry.emplace<InputComponent>(player);
    
    // Basic Sword Stats
    m_registry.emplace<WeaponComponent>(player, 10.0f, 60.0f, 0.15f, 1500.0f, 0.0f);

    if (playerTexture.id > 0) {
        // Character is big (1024x1024), scale down to ~100px
        m_registry.emplace<SpriteComponent>(player, playerTexture, 0.1f);
    }

    // 3. Spawn Background Entities (Enemies/Particles)
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> distPos(0, (float)m_screenWidth);
    std::uniform_real_distribution<float> distVel(-30, 30);

    const int ENTITY_COUNT = 1000; 
    LOG_INFO("Spawning {} background entities...", ENTITY_COUNT);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = m_registry.create();
        m_registry.emplace<Position>(entity, distPos(gen), distPos(gen));
        m_registry.emplace<Velocity>(entity, distVel(gen), distVel(gen));
        m_registry.emplace<ColorComponent>(entity, Color{ (unsigned char)(gen() % 255), (unsigned char)(gen() % 255), (unsigned char)(gen() % 255), 150 });
        
        // Give them health so they can be destroyed
        m_registry.emplace<HealthComponent>(entity, 30.0f, 30.0f);
    }

    LOG_INFO("Taskflow initialized with {} workers", std::thread::hardware_concurrency());

    // 4. Init Camera
    m_camera = { 0 };
    m_camera.zoom = 1.0f;
    m_camera.offset = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
    m_camera.rotation = 0.0f;
    m_camera.target = { (float)m_screenWidth / 2.0f, (float)m_screenHeight / 2.0f };
}

// Functor to avoid MinGW "local type" error with Taskflow templates
struct EntityUpdateTask {
    entt::registry& registry;
    systems::SpatialHashGrid& grid; // Reference to Grid
    float dt;
    int worldWidth;
    int worldHeight;

    void operator()(entt::entity entity) const {
        // EnTT's registry.get is efficient
        auto [pos, vel] = registry.get<Position, Velocity>(entity);
        PhysicsSystem::updateEntity(entity, pos, vel, grid, registry, dt, worldWidth, worldHeight);
    }
};

void Game::update(float dt) {
    // 1. Process Input
    InputSystem::update(m_registry);

    // 2. Apply Player Input to Velocity & Update Camera
    auto playerView = m_registry.view<PlayerTag, InputComponent, Velocity, Position>();
    for (auto entity : playerView) {
        auto& input = playerView.get<InputComponent>(entity);
        auto& vel = playerView.get<Velocity>(entity);
        const auto& pos = playerView.get<Position>(entity);
        
        float speed = 300.0f; // Player movement speed
        vel.vx = input.moveX * speed;
        vel.vy = input.moveY * speed;

        // Camera Follow (Smooth Lerp)
        float lerpSpeed = 5.0f;
        m_camera.target.x += (pos.x - m_camera.target.x) * lerpSpeed * dt;
        m_camera.target.y += (pos.y - m_camera.target.y) * lerpSpeed * dt;
    }

    // 3. Combat System (Process Attacks)
    // Note: We run this BEFORE rebuilding the grid, because if entities die, we want them gone.
    // However, Combat needs the Grid to find targets. So it uses the Grid from LAST frame.
    // This is acceptable latency (16ms).
    CombatSystem::update(m_registry, m_spatialGrid, m_camera, dt);

    // 4. Rebuild Spatial Grid
    auto view = m_registry.view<Position, Velocity>();
    m_spatialGrid.rebuild(view, m_registry);

    // 5. Parallel Physics Execution
    m_taskflow.clear(); 
    std::vector<entt::entity> entities(view.begin(), view.end());

    // Use a larger world boundary (e.g., 3000x3000) so entities don't bounce at screen edges
    int worldSize = 3000;
    utils::parallel_for_each(m_taskflow, entities.begin(), entities.end(), 
        EntityUpdateTask{m_registry, m_spatialGrid, dt, worldSize, worldSize});

    m_executor.run(m_taskflow).wait();
}

void Game::render() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    BeginMode2D(m_camera);
        // Draw World Grid for visual reference
        const int gridSize = 100;
        const int worldSize = 3000;
        for(int x = 0; x <= worldSize; x += gridSize) DrawLine(x, 0, x, worldSize, LIGHTGRAY);
        for(int y = 0; y <= worldSize; y += gridSize) DrawLine(0, y, worldSize, y, LIGHTGRAY);

        RenderSystem::render(m_registry);
    EndMode2D();
    
    DrawFPS(10, 10);
    DrawText("WASD to Move", 10, 40, 20, DARKGRAY);
    
    EndDrawing();
}

void Game::run() {
    LOG_INFO("Starting Game Loop...");
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        // Clamp dt to prevent physics explosion during lag spikes or window dragging
        if (dt > 0.05f) dt = 0.05f;

        update(dt);
        render();
    }
}

void Game::cleanup() {
    m_resourceManager.unloadAll();
    m_registry.clear();
}