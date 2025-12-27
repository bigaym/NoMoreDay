#include "LevelManager.hpp"
#include "../tools/Logger.hpp"

LevelManager::LevelManager() 
    : m_currentLevel(1) {
}

LevelManager::~LevelManager() {
    cleanup();
}

void LevelManager::initialize() {
    m_mapSystem = std::make_unique<MapSystem>();
    m_enemySystem = std::make_unique<EnemySpawnSystem>();
    m_fogSystem = std::make_unique<FogOfWarSystem>();
    
    LOG_INFO("LevelManager initialized");
}

void LevelManager::loadNewLevel(const std::string& biome, int width, int height) {
    cleanup();
    
    m_currentBiome = biome;
    
    // 初始化系统
    initialize();
    
    // 生成关卡
    generateLevel(biome, width, height);
    
    LOG_INFO("Loaded new level: {} ({}, {})", biome, width, height);
}

void LevelManager::update(float dt, entt::registry& registry, const Position& playerPos) {
    if (m_mapSystem && m_enemySystem && m_fogSystem) {
        // 更新战争迷雾
        float viewRadius = 200.0f; // 默认值
        // 尝试从玩家实体获取视野组件
        auto view = registry.view<const PlayerTag, const VisionComponent>();
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
         
         // 更新敌人行为 (仇恨、回血)
         m_enemySystem->updateEnemyBehavior(dt, playerPos, registry);
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
    if (m_mapSystem) {
        m_mapSystem->generateMap(width, height, biome);
        
        // 初始化战争迷雾系统
        m_fogSystem->initialize(width, height);
        
        // 初始化敌人生成系统
        m_enemySystem->initializeLevel(width, height, *m_mapSystem, biome);
        
        LOG_INFO("Generated level with biome: {}, size: {}x{}", biome, width, height);
    }
}

void LevelManager::setLevelParameters(const std::string& biome) {
    // 根据生物群系设置关卡参数
    if (biome == "cave") {
        // 洞穴关卡参数
    } else if (biome == "dungeon") {
        // 地牢关卡参数
    } else if (biome == "demon" || biome == "hell") {
        // 恶魔/地狱关卡参数
    }
}