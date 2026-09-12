# P4 审查修复报告（damage pipeline modernization）

- 范围: 独立 cpp-reviewer 对 P0–P4 未提交变更提出的 M1/M2/M3/M4 与 m1–m6。
- 环境: Windows + MSVC (VS 2026 generator), `RelWithDebInfo`, `build.bat`。
- 约束: 未 commit；未改动无关内容；注释中文、标识符/日志英文。
- 证据日志（同目录）:
  - `P4-review-build.log`（最终 build.bat，exit=0）
  - `P4-review-ctest-ci.log`、`P4-review-ctest-integration.log`
  - `P4-review-m1-bugrepro.log`（M1 测试在旧语义下的失败复现）

---

## 1. M1（Major）Poison/Shadow 索引错位

### 根因
存在两条位序，仅 4/5 位互换：
- 位序 A（`DamagePool`/`Tag` 位）：`Tag::Shadow=1<<4`、`Tag::Poison=1<<5`，故池索引 4=Shadow、5=Poison；
- 位序 B（`DamageType`/`CombatStats.resistances[]`）：`DamageType::Poison=4`、`Shadow=5`，`resistances[]` 由 `StatType::ResistPhysical+i` 填充，故索引 4=Poison、5=Shadow。

旧实现把池索引 j 直接当作 DamageType/resistances 索引消费，导致纯 Shadow 读 Poison 抗性、纯 Poison 读 Shadow 抗性。

### 修复
- 新增集中映射 `src/game/systems/combat/damage/DamageTypes.hpp`（含两条位序的中文说明）：
  - `PoolIndexToDamageType(pool_index)`：0–3 恒等，4→Shadow(5)，5→Poison(4)，越界→`DamageType::Count`；
  - `PoolIndexToResistIndex(pool_index)`：同上映射为 StatType/resistances 索引；
  - 附带 `DamageTypeToPoolIndex` / `DamageTypeToResistIndex` 供反向场景。
- 单目标生产路径 `src/game/systems/combat/DamageMitigationService.cpp`：
  - `:126` 由 `final_type` 得到池索引后，`:129-132` 映射出 `damage_type` 与 `resist_index`；
  - `:144` `resistances[resist_index]`；`:147` `ApplySkillScopedResistEffects(..., damage_type)`；
  - `:162-163` `:191-194` 元素比较改用 `damage_type`；`:206/:238` `ElementTagOf(damage_type)`；`:225` cap suppression 传 `damage_type`。
- 批量标量内核 `src/game/systems/combat/DamagePipeline.cpp:1690-1701`：
  - `resistances[resist_idx]`、`debuff_resist_aggregate(..., pooled_type)`、`debuff_resist_cap_suppression(..., pooled_type)`。
- 批量 SIMD 内核 `src/game/systems/combat/DamagePipeline.cpp:1811-1827`：同上映射。
- 头文件 `#include` 于 `DamagePipeline.cpp:21`、`DamageMitigationService.cpp:2`。

### 跨位序审计（指令第 3 条）
用 `rg` 扫描 `static_cast<DamageType>`、`resistances[`、`damage_multipliers[`、`ElementTagOf`、`countr_zero`：
- `DamageConversion.hpp` 与 `DamagePipeline.cpp:934-936`（及转换实例重建 `:1014-1022`）：整条转换链一致地以“DamageType 变量 = 池索引”使用（`1<<idx` 还原 Tag），自洽往返，未改。
- `NemesisGenerator.cpp:337-346`：以 `DamageType::X` 枚举值写 `resistances[]`，与 StatType 序一致，未改。
- `UICharacterController.cpp:644`：以 `DamageType` 枚举读 `resistances[]`，同序，未改。
- `DamagePipeline.cpp:1099-1120`：以 `Tag` switch 映射到 `StatType::PoisonDamage/ShadowDamage`，未按池索引读，正确。
- `AttributePipeline.cpp:781-783/800-804`、`StatsSystem.cpp:184-212`：`damage_multipliers[]/damage_percent_*[]/resistances[]` 均按 `StatType` 序写入与读取，同上，未按池索引消费。
- 快照构建 `DamagePipeline.cpp:1527-1528` 的 `base_damage[i]` 来自仿真 `Calculate` 的 `final_pool`（池序），逐元素乘区在仿真内部经 `Tag→StatType` switch 施加，无跨序。
- 结论: 除已修复的三处外，未发现其他同型跨位序消费；`damage_multipliers`/per-element 数组不存在按 pool 位索引读取 StatType 序数组的情况。

