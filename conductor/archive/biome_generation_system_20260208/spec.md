# 生物群落地图生成系统 规格说明书 (V1.0)

> **Track ID**: `biome_generation_system_20260208`
> **设计参考**: `设计文档/地图生物群落与城镇皮肤设计.md`
> **状态**: ⏳ 待确认

---

## 1. 概述 (Overview)

基于设计文档，为 NoMoreDay 实现完整的生物群落地图生成系统。支持 21 种战斗生物群落和 6 种城镇皮肤，通过数据驱动的方式实现不同风格地图的随机生成。

### 1.1 核心特性

| 特性 | 规格 |
|------|------|
| **群落类型** | 27 种 (6 城镇 + 7 开阔 + 7 迷宫 + 7 特殊) |
| **生成算法** | CA (Cellular Automata) + 特殊结构生成器 |
| **敌人池** | 9 种种族: BEAST, GOBLIN, UNDEAD, DEMON, CORRUPTED, CULTIST, ELVES, MACHINE, ELEMENTAL |
| **空气墙** | C组地图独有机制，物理阻挡但视觉透明 |
| **特殊机制** | 低重力、可破坏地形、动态刷怪点、迷雾视野 |

### 1.2 设计目标

1. **数据驱动**: 所有群落参数存储于 `biomes.json`，无需修改代码即可调整。
2. **风格分明**: 三类地图（开阔/迷宫/特殊）有截然不同的游戏体验。
3. **渲染增强**: 空气墙、动态背景、特殊光效。
4. **生态完整**: 每个群落有专属怪物池和掉落表。
5. **扩展性**: 支持未来新增群落类型和特殊机制。

---

## 2. 数据模型 (Data Model)

### 2.1 BiomeConfig 扩展

```cpp
// ============================================================
// BiomeRegistry.hpp - 扩展后的群落配置
// ============================================================
namespace NoMoreDay {

// 群落风格分类
enum class BiomeStyle : uint8_t {
    Town = 0,      // 城镇 (安全区)
    Open = 1,      // 开阔战场 (A组)
    Maze = 2,      // 复杂迷宫 (B组)
    Special = 3    // 混合与特殊结构 (C组)
};

// 特殊机制标志位
enum class BiomeFeature : uint32_t {
    None           = 0,
    AirWall        = 1 << 0,   // 空气墙 (透明不可通行)
    LowGravity     = 1 << 1,   // 低重力 (跳跃距离增加)
    Destructible   = 1 << 2,   // 可破坏地形
    DynamicSpawner = 1 << 3,   // 动态刷怪点墙壁
    LimitedVision  = 1 << 4,   // 迷雾视野限制
    SpeedZone      = 1 << 5,   // 加速带
    FrictionMod    = 1 << 6,   // 摩擦力修改
    VisualFilter   = 1 << 7    // 视觉滤镜
};

// 扩展后的群落配置
struct BiomeConfig {
    // === 基础属性 ===
    std::string id;                           // 唯一标识 (如 "sun_prairie")
    BiomeID numericId = BiomeID::None;        // 枚举ID
    std::string name;                         // 显示名称
    BiomeStyle style = BiomeStyle::Open;      // 风格分类
    
    // === 视觉属性 ===
    Color floorColor = DARKBROWN;
    Color wallColor = DARKGRAY;
    Color ambientColor = WHITE;               // 环境光颜色
    std::string backgroundShader;             // 背景shader (空气墙地图用)
    std::string visualFilterShader;           // 视觉滤镜shader
    
    // === CA生成参数 ===
    float wallProbability = 0.45f;            // 初始墙壁概率
    int smoothIterations = 5;                 // 平滑迭代次数
    int wallBirthLimit = 4;                   // 邻居>=此值变墙
    int wallDeathLimit = 3;                   // 邻居<此值变地板
    
    // === 特殊机制 ===
    uint32_t features = 0;                    // BiomeFeature 位掩码
    float frictionMultiplier = 1.0f;          // 摩擦力倍率
    float gravityMultiplier = 1.0f;           // 重力倍率
    float visionRadius = 0.0f;                // 视野限制 (0=无限制)
    
    // === 怪物生态 ===
    std::vector<std::string> enemyPool;       // 允许的怪物种族
    int maxEnemies = 50;
    bool isSafeZone = false;
    
    // === 便捷方法 ===
    [[nodiscard]] bool hasFeature(BiomeFeature f) const noexcept {
        return (features & static_cast<uint32_t>(f)) != 0;
    }
};

} // namespace NoMoreDay
```

