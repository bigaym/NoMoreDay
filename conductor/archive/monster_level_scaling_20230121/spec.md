# Monster Level Scaling System - Technical Specification

## 1. 概述

### 1.1 目标
实现怪物属性随地图等级和玩家等级动态成长的机制，提供类似 Diablo 2/3、POE、Grim Dawn 的游戏体验。

### 1.2 参考游戏
- **Diablo 2**: 指数成长 HP (~1.12^Lv)，难度分层抗性
- **Diablo 3**: 平滑经验曲线，等级同步
- **Path of Exile**: Rating 评级系统，区域等级机制
- **Last Epoch**: Level Factor 缩放系统

---

## 2. 数学公式定义

### 2.1 HP 成长曲线 (激进指数)

```
HP(Lv) = BaseHP × (1 + HP_GROWTH_RATE)^(Lv - 1) × RarityMult
```

| 参数 | 值 | 说明 |
|------|-----|------|
| `HP_GROWTH_RATE` | 0.10 | 每级 +10% |
| `BaseHP` | 种族定义 | `kRaceData[race].baseHP` |
| `RarityMult` | 见下表 | |

**稀有度 HP 乘数**:
| Rarity | Multiplier |
|--------|------------|
| Normal | 1.0 |
| Champion | 2.5 |
| Elite | 5.0 |
| Boss | 15.0 |
| Nemesis | 25.0 |

**示例** (Undead, BaseHP=30):
- Lv 1: 30 × 1.0 = 30
- Lv 50: 30 × 1.10^49 ≈ 3,540
- Lv 100: 30 × 1.10^99 ≈ 418,260

### 2.2 Damage 成长曲线

```
Damage(Lv) = BaseDmg × (1 + DMG_GROWTH_RATE)^(Lv - 1) × RarityMult
```

| 参数 | 值 | 说明 |
|------|-----|------|
| `DMG_GROWTH_RATE` | 0.08 | 每级 +8% |
| Variance | ±10% | `min = dmg × 0.9, max = dmg × 1.1` |

**稀有度 Damage 乘数**:
| Rarity | Multiplier |
|--------|------------|
| Normal | 1.0 |
| Champion | 1.25 |
| Elite | 1.6 |
| Boss | 2.5 |
| Nemesis | 3.0 |

### 2.3 怪物等级同步

```cpp
int SyncLevel(int areaLevel, int playerLevel) {
    int minLevel = std::max(1, playerLevel - 5);
    return std::max(minLevel, std::max(1, areaLevel));
}
```

**边界条件**:
- `areaLevel == 0` → 修正为 `1`
- `playerLevel <= 5` → `minLevel = 1`
- 怪物等级始终 >= 1

### 2.4 护甲成长 (线性减伤)

**目标**: 
- Lv 1: 0% 减伤
- Lv 100: 20% 减伤

**现有护甲公式** (DamagePipeline):
```
DR = Armor / (Armor + LevelFactor × ARMOR_BASE)
LevelFactor = LEVEL_BASE + Lv × LEVEL_LINEAR + Lv² × LEVEL_QUADRATIC
```

**反推护甲值**:
```cpp
// 目标 DR(Lv) = 0.002 × (Lv - 1)  (线性从 0% 到 19.8%)
float targetDR = std::min(0.20f, 0.002f * (level - 1));
float levelFactor = ComputeLevelFactor(level);
// Armor = DR × LevelFactor × ARMOR_BASE / (1 - DR)
float scaledArmor = (targetDR * levelFactor * ARMOR_BASE) / (1.0f - targetDR);
```

### 2.5 抗性成长

**Lv 1-100**: 使用种族基础抗性，不额外增加

**Lv 100+**:
```cpp
float bonusRes = 0.0f;
if (level > 100) {
    int overLevel = level - 100;
    float resPerLevel = GetResGrowthByRarity(rarity);
    bonusRes = overLevel * resPerLevel;
}
finalRes = std::min(0.75f, baseRes + bonusRes);
```

**抗性增长率 (每级)**:
| Rarity | Growth/Level |
|--------|--------------|
| Normal | 0.002 (0.2%) |
| Champion | 0.004 (0.4%) |
| Elite | 0.006 (0.6%) |
| Boss | 0.008 (0.8%) |
| Nemesis | 0.010 (1.0%) |

