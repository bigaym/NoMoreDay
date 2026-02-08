# 生物群落地图生成系统 实施计划 (V1.0)

> **Track ID**: `biome_generation_system_20260208`
> **依赖 Spec**: `spec.md` (V1.0)
> **预计工时**: 7-10 天

---

## 📌 阶段总览

| 阶段 | 子Track | 名称 | 核心产出 | 预计工时 | 状态 |
|------|---------|------|----------|----------|------|
| **Phase 1** | 1.x | 数据驱动层 | BiomeConfig扩展, JSON解析, 枚举定义 | 1 天 | ⏳ 待开始 |
| **Phase 2** | 2.x | 渲染与物理增强 | 空气墙渲染器, 背景Shader, 特殊物理 | 2 天 | ⏳ 待开始 |
| **Phase 3** | 3.x | 生成算法演进 | 三类策略生成器, CA参数调优 | 2-3 天 | ⏳ 待开始 |
| **Phase 4** | 4.x | 动态交互逻辑 | 可破坏地形, 动态刷怪墙, 加速带 | 1.5 天 | ⏳ 待开始 |
| **Phase 5** | 5.x | 生态集成 | 怪物池映射, 掉落表关联 | 1 天 | ⏳ 待开始 |
| **Phase 6** | 6.x | 测试与打磨 | 全群落测试, 性能基准, Bug修复 | 1 天 | ⏳ 待开始 |

---

## 🔴 Sub-Track 1: 数据驱动层 (Foundation)

> **目标**: 建立完整的群落数据基础设施

### Task 1.1: BiomeID 枚举扩展 ⏱️ 0.5h
- [x] 在 `Common.hpp` 中扩展 `BiomeID` 枚举
- [x] 添加所有 27 种群落ID (城镇6 + 战斗21)
- [x] 确保 `Town=1`, `Cave=10` 保持兼容

**文件修改**:
```
src/game/components/Common.hpp
```

### Task 1.2: BiomeStyle 和 BiomeFeature 定义 ⏱️ 0.5h
- [x] 定义 `BiomeStyle` 枚举 (Town, Open, Maze, Special)
- [x] 定义 `BiomeFeature` 位掩码枚举
- [x] 添加辅助宏/函数用于位操作

**文件修改**:
```
src/game/components/Common.hpp
```

### Task 1.3: BiomeConfig 结构体扩展 ⏱️ 1h
- [x] 扩展 `BiomeConfig` 添加新字段
- [x] 添加 `style`, `features`, 特殊机制参数
- [x] 添加视觉属性 (ambientColor, backgroundShader, visualFilterShader)
- [x] 实现 `hasFeature()` 便捷方法

**文件修改**:
```
src/game/data/BiomeRegistry.hpp
src/game/data/BiomeRegistry.cpp
```

### Task 1.4: biomes.json 扩展与解析 ⏱️ 2h
- [x] 升级 `biomes.json` schema 到 version 2
- [x] 添加全部 27 种群落配置
- [x] 实现新字段的 JSON 解析逻辑
- [x] 添加 `features` 字符串数组到位掩码的转换

**文件修改**:
```
assets/data/biomes.json
src/game/data/BiomeRegistry.cpp (LoadFromJSON方法)
```

### Task 1.5: 单元测试 - BiomeRegistry ⏱️ 0.5h
- [x] 测试 JSON 加载完整性
- [x] 测试 `GetBiome(BiomeID)` 和 `GetBiome(std::string)`
- [x] 测试 `hasFeature()` 位掩码逻辑

---

## 🟠 Sub-Track 2: 渲染与物理增强 (Visual & Physics)

> **目标**: 实现空气墙渲染管线和特殊物理效果

### Task 2.1: 空气墙标记系统 ⏱️ 1h
- [ ] 扩展 `Tile` 结构添加 `isAirWall` 标记 (或使用现有type+biome判断)
- [ ] 修改 `MapSystem::render()` 跳过空气墙瓦片的墙壁渲染
- [ ] 确保物理层仍然阻挡移动

**文件修改**:
```
src/game/components/MapComponent.hpp
src/game/systems/world/MapSystem.cpp
```

### Task 2.2: AirWallRenderer 实现 ⏱️ 2h
- [ ] 创建 `AirWallRenderer` 类
- [ ] 实现 `Initialize()`: 加载背景Shader
- [ ] 实现 `RenderBackground()`: 渲染星空/云海背景
- [ ] 集成到渲染管线 (在地图渲染之前)

**新建文件**:
```
src/game/systems/render/AirWallRenderer.hpp
src/game/systems/render/AirWallRenderer.cpp
```

### Task 2.3: 背景Shader编写 ⏱️ 1.5h
- [ ] `sky_background.fs`: 星空效果 (用于漂浮群岛、云顶天宫)
- [ ] `coral_background.fs`: 深海效果 (用于珊瑚遗迹)
- [ ] 统一 uniform 接口 (time, cameraOffset, zoom)

**新建文件**:
```
assets/shaders/backgrounds/sky_background.fs
assets/shaders/backgrounds/coral_background.fs
```

