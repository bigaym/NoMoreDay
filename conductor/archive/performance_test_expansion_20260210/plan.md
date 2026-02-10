# 性能测试扩展 - 实现计划

| 属性 | 值 |
|-----|---|
| **Track ID** | `performance_test_expansion_20260210` |
| **优先级** | **P1 MEDIUM** |
| **状态** | ✅ 已完成（Phase 0-4 全部完成） |
| **关联 Spec** | [spec.md](./spec.md) |

---

## 任务清单

### Phase 0: 基础设施准备 (Infrastructure)

> **目标**: 提取公共基准测试工具，为后续所有测试提供统一的测量框架。

- [x] **0.1** 创建 `tests/BenchmarkUtils.hpp`
  - **操作**: 新建文件
  - **内容**: 
    - 提取 `BenchmarkStats` 结构体（含 min, max, mean, median, p99）
    - 提取 `CalculateStats()` 函数
    - 新增 `ScopedTimer` RAII 计时器
    - 新增 `LOG_BENCHMARK(name, stats, target)` 宏，统一输出格式
  - **参考**: `tests/performance/RenderingBenchmark.cpp` 第 22-51 行
  - **预估**: 30min
  - **验收**: 头文件可被 include，无编译错误

- [x] **0.2** 重构已有 `RenderingBenchmark.cpp` 使用 `BenchmarkUtils.hpp`
  - **操作**: 修改 `tests/performance/RenderingBenchmark.cpp`
  - **内容**: 删除本地 `BenchmarkStats` 和 `CalculateStats` 定义，改为 `#include "BenchmarkUtils.hpp"`
  - **约束**: 不改变任何测试逻辑，仅替换工具函数来源
  - **预估**: 15min
  - **验收**: 编译通过，测试行为不变

- [x] **0.3** 重构已有 `GPUSyncBenchmark.cpp` 使用 `BenchmarkUtils.hpp`
  - **操作**: 修改 `tests/performance/GPUSyncBenchmark.cpp`
  - **内容**: 删除本地 `GPUSyncBenchmarkStats` 和 `CalculateGPUMetrics`，改用公共版本
  - **预估**: 15min
  - **验收**: 编译通过，测试行为不变

---

### Phase 1: P0 核心热路径 (Critical Hot Paths)

> **目标**: 覆盖 CPU 主循环 (60 FPS / 16.67ms) 中帧时间消耗最大的四个系统。  
> **依赖**: Phase 0 完成

- [x] **1.1** 创建 `DamagePipelineBenchmark.cpp`
  - **操作**: 新建 `tests/performance/DamagePipelineBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] DamagePipeline - Single Calculate`
      - Setup: 1 attacker (完整 CombatStats + 多个 DamageModifier), 1 defender
      - 构建含 Physical + Fire 的 `DamagePool`
      - 迭代 10000 次 `DamagePipeline::Calculate()`
      - 记录 Mean, P99
      - Target: Mean < 0.01ms per calculate
    - **Test B**: `[Performance] DamagePipeline - CalculateBatch 200 Targets`
      - Setup: 1 attacker, 200 defenders (各有不同抗性配置)
      - 使用 `tf::Executor` 进行并行化
      - 迭代 100 次
      - Target: Mean < 1.0ms, P99 < 2.0ms
    - **Test C**: `[Performance] DamagePipeline - Batch Scaling`
      - 对 50/100/200/500 个 defenders 分别测量
      - 验证线性度（500 目标不应超过 50 目标的 12 倍）
  - **关键 include**:
    - `game/systems/combat/DamagePipeline.hpp`
    - `game/components/Stats.hpp`
    - `game/components/Combat.hpp`
    - `game/components/Buff.hpp`
    - `<taskflow/taskflow.hpp>`
  - **预估**: 1.5h
  - **验收**: 三个 TEST_CASE 通过，输出包含 Mean/P99

- [x] **1.2** 创建 `PhysicsSystemBenchmark.cpp`
  - **操作**: 新建 `tests/performance/PhysicsSystemBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] PhysicsSystem - updateAll 10K Entities`
      - Setup: 10000 entities with Position + Velocity + Radius
      - SpatialHashGrid 初始化，5000x5000 世界
      - 用 `tf::Executor` 并行化
      - 迭代 100 帧
      - Target: Mean < 3.0ms, P99 < 5.0ms
    - **Test B**: `[Performance] PhysicsSystem - High Density Collision`
      - Setup: 5000 entities 聚集在 200x200 区域（高碰撞密度）
      - 迭代 100 帧
      - 记录碰撞解决耗时
    - **Test C**: `[Performance] PhysicsSystem - Force Fields`
      - Setup: 10000 entities + 50 ForceField 实体
      - `applyForceFields()` 迭代 100 帧
      - Target: Mean < 0.5ms
  - **关键 include**:
    - `engine/physics/PhysicsSystem.hpp`
    - `engine/physics/SIMDSpatialGrid.hpp`
    - `game/components/Common.hpp`
    - `<taskflow/taskflow.hpp>`
  - **预估**: 1.5h
  - **验收**: 三个 TEST_CASE 通过，输出包含 Mean/P99