### 2.2 BiomeID 枚举扩展

```cpp
// ============================================================
// Common.hpp 中的 BiomeID 枚举扩展
// ============================================================
namespace NoMoreDay {

enum class BiomeID : uint8_t {
    None = 0,
    
    // === 城镇 (T01-T06) ===
    Town = 1,                 // 默认城镇 (兼容旧存档)
    Town_SwordImmortal = 2,   // 青峦仙阁 (剑修)
    Town_Mage = 3,            // 以太枢纽 (法师)
    Town_Mech = 4,            // 铁魂工坊 (机械)
    Town_Shadow = 5,          // 幽影黑市 (刺客)
    Town_Beast = 6,           // 先祖巨木 (野兽)
    Town_Radiant = 7,         // 圣辉大教堂 (圣职)
    
    // === 开阔战场 A组 (C01-C07) ===
    Cave = 10,                // 默认洞穴 (兼容旧存档)
    SunPrairie = 11,          // 炽阳草原
    IceTundra = 12,           // 永冻苔原
    CrimsonWaste = 13,        // 血色荒野
    DustSea = 14,             // 尘埃海原
    VoidFlats = 15,           // 虚空平原
    EmeraldWet = 16,          // 翡翠湿地
    AshPlain = 17,            // 灰烬平原
    
    // === 复杂迷宫 B组 (C08-C14) ===
    GloomSpire = 20,          // 幽暗石林
    MagmaVeins = 21,          // 熔岩脉动
    JadeMine = 22,            // 翡翠矿洞
    DrownedLib = 23,          // 沉没图书馆
    ClockCore = 24,           // 钟楼核心
    AncientCrypt = 25,        // 古老墓穴
    CrystalLab = 26,          // 晶簇迷宫
    
    // === 特殊结构 C组 (C15-C21) ===
    FloatingIsle = 30,        // 漂浮群岛
    CoralRuin = 31,           // 珊瑚遗迹
    WhisperWood = 32,         // 叹息森林
    HolyArena = 33,           // 神圣竞技场
    HiveNest = 34,            // 腐蚀巢穴
    SkyPalace = 35,           // 云顶天宫
    AbyssalGap = 36,          // 深渊之渊
    
    COUNT
};

} // namespace NoMoreDay
```

### 2.3 JSON 配置示例

```json
{
  "version": 2,
  "biomes": [
    {
      "id": "floating_isle",
      "numeric_id": 30,
      "name": "漂浮群岛",
      "style": "special",
      
      "floorColor": [100, 150, 255, 255],
      "wallColor": [50, 80, 120, 255],
      "ambientColor": [200, 220, 255, 255],
      "backgroundShader": "shaders/sky_background.fs",
      
      "wallProbability": 0.42,
      "smoothIterations": 5,
      
      "features": ["air_wall"],
      "frictionMultiplier": 1.0,
      "gravityMultiplier": 1.0,
      "visionRadius": 0.0,
      
      "enemyPool": ["elves", "elemental"],
      "maxEnemies": 200,
      "isSafeZone": false
    },
    {
      "id": "abyssal_gap",
      "numeric_id": 36,
      "name": "深渊之渊",
      "style": "special",
      
      "floorColor": [20, 15, 30, 255],
      "wallColor": [10, 8, 15, 255],
      "ambientColor": [40, 30, 60, 255],
      
      "wallProbability": 0.35,
      "smoothIterations": 4,
      
      "features": ["limited_vision"],
      "visionRadius": 150.0,
      
      "enemyPool": ["undead", "demon"],
      "maxEnemies": 300,
      "isSafeZone": false
    }
  ]
}
```

