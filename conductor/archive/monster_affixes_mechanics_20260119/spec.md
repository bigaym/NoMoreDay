# Specification: Advanced Combat Mechanics (Monster Affix V2 - Part 3)

## 1. Overview
本 Track 聚焦于增加战斗的策略深度，引入实体克隆、伤害拦截机制以及资源剥夺。这些词缀通常出现在稀有或传奇怪物身上，要求玩家改变战斗节奏。

## 2. Technical Architecture

### 2.1 Core Components

#### `LinkComponent`
用于建立两个实体之间的逻辑连接（如护盾源 -> 护盾目标）。
```cpp
struct LinkComponent {
    entt::entity target;
    LinkType type; // Shielding, SoulLink, etc.
    float visualWidth;
    Color color;
};
```

#### `CloneComponent`
标记该实体为克隆体，包含对本体的引用（可选）和属性修正。
```cpp
struct CloneComponent {
    entt::entity parent;
    float damageMultiplier; // e.g. 0.5
    float healthMultiplier; // e.g. 0.1
};
```

#### `ResourceDrainComponent`
定义资源剥夺光环。
```cpp
struct ResourceDrainComponent {
    float radius;
    float drainRate; // per second
    ResourceType resource; // Mana, Stamina
    bool safeZoneInside; // true = 甜甜圈模式 (内圈安全)
};
```

### 2.2 Core Systems

#### `DamagePipeline` (Hooks)
需要在现有的伤害计算管线中插入新的拦截点 (Interceptors)：
1.  **Pre-Calculation**: 检查 `Invulnerable` (无敌) 状态。
2.  **Mitigation**: 检查 `Suppressor` (距离减免)。

#### `EntityFactory` (New Method)
需要一个新的 API 来“深拷贝”一个实体及其外观组件，但重置其状态组件。
`entt::entity CloneEntity(entt::registry& reg, entt::entity source);`

## 3. Affix Specifics

### 3.1 Mirror Image (镜像)
*   **机制**: `Cloning`。
*   **触发**:
    *   生命值降至 50% 时触发一次。
    *   或者受到暴击时有 5% 几率触发（有冷却）。
*   **行为**:
    *   生成 2 个外观完全相同的克隆体。
    *   克隆体属性: 10% HP, 50% Damage。
    *   克隆体拥有短暂的 `Stealth` 或无敌帧以防被秒杀。
*   **AI**: 克隆体继承本体的基础 AI，但不继承复杂词缀。

### 3.2 Shielding (护盾)
*   **机制**: `Buff Support`。
*   **行为**:
    *   周期性扫描半径 300px 内的非 `Shielding` 友军。
    *   为他们施加 `Invulnerable` (无敌) Buff。
    *   只要本体存活且距离足够，Buff 持续刷新。
*   **视觉**: 本体与每个受保护目标之间有一条金色的能量连线 (使用 `LineRenderer` 或粒子流)。

### 3.3 Suppressor (压制)
*   **机制**: `Proximity Check`。
*   **行为**:
    *   **Proximity Shield**: 来自半径 200px **以外** 的所有伤害减少 90%。
    *   迫使远程玩家必须进入近战范围输出。
*   **实现**: 在 `DamagePipeline` 中，计算 `distance(attacker, defender)`。如果 `dist > 200` 且 defender 有 `Suppressor` 标签，应用 `0.1` 伤害系数。
*   **视觉**: 明显的红色半透明球形护盾罩住怪物。

### 3.4 Soul Eater (噬魂)
*   **机制**: `Scaling on Death`。
*   **行为**:
    *   怪物拥有 `SoulEaterComponent`。
    *   监听全局 `CombatEventType::EntityDeath`。
    *   若死亡单位在半径 400px 内，`stackCount++`。
    *   每层效果: Size +2%, Damage +5%, AttackSpeed +2%。上限 50 层。
*   **反制**: 玩家需尽快击杀本体，避免它因小怪死亡而成长为 Boss 级怪物。

### 3.5 Mana Siphon (虹吸)
*   **机制**: `Resource Denial Aura`。
*   **行为**:
    *   在怪物周围产生一个巨大的蓝色光环 (半径 400)。
    *   **光环是一个环形**：
        *   半径 0 - 150: 安全区。
        *   半径 150 - 400: 危险区，每秒扣除玩家 20% 最大法力值。
*   **实现**: `MonsterAffixSystem::Update` 中检测玩家位置并修改 `PlayerState` 的 Mana。
*   **视觉**: 巨大的蓝色圆环特效，内圈空心。

## 4. Integration Plan
1.  **Damage Logic**: 修改 `src/game/systems/combat/DamagePipeline.cpp` 添加 Suppressor 和 Invulnerable 检查。
2.  **Factory**: 在 `ItemFactory` 或 `EnemySpawnSystem` 中实现克隆逻辑。
3.  **VFX**: 实现连线渲染 (`Shielding`) 和 圆环渲染 (`Mana Siphon`)。
4.  **Affix Logic**: 完善 `MonsterAffixSystem` 处理上述逻辑。
