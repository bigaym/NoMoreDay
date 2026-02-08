#pragma once

#include "game/components/Common.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <entt/entt.hpp>
#include <map>
#include <random>
#include <string>
#include <vector>


// 敌人生数据结构
struct EnemySpawnData {
  Position position;
  int enemyType;             // Type (EnemyRace::Type)
  int enemyVariant;          // Variant/Archetype (0-4)
  bool isAlive;              // 当前是否已生成实体
  entt::entity entityId;     // 对应的实体ID
  bool allowRespawn = false; // 是否允许重生
};

namespace NoMoreDay {
struct ResonanceResult;
struct ActiveDimensionalState;
}

class EnemySpawnSystem {
private:
  std::vector<EnemySpawnData> m_spawnData;
  int m_mapWidth;
  int m_mapHeight;
  std::mt19937 m_gen;

  // 生成参数
  float m_activationDistance = 600.0f;   // 进入此范围生成
  float m_deactivationDistance = 800.0f; // 超出此范围销毁

  // 共鸣修正 (Dimensional Mosaic System)
  struct ResonanceMods {
    float densityMultiplier = 1.0f;
    int levelBonus = 0;
    float dropRateBonus = 0.0f;
    int dominantElement = 0; // 0=None, 1=Fire, etc.
    float hpMultiplier = 1.0f;
    float damageMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
  } m_resonanceMods;
  
  int m_areaLevel = 1; // 当前区域等级

public:
  EnemySpawnSystem();
  ~EnemySpawnSystem();

  // 初始化关卡 (Legacy)
  void initializeLevel(int width, int height, int level, const MapSystem &mapSystem,
                       NoMoreDay::BiomeID biome);

  // Async Loading Support
  void initData(int width, int height, int level, const MapSystem &mapSystem,
                NoMoreDay::BiomeID biome,
                const NoMoreDay::ActiveDimensionalState *state = nullptr);
  void initTextures();

  // 更新生成/销毁逻辑
  void updateEnemySpawning(const Position &playerPos, entt::registry &registry,
                           float dt = 0.016f,
                           MapSystem *mapSystem = nullptr);
  
  // Spec 2.3: Re-schedule dormant entities
  void updateDormantEntities(entt::registry& registry, const Position& playerPos, int gridW, int gridH);

private:
  // 具体的生成逻辑
  void spawnEnemy(entt::registry &registry, EnemySpawnData &data);
  // 具体的销毁逻辑
  void despawnEnemy(entt::registry &registry, EnemySpawnData &data);
  void updateSpawnerWalls(entt::registry &registry, MapSystem &mapSystem,
                          const Position &playerPos, float dt);
};