---

## 3. 系统架构 (Architecture)

### 3.1 生成器类层级

```
┌─────────────────────────────────────────────────────────────┐
│                    MapGenerator (Abstract Base)             │
│  - virtual Generate(width, height, seed, params) = 0        │
└──────────────────────────┬──────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────────────┐
│ CaveMapGenerator│ │ MosaicMapGenerator│ │   BiomeMapGenerator    │
│ (现有，保留兼容) │ │  (现有，拼图用)  │ │     (新增，策略模式)     │
└─────────────────┘ └─────────────────┘ └───────────┬─────────────┘
                                                    │
                           ┌────────────────────────┼────────────────────────┐
                           │                        │                        │
                           ▼                        ▼                        ▼
                 ┌─────────────────┐ ┌────────────────────────┐ ┌────────────────────────┐
                 │ OpenBiomeStrategy│ │   MazeBiomeStrategy    │ │  SpecialBiomeStrategy  │
                 │  (A组：低墙率)   │ │   (B组：高墙率)        │ │ (C组：空气墙+特殊机制)  │
                 └─────────────────┘ └────────────────────────┘ └────────────────────────┘
```

### 3.2 BiomeMapGenerator 核心接口

```cpp
// ============================================================
// BiomeMapGenerator.hpp
// ============================================================
namespace NoMoreDay {

// 生成策略接口
class IBiomeStrategy {
public:
    virtual ~IBiomeStrategy() = default;
    
    virtual void GenerateTerrain(
        std::vector<Tile>& grid,
        int width, int height,
        const BiomeConfig& config,
        uint32_t seed
    ) = 0;
    
    virtual void PlaceSpecialStructures(
        std::vector<Tile>& grid,
        int width, int height,
        const BiomeConfig& config,
        uint32_t seed
    ) = 0;
};

// 开阔地策略 (A组)
class OpenBiomeStrategy : public IBiomeStrategy {
public:
    void GenerateTerrain(...) override;
    void PlaceSpecialStructures(...) override;
};

// 迷宫策略 (B组)
class MazeBiomeStrategy : public IBiomeStrategy {
public:
    void GenerateTerrain(...) override;
    void PlaceSpecialStructures(...) override;
private:
    void GenerateCorridors(...);  // 走廊生成
    void PlaceDeadEnds(...);      // 死胡同放置
};

// 特殊结构策略 (C组)
class SpecialBiomeStrategy : public IBiomeStrategy {
public:
    void GenerateTerrain(...) override;
    void PlaceSpecialStructures(...) override;
private:
    void GenerateFloatingPlatforms(...);  // 浮空平台
    void GenerateBridges(...);            // 连接桥梁
    void MarkAirWalls(...);               // 标记空气墙
};

// 群落地图生成器 (策略模式)
class BiomeMapGenerator : public MapGenerator {
public:
    MapData Generate(int width, int height, uint32_t seed,
                     float wallProb, int iterations) override;
    
    // 根据群落配置生成
    MapData GenerateForBiome(int width, int height, 
                             const BiomeConfig& config, 
                             uint32_t seed);

private:
    std::unique_ptr<IBiomeStrategy> CreateStrategy(BiomeStyle style);
    
    void ApplyFeatures(std::vector<Tile>& grid, int width, int height,
                       const BiomeConfig& config);
    void EnsureConnectivity(std::vector<Tile>& grid, int width, int height);
    void PlaceExits(std::vector<Tile>& grid, int width, int height, uint32_t seed);
};

} // namespace NoMoreDay
```

### 3.3 空气墙渲染管线

