# 场景生成系统架构设计

## 1. 系统概述

基于NoMoreDay项目现有的ECS架构，设计一个完整的场景生成系统，包含地图生成、敌人管理系统和战争迷雾系统。

## 2. 地图生成系统

### 2.1 核心数据结构

```cpp
// 地图瓦片类型
struct Tile {
    enum Type { WALL, FLOOR, DOOR, STAIRS_UP, STAIRS_DOWN };
    Type type;
    bool isExplored;
    uint8_t visibility; // 0=未探索, 1=已探索, 2=可见
};

// 地图生成器
class MapGenerator {
public:
    // 改进型细胞自动机生成
    static std::vector<std::vector<Tile>> GenerateCaveMap(int width, int height);
    
    // 连通性检查和主区域提取
    static void EnsureConnectivity(std::vector<std::vector<Tile>>& map);
    
    // 边界处理
    static void ApplyBoundaries(std::vector<std::vector<Tile>>& map);
};

class MapSystem {
private:
    std::vector<std::vector<Tile>> m_mapGrid;
    int m_width, m_height;
    
public:
    void generateMap(int width, int height, const std::string& biome);
    const std::vector<std::vector<Tile>>& getMap() const { return m_mapGrid; }
    bool isWalkable(int x, int y) const;
    Tile::Type getTileType(int x, int y) const;
};
```

### 2.2 细胞自动机算法实现

```cpp
// 地图生成的具体实现
class CaveMapGenerator {
public:
    static std::vector<std::vector<Tile>> Generate(int width, int height) {
        // 1. 初始化：45%几率墙壁，55%几率地板
        std::vector<std::vector<Tile>> grid(width, std::vector<Tile>(height));
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                grid[x][y].type = (dist(gen) < 0.45f) ? Tile::WALL : Tile::FLOOR;
            }
        }
        
        // 2. 平滑迭代：4-5次，邻居>4个墙壁则变墙
        for (int iteration = 0; iteration < 5; ++iteration) {
            grid = smoothIteration(grid, width, height);
        }
        
        // 3. 连通性检查：保留最大连通区域
        ensureConnectivity(grid, width, height);
        
        // 4. 边界处理：外圈设为墙壁
        applyBoundaries(grid, width, height);
        
        return grid;
    }
    
private:
    static std::vector<std::vector<Tile>> smoothIteration(
        const std::vector<std::vector<Tile>>& grid, int width, int height) {
        std::vector<std::vector<Tile>> newGrid = grid;
        
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int wallCount = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (grid[x + dx][y + dy].type == Tile::WALL) {
                            wallCount++;
                        }
                    }
                }
                
                if (wallCount > 4) {
                    newGrid[x][y].type = Tile::WALL;
                } else {
                    newGrid[x][y].type = Tile::FLOOR;
                }
            }
        }
        
        return newGrid;
    }
    
    static void ensureConnectivity(std::vector<std::vector<Tile>>& grid, int width, int height);
    static void applyBoundaries(std::vector<std::vector<Tile>>& grid, int width, int height);
};
```

## 3. 敌人管理系统

### 3.1 休眠激活系统

```cpp
// 敌人生数据结构
struct EnemySpawnData {
    Position position;
    uint16_t enemyType;     // 种族ID
    uint8_t level;
    bool isElite;
    bool isAlive;
    float lastKnownHP;
    
    // 群聚信息
    uint8_t clusterID;
    bool isBoss;
};

// 激活的敌人数据
struct ActiveEnemyData {
    entt::entity entity;
    EnemySpawnData spawnData;
    float activationDistance;
};

class EnemySpawnSystem {
private:
    std::vector<EnemySpawnData> m_spawnData;
    std::unordered_map<entt::entity, size_t> m_activeEnemies;
    std::vector<ActiveEnemyData> m_activeEnemyList;
    
    // 空间分区用于快速查询
    systems::SpatialHashGrid m_spawnGrid;
    
public:
    void initializeLevel(int mapWidth, int mapHeight);
    void updateSpawnStatus(const Position& playerPos);
    
private:
    void spawnEnemiesNearPlayer(const Position& playerPos);
    void despawnEnemiesFarFromPlayer(const Position& playerPos);
    void updateSpawnDataFromEntity(entt::entity entity);
};
```

### 3.2 种族和职业系统

```cpp
// 敌人种族定义
struct EnemyRace {
    enum Type { UNDEAD, DEMON, CORRUPTED, CULTIST };
    Type raceType;
    float baseHP, baseDamage, baseSpeed;
    std::vector<std::string> resistances;
    std::string texturePath;  // 资源路径
};

// 敌人职业/行为模板
struct EnemyArchetype {
    enum Type { FODDER, TANK, RANGER, ASSASSIN };
    Type archetypeType;
    std::function<void(entt::registry&, entt::entity, float)> aiBehavior;
    
    // AI行为实现
    static void FodderBehavior(entt::registry& reg, entt::entity entity, float dt);
    static void TankBehavior(entt::registry& reg, entt::entity entity, float dt);
    static void RangerBehavior(entt::registry& reg, entt::entity entity, float dt);
    static void AssassinBehavior(entt::registry& reg, entt::entity entity, float dt);
};
```

## 4. 战争迷雾系统

### 4.1 可见性管理

```cpp
class FogOfWarSystem {
private:
    std::vector<uint8_t> m_visibilityGrid;  // 0=未探索, 1=已探索, 2=可见
    Texture2D m_fogTexture;                 // 雾层纹理
    int m_width, m_height;
    
public:
    void initialize(int width, int height);
    void updateVisibility(const Position& playerPos, float viewRadius);
    void renderFog();
    
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    
private:
    void updateFogTexture();
    void floodFillExplore(int startX, int startY);
};
```

