#include "LevelManager.hpp"
#include "ResourceManager.hpp"
#include "../tools/Logger.hpp"

LevelManager::LevelManager() 
    : m_currentLevel(1) {
}

LevelManager::~LevelManager() {
    LOG_INFO("Shutting down LevelManager...");
    cleanup();
    LOG_INFO("LevelManager shutdown completed");
}

void LevelManager::initialize(ResourceManager& resources) {
    m_resources = &resources;
    LOG_INFO("LevelManager initialized with ResourceManager");
}

void LevelManager::loadNewLevel(const std::string& biome, int width, int height, int level) {
    LOG_INFO("Loading new level synchronously: {} ({}x{}) Level: {}", biome, width, height, level);
    auto data = prepareLevel(biome, width, height, level);
    activateLevel(std::move(data));
}

LevelManager::LevelData LevelManager::prepareLevel(const std::string& biome, int width, int height, int level) {
    LOG_INFO("Preparing level data for: {} ({}x{}) Level: {}", biome, width, height, level);
    
    LevelData data;
    data.biome = biome;
    data.width = width;
    data.height = height;
    data.level = level;
    
    data.map = std::make_unique<MapSystem>();
    data.enemy = std::make_unique<EnemySpawnSystem>();
    data.fog = std::make_unique<FogOfWarSystem>();
    
    // CPU Generation (地图和敌人数据)
    data.map->generateMap(width, height, biome);
    data.enemy->initData(width, height, *data.map, biome);
    // 注意: FogOfWarSystem 现在需要 ResourceManager, 在 activateLevel 中初始化
    
    return data;
}

void LevelManager::activateLevel(LevelData&& data) {
    LOG_INFO("Activating level: {} ({}x{}) Level: {}", data.biome, data.width, data.height, data.level);
    
    cleanup(); // Clean old level (including GPU textures)
    
    m_mapSystem = std::move(data.map);
    m_enemySystem = std::move(data.enemy);
    m_fogSystem = std::move(data.fog);
    m_currentBiome = data.biome;
    m_currentLevel = data.level;
    
    // GPU Initialization (Must be on Main Thread)
    if (m_fogSystem && m_resources) {
        m_fogSystem->initialize(*m_resources, data.width, data.height);
    }
    if (m_enemySystem) {
        m_enemySystem->initTextures();
    }
    
    LOG_INFO("Level activated successfully");
}

void LevelManager::update(float dt, entt::registry& registry, const Position& playerPos) {
    if (m_mapSystem && m_enemySystem && m_fogSystem) {
        // 更新战争迷雾 (GPU 计算)
        float viewRadius = 200.0f; // 默认值
        auto view = registry.view<const PlayerTag, const VisionComponent>();
        for (auto [entity, vision] : view.each()) {
            viewRadius = vision.radius;
        }
        m_fogSystem->updateVisibility(playerPos, viewRadius);
        
        // GPU FogOfWarSystem 直接生成纹理, 无需同步到 MapSystem
        // 渲染时 FogSystem 和 MapSystem 独立渲染

        // 更新敌人生成状态
        m_enemySystem->updateEnemySpawning(playerPos, registry);
    } 
}

void LevelManager::render(const Camera2D& camera) {
    if (m_mapSystem) {
        m_mapSystem->render(camera);
    }
}

void LevelManager::cleanup() {
    m_mapSystem.reset();
    m_enemySystem.reset();
    m_fogSystem.reset();
    
    LOG_INFO("LevelManager cleaned up");
}

void LevelManager::generateLevel(const std::string& biome, int width, int height) {
   // Legacy / Unused in new flow
   // Kept to satisfy header declaration if I didn't remove it from header private section?
   // I didn't remove it from header private section yet.
   // But I can leave it empty or implementing via prepare/activate
   loadNewLevel(biome, width, height);
}

void LevelManager::setLevelParameters(const std::string& biome) {
    // Unused currently
}