```cpp
// ============================================================
// AirWallRenderer.hpp - 空气墙渲染器
// ============================================================
namespace NoMoreDay {

class AirWallRenderer {
public:
    void Initialize(const std::string& backgroundShaderPath);
    void Shutdown();
    
    // 渲染空气墙区域
    void RenderBackground(const Camera2D& camera,
                         const std::vector<Tile>& grid,
                         int width, int height,
                         float time);
    
private:
    Shader m_backgroundShader;
    RenderTexture2D m_backgroundRT;
    
    // Uniform locations
    int m_locTime;
    int m_locCameraOffset;
    int m_locZoom;
};

} // namespace NoMoreDay
```

---

## 4. 渲染规格 (Rendering Specification)

### 4.1 空气墙渲染逻辑

```cpp
// 在 MapSystem::render() 中
void MapSystem::render(const Camera2D& camera) const {
    const auto& biome = BiomeRegistry::Get().GetBiome(m_currentBiomeId);
    
    for (int y = 0; y < m_mapData.height; ++y) {
        for (int x = 0; x < m_mapData.width; ++x) {
            const Tile& tile = m_mapData.grid[y * m_mapData.width + x];
            
            if (tile.type == Tile::Type::WALL) {
                if (biome.hasFeature(BiomeFeature::AirWall)) {
                    // 空气墙：不渲染墙壁纹理，让背景透过
                    // 由 AirWallRenderer 处理背景
                    continue;
                } else {
                    // 常规墙壁：渲染实体纹理
                    DrawRectangle(x * TILE_SIZE, y * TILE_SIZE,
                                  TILE_SIZE, TILE_SIZE, biome.wallColor);
                }
            } else {
                // 地板渲染
                DrawRectangle(x * TILE_SIZE, y * TILE_SIZE,
                              TILE_SIZE, TILE_SIZE, biome.floorColor);
            }
        }
    }
}
```

### 4.2 背景Shader示例 (星空/云海)

```glsl
// sky_background.fs
#version 430

in vec2 fragTexCoord;
out vec4 finalColor;

uniform float time;
uniform vec2 cameraOffset;
uniform float zoom;

void main() {
    vec2 uv = (fragTexCoord + cameraOffset * 0.001) * zoom;
    
    // 简单星空效果
    float stars = step(0.998, fract(sin(dot(uv * 200.0, vec2(12.9898, 78.233))) * 43758.5453));
    vec3 color = mix(vec3(0.02, 0.02, 0.08), vec3(0.8, 0.9, 1.0), stars);
    
    // 云雾层
    float clouds = sin(uv.x * 3.0 + time * 0.1) * cos(uv.y * 2.0 + time * 0.05) * 0.5 + 0.5;
    color += vec3(0.1, 0.15, 0.2) * clouds * 0.3;
    
    finalColor = vec4(color, 1.0);
}
```

### 4.3 视觉滤镜 (深渊迷雾)

```glsl
// abyss_fog.fs
#version 430

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;  // 场景渲染结果
uniform vec2 playerPos;      // 玩家位置 (屏幕空间)
uniform float visionRadius;  // 视野半径

void main() {
    vec4 sceneColor = texture(texture0, fragTexCoord);
    
    // 计算到玩家的距离
    float dist = length(fragTexCoord - playerPos);
    
    // 视野衰减
    float visibility = 1.0 - smoothstep(visionRadius * 0.7, visionRadius, dist);
    
    // 应用迷雾
    vec3 fogColor = vec3(0.02, 0.015, 0.03);
    finalColor = vec4(mix(fogColor, sceneColor.rgb, visibility), 1.0);
}
```

---

## 5. 物理与交互规格 (Physics & Interaction)

### 5.1 特殊机制实现

| 机制 | 实现方式 | 影响系统 |
|------|----------|----------|
| **空气墙** | `Tile.type == WALL` 但渲染跳过 | MapSystem, Renderer |
| **低重力** | `PhysicsComponent.gravityScale *= config.gravityMultiplier` | PhysicsSystem |
| **滑冰摩擦** | `MovementComponent.friction *= config.frictionMultiplier` | MovementSystem |
| **可破坏地形** | 新增 `DestructibleTileComponent` | DamageSystem, MapSystem |
| **动态刷怪墙** | 新增 `SpawnerWallComponent` | EnemySpawnSystem |
| **迷雾视野** | 后处理Shader + 限制敌人渲染 | FogOfWar, Renderer |
| **加速带** | 新增 `SpeedZoneComponent` (触发器) | MovementSystem |

