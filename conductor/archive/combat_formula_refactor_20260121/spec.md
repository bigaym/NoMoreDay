# 战斗属性计算重构 - 技术规格

## 1. 概述

本次重构参考 **Last Epoch** 的成熟公式体系，引入**等级缩放**与**逐渐衰减 (Diminishing Returns)** 机制。

### 1.1 设计目标

| 目标 | 描述 |
|-----|-----|
| **闪避上限** | 90% (Last Epoch 为 85%，本项目上调) |
| **闪避评级缩放** | 8000-10000 评级 → 85% (参考 LE)，缩放至 90% |
| **等级缩放** | 护甲、闪避、格挡均受地图等级影响 |
| **负护甲** | 增加受到的伤害 (类似 More 乘区) |
| **词缀系统** | 基础值 + 百分比增加 (Flat + Increased) |

---

## 2. 核心公式

### 2.1 护甲减伤公式

```
level_factor = 10 + 0.5 * areaLevel + 0.05 * areaLevel²

effective_armor = (base_armor + flat_armor) * (1 + increased_armor%) - armor_penetration

if effective_armor >= 0:
    damage_reduction = effective_armor / (effective_armor + level_factor)
else:
    # 负护甲 = 增伤 (More Damage Taken)
    damage_multiplier = 1 + |effective_armor| / (|effective_armor| + level_factor)
```

**示例 (Area Level 100)**:
| 有效护甲 | 物理减伤 |
|---------|---------|
| 0 | 0% |
| 560 | 50% |
| 1120 | 66.7% |
| 5600 | 90.9% |
| -560 | +50% 增伤 |

### 2.2 闪避公式

**Last Epoch 原版 (Cap 85%)**:
```
dodge_chance = (1 - 1 / ((0.1x + 0.001x²) / level_factor + 1)) * 0.85
```

**本项目调整 (Cap 90%)**:
```
level_factor = 10 + 0.5 * areaLevel + 0.05 * areaLevel²

dodge_numerator = 0.1 * dodge_rating + 0.001 * dodge_rating²
dodge_chance = (1 - 1 / (dodge_numerator / level_factor + 1)) * 0.90
```

**示例 (Area Level 100, Cap 90%)**:
| 闪避评级 | 闪避率 |
|---------|-------|
| 0 | 0% |
| 1000 | ~53% |
| 2000 | ~74% |
| 4000 | ~82% |
| 8000 | ~87% |
| 10000 | ~88.5% |
| ∞ | 90% |

### 2.3 格挡公式

```
block_chance = min(75%, raw_block_chance)

block_effectiveness = block_amount / (block_amount + level_factor)
# 格挡时：damage_taken = damage * (1 - block_effectiveness)
```

### 2.4 属性聚合公式 (通用)

所有可堆叠属性 (生命值、暴击等) 采用统一公式：
```
final_value = (base_value + flat_bonus) * (1 + increased_percent) * more_multiplier
```

---

## 3. 数据结构变更

### 3.1 新增常量 (`Common.hpp`)

```cpp
namespace NoMoreDay::Constants::Combat::Scaling {
    // 等级缩放因子 (Last Epoch 风格)
    constexpr float LEVEL_BASE = 10.0f;
    constexpr float LEVEL_LINEAR = 0.5f;
    constexpr float LEVEL_QUADRATIC = 0.05f;
    
    // 闪避评级系数
    constexpr float DODGE_RATING_LINEAR = 0.1f;
    constexpr float DODGE_RATING_QUADRATIC = 0.001f;
    constexpr float DODGE_MAX_CHANCE = 0.90f;  // 90% 上限
    
    // 格挡上限
    constexpr float BLOCK_MAX_CHANCE = 0.75f;
    
    // 辅助函数: 计算等级因子
    inline constexpr float LevelFactor(int level) {
        return LEVEL_BASE + LEVEL_LINEAR * level + LEVEL_QUADRATIC * level * level;
    }
}
```

