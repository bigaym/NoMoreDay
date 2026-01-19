# Specification: Physics & Crowd Control Affixes (Monster Affix V2 - Part 2)

## 1. Overview
本 Track 旨在通过物理交互和位移控制来增加战斗的混乱度和策略性。我们将扩展物理引擎以支持“力场”，并实现动态地形修改能力。核心是打破玩家的舒适站位。

## 2. Technical Architecture

### 2.1 Core Components

#### `ForceFieldComponent`
定义一个径向力场。
```cpp
struct ForceFieldComponent {
    float strength;       // 正值=排斥(Repel), 负值=吸引(Attract)
    float radius;         // 作用半径
    float activeDuration; // 激活时长 (用于脉冲式力场)
    float cooldown;       // 冷却时间
    bool isAlwaysOn;      // 是否常驻
};
```

#### `DynamicObstacleComponent`
标记实体为动态生成的障碍物（墙体）。
```cpp
struct DynamicObstacleComponent {
    float lifetime;       // 持续时间，归零销毁
    uint8_t solidity;     // 碰撞层级/类型
};
```

#### `PlayerState` Extensions
在 `PlayerState` 中增加状态标记：
*   `bool isRooted`: 无法移动，但可以攻击/施法。
*   `bool isSilenced`: 无法施法，但可以移动/普攻 (预留)。

### 2.2 Core Systems

#### `PhysicsSystem` (Extension)
*   **Force Resolution**: 在物理步进 (Step) 前，遍历所有 `ForceFieldComponent`。
*   对范围内的动态刚体（玩家、其他怪物）应用力向量 `F = normalize(dir) * strength * (1 - dist/radius)`.
*   **Optimization**: 使用 `SpatialGrid` 加速查询。

#### `MapSystem` (Update)
*   需要提供 API `SpawnDynamicObstacle(Rect bounds)`。
*   当障碍物生成/销毁时，需要局部更新寻路网格 (`GridMap` / FlowField)，否则怪物会试图穿墙并卡住。
    *   *简化方案*: 暂时不更新全局 FlowField，仅依赖物理碰撞防止穿过。怪物的 AI 可能会在墙前卡顿，但这对于临时墙体（5秒）是可以接受的。

## 3. Affix Specifics

### 3.1 Vortex (漩涡)
*   **机制**: `Periodic Pull`。
*   **行为**:
    *   每 5 秒激活一次，持续 2 秒。
    *   激活期间，将半径 400px 内的玩家强力拉向怪物中心。
*   **实现**: 添加 `ForceFieldComponent` (strength = -500.0f, isAlwaysOn = false).
*   **视觉**: 怪物脚下出现扭曲的空气波动效果 (Distortion Shader) 和向心流动的粒子。

### 3.2 Waller (筑墙)
*   **机制**: `Terrain Modification`。
*   **行为**:
    *   在玩家周围生成 U 型（或两道平行）墙体。
    *   墙体是静态刚体，阻挡移动和非穿透性投射物。
    *   持续 5 秒后自动销毁。
*   **实现**: `EntityFactory::SpawnWall(position, rotation)`。
*   **视觉**: 地面升起岩石/骨墙模型。

### 3.3 Entangler (纠缠)
*   **机制**: `OnHit Debuff`。
*   **行为**:
    *   攻击命中玩家时，有 20% 几率施加 `Rooted` 状态。
    *   持续 1.5 秒。
*   **实现**:
    *   在 `DamagePipeline` 的 `ApplyDebuffs` 阶段处理。
    *   `InputSystem` 需要检查 `PlayerState::isRooted`，若为真则忽略移动输入。
*   **视觉**: 绿色藤蔓缠绕玩家腿部。

### 3.4 Teleporter V2 (闪烁 - 优化版)
*   **机制**: `AI Behavior Override`。
*   **改进**:
    *   旧版: 瞬间坐标变更。
    *   新版: `FadeOut` (0.2s) -> `Move` -> `FadeIn` (0.2s) -> `Attack`.
    *   增加冷却时间指示器。
*   **反制**: 瞬移落地时有 0.5s 硬直，给予玩家反应时间。

## 4. Integration Plan
1.  **Physics**: 修改 `src/engine/physics/PhysicsSystem.cpp`，实现 `ApplyForceFields` 函数。
2.  **State**: 更新 `src/game/components/PlayerState.hpp`。
3.  **Input**: 修改 `src/engine/input/InputSystem.cpp` 增加对 Rooted 状态的判断。
4.  **Affix Logic**: 在 `MonsterAffixSystem` 中实现 Vortex 的计时器和 Waller 的生成逻辑。