- [x] **1.3** 创建 `ProjectileSystemBenchmark.cpp`
  - **操作**: 新建 `tests/performance/ProjectileSystemBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] ProjectileSystem - 500 Projectiles Update`
      - Setup: 500 个 Projectile 实体（含 Position, Velocity, Projectile 组件）
      - SpatialGrid 中有 2000 个敌人作为碰撞目标
      - 迭代 100 帧
      - Target: Mean < 1.0ms, P99 < 2.0ms
    - **Test B**: `[Performance] ProjectileSystem - 2000 Projectiles Stress`
      - 同 Test A，但 2000 弹射物
      - Target: Mean < 4.0ms
    - **Test C**: `[Performance] ProjectileSystem - Split Cascade`
      - Setup: 50 个 SplitOnHit 弹射物 + 目标
      - 记录单帧分裂级联的峰值耗时
  - **关键 include**:
    - `game/systems/skill/ProjectileSystem.hpp`
    - `game/components/Projectile.hpp`
    - `engine/physics/SIMDSpatialGrid.hpp`
    - `<taskflow/taskflow.hpp>`
  - **预估**: 1.5h
  - **验收**: 三个 TEST_CASE 通过

- [x] **1.4** 创建 `AISystemBenchmark.cpp`
  - **操作**: 新建 `tests/performance/AISystemBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] AISystem - 5000 Enemies Update`
      - Setup: 5000 个敌人（AIComponent + Position + Velocity），玩家在中心
      - SpatialHashGrid 初始化
      - Mock 流场：预填充 std::vector<Vector2> 
      - 迭代 100 帧
      - Target: Mean < 2.0ms, P99 < 4.0ms
    - **Test B**: `[Performance] AISystem - findNearestTarget Scaling`
      - Setup: 在 10000 个实体中查找最近目标
      - 分别测试 maxRange = 100/500/1000
      - 每组 1000 次查询
  - **关键 include**:
    - `game/systems/ai/AISystem.hpp`
    - `game/components/AIComponent.hpp`
    - `engine/physics/SpatialGrid.hpp`
  - **预估**: 1.5h
  - **验收**: 两个 TEST_CASE 通过

---

### Phase 2: P1 高频更新系统 (Frequent Update Systems)

> **目标**: 覆盖每帧更新但单帧耗时略低于 P0 的 CPU 系统，以及 GPU 渲染管线 (180 FPS / 5.56ms) 中的关键系统。  
> **依赖**: Phase 0 完成（与 Phase 1 可并行）

- [x] **2.1** 创建 `SkillSystemBenchmark.cpp`
  - **操作**: 新建 `tests/performance/SkillSystemBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] SkillSystem - Update 100 Active Skills`
      - Setup: 100 个实体各有 SkillBarComponent（6 技能槽, 各有冷却和状态）
      - 迭代 300 帧
      - Target: Mean < 0.5ms, P99 < 1.0ms
    - **Test B**: `[Performance] SkillSystem - UpdateCooldowns Batch`
      - Setup: 5000 实体 × 6 技能槽的 CD 更新
      - Target: Mean < 0.3ms
    - **Test C**: `[Performance] SkillSystem - GetEffectiveSkillTags`
      - 10000 次 Tag 合并查询
  - **关键 include**:
    - `game/systems/skill/SkillSystem.hpp`
    - `game/components/SkillBarComponent.hpp`
  - **预估**: 1h
  - **验收**: 三个 TEST_CASE 通过

- [x] **2.2** 创建 `HazardSystemBenchmark.cpp`
  - **操作**: 新建 `tests/performance/HazardSystemBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] HazardSystem - 200 Hazards Update`
      - Setup: 200 个 HazardComponent 实体 + 500 个碰撞目标
      - SpatialHashGrid 初始化
      - 迭代 100 帧
      - Target: Mean < 0.5ms, P99 < 1.0ms
    - **Test B**: `[Performance] HazardSystem - DealAreaDamage Stress`
      - 100 次 DealAreaDamage 调用（各 radius=100, 场景中 500 实体）
  - **关键 include**:
    - `game/systems/combat/HazardSystem.hpp`
    - `game/components/HazardComponent.hpp`（在 Combat.hpp 中）
    - `engine/physics/SpatialGrid.hpp`
  - **预估**: 1h
  - **验收**: 两个 TEST_CASE 通过