### 回归测试与证据
新增 `tests/unit/DamageElementIndexTests.cpp`（doctest 自动 glob 收录）：
1. `[Unit] DamageElementIndex - single target Shadow/Poison resist mapping`
   - 守方 `resistances[Poison(4)]=0.8`、`resistances[Shadow(5)]=0.1`；纯 Shadow 100 → 90；纯 Poison 100 → 25（0.8 被 `RESISTANCE_MAX=0.75` 钳制）。
2. `[Unit] DamageElementIndex - batch kernels Shadow/Poison resist mapping`
   - N=8（触发 SIMD）对 Shadow/Poison 分别跑强制标量与自动 SIMD，断言 8 个目标扣血均为 90 / 25，并用 `BatchKernelStats` 校验确实覆盖两条内核。

**旧语义失败复现（真实证据）**: 临时把 `DamageTypes.hpp` 两个映射改为 legacy 直取（4→4、5→5，等价旧行为）后重建并运行：
```
[doctest] test cases: 3 | 1 passed | 2 failed | 1531 skipped
[doctest] assertions: 56 | 22 passed | 34 failed
values: CHECK( 25 == Approx( 90 ) )   // 纯 Shadow 旧行为读到 Poison 抗性
values: CHECK( 90 == Approx( 25 ) )   // 纯 Poison 旧行为读到 Shadow 抗性
Status: FAILURE!
```
随后恢复正确映射、重建：`3 | 3 passed`。完整日志见 `P4-review-m1-bugrepro.log`。

---

## 2. M2（Major）SIMD 与标量护甲结算分歧

### 根因
- 标量 `DamagePipeline.cpp:1680`: `armor = (def_stats ? armor : 0) + endgameArmorDelta`；
- SIMD 旧: `(j==0 && ds) ? (ds->armor + delta) : 0`。
无 `CombatStats` 且 endgame 护甲增量非 0 时，标量计入 delta、SIMD 记 0。

### 修复
`src/game/systems/combat/DamagePipeline.cpp:1830-1832` 改为与标量一致：
```cpp
armor_batch_data[k] =
    (j == 0) ? ((ds ? ds->armor : 0.0f) + endgame_armor_delta_data[k]) : 0.0f;
```

### 测试
- 现有 `DamagePipelineP3b` 标量/SIMD 一致性套件全部通过（`*DamagePipeline*`: 42 cases / 5785 assertions）。
- “无 CombatStats + endgame 护甲增量”场景**未能注入**：现有 builtin（`EndgameModifierContract.cpp:209-245`）与 `assets/data/endgame_modifier_contracts.json` 均无 `incoming_armor_bonus != 0` 的合同，且没有公开的运行时注册接口，无法在测试中构造非零 endgame 护甲增量。该分支由代码审查保证：两内核现在使用完全相同的表达式与同一 endgame delta 数据源。

---

## 3. M3（Major）Skill5 node 555 暴伤单位冲突

### 根因
`SkillSpecializationBaker.cpp:624` 旧为 `GetFloat(5,555,...)*points*100.0f`（2 点=40.0），而其他节点/消费端（node 332 `:479`、`SummonCombatBridge.cpp:88`、`BeamChannelDeliverySystem.cpp:294/:1184`）均按分数制直接累加到暴击倍率。

### 修复
- `SkillSpecializationBaker.cpp:624` 去掉 `* 100.0f` → 2 点 = 0.4（`profile.delivery.bonus_crit_damage`）。
- `InfiniteBlades.cpp:213` 去掉 `/ 100.0f` → `(1.0f + profile->delivery.bonus_crit_damage)`（配合分数制；4 点仍为 ×1.8，行为保持）。
- `tests/unit/SkillSpecializationBakerTests.cpp:232,236`：注释改为 `2 * 0.2 = 0.4`，期望 `Approx(0.4f)`。

### 测试
- `*SkillSpecializationBaker*`: 7 cases / 113 assertions 通过。
- `*Skill 5*`（含 `Intent Burst 554 & Multiplier 555`）: 14 cases / 145 assertions 通过。
- `*Skill 3*`（BladeFormation 相关）: 24 cases / 213 assertions 通过。
- `BladeFormationNodes.cpp:498` 的 node 332 期望 `1.0f` 不受影响（333/332 走分数制路径）。

---

## 4. Minor

