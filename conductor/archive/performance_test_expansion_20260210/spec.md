# 性能测试扩展 - 技术规格书

## 1. 概述

### 1.1 背景

当前 `tests/performance/` 目录包含 9 个性能基准测试文件，覆盖了 GPU 渲染管线（粒子、弹出文字、MDI、实体同步）、空间网格、仓库系统、属性计算、群落生成和无分支优化。但对核心游戏逻辑的**每帧热路径**（战斗、物理、AI、弹射物、技能）缺乏覆盖，这些系统恰好是帧时间预算的最大消耗者。

### 1.2 设计目标

| 目标 | 描述 | 优先级 |
|------|------|--------|
| **伤害管线批处理** | 对 `DamagePipeline::CalculateBatch` 的 SIMD + Taskflow 并行路径进行基准测试 | **P0** |
| **物理系统并行化** | 对 `PhysicsSystem::updateAll` 的 Taskflow 并行碰撞+移动进行基准测试 | **P0** |
| **弹射物系统** | 对 `ProjectileSystem::Update` 的空间查询+碰撞检测进行基准测试 | **P0** |
| **AI 系统** | 对 `AISystem::update` 的流场查询+行为决策树进行基准测试 | **P0** |
| **技能系统** | 对 `SkillSystem::Update` 的冷却管理+状态机进行基准测试 | **P1** |
| **危险区域系统** | 对 `HazardSystem::Update` 的多类型危险区域处理进行基准测试 | **P1** |
| **怪物词缀系统** | 对 `MonsterAffixSystem::Update` 的复杂逻辑迭代进行基准测试 | **P1** |
| **GPU 流场系统** | 对 `GPUFlowFieldSystem::Update` 的 Compute Shader 调度进行基准测试 | **P1** |
| **敌人批量生成** | 对 `EnemySpawnSystem::spawnEnemy` 的大规模实体创建进行基准测试 | **P2** |
| **存档序列化** | 对 `SaveManager::createSnapshot/restoreFromSnapshot` 的序列化速度进行基准测试 | **P2** |
| **物品工厂批量生成** | 对 `ItemFactory` 的批量武器/装备创建进行基准测试 | **P2** |
| **战雾系统** | 对 `FogOfWarSystem::updateVisibility` 的 Compute Shader 调度进行基准测试 | **P2** |

---

## 2. 已有覆盖分析

### 2.1 已覆盖系统

| 文件 | 覆盖系统 | 关键指标 |
|------|---------|---------|
| `RenderingBenchmark.cpp` | ParticleSystem, PopupRenderer, GPUEntitySystem | Mean/P99, < 0.5ms/0.3ms/3.0ms |
| `GPUSyncBenchmark.cpp` | GPUPhysicsSync, GPUVisualSync | Mean/P99, < 1.5ms/0.5ms |
| `MDIRenderBenchmark.cpp` | MDI vs Legacy 渲染 | CPU+GPU 耗时对比 |
| `SpatialGridBenchmark.cpp` | SIMDSpatialGrid rebuild + query | us 级别统计 |
| `StashBenchmark.cpp` | Stash sort/search/auto-deposit | us 级别统计 |
| `StatsBenchmark.cpp` | StatsSystem Recalculate + GetStatWithTags | us/entity, us/query |
| `BiomeGenerationBenchmark.cpp` | BiomeMapGenerator 256x256 | ms/generation |
| `BranchlessBenchmark.cpp` | SelectF vs if-else | us 对比 |
| `DropSystemBenchmark.cpp` | DropSystem (⚠️ 已 `skip(true)`) | us 统计 |

### 2.2 覆盖缺口（按帧时间影响排序）

