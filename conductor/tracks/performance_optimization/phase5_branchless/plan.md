# Phase 5: Branchless Combat Logic 实施计划

**Track ID**: `performance_optimization/phase5_branchless`  
**状态**: 📋 Planned  
**预计工时**: 1.5 天  
**前置依赖**: Phase 2 (Group 遍历模式)

---

## 任务分解 (Task Breakdown)

### Task 5.1: 创建 Branchless 工具库 ⬜
**优先级**: Critical  
**预计时间**: 1h

**操作**:
1. 创建 `src/core/util/Branchless.hpp`
2. 实现以下工具函数:
   - `BoolToMask(bool) -> int32_t`
   - `Select(bool, int, int) -> int`
   - `SelectF(bool, float, float) -> float`
   - `MultFactor(bool, float) -> float`

**验收条件**:
- [ ] 编译通过
- [ ] 单元测试验证正确性

---

### Task 5.2: 重构 CombatSystem::CalculateFinalDamage ⬜
**优先级**: High  
**预计时间**: 2h

**操作**:
1. 定位 `CombatSystem.cpp` 中的伤害计算函数
2. 识别所有可优化的分支:
   - 暴击判断
   - 护甲穿透判断
   - 吸血判断
   - 伤害类型判断
3. 使用 `SelectF` 和 `MultFactor` 替换分支

**示例改动**:
```cpp
// Before
float finalDamage = baseDamage;
if (isCrit) {
    finalDamage *= critMult;
}
if (hasArmorPen) {
    effectiveArmor = std::max(0.0f, armor - armorPen);
} else {
    effectiveArmor = armor;
}

// After
float critFactor = util::MultFactor(isCrit, critMult);
float finalDamage = baseDamage * critFactor;
float effectiveArmor = util::SelectF(hasArmorPen, 
    std::max(0.0f, armor - armorPen), 
    armor);
```

---

### Task 5.3: 重构 StatsSystem Buff 应用逻辑 ⬜
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 定位 `StatsSystem.cpp` 中的 Buff 应用逻辑
2. 识别 switch/if-else 链
3. 转换为函数指针表或直接索引

**设计**:
```cpp
// 定义 Buff 应用函数表
namespace {
    using ApplyFn = std::function<void(CombatStats&, float)>;
    const ApplyFn kBuffApplyTable[] = {
        [](CombatStats& s, float v) { s.max_health += v; },     // MaxHealth
        [](CombatStats& s, float v) { s.armor += v; },          // Armor
        [](CombatStats& s, float v) { s.crit_chance += v; },    // CritChance
        // ...按 StatType 枚举顺序定义
    };
}

void ApplyModifier(CombatStats& stats, const StatModifier& mod) {
    // 无分支：直接索引调用
    if (mod.mode == ModifierMode::Flat) {
        kBuffApplyTable[static_cast<size_t>(mod.type)](stats, mod.value);
    }
    // ...
}
```

---

### Task 5.4: 重构 MonsterAffixSystem 条件逻辑 ⬜
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 定位 `MonsterAffixSystem.cpp` 中的 `ProcessXXX` 函数
2. 识别触发条件分支
3. 使用无分支技术优化

**示例**:
```cpp
// Before (Molten 词缀)
if (timeSinceLastPulse >= pulseCooldown) {
    CreateMoltenPool(entity);
    timeSinceLastPulse = 0;
}

// After
float shouldPulse = (float)(timeSinceLastPulse >= pulseCooldown);
timeSinceLastPulse = util::SelectF(shouldPulse > 0.5f, 0.0f, timeSinceLastPulse);
// 注意: CreateMoltenPool 仍需分支保护 (有副作用)
```

> **[NOTE]** 对于有副作用的操作 (如创建实体)，仍需保留分支。无分支主要用于纯计算。

---

### Task 5.5: 创建 Branchless 单元测试 ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 创建 `tests/unit/BranchlessTest.hpp`
2. 测试每个工具函数的正确性
3. 测试边界条件 (0, 负数, 最大值)

---

### Task 5.6: 创建 Benchmark 对比 ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 创建 `benchmarks/BranchlessBenchmark.cpp`
2. 对比场景:
   - 10000 次伤害计算 (原版 vs 无分支版)
3. 使用 `perf stat` 测量分支预测失败率
4. 输出性能对比报告

**Benchmark 框架**:
```cpp
static void BM_DamageCalc_Original(benchmark::State& state) {
    for (auto _ : state) {
        for (int i = 0; i < 10000; ++i) {
            benchmark::DoNotOptimize(CalculateDamageOriginal(testData[i]));
        }
    }
}

static void BM_DamageCalc_Branchless(benchmark::State& state) {
    for (auto _ : state) {
        for (int i = 0; i < 10000; ++i) {
            benchmark::DoNotOptimize(CalculateDamageBranchless(testData[i]));
        }
    }
}

BENCHMARK(BM_DamageCalc_Original);
BENCHMARK(BM_DamageCalc_Branchless);
```

---

## 依赖关系

```
Task 5.1 ──► Task 5.2
              │
              ├──► Task 5.3
              │
              └──► Task 5.4
                      │
                      ▼
              Task 5.5 ──► Task 5.6
```

---

## 验收清单

- [ ] `Branchless.hpp` 工具库创建完成
- [ ] `CombatSystem` 伤害计算已重构
- [ ] `StatsSystem` Buff 应用已重构
- [ ] `MonsterAffixSystem` 条件逻辑已优化
- [ ] `BranchlessTest` 通过
- [ ] Benchmark 显示性能提升
- [ ] 所有现有测试通过

---

## 回滚计划

若遇到严重问题:
1. 在 `Branchless.hpp` 中将所有函数实现改为分支版本
2. 无需修改调用点，API 保持一致

---

## 注意事项

### 不适用场景

以下场景**不应**使用无分支优化:
1. 分支内包含复杂逻辑 (如函数调用、内存分配)
2. 分支预测准确率 > 95%
3. 会严重损害可读性的情况

### 代码风格

所有无分支代码必须添加注释说明:
```cpp
// [BRANCHLESS] 使用无分支乘法因子实现条件暴击
// 原逻辑: if (isCrit) damage *= critMult;
float critFactor = util::MultFactor(isCrit, critMult);
damage *= critFactor;
```

---

*规划者: Gemini (Skill: designer)*