### 4.2 小地图系统

```cpp
class MinimapSystem {
private:
    Texture2D m_minimapTexture;
    bool m_needsUpdate;
    
public:
    void initialize(int width, int height);
    void update(const std::vector<std::vector<Tile>>& map,
                const std::vector<uint8_t>& visibility);
    void render();
    
private:
    void updateMinimapTexture(const std::vector<std::vector<Tile>>& map,
                             const std::vector<uint8_t>& visibility);
};
```

## 5. 关卡上下文管理

```cpp
class LevelContext {
private:
    std::unique_ptr<MapSystem> m_mapSystem;
    std::unique_ptr<EnemySpawnSystem> m_enemySystem;
    std::unique_ptr<FogOfWarSystem> m_fogSystem;
    std::unique_ptr<MinimapSystem> m_minimapSystem;
    
    // 种族池
    std::vector<EnemyRace::Type> m_activeRaces;
    
public:
    void initialize(const std::string& biome);
    void update(float dt, const Position& playerPos);
    void cleanup();
    
    // 获取系统引用
    MapSystem& getMapSystem() { return *m_mapSystem; }
    EnemySpawnSystem& getEnemySystem() { return *m_enemySystem; }
    FogOfWarSystem& getFogSystem() { return *m_fogSystem; }
};

class LevelManager {
private:
    std::unique_ptr<LevelContext> m_currentLevel;
    
public:
    void loadNewLevel(const std::string& biome);
    void transitionToNewLevel(const std::string& biome);
    LevelContext& getCurrentLevel() { return *m_currentLevel; }
};
```

## 6. ECS组件扩展

```cpp
// 新增的组件
struct MapTileComponent {
    int gridX, gridY;
    Tile::Type tileType;
};

struct VisibilityComponent {
    uint8_t visibilityLevel;  // 0=未探索, 1=已探索, 2=可见
};

struct SpawnDataComponent {
    size_t spawnDataIndex;    // 指向EnemySpawnSystem中的数据
    bool isFromSpawnData;     // 标记是否从休眠数据激活
};

// 在现有AIComponent基础上扩展
struct AIStateComponent {
    AIType aiType;
    float detectionRange;
    float attackRange;
    float speed;
    entt::entity target;
    float stateTimer;
    
    // 新增：激活范围相关
    float activationRange;
    float deactivationRange;
};
```

## 7. 系统集成

在Game类中集成新系统：

```cpp
class Game {
    // ... 现有成员 ...
    
private:
    // 新增系统
    std::unique_ptr<LevelManager> m_levelManager;
    
    void update(float dt) override {
        // 获取玩家位置
        Position playerPos = getPlayerPosition();
        
        // 1. 处理输入
        InputSystem::update(m_registry);
        
        // 2. 更新玩家移动
        updatePlayerMovement(dt);
        
        // 3. 更新关卡系统（地图、敌人、迷雾）
        m_levelManager->getCurrentLevel().update(dt, playerPos);
        
        // 4. AI系统更新
        AISystem::update(m_registry, m_spatialGrid, playerPos, dt);
        
        // 5. 战斗系统更新
        CombatSystem::update(m_registry, m_spatialGrid, m_camera, dt);
        
        // 6. 重建空间网格
        rebuildSpatialGrid();
        
        // 7. 并行物理更新
        updatePhysicsParallel(dt);
    }
};
```

## 8. 实现文件结构

### 8.1 组件定义文件
- [`src/components/MapComponent.hpp`](src/components/MapComponent.hpp:1) - 定义地图相关组件
- [`src/components/EnemyComponent.hpp`](src/components/EnemyComponent.hpp:1) - 定义敌人相关组件

### 8.2 系统实现文件
- [`src/systems/MapSystem.hpp`](src/systems/MapSystem.hpp:1) - 地图系统头文件
- [`src/systems/MapSystem.cpp`](src/systems/MapSystem.cpp:1) - 地图系统实现
- [`src/systems/EnemySpawnSystem.hpp`](src/systems/EnemySpawnSystem.hpp:1) - 敌人生成系统头文件
- [`src/systems/EnemySpawnSystem.cpp`](src/systems/EnemySpawnSystem.cpp:1) - 敌人生成系统实现
- [`src/systems/FogOfWarSystem.hpp`](src/systems/FogOfWarSystem.hpp:1) - 战争迷雾系统头文件
- [`src/systems/FogOfWarSystem.cpp`](src/systems/FogOfWarSystem.cpp:1) - 战争迷雾系统实现

### 8.3 管理器文件
- [`src/core/LevelManager.hpp`](src/core/LevelManager.hpp:1) - 关卡管理器头文件
- [`src/core/LevelManager.cpp`](src/core/LevelManager.cpp:1) - 关卡管理器实现

## 9. 实现建议

1. **性能优化**：使用空间哈希网格优化敌人激活/休眠查询
2. **内存管理**：休眠数据使用紧凑结构体，减少内存占用
3. **随机性**：确保地图和敌人生成的随机种子可重现
4. **扩展性**：为不同生物群系（末世/异界/幻想）预留接口
5. **调试**：添加可视化工具显示地图、敌人分布和激活状态

这个架构设计充分利用了现有的ECS系统，保持了良好的性能和扩展性，同时满足了设计文档中提到的所有要求。