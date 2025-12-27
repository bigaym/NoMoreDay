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
};

class EnemySpawnSystem {
private:
    std::vector<EnemySpawnData> m_spawnData;
    int m_mapWidth;
    int m_mapHeight;
    std::map<int, Texture2D> m_raceTextures;
    std::mt19937 m_gen;
    const MapSystem* m_mapSystemPtr = nullptr; // 保存地图引用用于寻路
    
    // 生成参数
    float m_activationDistance = 600.0f;  // 进入此范围生成
    float m_deactivationDistance = 800.0f; // 超出此范围销毁
    
public:
    EnemySpawnSystem();
    ~EnemySpawnSystem();
    
    // 初始化关卡敌人数据
    void initializeLevel(int width, int height, const MapSystem& mapSystem, const std::string& biome);
    
    // 每帧更新：检查距离，生成或销毁敌人
    void updateEnemySpawning(const Position& playerPos, entt::registry& registry);
    
    // 更新敌人行为状态 (仇恨管理、回血)
    void updateEnemyBehavior(float dt, const Position& playerPos, entt::registry& registry);
    
private:
    // 具体的生成逻辑
    void spawnEnemy(entt::registry& registry, EnemySpawnData& data);
    // 具体的销毁逻辑
    void despawnEnemy(entt::registry& registry, EnemySpawnData& data);
};