#include "LevelManager.hpp"
#include "../tools/Logger.hpp"

LevelManager::LevelManager() 
    : m_currentLevel(1) {
}

LevelManager::~LevelManager() {
    LOG_INFO("Shutting down LevelManager...");
    cleanup();
    LOG_INFO("LevelManager shutdown completed");
}

void LevelManager::initialize() {
    // This is now just a placeholder or for empty init
    // Real init happens in activateLevel
    LOG_INFO("LevelManager initialized (empty state)");
}

void LevelManager::loadNewLevel(const std::string& biome, int width, int height) {
    LOG_INFO("Loading new level synchronously: {} ({}x{})", biome, width, height);
    auto data = prepareLevel(biome, width, height);
    activateLevel(std::move(data));
}

LevelManager::LevelData LevelManager::prepareLevel(const std::string& biome, int width, int height) {
    LOG_INFO("Preparing level data for: {} ({}x{})", biome, width, height);
    
    LevelData data;
    data.biome = biome;
    data.width = width;
    data.height = height;
    
    data.map = std::make_unique<MapSystem>();
    data.enemy = std::make_unique<EnemySpawnSystem>();
    data.fog = std::make_unique<FogOfWarSystem>();
    
    // CPU Generation
    data.map->generateMap(width, height, biome);
    data.fog->initData(width, height);
    data.enemy->initData(width, height, *data.map, biome);
    
    return data;
}

void LevelManager::activateLevel(LevelData&& data) {
    LOG_INFO("Activating level: {} ({}x{})", data.biome, data.width, data.height);
    
    cleanup(); // Clean old level (including GPU textures)
    
    m_mapSystem = std::move(data.map);
    m_enemySystem = std::move(data.enemy);
    m_fogSystem = std::move(data.fog);
    m_currentBiome = data.biome;
    
    // GPU Initialization (Must be on Main Thread)
    if (m_fogSystem) {
        m_fogSystem->initTexture();
    }
    if (m_enemySystem) {
        m_enemySystem->initTextures();
    }
    
    LOG_INFO("Level activated successfully");
}

void LevelManager::update(float dt, entt::registry& registry, const Position& playerPos) {
    if (m_mapSystem && m_enemySystem && m_fogSystem) {
        // 更新战争迷雾
        float viewRadius = 200.0f; // 默认值
        // 尝试从玩家实体获取视野组件
        auto view = registry.view<const PlayerTag, const VisionComponent>();
        if (view.begin() == view.end()) {
             // It's possible player died or not spawned yet
        }
        for (auto [entity, vision] : view.each()) {
            viewRadius = vision.radius;
        }
        m_fogSystem->updateVisibility(playerPos, viewRadius);
        
        // 同步可见性到 MapSystem (确保渲染正确)
        int w = m_mapSystem->getWidth();
        int h = m_mapSystem->getHeight();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                m_mapSystem->setVisibility(x, y, m_fogSystem->getVisibility(x, y));
            }
        }

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
