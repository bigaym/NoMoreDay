# Specification: Environmental & Hazard Affixes (Monster Affix V2 - Part 1)

## 1. Overview
本 Track 旨在实现高视觉冲击力、改变战场环境的“地板技”词缀。核心目标是迫使玩家保持移动，惩罚站桩输出。我们将引入通用的 `HazardSystem` 来统一管理地面持续伤害区域（AoE Zones）和延迟爆炸物。

## 2. Technical Architecture

### 2.1 Core Components

#### `HazardComponent`
定义一个通用的伤害区域或延迟爆炸物。
```cpp
struct HazardComponent {
    float damagePerTick;      // 每次判定造成的伤害
    float tickInterval;       // 伤害判定间隔 (秒)
    float currentTickTimer;   // 运行时计时器
    
    float duration;           // 总持续时间 (秒)
    float radius;             // 伤害半径
    
    DamageType damageType;    // 伤害类型 (Fire, Poison, True, etc.)
    bool isDelayedExplosion;  // true: 倒计时结束造成一次性伤害; false: 持续 DoT
    
    // Target filtering
    bool hitsPlayers = true;
    bool hitsEnemies = false;
};
```

#### `HazardVisualComponent`
将逻辑实体与 GPU 粒子系统关联。
```cpp
struct HazardVisualComponent {
    uint32_t particleSystemID; // 关联的粒子效果 ID
    Color tintColor;
    float visualScale;
};
```

### 2.2 Core Systems

#### `HazardSystem`
负责处理所有拥有 `HazardComponent` 的实体。
1.  **Life Cycle**: 更新 `duration`，超时销毁实体。
2.  **Tick Logic**: 更新 `tickInterval`。
3.  **Spatial Query**: 当 Tick 触发或爆炸触发时，使用 `SpatialGrid` 查询范围内的目标。
4.  **Damage Application**: 生成 `CombatEvent` 并分发给 `DamagePipeline`。

#### `MonsterAffixSystem` (Extension)
扩展现有的 `MonsterAffixSystem` 以在特定时机（Update/OnHit/OnDeath）生成 Hazard 实体。

## 3. Affix Specifics

### 3.1 Frozen (极寒)
*   **机制**: `Frozen Orb`。怪物周期性生成一个冰球实体。
*   **行为**:
    *   冰球生成时获得初速度指向玩家。
    *   移动 2秒 后停止。
    *   停止 1秒 后爆炸 (Delayed Explosion)。
*   **效果**:
    *   爆炸造成 **Cold Damage**。
    *   施加 **Freeze** (冻结) 或 **Chill** (强力减速) Buff。
*   **视觉**: 蓝色光球，爆炸时有冰霜粒子溅射。

### 3.2 Toxic (剧毒)
*   **机制**: `Volatile Death`。怪物死亡时 (`OnDeath`) 触发。
*   **行为**:
    *   生成 3 个 `VolatileOrb` 实体。
    *   Orbs 使用简单的追踪逻辑 (`HomingComponent`) 飞向玩家。
    *   撞击玩家或飞行 3秒 后爆炸，在地面生成 `ToxicPool`。
*   **效果**:
    *   `ToxicPool` 是一个持续 5秒 的 DoT Hazard。
    *   造成 **Poison Damage** 并叠加中毒层数。
*   **视觉**: 绿色粘液球，地面生成冒泡的绿色沼泽。

### 3.3 Void Zone (虚空)
*   **机制**: `Static Spawner`。每隔 5-8 秒在玩家当前脚下生成一个区域。
*   **行为**:
    *   预警阶段 (1秒): 地面显示暗紫色圈，无伤害。
    *   激活阶段 (4秒): 区域变为高伤害 Hazard。
*   **效果**:
    *   造成 **True Damage** (真实伤害，无视护甲/抗性)。
    *   伤害频率极高 (e.g., 每 0.2s 一次)。
*   **视觉**: 紫色/黑色扭曲 Shader，类似黑洞。

### 3.4 Storm Strider (雷行)
*   **机制**: `Counter-Attack`。受击时 (`OnHit`) 有几率触发。
*   **行为**:
    *   在怪物当前位置生成一个“残影”实体。
    *   残影不移动，持续 1.5秒 后自爆。
*   **效果**:
    *   小范围 (Radius 50) **Lightning Damage**。
*   **视觉**: 黄色半透明的怪物剪影，带有电弧粒子。

## 4. Integration Plan
1.  **Define Components**: 在 `src/game/components/` 下创建 `HazardComponents.hpp`。
2.  **Implement System**: 在 `src/game/systems/combat/` 下创建 `HazardSystem.cpp/hpp`。
3.  **VFX**: 在 `assets/shaders/particle.compute` 中添加新的粒子行为类型 (Freeze, Toxic)。
4.  **Registry**: 在 `MonsterAffixRegistry` 中注册新词缀，并在 `MonsterAffixSystem` 中实现生成逻辑。
