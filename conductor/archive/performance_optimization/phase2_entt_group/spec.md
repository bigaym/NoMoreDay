# Phase 2: EnTT Group Memory Optimization 规格说明书

**Track ID**: `performance_optimization/phase2_entt_group`  
**优先级**: P1 (内存效率)  
**预计收益**: 属性系统更新 Cache Miss 降低 80%+  
**依赖**: 无 (可与 Phase 1 并行)

---

## 1. 问题陈述 (Problem Statement)

### 当前内存访问模式
```
StatsSystem::Recalculate()
├── registry.view<CombatStats, ModifierList>
│   ├── CombatStats 在 Pool A (地址 0x1000)
│   └── ModifierList 在 Pool B (地址 0x5000) ← Cache Miss!
└── 遍历时内存跳跃 4KB+
```

### EnTT View vs Group 对比
| 特性 | `entt::view` | `entt::group` |
|-----|--------------|---------------|
| 内存布局 | 各组件独立池 | 物理内存重排 |
| 遍历方向 | 最小池优先 | 内存连续 |
| 适用场景 | 稀疏访问 | 热点遍历 |

---

## 2. 技术方案 (Technical Design)

### 2.1 Group 定义

```cpp
// 在 Registry 初始化时定义 Owning Group
// Owning Group 会强制组件在内存上按实体顺序排列

// Combat Group: 属性计算核心
using CombatGroup = entt::group<
    entt::owned_t<CombatStats, ModifierList>,  // 这些组件将被重排
    entt::get_t<PrimaryStats>                   // 只读访问，不重排
>;

// Render Group: GPU 同步核心
using RenderGroup = entt::group<
    entt::owned_t<Position, Velocity, Radius, GPUIndex>
>;

// AI Group: AI 更新核心
using AIGroup = entt::group<
    entt::owned_t<AIComponent, Position, Velocity>,
    entt::get_t<EnemyTag>
>;
```

### 2.2 Group 注册位置

> **[CRITICAL]** EnTT Group 必须在添加任何拥有的组件**之前**创建。

```cpp
// src/game/registry/GroupRegistry.hpp
namespace NoMoreDay::groups {

void RegisterGroups(entt::registry& registry) {
    // 必须在游戏开始前注册所有 Group
    registry.group<CombatStats, ModifierList>(entt::get<PrimaryStats>);
    registry.group<Position, Velocity, Radius, GPUIndex>();
    registry.group<AIComponent, Position, Velocity>(entt::get<EnemyTag>);
}

} // namespace
```

### 2.3 使用模式

#### Before (View 遍历)
```cpp
void StatsSystem::Recalculate(entt::registry& reg) {
    auto view = reg.view<CombatStats, ModifierList>();
    for (auto entity : view) {
        auto& stats = view.get<CombatStats>(entity);   // 随机访问
        auto& mods = view.get<ModifierList>(entity);   // Cache Miss!
        Apply(stats, mods);
    }
}
```

#### After (Group 遍历)
```cpp
void StatsSystem::Recalculate(entt::registry& reg) {
    auto group = reg.group<CombatStats, ModifierList>(entt::get<PrimaryStats>);
    for (auto [entity, stats, mods] : group.each()) {
        // stats 和 mods 在内存上连续，缓存命中率极高
        Apply(stats, mods);
    }
}
```

---

## 3. 影响范围分析

### 3.1 需要改造的系统

| 系统 | 文件 | 当前实现 | 目标 Group |
|------|------|----------|------------|
| StatsSystem | `StatsSystem.cpp` | `view<CombatStats, ModifierList>` | CombatGroup |
| GPUEntitySystem | `GPUEntitySystem.cpp` | `view<Position, Velocity, Radius, GPUIndex>` | RenderGroup |
| AISystem | `AISystem.cpp` | `view<AIComponent, Position>` | AIGroup |
| ProjectileSystem | `ProjectileSystem.cpp` | `view<Projectile, Position, Velocity>` | - (独立实现) |

### 3.2 限制与约束

> **[WARNING]** 创建 Owning Group 后，被"拥有"的组件**不能**再被其他 Group 拥有。

**冲突检查**:
```
CombatStats ─── owned by CombatGroup ✓
ModifierList ── owned by CombatGroup ✓
Position ────── owned by RenderGroup ✓
Velocity ────── owned by RenderGroup ✓
               
// 以下是非法的！
registry.group<Position, CombatStats>(); // ERROR: Position 已被 RenderGroup 拥有
```

---

## 4. 实现计划

### Task 2.1: 创建 GroupRegistry
**文件**: `src/game/registry/GroupRegistry.hpp`

### Task 2.2: 重构 StatsSystem
**文件**: `src/game/systems/stats/StatsSystem.cpp`
- 替换 `view` 为 `group`
- 优化遍历循环

### Task 2.3: 重构 GPUEntitySystem::Update
**文件**: `src/engine/render/GPUEntitySystem.cpp`
- 使用 RenderGroup 收集实体数据

### Task 2.4: 重构 AISystem
**文件**: `src/game/systems/ai/AISystem.cpp`
- 使用 AIGroup 遍历

### Task 2.5: 集成测试
- 验证所有现有测试通过
- 新增 GroupLayoutTest 验证内存连续性

---

## 5. 验收标准

| 指标 | 基准 | 目标 |
|------|------|------|
| StatsSystem 单帧耗时 | TBD | 降低 50% |
| Cache Miss 率 (perf stat) | High | < 5% |
| 所有现有测试 | Pass | Pass |

---

## 6. 风险与缓解

| 风险 | 缓解策略 |
|------|----------|
| 组件添加顺序问题 | 在 GameplayState::Init 最早期调用 RegisterGroups |
| Group 冲突 | 设计时规划好组件归属，使用静态断言检测 |
| 性能不如预期 | 回退到 View 模式，无代码损失 |

---

*设计者: Gemini (Skill: designer)*
