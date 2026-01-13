#pragma once

#include "game/components/Common.hpp"
#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/MapSystem.hpp"
#include <cassert>
#include <memory>

class ResourceManager;

class LevelManager {
private:
  std::unique_ptr<MapSystem> m_mapSystem;
  std::unique_ptr<EnemySpawnSystem> m_enemySystem;
  std::unique_ptr<FogOfWarSystem> m_fogSystem;

  // 当前关卡信息
  std::string m_currentBiome;
  int m_currentLevel;

  // 资源管理器引用 (GPU 初始化需要)
  ResourceManager *m_resources = nullptr;

  // 关卡尺寸
  static constexpr int DEFAULT_MAP_WIDTH = 128;
  static constexpr int DEFAULT_MAP_HEIGHT = 128;

public:
  struct LevelData {
    std::unique_ptr<MapSystem> map;
    std::unique_ptr<EnemySpawnSystem> enemy;
    std::unique_ptr<FogOfWarSystem> fog;
    std::string biome;
    int width, height, level;
  };
  LevelManager();
  ~LevelManager();

  // 初始化关卡管理器 (传入 ResourceManager 用于 GPU 资源加载)
  void initialize(ResourceManager &resources);

  // 加载新关卡 (Synchronous legacy wrapper)
  void loadNewLevel(const std::string &biome = "cave",
                    int width = DEFAULT_MAP_WIDTH,
                    int height = DEFAULT_MAP_HEIGHT, int level = 1);

  // 加载拼图关卡
  void loadMosaicLevel(const NoMoreDay::MosaicGrid &grid,
                       const NoMoreDay::ResonanceResult &resonance,
                       entt::registry *registry, int width = DEFAULT_MAP_WIDTH,
                       int height = DEFAULT_MAP_HEIGHT);

  // Async Loading Support
  // 1. Prepare data (Thread Safe, CPU Only)
  LevelData prepareLevel(const std::string &biome, int width, int height,
                         int level);
  LevelData prepareMosaicLevel(const NoMoreDay::MosaicGrid &grid,
                               const NoMoreDay::ResonanceResult &resonance,
                               entt::registry *registry, int width, int height);

  // 2. Activate prepared level (Main Thread, GPU Init)
  void activateLevel(LevelData &&data);

  // 更新关卡系统
  void update(float dt, entt::registry &registry, const Position &playerPos);

  // 渲染关卡
  void render(const Camera2D &camera);

  // 清理当前关卡
  void cleanup();

  // 获取系统引用 (带空指针检查)
  MapSystem &getMapSystem() {
    assert(m_mapSystem && "LevelManager: MapSystem not initialized");
    return *m_mapSystem;
  }
  const MapSystem &getMapSystem() const {
    assert(m_mapSystem && "LevelManager: MapSystem not initialized");
    return *m_mapSystem;
  }
  EnemySpawnSystem &getEnemySystem() {
    assert(m_enemySystem && "LevelManager: EnemySpawnSystem not initialized");
    return *m_enemySystem;
  }
  EnemySpawnSystem &getEnemySpawnSystem() { return getEnemySystem(); }

  const EnemySpawnSystem &getEnemySystem() const {
    assert(m_enemySystem && "LevelManager: EnemySpawnSystem not initialized");
    return *m_enemySystem;
  }
  FogOfWarSystem &getFogSystem() {
    assert(m_fogSystem && "LevelManager: FogOfWarSystem not initialized");
    return *m_fogSystem;
  }
  const FogOfWarSystem &getFogSystem() const {
    assert(m_fogSystem && "LevelManager: FogOfWarSystem not initialized");
    return *m_fogSystem;
  }

  // 检查系统是否已初始化
  bool isInitialized() const {
    return m_mapSystem && m_enemySystem && m_fogSystem;
  }

  // 获取当前关卡信息
  const std::string &getCurrentBiome() const { return m_currentBiome; }
  int getCurrentLevel() const { return m_currentLevel; }

private:
  // 生成关卡
  void generateLevel(const std::string &biome, int width, int height);

  // 设置关卡参数
  void setLevelParameters(const std::string &biome);
};