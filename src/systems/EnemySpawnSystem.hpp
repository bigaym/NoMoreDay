#pragma once

#include <vector>
#include <string>
#include <map>
#include <entt/entt.hpp>
#include <random>
#include "../components/Common.hpp"
#include "MapSystem.hpp"

// 敌人生数据结构
struct EnemySpawnData {
    Position position;
    int enemyType;     // 简单起见，0=Skeleton, 1=Demon
    bool isAlive;      // 当前是否已生成实体
    entt::entity entityId; // 对应的实体ID
    bool allowRespawn = false; // 是否允许重生
};

class EnemySpawnSystem {
private:
    std::vector<EnemySpawnData> m_spawnData;
    int m_mapWidth;
    int m_mapHeight;
    std::map<int, Texture2D> m_raceTextures;
    std::vector<int> m_pendingRaces;
    std::mt19937 m_gen;
    
    // 生成参数
    float m_activationDistance = 600.0f;  // 进入此范围生成
    float m_deactivationDistance = 800.0f; // 超出此范围销毁
    
public:
    EnemySpawnSystem();
    ~EnemySpawnSystem();
    
    // 初始化关卡 (Legacy)
    void initializeLevel(int width, int height, const MapSystem& mapSystem, const std::string& biome);
    
    // Async Loading Support
    void initData(int width, int height, const MapSystem& mapSystem, const std::string& biome);
    void initTextures();
    
    // 更新生成/销毁逻辑
    void updateEnemySpawning(const Position& playerPos, entt::registry& registry);
    
private:
    // 具体的生成逻辑
    void spawnEnemy(entt::registry& registry, EnemySpawnData& data);
    // 具体的销毁逻辑
    void despawnEnemy(entt::registry& registry, EnemySpawnData& data);
};