- [x] **2.3** 创建 `MonsterAffixBenchmark.cpp`
  - **操作**: 新建 `tests/performance/MonsterAffixBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] MonsterAffixSystem - 500 Affixed Enemies`
      - Setup: 500 个敌人，每个 2-4 个随机词缀（MonsterAffixComponent）
      - 含 Molten + Teleporter + Frozen + VoidZone 等类型
      - SpatialHashGrid 初始化
      - 迭代 100 帧
      - Target: Mean < 0.5ms, P99 < 1.0ms
  - **关键 include**:
    - `game/systems/combat/MonsterAffixSystem.hpp`
    - `game/components/MonsterAffixComponent.hpp`（在 EnemyComponent.hpp 中）
  - **预估**: 1h
  - **验收**: TEST_CASE 通过

- [x] **2.4** 创建 `FlowFieldBenchmark.cpp` **(GPU 渲染管线 - 180 FPS 帧预算)**
  - **操作**: 新建 `tests/performance/FlowFieldBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] GPUFlowFieldSystem - 256x256 Map Update`
      - Setup: 初始化 GPUFlowFieldSystem (256x256)
      - 提供全零 costMap + 目标位置
      - 迭代 50 帧
      - Target: Mean < 0.8ms, P99 < 1.5ms *(GPU 180 FPS 帧预算: 5.56ms)*
    - **Test B**: `[Performance] GPUFlowFieldSystem - CrowdDensity 5000`
      - Setup: 5000 个实体的 PersistentBuffer
      - 迭代 50 帧
      - Target: Mean < 0.5ms
  - **关键 include**:
    - `engine/render/GPUFlowFieldSystem.hpp`
    - `engine/render/PersistentBuffer.hpp`
    - `engine/resource/ResourceManager.hpp`
  - **⚠️ 注意**: 需要 OpenGL 上下文（main.cpp 已通过 InitWindow 提供）
  - **预估**: 1.5h
  - **验收**: 两个 TEST_CASE 通过

---

### Phase 3: P2 事件驱动 / 加载时系统 (Event-driven / Loading)

> **目标**: 覆盖非每帧但对用户体验有显著影响的系统（加载、存档、大规模生成）。  
> **依赖**: Phase 0 完成（与 Phase 1/2 可并行）

- [x] **3.1** 创建 `EnemySpawnBenchmark.cpp`
  - **操作**: 新建 `tests/performance/EnemySpawnBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] EnemySpawnSystem - Batch Spawn 100 Enemies`
      - Setup: 初始化 EnemySpawnSystem, 准备 100 个 EnemySpawnData
      - 记录 100 次 `spawnEnemy()` 的总耗时
      - Target: < 5.0ms total
    - **Test B**: `[Performance] EnemySpawnSystem - updateEnemySpawning`
      - Setup: 已有 200 个 spawn points, 模拟玩家移动触发刷怪
      - 迭代 100 帧
  - **关键 include**:
    - `game/systems/world/EnemySpawnSystem.hpp`
    - `game/components/EnemyComponent.hpp`
    - `game/components/AIComponent.hpp`
  - **预估**: 1h
  - **验收**: 两个 TEST_CASE 通过

- [x] **3.2** 创建 `SaveManagerBenchmark.cpp`
  - **操作**: 新建 `tests/performance/SaveManagerBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] SaveManager - createSnapshot (1000 items)`
      - Setup: 创建玩家实体，装备栏满 + 背包满 + 3个仓库 tab 满（~500 items）
      - 记录 `createSnapshot()` 耗时
      - 迭代 50 次
      - Target: Mean < 10.0ms
    - **Test B**: `[Performance] SaveManager - restoreFromSnapshot`
      - 从 snapshot 恢复
      - Target: Mean < 15.0ms
  - **关键 include**:
    - `engine/persistence/SaveManager.hpp`
    - `game/components/PlayerProfile.hpp`
    - `game/components/EquipmentComponent.hpp`
  - **预估**: 1h
  - **验收**: 两个 TEST_CASE 通过

- [x] **3.3** 创建 `ItemFactoryBenchmark.cpp`
  - **操作**: 新建 `tests/performance/ItemFactoryBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] ItemFactory - Batch Weapon Creation (1000)`
      - Setup: `ItemFactory::initialize()`, 然后循环 `createWeapon()` 1000 次
      - Target: < 5.0ms total
    - **Test B**: `[Performance] ItemFactory - Batch Armor Creation (1000)`
      - 同 Test A，替换为 `createArmor()`
    - **Test C**: `[Performance] ItemFactory - Legendary Affix Stress`
      - 1000 个传奇级物品（6 词缀 roll）
      - 记录词缀生成部分的耗时
  - **关键 include**:
    - `game/systems/item/ItemFactory.hpp`
    - `game/components/ItemComponent.hpp`
  - **预估**: 1h
  - **验收**: 三个 TEST_CASE 通过