### Task 2.4: 视觉滤镜后处理 ⏱️ 1.5h
- [ ] `abyss_fog.fs`: 深渊迷雾效果
- [ ] `coral_filter.fs`: 蓝色深海滤镜
- [ ] 修改渲染管线支持后处理Shader

**新建文件**:
```
assets/shaders/filters/abyss_fog.fs
assets/shaders/filters/coral_filter.fs
```

### Task 2.5: 特殊物理组件 ⏱️ 1h
- [ ] 实现低重力: `gravityMultiplier` 应用到 PhysicsSystem
- [ ] 实现摩擦力修改: `frictionMultiplier` 应用到 MovementSystem
- [ ] 创建 `BiomeEffectComponent` 缓存当前群落效果

**文件修改**:
```
src/game/systems/physics/PhysicsSystem.cpp
src/game/systems/combat/MovementSystem.cpp
```

---

## 🟡 Sub-Track 3: 生成算法演进 (Generation Algorithms)

> **目标**: 实现三类风格的地图生成策略

### Task 3.1: IBiomeStrategy 接口定义 ⏱️ 0.5h
- [ ] 定义 `IBiomeStrategy` 虚基类
- [ ] 定义 `GenerateTerrain()` 和 `PlaceSpecialStructures()` 接口
- [ ] 定义通用参数结构体

**新建文件**:
```
src/game/systems/world/BiomeStrategies.hpp
```

### Task 3.2: OpenBiomeStrategy (A组) ⏱️ 2h
- [ ] 实现低墙率 CA 生成 (wallProb 0.15-0.22)
- [ ] 减少平滑迭代次数 (2-3次) 保持空旷
- [ ] 稀疏障碍物放置
- [ ] 验证: 生成地图 >70% 为地板

**新建文件**:
```
src/game/systems/world/OpenBiomeStrategy.cpp
```

### Task 3.3: MazeBiomeStrategy (B组) ⏱️ 3h
- [ ] 实现高墙率 CA 生成 (wallProb 0.38-0.48)
- [ ] 增加平滑迭代次数 (5-6次) 形成走廊
- [ ] 走廊宽度控制 (2-3格)
- [ ] 死胡同检测与标记 (用于宝箱放置)
- [ ] 验证: 生成地图有明显的通道结构

**新建文件**:
```
src/game/systems/world/MazeBiomeStrategy.cpp
```

### Task 3.4: SpecialBiomeStrategy (C组) ⏱️ 4h
- [ ] 实现浮空平台生成 (FloatingIsle, SkyPalace)
- [ ] 实现桥梁连接算法
- [ ] 实现中心竞技场 (HolyArena)
- [ ] 实现动态刷怪墙放置 (HiveNest)
- [ ] 空气墙区域标记

**新建文件**:
```
src/game/systems/world/SpecialBiomeStrategy.cpp
```

### Task 3.5: BiomeMapGenerator 集成 ⏱️ 1h
- [ ] 创建 `BiomeMapGenerator` 类
- [ ] 实现 `CreateStrategy(BiomeStyle)` 工厂方法
- [ ] 实现 `GenerateForBiome()` 主入口
- [ ] 集成到 `MapSystem::generateMap()`

**新建文件**:
```
src/game/systems/world/BiomeMapGenerator.hpp
src/game/systems/world/BiomeMapGenerator.cpp
```

### Task 3.6: 连通性与出口放置 ⏱️ 1h
- [ ] 确保所有策略生成的地图连通
- [ ] 统一出口放置逻辑 (楼梯、传送门)
- [ ] 特殊结构出口位置验证

---

## 🟢 Sub-Track 4: 动态交互逻辑 (Dynamic Interactions)

> **目标**: 实现群落特殊机制

### Task 4.1: DestructibleTileComponent ⏱️ 1.5h
- [ ] 定义 `DestructibleTileComponent` 组件
- [ ] 实现可破坏瓦片的伤害接收
- [ ] 实现破坏后地形变化 (WALL -> FLOOR)
- [ ] 添加破坏特效和残骸

**新建文件**:
```
src/game/components/DestructibleTileComponent.hpp
```

**文件修改**:
```
src/game/systems/combat/DamageSystem.cpp
src/game/systems/world/MapSystem.cpp
```

### Task 4.2: SpawnerWallComponent ⏱️ 2h
- [ ] 定义 `SpawnerWallComponent` 组件
- [ ] 实现定时刷怪逻辑
- [ ] 与 `EnemySpawnSystem` 集成
- [ ] 添加视觉效果 (触手墙壁动画)

**新建文件**:
```
src/game/components/SpawnerWallComponent.hpp
```

