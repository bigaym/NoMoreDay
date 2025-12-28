#pragma once

#include <memory>
#include "../systems/MapSystem.hpp"
#include "../systems/EnemySpawnSystem.hpp"
#include "../systems/FogOfWarSystem.hpp"
#include "../components/Common.hpp"

class LevelManager {
private:
    std::unique_ptr<MapSystem> m_mapSystem;
    std::unique_ptr<EnemySpawnSystem> m_enemySystem;
    std::unique_ptr<FogOfWarSystem> m_fogSystem;
    
    // 当前关卡信息
    std::string m_currentBiome;
    int m_currentLevel;
    
    // 关卡尺寸
    static constexpr int DEFAULT_MAP_WIDTH = 128;
    static constexpr int DEFAULT_MAP_HEIGHT = 128;

public:
    LevelManager();
    ~LevelManager();
    
    // 初始化关卡管理器
    void initialize();
    
    // 加载新关卡
    void loadNewLevel(const std::string& biome = "cave", 
                     int width = DEFAULT_MAP_WIDTH, 
                     int height = DEFAULT_MAP_HEIGHT);
    
    // 更新关卡系统
    void update(float dt, entt::registry& registry, const Position& playerPos);
    
    // 渲染关卡
    void render(const Camera2D& camera);
    
    // 清理当前关卡
    void cleanup();
    
    // 获取系统引用
    MapSystem& getMapSystem() { return *m_mapSystem; }
    const MapSystem& getMapSystem() const { return *m_mapSystem; }
    EnemySpawnSystem& getEnemySystem() { return *m_enemySystem; }
    const EnemySpawnSystem& getEnemySystem() const { return *m_enemySystem; }
    FogOfWarSystem& getFogSystem() { return *m_fogSystem; }
    const FogOfWarSystem& getFogSystem() const { return *m_fogSystem; }
    
    // 获取当前关卡信息
    const std::string& getCurrentBiome() const { return m_currentBiome; }
    int getCurrentLevel() const { return m_currentLevel; }

private:
    // 生成关卡
    void generateLevel(const std::string& biome, int width, int height);
    
    // 设置关卡参数
    void setLevelParameters(const std::string& biome);
};