```
┌─────────────────────────────────────────────────────────────────────┐
│ CPU 逻辑循环帧时间预算 (16.67ms for 60 FPS)                         │
│                                                                     │
│ ■■■■ PhysicsSystem::updateAll        (~2-4ms)  ❌ 未覆盖            │
│ ■■■  AISystem::update                (~1-3ms)  ❌ 未覆盖            │
│ ■■■  ProjectileSystem::Update        (~1-3ms)  ❌ 未覆盖            │
│ ■■■  DamagePipeline::CalculateBatch  (~1-2ms)  ❌ 未覆盖            │
│ ■■   CombatSystem::update            (~0.5-2ms) ❌ 未覆盖           │
│ ■■   SkillSystem::Update             (~0.5-1ms) ❌ 未覆盖           │
│ ■■   HazardSystem::Update            (~0.5-1ms) ❌ 未覆盖           │
│ ■    MonsterAffixSystem::Update      (~0.3-1ms) ❌ 未覆盖           │
├─────────────────────────────────────────────────────────────────────┤
│ GPU 渲染帧时间预算 (5.56ms for 180 FPS)                             │
│                                                                     │
│ ■■   GPUFlowFieldSystem::Update      (~0.2-1ms) ❌ 未覆盖          │
│ ■    FogOfWarSystem::updateVisibility (~0.1-0.5ms) ❌ 未覆盖        │
│ ░    GPUEntitySystem::Update         (~2.8ms)  ✅ 已覆盖            │
│ ░    GPUEntitySync                   (~1.5ms)  ✅ 已覆盖            │
│ ░    ParticleSystem::Update          (~0.3ms)  ✅ 已覆盖            │
│ ░    PopupRenderer::Render           (~0.2ms)  ✅ 已覆盖            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 测试基础设施规范

### 3.1 统一基准测试框架

所有新增测试**必须**复用 `RenderingBenchmark.cpp` 中已定义的 `BenchmarkStats` 和 `CalculateStats` 工具。为了避免重复定义，将其提取到公共头文件：

```cpp
// tests/BenchmarkUtils.hpp
#pragma once

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