**文件修改**:
```
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 4.3: SpeedZoneComponent ⏱️ 1h
- [ ] 定义 `SpeedZoneComponent` 组件
- [ ] 实现加速带触发逻辑
- [ ] 与 `MovementSystem` 集成
- [ ] 添加视觉效果 (流线粒子)

**新建文件**:
```
src/game/components/SpeedZoneComponent.hpp
```

### Task 4.4: 迷雾视野系统 ⏱️ 1.5h
- [ ] 修改 `FogOfWar` 支持群落配置的视野限制
- [ ] 实现 `visionRadius` 参数应用
- [ ] 限制超出视野范围的敌人渲染
- [ ] 与后处理滤镜联动

**文件修改**:
```
src/game/systems/world/MapSystem.cpp (updateVisibility)
src/game/systems/render/RenderSystem.cpp
```

---

## 🔵 Sub-Track 5: 生态集成 (Monster Ecology)

> **目标**: 完成怪物池与群落的绑定

### Task 5.1: 种族名称映射表 ⏱️ 0.5h
- [ ] 创建 `kRaceNameMap` 静态映射表
- [ ] 支持 JSON 中的字符串到 `EnemyRace::Type` 转换
- [ ] 添加大小写不敏感匹配

**文件修改**:
```
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 5.2: EnemySpawnSystem 适配 ⏱️ 1.5h
- [ ] 修改 `initData()` 读取群落 `enemyPool`
- [ ] 实现 `selectRace()` 基于权重随机选择
- [ ] 验证城镇 `isSafeZone` 不刷怪

**文件修改**:
```
src/game/systems/world/EnemySpawnSystem.hpp
src/game/systems/world/EnemySpawnSystem.cpp
```

### Task 5.3: 掉落表关联 (可选) ⏱️ 1h
- [ ] 扩展 `LootTable` 支持群落修正
- [ ] 特定群落增加特定材料掉率
- [ ] 与 `ItemDropSystem` 集成

**文件修改**:
```
src/game/systems/loot/ItemDropSystem.cpp
```

---

## 🟣 Sub-Track 6: 测试与打磨 (Testing & Polish)

> **目标**: 确保系统稳定性和体验完整性

### Task 6.1: 单元测试 - 生成算法 ⏱️ 1h
- [ ] 测试 A组地图墙率在 15%-22%
- [ ] 测试 B组地图走廊连通性
- [ ] 测试 C组地图空气墙正确标记
- [ ] 测试所有地图 `EnsureConnectivity()` 通过

### Task 6.2: 集成测试 - 全群落遍历 ⏱️ 1.5h
- [ ] 编写测试脚本遍历所有 27 种群落
- [ ] 验证每个群落正常加载、渲染、刷怪
- [ ] 截图存档用于回归测试

### Task 6.3: 性能基准 ⏱️ 0.5h
- [ ] 256x256 地图生成时间 < 100ms
- [ ] 空气墙渲染 FPS 下降 < 5%
- [ ] 迷雾视野渲染开销验证

### Task 6.4: Bug 修复与细节打磨 ⏱️ 1h
- [ ] 修复发现的问题
- [ ] 调优视觉参数 (颜色、光效)
- [ ] 完善错误处理与日志

---

## 📋 任务依赖图

```
Phase 1 (数据层)
    │
    ├── 1.1 BiomeID枚举 ──┐
    ├── 1.2 Style/Feature ┼── 1.3 BiomeConfig扩展 ── 1.4 JSON解析 ── 1.5 单元测试
    │
    v
Phase 2 (渲染/物理) ────────────────────────────────┐
    │                                               │
    ├── 2.1 空气墙标记 ── 2.2 AirWallRenderer       │
    ├── 2.3 背景Shader ──┘                          │
    ├── 2.4 视觉滤镜                                │
    └── 2.5 特殊物理                                │
                                                    │
Phase 3 (生成算法) ─────────────────────────────────┤
    │                                               │
    ├── 3.1 IBiomeStrategy接口                      │
    ├── 3.2 OpenBiomeStrategy (A组)                 │
    ├── 3.3 MazeBiomeStrategy (B组)                 │
    ├── 3.4 SpecialBiomeStrategy (C组) ─────────────┤
    ├── 3.5 BiomeMapGenerator集成                   │
    └── 3.6 连通性验证                              │
                                                    │
Phase 4 (动态交互) ─────────────────────────────────┤
    │                                               │
    ├── 4.1 DestructibleTile                        │
    ├── 4.2 SpawnerWall                             │
    ├── 4.3 SpeedZone                               │
    └── 4.4 迷雾视野                                │
                                                    │
Phase 5 (生态集成) ─────────────────────────────────┤
    │                                               │
    ├── 5.1 种族映射表                              │
    ├── 5.2 EnemySpawnSystem适配                    │
    └── 5.3 掉落表关联                              │
                                                    │
                                                    v
Phase 6 (测试打磨) ─────────────────────────────────
    │
    ├── 6.1 单元测试
    ├── 6.2 全群落集成测试
    ├── 6.3 性能基准
    └── 6.4 Bug修复
```

---

## 📝 备注

1. **并行可行性**: Phase 2/3/4/5 在 Phase 1 完成后可部分并行开发
2. **风险项**: 特殊策略生成 (Task 3.4) 复杂度最高，预留额外缓冲时间
3. **MVP 策略**: 若时间紧张，可优先完成 A组+B组，C组特殊机制后续迭代

---

*计划版本: 1.0*
*最后更新: 2026-02-08*