### 5.2 DestructibleTileComponent

```cpp
// ============================================================
// DestructibleTileComponent.hpp
// ============================================================
struct DestructibleTileComponent {
    float maxHP = 100.0f;
    float currentHP = 100.0f;
    std::string debrisType = "wood";  // 破坏后的残骸类型
    bool isDestroyed = false;
    
    // 破坏后转变为什么Tile类型
    Tile::Type destroyedType = Tile::Type::FLOOR;
};
```

### 5.3 SpawnerWallComponent (腐蚀巢穴)

```cpp
// ============================================================
// SpawnerWallComponent.hpp - 动态刷怪墙壁
// ============================================================
struct SpawnerWallComponent {
    float spawnInterval = 5.0f;       // 刷怪间隔
    float spawnTimer = 0.0f;
    int maxSpawns = 10;               // 最大刷怪次数
    int currentSpawns = 0;
    std::vector<std::string> spawnPool;  // 可刷新的怪物类型
    bool isActive = true;
};
```

---

## 6. 怪物生态规格 (Monster Ecology)

### 6.1 种族-群落映射表

| 群落ID | 名称 | 主要种族 | 次要种族 |
|--------|------|----------|----------|
| `sun_prairie` | 炽阳草原 | BEAST | GOBLIN |
| `ice_tundra` | 永冻苔原 | UNDEAD | ELEMENTAL |
| `crimson_waste` | 血色荒野 | DEMON | CULTIST |
| `gloom_spire` | 幽暗石林 | UNDEAD | CULTIST |
| `magma_veins` | 熔岩脉动 | DEMON | ELEMENTAL |
| `floating_isle` | 漂浮群岛 | ELVES | ELEMENTAL |
| `hive_nest` | 腐蚀巢穴 | CORRUPTED | - |
| ... | ... | ... | ... |

### 6.2 EnemySpawnSystem 适配

```cpp
// EnemySpawnSystem 中的种族选择逻辑
void EnemySpawnSystem::selectRace(const BiomeConfig& config) {
    if (config.enemyPool.empty()) {
        m_availableRaces = {EnemyRace::UNDEAD};  // 默认
        return;
    }
    
    m_availableRaces.clear();
    for (const auto& raceName : config.enemyPool) {
        auto it = kRaceNameMap.find(raceName);
        if (it != kRaceNameMap.end()) {
            m_availableRaces.push_back(it->second);
        }
    }
}
```

---

## 7. 验收标准 (Acceptance Criteria)

### 7.1 数据层
- [ ] `biomes.json` 包含全部 27 种群落配置
- [ ] `BiomeRegistry` 正确加载所有配置
- [ ] 新 `BiomeID` 枚举与 JSON `numeric_id` 一一对应

### 7.2 生成算法
- [ ] A组地图墙率 15%-22%，开阔适合海量怪群
- [ ] B组地图墙率 38%-48%，具有走廊和死胡同
- [ ] C组地图具有空气墙和特殊结构

### 7.3 渲染与物理
- [ ] 空气墙地图背景正确渲染 (星空/云海)
- [ ] 深渊迷雾效果正确限制视野
- [ ] 低重力/滑冰摩擦力正确应用

### 7.4 怪物生态
- [ ] 每个群落仅刷新对应 `enemyPool` 中的种族
- [ ] 城镇安全区不刷怪

---

## 8. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **存档兼容** | 旧存档使用旧BiomeID | 保留 `Town=1`, `Cave=10` 的枚举值 |
| **性能** | 空气墙地图需额外背景渲染 | 使用简单Shader，限制透明区域 |
| **生成失败** | 特殊结构可能生成不连通 | `EnsureConnectivity()` 强制保证 |

---

*规格版本: 1.0*
*最后更新: 2026-02-08*
