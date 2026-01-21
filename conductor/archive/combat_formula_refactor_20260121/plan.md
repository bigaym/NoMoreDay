# 战斗属性计算重构 - 实现计划

## Track 状态

| 属性 | 值 |
|-----|---|
| **Track ID** | `combat_formula_refactor_20260121` |
| **创建时间** | 2026-01-21 |
| **优先级** | HIGH |
| **预估工期** | 2-3 天 |
| **状态** | ✅ COMPLETED |

---

## 任务清单

### Phase 1: 基础设施 (Infrastructure)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 1.1 | 在 `Common.hpp` 添加 `Scaling` 常量命名空间 | ✅ DONE | 0.5h |
| 1.2 | 创建 `CombatFormula.hpp` 公式工具头文件 | ✅ DONE | 1h |
| 1.3 | 扩展 `CombatStats` 添加评级字段 | ✅ DONE | 0.5h |

### Phase 2: 核心公式实现 (Core Formula)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 2.1 | 实现 `CalculateArmorMultiplier()` | ✅ DONE | 1h |
| 2.2 | 实现 `CalculateDodgeChance()` | ✅ DONE | 1h |
| 2.3 | 实现 `CalculateBlockEffectiveness()` | ✅ DONE | 0.5h |
| 2.4 | 集成到 `StatsSystem::BakeStats()` | ✅ DONE | 1h |

### Phase 3: 伤害管线重构 (Damage Pipeline)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 3.1 | 重构 `DamagePipeline.cpp` 护甲计算 (单次路径) | ✅ DONE | 1h |
| 3.2 | 重构 `DamagePipeline.cpp` 护甲计算 (SIMD 路径) | ✅ DONE | 1.5h |
| 3.3 | 重构 `CombatSystem.cpp` 闪避判定 | ✅ DONE | 1h |
| 3.4 | 重构 `CombatSystem.cpp` 格挡判定 | ✅ DONE | 1h |

### Phase 4: 词缀系统同步 (Affix System)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 4.1 | 添加评级类词缀枚举 (`ItemStats.hpp`) | ✅ DONE | 0.5h |
| 4.2 | 更新 `StatsSystem` 词缀绑定映射 | ✅ DONE | 1h |
| 4.3 | 更新 `ItemFactory` 装备生成逻辑 | ✅ DONE | 1h |

### Phase 5: UI 与显示 (UI & Display)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 5.1 | 更新 `UICharacter.cpp` 显示有效值 | ✅ DONE | 1h |
| 5.2 | 更新装备 Tooltip 显示评级 | ✅ DONE | 0.5h |

### Phase 6: 测试 (Testing)

| ID | 任务 | 状态 | 预估 |
|----|-----|------|-----|
| 6.1 | 编写 `CombatFormulaTest.hpp` 单元测试 | ✅ DONE | 2h |
| 6.2 | 编写 `CombatBalanceTest.hpp` 集成测试 | ✅ DONE | 1h |
| 6.3 | 运行全量测试套件并修复回归 | ✅ DONE | 1h |

---

## 依赖关系

```
Phase 1 ──┬──> Phase 2 ──> Phase 3
          │
          └──> Phase 4 ──> Phase 5
          
Phase 3 + Phase 4 ──> Phase 6
```

---

## 风险登记

| 风险 | 影响 | 缓解措施 |
|-----|-----|---------|
| 破坏现有平衡 | HIGH | 提供常量开关可回退旧公式 |
| SIMD 路径不一致 | MEDIUM | 封装到统一函数，避免代码重复 |
| 存档兼容性 | LOW | 新字段使用默认值 |

---

## 变更日志

| 日期 | 变更内容 |
|-----|---------|
| 2026-01-21 | 创建初始计划 |
| 2026-01-21 | 完成所有 Phase，通过单元测试与集成测试 |