**Hard Cap**: 75% (0.75)

### 2.6 经验公式 (D3 风格)

```cpp
float GetXPMultiplier(int monsterLevel, int playerLevel) {
    int diff = std::abs(playerLevel - monsterLevel);
    if (diff <= 5) {
        return 1.0f; // 无惩罚
    }
    // 每超过 5 级，-10%，最低 10%
    return std::max(0.1f, 1.0f - (diff - 5) * 0.1f);
}

float finalXP = baseXP × (1 + XP_GROWTH_RATE)^(Lv - 1) × xpMult;
```

| 参数 | 值 |
|------|-----|
| `XP_GROWTH_RATE` | 0.05 |
| `MIN_XP_MULT` | 0.1 (10%) |

---

## 3. 数据结构

### 3.1 Constants (Common.hpp)

```cpp
namespace NoMoreDay::Constants::Scaling::Monster {
    // --- HP Growth ---
    constexpr float HP_GROWTH_RATE = 0.10f;
    
    // --- Damage Growth ---
    constexpr float DMG_GROWTH_RATE = 0.08f;
    constexpr float DMG_VARIANCE_MIN = 0.90f;
    constexpr float DMG_VARIANCE_MAX = 1.10f;
    
    // --- Armor Growth ---
    constexpr float TARGET_DR_AT_100 = 0.20f;
    constexpr float DR_PER_LEVEL = 0.002f;
    
    // --- Resistance Growth (Lv 100+) ---
    constexpr float RES_GROWTH_NORMAL = 0.002f;
    constexpr float RES_GROWTH_CHAMPION = 0.004f;
    constexpr float RES_GROWTH_ELITE = 0.006f;
    constexpr float RES_GROWTH_BOSS = 0.008f;
    constexpr float RES_GROWTH_NEMESIS = 0.010f;
    constexpr float RES_HARD_CAP = 0.75f;
    
    // --- XP Growth ---
    constexpr float XP_GROWTH_RATE = 0.05f;
    constexpr float XP_DIFF_THRESHOLD = 5.0f;
    constexpr float XP_PENALTY_PER_LEVEL = 0.10f;
    constexpr float XP_MIN_MULT = 0.10f;
    
    // --- Level Sync ---
    constexpr int LEVEL_SYNC_OFFSET = 5;
    
    // --- Rarity HP Multipliers ---
    constexpr float RARITY_HP_NORMAL = 1.0f;
    constexpr float RARITY_HP_CHAMPION = 2.5f;
    constexpr float RARITY_HP_ELITE = 5.0f;
    constexpr float RARITY_HP_BOSS = 15.0f;
    constexpr float RARITY_HP_NEMESIS = 25.0f;
    
    // --- Rarity Damage Multipliers ---
    constexpr float RARITY_DMG_NORMAL = 1.0f;
    constexpr float RARITY_DMG_CHAMPION = 1.25f;
    constexpr float RARITY_DMG_ELITE = 1.6f;
    constexpr float RARITY_DMG_BOSS = 2.5f;
    constexpr float RARITY_DMG_NEMESIS = 3.0f;
}
```

### 3.2 MonsterScaling 工具类

```cpp
// src/game/utils/MonsterScaling.hpp
namespace NoMoreDay {

struct MonsterScalingResult {
    float maxHealth;
    float minDamage;
    float maxDamage;
    float armor;
    std::array<float, 6> resistances; // 对应 DamageType
    float xpValue;
};

class MonsterScaling {
public:
    // 计算最终属性
    static MonsterScalingResult Calculate(
        EnemyRace::Type race,
        int level,
        EnemyRarityComponent::Rarity rarity);
    
    // 等级同步
    static int SyncLevel(int areaLevel, int playerLevel);
    
    // 经验乘数 (D3 风格)
    static float GetXPMultiplier(int monsterLevel, int playerLevel);
    
    // 稀有度乘数查询
    static float GetHPMultiplier(EnemyRarityComponent::Rarity rarity);
    static float GetDamageMultiplier(EnemyRarityComponent::Rarity rarity);
    static float GetResistanceGrowth(EnemyRarityComponent::Rarity rarity);
    
private:
    // 指数成长辅助
    static float PowerCurve(float rate, int level);
    
    // 护甲反推
    static float ComputeArmorForTargetDR(int level, float targetDR);
};

} // namespace NoMoreDay
```

