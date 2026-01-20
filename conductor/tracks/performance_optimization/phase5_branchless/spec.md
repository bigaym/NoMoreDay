# Phase 5: Branchless Combat Logic 规格说明书

**Track ID**: `performance_optimization/phase5_branchless`  
**优先级**: P3 (流水线优化)  
**预计收益**: 大规模循环分支预测失败率降低 95%+  
**依赖**: Phase 2 (Group 遍历模式)

---

## 1. 问题陈述 (Problem Statement)

### 1.1 分支预测失败的代价

| CPU 操作 | 周期数 |
|---------|--------|
| 正确预测的分支 | ~1-2 |
| 预测失败的分支 | ~15-20 (流水线刷新) |
| 算术运算 | ~1 |

### 1.2 问题代码示例

```cpp
// CombatSystem::CalculateDamage 中的分支
for (auto [entity, stats] : view.each()) {
    float damage = baseDamage;
    
    if (stats.hasCrit) {           // 分支 1: ~50% 命中率
        damage *= stats.critMult;
    }
    
    if (target.hasBarrier) {       // 分支 2: ~30% 命中率
        damage = ApplyBarrier(damage, target);
    }
    
    if (stats.hasLifeSteal) {      // 分支 3: ~20% 命中率
        Heal(entity, damage * stats.lifeSteal);
    }
    // ... 更多分支
}
```

### 1.3 预测失败率估算

假设 10000 实体，3 个独立分支:
- 理论预测失败次数 ≈ 10000 × 3 × 0.5 = 15000 次
- 每次失败 ~15 周期 = 225,000 周期浪费
- CPU 3GHz = ~0.075ms 纯浪费

---

## 2. 技术方案 (Technical Design)

### 2.1 核心技术: 位掩码转换

```cpp
// 将布尔条件转换为全 0 或全 1 的整数掩码
// condition = true  → mask = 0xFFFFFFFF (-1)
// condition = false → mask = 0x00000000 (0)
int mask = -(int)condition;

// 使用掩码进行条件选择 (无分支)
float result = (valueIfTrue & mask) | (valueIfFalse & ~mask);

// 对于浮点数，使用算术形式
float result = valueIfFalse + (valueIfTrue - valueIfFalse) * (float)condition;
```

### 2.2 暴击计算优化

#### Before (有分支)
```cpp
if (isCrit) {
    damage *= critMultiplier;
}
```

#### After (无分支)
```cpp
// 方法 1: 乘法形式
float mult = 1.0f + (critMultiplier - 1.0f) * (float)isCrit;
damage *= mult;

// 方法 2: 选择形式 (更精确)
float noCritDamage = damage;
float critDamage = damage * critMultiplier;
damage = isCrit ? critDamage : noCritDamage;  // 编译器可能优化为 cmov

// 方法 3: 位操作 (整数版本)
uint32_t mask = -(uint32_t)isCrit;
uint32_t result = (critResult & mask) | (normalResult & ~mask);
```

### 2.3 Buff 叠加优化

#### Before
```cpp
void ApplyBuffs(CombatStats& stats, const BuffList& buffs) {
    for (const auto& buff : buffs) {
        if (buff.type == BuffType::Strength) {
            stats.strength += buff.value;
        } else if (buff.type == BuffType::Speed) {
            stats.moveSpeed *= (1.0f + buff.value);
        }
        // ... 更多分支
    }
}
```

#### After (查表 + 无分支)
```cpp
// 预定义 Buff 应用函数表
using BuffApplyFn = void(*)(CombatStats&, float value);
static constexpr BuffApplyFn kBuffApplyTable[] = {
    [](CombatStats& s, float v) { s.strength += v; },     // Strength
    [](CombatStats& s, float v) { s.moveSpeed *= (1.0f + v); }, // Speed
    // ...
};

void ApplyBuffs(CombatStats& stats, const BuffList& buffs) {
    for (const auto& buff : buffs) {
        // 直接索引，无分支
        kBuffApplyTable[(size_t)buff.type](stats, buff.value);
    }
}
```

### 2.4 抗性计算优化

#### Before
```cpp
float ApplyResistances(float damage, DamageType type, const CombatStats& stats) {
    float resistance = 0.0f;
    switch (type) {
        case DamageType::Fire: resistance = stats.resistances[0]; break;
        case DamageType::Cold: resistance = stats.resistances[1]; break;
        // ...
    }
    return damage * (1.0f - resistance);
}
```

#### After (数组索引)
```cpp
float ApplyResistances(float damage, DamageType type, const CombatStats& stats) {
    // DamageType 已设计为连续枚举，直接索引
    float resistance = stats.resistances[(size_t)type];
    return damage * (1.0f - resistance);
}
```

---

## 3. 目标代码位置

| 系统 | 文件 | 函数 | 优化点 |
|------|------|------|--------|
| CombatSystem | `CombatSystem.cpp` | `CalculateFinalDamage` | 暴击/穿透/吸血判断 |
| StatsSystem | `StatsSystem.cpp` | `RecalculateStats` | Buff 条件应用 |
| MonsterAffixSystem | `MonsterAffixSystem.cpp` | `ProcessXXX` | 词缀触发判断 |
| ProjectileSystem | `ProjectileSystem.cpp` | `OnHit` | 穿透/爆炸判断 |

---

## 4. 实现计划

### Task 5.1: 创建 Branchless 工具宏/函数
**文件**: `src/core/util/Branchless.hpp`

```cpp
#pragma once
#include <cstdint>

namespace NoMoreDay::util {

// 将 bool 转为全位掩码
inline int32_t BoolToMask(bool condition) {
    return -(int32_t)condition;
}

// 无分支选择 (整数)
inline int32_t Select(bool condition, int32_t ifTrue, int32_t ifFalse) {
    int32_t mask = BoolToMask(condition);
    return (ifTrue & mask) | (ifFalse & ~mask);
}

// 无分支选择 (浮点)
inline float SelectF(bool condition, float ifTrue, float ifFalse) {
    return ifFalse + (ifTrue - ifFalse) * (float)condition;
}

// 无分支乘法因子
inline float MultFactor(bool condition, float multiplier) {
    return 1.0f + (multiplier - 1.0f) * (float)condition;
}

} // namespace
```

### Task 5.2: 重构 CombatSystem::CalculateFinalDamage

### Task 5.3: 重构 StatsSystem Buff 应用

### Task 5.4: 重构 MonsterAffixSystem 条件逻辑

### Task 5.5: 创建 Benchmark 对比

---

## 5. 验收标准

| 指标 | 基准 | 目标 |
|------|------|------|
| 10k 实体伤害计算 | TBD | 降低 30% |
| 分支预测失败率 (perf stat) | ~25% | < 5% |
| 代码可读性 | - | 保持清晰 (充分注释) |

---

## 6. 风险与缓解

| 风险 | 缓解策略 |
|------|----------|
| 可读性下降 | 封装到 `Branchless.hpp`，使用语义化函数名 |
| 浮点精度 | 对关键计算保留原有分支版本作为 Debug 模式 |
| 编译器已优化 | 使用 Benchmark 验证，若无提升则不改 |

---

## 7. 补充说明

### 7.1 何时**不**使用无分支

- 分支预测准确率 > 95% 的情况 (例如 `if (entity == nullptr)`)
- 分支内有复杂逻辑 (函数调用、内存分配)
- 代码可读性严重受损的情况

### 7.2 编译器提示

```cpp
// 使用 [[likely]] / [[unlikely]] 提示编译器
if (isCrit) [[unlikely]] {
    // 暴击较少见
}
```

---

*设计者: Gemini (Skill: designer)*