- [x] **3.4** 创建 `FogOfWarBenchmark.cpp` **(GPU 渲染管线 - 180 FPS 帧预算)**
  - **操作**: 新建 `tests/performance/FogOfWarBenchmark.cpp`
  - **内容**:
    - **Test A**: `[Performance] FogOfWarSystem - updateVisibility 256x256`
      - Setup: 初始化 FogOfWarSystem (256x256)
      - 模拟玩家从 (0,0) 到 (256,256) 移动
      - 迭代 100 帧
      - Target: Mean < 0.3ms, P99 < 0.8ms *(GPU 180 FPS 帧预算: 5.56ms)*
    - **Test B**: `[Performance] FogOfWarSystem - syncToCPU`
      - 强制 GPU→CPU 回读
      - 记录回读延迟
  - **关键 include**:
    - `game/systems/world/FogOfWarSystem.hpp`
    - `engine/resource/ResourceManager.hpp`
  - **⚠️ 注意**: 需要 OpenGL 上下文
  - **预估**: 1h
  - **验收**: 两个 TEST_CASE 通过

---

### Phase 4: 验证与回归 (Validation & Regression)

> **目标**: 确保所有 Phase 完成后的整体回归和 DropSystem 恢复。  
> **依赖**: Phase 1-3 完成

- [x] **4.1** 修复 `DropSystemBenchmark.cpp` 的 `skip(true)` 标记
  - **操作**: 修改 `tests/performance/DropSystemBenchmark.cpp`
  - **内容**: 移除 `doctest::skip(true)`，确保测试可以正常执行
  - **验证**: 运行测试，确认无崩溃
  - **预估**: 15min
  - **验收**: DropSystem benchmark 正常运行并输出耗时

- [x] **4.2** 全量编译验证
  - **操作**: 运行 `.\build.bat`
  - **内容**: 确认所有 21 个性能测试文件（9 旧 + 12 新）可编译
  - **预估**: 15min
  - **验收**: 编译零错误、零警告

- [x] **4.3** 全量运行验证
  - **操作**: 运行 `./bin/NoMoreDayTests.exe -tc="[Performance]*"`
  - **内容**: 
    - 确认所有性能测试可执行
    - 收集所有系统的 baseline 数据
    - 记录到 `conductor/tracks/performance_test_expansion/baseline.md`
  - **预估**: 30min
  - **验收**: 所有测试运行完毕，baseline 数据已记录

---

## 任务依赖图

```
Phase 0 (基础设施)
   ├── 0.1 BenchmarkUtils.hpp
   ├── 0.2 重构 RenderingBenchmark
   └── 0.3 重构 GPUSyncBenchmark
         │
         ▼
   ┌─────┴──────────────────┐
   │                        │
Phase 1 (P0 热路径)    Phase 2 (P1 频繁更新)    Phase 3 (P2 事件驱动)
   ├── 1.1 DamagePipeline    ├── 2.1 SkillSystem     ├── 3.1 EnemySpawn
   ├── 1.2 PhysicsSystem     ├── 2.2 HazardSystem    ├── 3.2 SaveManager
   ├── 1.3 ProjectileSystem  ├── 2.3 MonsterAffix    ├── 3.3 ItemFactory
   └── 1.4 AISystem          └── 2.4 FlowField       └── 3.4 FogOfWar
         │                        │                        │
         └────────────────────────┼────────────────────────┘
                                  ▼
                          Phase 4 (验证)
                            ├── 4.1 DropSystem fix
                            ├── 4.2 全量编译
                            └── 4.3 全量运行
```

## 工时汇总

| Phase | 任务数 | 预估工时 |
|-------|--------|---------|
| Phase 0: 基础设施 | 3 | ~1h |
| Phase 1: P0 热路径 | 4 | ~6h |
| Phase 2: P1 高频更新 | 4 | ~4.5h |
| Phase 3: P2 事件驱动 | 4 | ~4h |
| Phase 4: 验证回归 | 3 | ~1h |
| **总计** | **18** | **~16.5h (2-3 工作日)** |

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 某些系统初始化需要完整上下文 | 测试 Setup 复杂度高 | 使用 `TestSetupScope` RAII + Mock 最小依赖 |
| GPU 测试需要 OpenGL 上下文 | CI 可能无 GPU | main.cpp 已初始化 Raylib Window；标记 GPU 测试可选 |
| Taskflow Executor 生命周期 | 多测试共享可能冲突 | 每个 TEST_CASE 使用局部 `tf::Executor` |
| Unity Build 下符号冲突 | 多个 benchmark 使用相同 helper 名 | 所有 helper 放入匿名 namespace 或独立 namespace |