### 3.2 `CombatStats` 扩展

```cpp
struct alignas(32) CombatStats {
    // 现有字段保持不变...
    
    // NEW: 评级系统 (用于等级缩放计算)
    float dodge_rating = 0.0f;    // 闪避评级
    float block_rating = 0.0f;    // 格挡评级 (= block_amount)
    
    // NEW: 有效值 (UI显示用, 由 StatsSystem::Bake 计算)
    float effective_dodge = 0.0f;       // 当前等级下的有效闪避率
    float effective_armor_dr = 0.0f;    // 当前等级下的护甲减伤
    float effective_block_eff = 0.0f;   // 当前等级下的格挡效能
    
    // NEW: 当前地图等级缓存 (避免重复查询)
    int cached_area_level = 1;
};
```

### 3.3 词缀系统 (`ItemStats.hpp`) 

新增评级类型词缀：
```cpp
enum class AffixType : uint16_t {
    // 现有...
    
    // NEW: 评级类型
    FlatDodgeRating,      // +X 闪避评级
    PercentDodgeRating,   // +X% 闪避评级
    FlatBlockRating,      // +X 格挡评级
    PercentBlockRating,   // +X% 格挡评级
};
```

---

## 4. 系统修改清单

### 4.1 StatsSystem.cpp

| 函数 | 修改内容 |
|-----|---------|
| `BakeStats()` | 调用缩放计算函数，填充 effective_* 字段 |
| `CalculateArmorDR()` | 新增: 计算等级缩放护甲减伤 |
| `CalculateDodgeChance()` | 新增: 计算等级缩放闪避率 |
| `CalculateBlockEffectiveness()` | 新增: 计算等级缩放格挡效能 |

### 4.2 DamagePipeline.cpp

| 位置 | 修改内容 |
|-----|---------|
| Line 403-423 | 替换护甲公式为等级缩放版本 |
| Line 628-638 | 更新 SIMD 批处理护甲路径 |
| Line 689-697 | 更新标量回退护甲路径 |

### 4.3 CombatSystem.cpp

| 位置 | 修改内容 |
|-----|---------|
| Line 171-184 | 使用 `effective_dodge` 替代 `dodge_chance` |
| Line 280-286 | 使用 `effective_block_eff` 替代硬编码公式 |
| Line 410-420 | 敌人路径同步更新 |
| Line 430-448 | 敌人格挡同步更新 |

---

## 5. API 契约

### 5.1 新增公共函数

```cpp
namespace NoMoreDay::CombatFormula {
    // 计算等级因子
    float LevelFactor(int area_level);
    
    // 计算护甲减伤 (正护甲 = 减伤, 负护甲 = 增伤)
    // 返回值: [0, 1) 减伤时, >1 增伤时
    float CalculateArmorMultiplier(float effective_armor, int area_level);
    
    // 计算闪避率
    // 返回值: [0, DODGE_MAX_CHANCE]
    float CalculateDodgeChance(float dodge_rating, int area_level);
    
    // 计算格挡效能
    // 返回值: [0, 1)
    float CalculateBlockEffectiveness(float block_amount, int area_level);
}
```

---

## 6. 测试矩阵

| 测试类型 | 文件 | 覆盖内容 |
|---------|-----|---------|
| Unit | `CombatFormulaTest.hpp` | 公式边界值、等级缩放、上限验证 |
| Integration | `CombatBalanceTest.hpp` | 实战伤害流程、词缀生效 |
| Benchmark | `CombatFormulaBenchmark.hpp` | SIMD 路径性能对比 |

---

## 7. 验收标准

- [ ] 8000 闪避评级 @ Level 100 ≈ 87% 闪避率
- [ ] 护甲公式遵循等级缩放
- [ ] 负护甲正确计算增伤
- [ ] UI 显示有效值而非原始评级
- [ ] 所有现有战斗测试通过
- [ ] 新增公式测试覆盖边界情况