namespace NoMoreDay::tests {

struct BenchmarkStats {
  double min_ms;
  double max_ms;
  double mean_ms;
  double median_ms;
  double p99_ms;
};

inline BenchmarkStats CalculateStats(const std::vector<double>& samples) {
  if (samples.empty()) return {0, 0, 0, 0, 0};
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
  double mean = sum / sorted.size();

  size_t medianIdx = sorted.size() / 2;
  double median = sorted[medianIdx];

  size_t idx99 = static_cast<size_t>(sorted.size() * 0.99);
  if (idx99 >= sorted.size()) idx99 = sorted.size() - 1;

  return {sorted.front(), sorted.back(), mean, median, sorted[idx99]};
}

// RAII scope timer for individual measurements
class ScopedTimer {
public:
  ScopedTimer(std::vector<double>& target) : m_target(target) {
    m_start = std::chrono::high_resolution_clock::now();
  }
  ~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    m_target.push_back(
      std::chrono::duration<double, std::milli>(end - m_start).count());
  }
private:
  std::vector<double>& m_target;
  std::chrono::high_resolution_clock::time_point m_start;
};

} // namespace NoMoreDay::tests
```

### 3.2 标准测试参数

| 参数 | 值 | 说明 |
|------|---|------|
| `WARMUP_FRAMES` | 10 | 预热帧数，排除冷启动噪声 |
| `BENCH_FRAMES` | 100-300 | 基准帧数，根据系统开销调整 |
| `CPU_TARGET_FPS` | 60 | **CPU 逻辑循环**目标帧率 (16.67ms 帧预算) |
| `GPU_TARGET_FPS` | 180 | **GPU 渲染**最低保证帧率 (5.56ms 帧预算) |
| `CPU_DT` | 1.0f/60.0f | CPU 逻辑固定时间步长 |
| `ENTITY_COUNT` | 参见各测试 | 不同场景使用不同规模 |

### 3.3 性能基线目标

基线目标基于 GEMINI.md 中记录的已验证数据（Intel Iris Xe GPU）。

**帧预算约束**:
- **CPU 逻辑循环**: 所有 CPU 系统单帧总和 **≤ 16.67ms** (60 FPS)
- **GPU 渲染管线**: 所有 GPU 系统单帧总和 **≤ 5.56ms** (180 FPS)

#### 3.3.1 CPU 逻辑系统 (60 FPS / 16.67ms 帧预算)

| 系统 | 实体规模 | Mean 目标 | P99 目标 |
|------|---------|----------|---------|
| DamagePipeline::CalculateBatch | 200 defenders | < 1.0ms | < 2.0ms |
| PhysicsSystem::updateAll | 10000 entities | < 3.0ms | < 5.0ms |
| ProjectileSystem::Update | 500 projectiles | < 1.0ms | < 2.0ms |
| AISystem::update | 5000 enemies | < 2.0ms | < 4.0ms |
| SkillSystem::Update | 100 active skills | < 0.5ms | < 1.0ms |
| HazardSystem::Update | 200 hazards | < 0.5ms | < 1.0ms |
| MonsterAffixSystem::Update | 500 affixed enemies | < 0.5ms | < 1.0ms |
| EnemySpawnSystem batch | 100 spawns | < 5.0ms | - |
| SaveManager snapshot | 1000 items | < 10.0ms | - |
| ItemFactory batch | 1000 items | < 5.0ms | - |

#### 3.3.2 GPU 渲染系统 (180 FPS / 5.56ms 帧预算)

| 系统 | 规模 | Mean 目标 | P99 目标 |
|------|------|----------|---------|
| GPUFlowFieldSystem::Update | 256x256 map | < 0.8ms | < 1.5ms |
| FogOfWarSystem::updateVisibility | 256x256 map | < 0.3ms | < 0.8ms |
| GPUEntitySystem::Update (已有) | 20000 entities | < 2.8ms | < 4.0ms |
| GPUParticleSystem::Update (已有) | 10k emission/s | < 0.3ms | < 0.5ms |
| PopupRenderer::Render (已有) | 50/frame | < 0.2ms | < 0.3ms |
| **GPU 总和** | - | **≤ 4.4ms** | **≤ 5.56ms** |

---

## 4. 各测试详细规范

### 4.1 P0: DamagePipelineBenchmark

**文件**: `tests/performance/DamagePipelineBenchmark.cpp`

**测试场景**:
1. **Single Calculate** - 单体伤害计算（含完整的元素转换、暴击、减伤流程）
2. **CalculateBatch (200 targets)** - 批量伤害计算（SIMD + Taskflow）
3. **CalculateBatch scaling** - 50/100/200/500 目标的线性度检测

**关键依赖**:
- `DamagePipeline::Calculate` / `CalculateBatch`
- `entt::registry` + `CombatStats` + `DamagePool`
- `tf::Executor` (多线程)

**Setup**: 创建 attacker（完整 CombatStats + Modifiers）, N 个 defenders, 构建 `DamagePool`。

---

### 4.2 P0: PhysicsSystemBenchmark

**文件**: `tests/performance/PhysicsSystemBenchmark.cpp`

**测试场景**:
1. **updateAll (10K entities)** - 完整物理更新循环（含 Taskflow 并行化）
2. **resolveCollisions stress** - 高密度碰撞场景
3. **applyForceFields (50 fields, 10K entities)** - 力场覆盖区域压力测试

**关键依赖**:
- `PhysicsSystem::updateAll`
- `SpatialHashGrid`
- `MapSystem` (可 mock 地图边界)
- `tf::Executor`

---

### 4.3 P0: ProjectileSystemBenchmark

**文件**: `tests/performance/ProjectileSystemBenchmark.cpp`

**测试场景**:
1. **Update (500 projectiles)** - 标准弹射物更新（含空间查询碰撞检测）
2. **High density (2000 projectiles)** - 极端场景压力测试
3. **Split/Explosion cascade** - 分裂弹+爆炸弹级联场景

**关键依赖**:
- `ProjectileSystem::Update`
- `SIMDSpatialGrid` / `SpatialHashGrid`
- `tf::Executor`

---

### 4.4 P0: AISystemBenchmark

**文件**: `tests/performance/AISystemBenchmark.cpp`

**测试场景**:
1. **update (5000 enemies)** - 全量 AI 更新（含流场查询 + 行为决策）
2. **updateAIEntity heavy** - 单体重计算（多状态、多行为分支）
3. **findNearestTarget scaling** - 100/500/1000 查询的线性度

**关键依赖**:
- `AISystem::update` / `updateAIEntity`
- `SpatialHashGrid`
- `GPUFlowFieldSystem` (需 mock 或预计算流场)
- `MapSystem`

---

### 4.5 P1: SkillSystemBenchmark

**文件**: `tests/performance/SkillSystemBenchmark.cpp`

**测试场景**:
1. **Update (100 active skills)** - 冷却管理 + 状态机更新
2. **UpdateCooldowns batch** - 6 技能槽 × 5000 实体的 CD 更新
3. **GetEffectiveSkillTags** - 标签合并查询性能

---

### 4.6 P1: HazardSystemBenchmark

**文件**: `tests/performance/HazardSystemBenchmark.cpp`

**测试场景**:
1. **Update (200 hazards)** - 完整危险区域更新
2. **DealAreaDamage stress** - 100 个 AOE × 500 实体的空间查询
3. **ProcessFrozenOrbs (50 orbs)** - 追踪冰球更新

---

### 4.7 P1: MonsterAffixBenchmark

**文件**: `tests/performance/MonsterAffixBenchmark.cpp`

**测试场景**:
1. **Update (500 affixed enemies)** - 完整词缀处理循环
2. **Mixed affixes stress** - 每个敌人 3-4 种不同词缀的处理

---

### 4.8 P1: FlowFieldBenchmark

**文件**: `tests/performance/FlowFieldBenchmark.cpp`

**测试场景**:
1. **Update (256x256 map)** - 完整流场 Compute Shader 调度
2. **UpdateCrowdDensity (5000 entities)** - 人群密度计算

**注意**: 此测试需要 OpenGL 上下文，可能需要与 MDIRenderBenchmark 类似的初始化流程。

---

### 4.9 P2: EnemySpawnBenchmark

**文件**: `tests/performance/EnemySpawnBenchmark.cpp`

**测试场景**:
1. **Batch spawn 100 enemies** - 大规模实体创建（含完整组件组装）
2. **updateEnemySpawning (full map)** - 完整刷怪循环

---

### 4.10 P2: SaveManagerBenchmark

**文件**: `tests/performance/SaveManagerBenchmark.cpp`

**测试场景**:
1. **createSnapshot (full inventory + 10 stash tabs)** - 序列化快照
2. **restoreFromSnapshot** - 反序列化恢复
3. **saveCharacterAsync** - 异步写入（含 JSON 构建时间）

---

### 4.11 P2: ItemFactoryBenchmark

**文件**: `tests/performance/ItemFactoryBenchmark.cpp`

**测试场景**:
1. **Batch createWeapon (1000 items)** - 批量武器生成
2. **Batch createArmor (1000 items)** - 批量装备生成
3. **Affix rolling stress** - 传奇级 6 词缀 × 1000 物品

---

### 4.12 P2: FogOfWarBenchmark

**文件**: `tests/performance/FogOfWarBenchmark.cpp`

**测试场景**:
1. **updateVisibility (256x256 map)** - Compute Shader 可见性更新
2. **syncToCPU** - GPU→CPU 数据回读延迟

**注意**: 此测试需要 OpenGL 上下文。

---

## 5. 文件边界约束

### 5.1 可修改文件

| 文件 | 操作 |
|------|------|
| `tests/performance/*.cpp` (新增) | **创建** |
| `tests/BenchmarkUtils.hpp` (新增) | **创建** - 提取公共基准测试工具 |
| `tests/CMakeLists.txt` | **修改** - 注册新测试文件 |
| `tests/performance/RenderingBenchmark.cpp` | **修改** - 使用新的 BenchmarkUtils |

### 5.2 只读参考文件（不可修改）

- `src/game/systems/combat/DamagePipeline.cpp/.hpp`
- `src/engine/physics/PhysicsSystem.cpp/.hpp`
- `src/game/systems/skill/ProjectileSystem.cpp/.hpp`
- `src/game/systems/ai/AISystem.cpp/.hpp`
- `src/game/systems/skill/SkillSystem.cpp/.hpp`
- `src/game/systems/combat/HazardSystem.cpp/.hpp`
- `src/game/systems/combat/MonsterAffixSystem.hpp`
- `src/engine/render/GPUFlowFieldSystem.cpp/.hpp`
- `src/game/systems/world/EnemySpawnSystem.cpp/.hpp`
- `src/engine/persistence/SaveManager.cpp/.hpp`
- `src/game/systems/item/ItemFactory.cpp/.hpp`
- `src/game/systems/world/FogOfWarSystem.cpp/.hpp`
- `src/game/components/*.hpp`

---

## 6. 验收标准

- [ ] AC-1: 所有 12 个新增性能测试文件均可编译通过
- [ ] AC-2: 所有 P0 测试的 Mean 指标满足 §3.3 中定义的目标
- [ ] AC-3: `BenchmarkUtils.hpp` 被所有新测试和已有测试统一引用
- [ ] AC-4: 每个测试用例都有 `[Performance]` 标签前缀
- [ ] AC-5: 超过目标值的测试以 `WARN` 级别输出，而非直接 `FAIL`
- [ ] AC-6: 不需要 GPU 上下文的测试可以在 CI/headless 环境中运行
- [ ] AC-7: 所有测试的 `BenchmarkStats` 输出包含 Mean, P99 两个关键指标