---

## 4. 系统交互

### 4.1 数据流

```
[LevelManager]           [PlayerStats]
      │                        │
      │ areaLevel              │ playerLevel
      ▼                        ▼
┌─────────────────────────────────────┐
│       MonsterScaling::SyncLevel     │
└─────────────────────────────────────┘
                  │
                  │ finalLevel
                  ▼
┌─────────────────────────────────────┐
│      MonsterScaling::Calculate      │
└─────────────────────────────────────┘
                  │
                  │ MonsterScalingResult
                  ▼
┌─────────────────────────────────────┐
│         EnemySpawnSystem            │
│   - HealthComponent                 │
│   - CombatStats (dmg, armor)        │
│   - EnemyStateComponent.level       │
└─────────────────────────────────────┘
                  │
                  │ StatsDirty
                  ▼
┌─────────────────────────────────────┐
│           StatsSystem               │
│   - Apply resistance growth (100+)  │
│   - Validate armor scaling          │
└─────────────────────────────────────┘
```

### 4.2 调用时机

| 系统 | 何时调用 | 调用函数 |
|------|---------|---------|
| `EnemySpawnSystem::spawnEnemy` | 怪物生成时 | `MonsterScaling::SyncLevel`, `MonsterScaling::Calculate` |
| `StatsSystem::Recalculate` | StatsDirty 触发 | 读取已计算的属性，应用抗性成长 |
| `XPAwardingSystem` | 怪物死亡时 | `MonsterScaling::GetXPMultiplier` |

---

## 5. 测试矩阵

### 5.1 单元测试

| 测试用例 | 输入 | 预期输出 |
|---------|------|---------|
| `PowerCurve_Lv1` | rate=0.10, lv=1 | 1.0 |
| `PowerCurve_Lv50` | rate=0.10, lv=50 | ~117.39 |
| `SyncLevel_AreaHigher` | area=50, player=30 | 50 |
| `SyncLevel_PlayerHigher` | area=20, player=60 | 55 |
| `SyncLevel_LowPlayer` | area=1, player=3 | 1 |
| `XPMult_InRange` | mon=50, player=52 | 1.0 |
| `XPMult_OutRange` | mon=50, player=60 | 0.5 |
| `Resistance_Under100` | lv=80, rarity=Elite | baseRes (无增加) |
| `Resistance_Over100` | lv=110, rarity=Elite | baseRes + 0.06 |

### 5.2 集成测试

- [ ] 在 Lv 1 区域生成怪物，验证属性 ≈ 种族基础值
- [ ] 在 Lv 100 区域生成 Boss，验证 HP 约为 `BaseHP × 1.10^99 × 15`
- [ ] 玩家 Lv 80 进入 Lv 50 区域，验证怪物等级 = max(50, 75) = 75
- [ ] 击杀等级差 >5 的怪物，验证 XP 减少

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 高等级 HP 溢出 float | 数值错误 | 使用 `double` 计算后转 `float`，或限制最大等级 |
| 护甲反推精度问题 | 减伤不精确 | 预计算查找表 (LUT) |
| 等级同步导致低级区域过难 | 玩家体验 | 可调整 `LEVEL_SYNC_OFFSET` 参数 |

---

## 7. 配置扩展点

- `HP_GROWTH_RATE`: 可改为 JSON 配置，支持热更新
- 稀有度乘数: 可扩展到 `MonsterRarityConfig.json`
- 难度分层: 预留 `DifficultyMultiplier` 参数

---

## Appendix: 成长曲线可视化

```
HP Growth (BaseHP=100, Normal Rarity)
╔════════╦═══════════╦═══════════════╗
║ Level  ║ Multiplier ║ Final HP      ║
╠════════╬═══════════╬═══════════════╣
║   1    ║   1.00     ║      100      ║
║  10    ║   2.36     ║      236      ║
║  25    ║   9.85     ║      985      ║
║  50    ║ 117.39     ║   11,739      ║
║  75    ║ 1,379.29   ║  137,929      ║
║  100   ║ 13,780.61  ║ 1,378,061     ║
╚════════╩═══════════╩═══════════════╝
```

---

**Document Version**: 1.0  
**Created**: 2026-01-21  
**Author**: Feature Architect Agent