| 项 | 修复 | 证据 |
|---|---|---|
| m1 必暴保底 | `DamagePipeline.cpp:1746`（标量）、`:1925`（SIMD）改为 `snap.crit_chance >= 1.0f \|\| (snap.crit_chance > 0.0f && GetFloat01() < snap.crit_chance)` | 新增 `[Unit] DamageElementIndex - batch guaranteed crit at chance 1.0`（标量+SIMD，crit_chance=1.0 → 伤害恒为 base×crit_damage） |
| m2 命名/日志 | `DamageInterceptors.cpp:35-40` `is_physical_projectile`→`is_projectile_source`，注释明确“投射物源交由 ProjectileSystem 处理（交叉引用 `ProjectileSystem.cpp:509-527`），此处仅跳过”；`:57` 日志改为 `Blade Ward: intercepted non-projectile-source hit...` | 编译通过；相关用例通过 |
| m3 转换入参防御 | `DamageConversion.hpp:66-75` 预检 `!std::isfinite(pct) \|\| pct <= 0` 并 `LOG_WARN`；`:88`、`:101` 两个规则循环跳过非法规则（避免在逐源循环重复告警）。引入 `core/logging/Logger.hpp`（该头仅 `DamagePipeline.cpp` 消费，成本低） | 编译通过 |
| m4 对齐 | `DamagePipeline.cpp:1762-1770` 全部 `alignas(32)`→`alignas(64)`（覆盖 AVX-512 W=16 的 `load_aligned` 需求） | 编译通过；P3b SIMD 一致性通过 |
| m5 测试卫生 | 删除 `tests/unit/DamagePipelineP1Tests.cpp:1` 的 `#pragma once` | 首行现为 `#include "TestCommon.hpp"` |
| m6 SIMD try_get 缓存 | **won't-fix**（性能预算已满足，避免热路径风险） | — |

---

## 5. M4（Major，契约处理，不改行为）

`DamagePipeline::CalculateBatch`（void 批量入口）无生产调用方（仅 tests/benchmark），缺 invuln/FrostAmp/source_entity 快照复用。已在其声明（`DamagePipeline.hpp:42-56`）加中文契约注释：测试/基准专用；生产批量走 `ResolveDamageBatch`→逐目标 `Calculate`；列明不等价点。**未改行为**，作为跟踪项。

---

## 6. 行为变化清单（供 balance 关注）

1. **Shadow/Poison 抗性结算修正（仅这两个元素）**：修复前纯 Shadow 误用 Poison 抗性、纯 Poison 误用 Shadow 抗性；修复后按正确元素结算。凡 Shadow/Poison 抗性不同的目标，两种元素的最终伤害互换为正确值。0–3（Physical/Fire/Cold/Lightning）不受影响。
2. **Skill5 node 555 数值口径**：`profile.delivery.bonus_crit_damage` 由“×100 的百分数”改为分数制（2 点 0.4、4 点 0.8）。直接消费该字段的 Summon/Beam 侧暴伤增量随之下调 100×；`InfiniteBlades` 侧同步去掉 `/100`，其 `bonus_damage_mult`（4 点 ×1.8）行为不变。
3. **SIMD 缺 CombatStats 目标的护甲**：现在正确计入 endgame 护甲增量（此前 SIMD 记 0）。仅影响“无 CombatStats + endgame 护甲修正”组合。
4. **必暴确定性**：crit_chance≥1.0 时不再依赖随机数，输出确定。
5. 其余（m2 日志/命名、m3 非法转换规则跳过、m4 对齐、m5 测试头、M4 注释）无玩法行为变化。

---

## 7. 验证汇总

| 命令 | 结果 |
|---|---|
| `build.bat`（RelWithDebInfo） | exit=0 |
| `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | 100% (1/1)，7.26s |
| `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | 100% (6/6)，5.50s |
| `NoMoreDayTests --test-case="*DamageElementIndex*"` | 3 cases / 56 assertions 通过 |
| `--test-case="*DamagePipeline*"` | 42 cases / 5785 assertions 通过 |
| `--test-case="*SkillSpecializationBaker*"` | 7 cases / 113 assertions 通过 |
| `--test-case="*Skill 5*"` / `"*Skill 3*"` | 14/145、24/213 通过 |
| `--test-case="*ElementPath*"` | 8 cases / 33 assertions 通过 |

本轮无 flake；已知 S1a GPU 计时与个别单用例未触发。

## 8. 未修项

- m6（SIMD 内重复 `try_get` 缓存）：性能预算已满足，避免热路径风险，won't-fix。
- M4 `CalculateBatch` 语义不等价：按决策不改行为，仅加契约注释，需后续跟踪。
- M2 “无 CombatStats + endgame 护甲增量”测试注入：现有合同不存在非零 `incoming_armor_bonus`，无法构造；靠两内核共用同一表达式 + 代码审查保证。
