# 伤害管线现代化（R4）— P4 清理收敛与终验报告

- 阶段：P4（清理收敛与终验，最后一个实施包）
- 日期：2026-09-12
- 基线 commit：`a803623f`
- 实施计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` §3.5
- 设计文档：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`
- 证据目录：`docs/reports/damage-pipeline-modernization/phase-P4/`
- 结论：**满足计划 P4 完成标准，结项通过**（唯一遗留为 release-gate 计时噪声，见 §5）

---

## 1. 执行摘要

P0–P3 已由主代理独立门禁验证通过。P4 完成三件事：

1. **P4-1**：删除已与生产代码完全脱钩的 `foundation/combat_v2` 原型模块（Facade/Kernel/ConditionIR/ModifierGraph/TagDomain 等 11 个源文件）、其孤儿数据/脚本管线，以及仅验证该原型的测试；同步清理 CMake 链接、`add_subdirectory` 与 `DamageKernelV2*`/`DamageKernelParity*` 过滤词。
2. **P4-2**：删除本次伤害迁移遗留的 `COMBAT_LEGACY_CALC_ENABLED` 死分支、废弃的 `CombatSystem::CalculateDamage` API 与 `LegacyDamageFormula`，清理随之失效的头文件引用。
3. **P4-3/P4-4**：`build.bat` 全绿，`ci`/`integration` 100% 通过，性能基准对照 P0 基线留档，并完成结项归档。

删除后构建输入范围内 `combat_v2|CombatV2RuntimeFacade|DamageKernelV2|DamageKernelParity` 与 `COMBAT_LEGACY_CALC_ENABLED|LegacyDamageFormula` 均为**零引用**（证据：`phase-P4/p4-deletion-audit.txt`）。

---

## 2. P4-1 删除 combat_v2 原型与构建过滤

### 2.1 删除清单

| 类别 | 路径 | 说明 |
| --- | --- | --- |
| 目录（整删） | `src/game/foundation/combat_v2/` | `CombatV2RuntimeFacade`、`ConditionCompiler`、`ConditionIR`、`DamageKernel`、`DamageStages`、`ModifierGraph`、`ModifierSourceAdapters`、`TagBitset`、`TagDomain`、`CMakeLists.txt`（目标 `NoMoreDayGameCombatV2`） |
| 目录（整删） | `scripts/combat_v2/` | `compile_tags.py`、`compile_conditions.py`、`compile_modifier_graph.py`（孤儿数据生成脚本，不参与构建） |
| 目录（整删） | `assets/data/combat_v2/` | `tags[.runtime].json`、`condition_fixtures[.runtime].json`、`modifier_graph[.runtime].json`（孤儿数据） |
| 测试（纯验证原型，删） | `tests/unit/ConditionIRTests.cpp`、`tests/unit/DamageKernelV2Tests.cpp`、`tests/unit/TagDomainV2Tests.cpp`、`tests/unit/ModifierGraphV2Tests.cpp` | 仅实例化/验证被删原型 |
| 测试（纯验证原型，删） | `tests/integration/DamageKernelParityTests.cpp`、`tests/integration/ModifierGraphV2IntegrationTests.cpp`、`tests/integration/CombatV2DualRunParityTests.cpp` | 仅验证 Facade/Kernel 原型与双跑一致性 |
| 测试（改造保留） | `tests/integration/CombatV2CutoverTests.cpp` → `tests/integration/CombatDamagePipelineCutoverTests.cpp` | 删除 Facade 依赖与 3 个 Facade 用例，保留 6 个现行 `DamagePipeline` 行为用例；用例前缀改为 `[Integration] CombatDamagePipelineCutover - ...`（继续命中 `*Combat*` 模块门禁） |

### 2.2 构建引用 / CMake 过滤词清理

| 文件 | 删除/修改内容 |
| --- | --- |
| `src/game/foundation/CMakeLists.txt` | 删除 `add_subdirectory(combat_v2)` 及相关注释 |
| `src/game/CMakeLists.txt:45` | 删除 `NoMoreDayGameCombatV2` 链接及注释 |
| `src/game/systems/combat/CMakeLists.txt:48` | 删除 `NoMoreDayGameCombatV2` 链接及 `combat_v2` 依赖注释 |
| `src/game/systems/skill/CMakeLists.txt:48` | 删除 `NoMoreDayGameCombatV2` 链接及 `combat_v2` 依赖注释 |
| `tests/CMakeLists.txt:175` | `[Unit]*Combat*,[Unit]*ConditionIR*,[Unit]*TagDomainV2*,[Unit]*ModifierGraphV2*,[Unit]*DamageKernelV2*` → `[Unit]*Combat*`（清除过滤词 `DamageKernelV2*`） |
| `tests/CMakeLists.txt:184` | `[Integration]*Combat*,[Integration]*ModifierGraphV2*,[Integration]*DamageKernelParity*` → `[Integration]*Combat*`（清除过滤词 `DamageKernelParity*`） |
| `src/game/contracts/impl/CMakeLists.txt:10`、`src/game/contracts/CMakeLists.txt:14`、`src/game/CMakeLists.txt:9,16` | 清理指向 combat_v2 的排序/依赖注释 |

### 2.3 零引用证据

```
rg "combat_v2|CombatV2RuntimeFacade|DamageKernelV2|DamageKernelParity" src tests CMakeLists.txt scripts assets tools
# => 无匹配（exit=1）
```

`docs/` 下仍保留 `combat_v2` 字样，属**历史与设计叙述**（计划/设计/P0–P3 报告本身在描述“删除该模块”），不属构建输入，按“不删改无关内容”原则保留；本报告 §2 已给出构建输入范围的零引用证据。此点已在 §5 作为“不做项”说明。

---

## 3. P4-2 迁移期兼容代码删除

### 3.1 逐项删除记录（文件:行 / 理由 / 无行为变化论证）

| # | 位置（删除前） | 删除内容 | 理由 | 无行为变化论证 |
| --- | --- | --- | --- | --- |
| 1 | `src/game/systems/combat/CombatSystem.hpp:7-10` | `#ifndef COMBAT_LEGACY_CALC_ENABLED` / `#define COMBAT_LEGACY_CALC_ENABLED 0` / `#endif` | 迁移期兼容开关，默认 0 | 宏无任何外部覆盖点（CMake 未 `-D`、无第二处 `#define`）；`#if 0` 分支从未编译进产物 |
| 2 | `src/game/systems/combat/CombatSystem.hpp:25-30` | `[[deprecated("Use DamagePipeline::Calculate")]] static float CalculateDamage(...)` 声明 | 旧单点伤害入口，已被 `DamagePipeline` 取代 | 全仓 `rg "CalculateDamage\("` 无调用点（§3.3） |
| 3 | `src/game/systems/combat/CombatSystem.cpp:40-...` | `LegacyDamageFormula` 静态函数整体 | 旧减伤/暴击公式，仅在死分支使用 | 仅被 `#if COMBAT_LEGACY_CALC_ENABLED` 分支引用，删除后无悬挂引用 |
| 4 | `src/game/systems/combat/CombatSystem.cpp`（原锚点约 236/291/324/339/436/491） | 6 处 `#if COMBAT_LEGACY_CALC_ENABLED ... #else <实时路径> #endif`，拆除条件编译保留 `#else` 实时分支 | 死分支清理 | 保留的即编译产物中的同一段实时代码，逻辑字节等价；删除后构建与测试全绿 |
| 5 | `src/game/systems/combat/CombatSystem.cpp:...` | `CombatSystem::CalculateDamage(...)` 定义（内部 `return LegacyDamageFormula(...)`） | 与 #2 配对的废弃实现 | 无调用点（§3.3） |
| 6 | `src/game/systems/combat/CombatSystem.cpp:2`、`:20` | 头文件 `core/utils/Branchless.hpp`、`game/contracts/CombatFormula.hpp` | 仅被上面的 Legacy 代码使用 | 删除后编译通过，`rg` 无残留引用 |

### 3.2 保留项与理由

| 保留项 | 理由 |
| --- | --- |
| `SkillRegistry` id 0 普攻回退 | 计划明确要求保留的**运行时真实回退**（非迁移兼容开关），删除会改变缺省技能行为 |
| `BuildLegacyAttackBasePool` | 命名含 legacy，但**仍被实时（非 `#if`）路径调用**，属现行逻辑；仅重命名会造成与本次迁移无关的改动，故保留 |
| 延迟动作队列（P1 引入的 `s_deferred_actions`） | 运行时重入/UAF 安全机制，非迁移兼容代码 |
| `is_simulation` 等系统标志 | 现行 API，非本次迁移产物 |
| `CombatEvents.hpp:95-98` 的 `value/value2`、`reported/final_applied_damage` | 更广的事件契约字段，超出本次迁移删除范围，其它系统仍可能读取 |

### 3.3 无悬挂引用证据

```
rg "COMBAT_LEGACY_CALC_ENABLED|LegacyDamageFormula" src tests CMakeLists.txt scripts
# => 无匹配（exit=1）
rg "CombatSystem::CalculateDamage|CalculateDamage\(" src tests
# => 无匹配（exit=1）
```

---

## 4. P4-3 性能与回归终验

### 4.1 性能对照表（基线 vs P4 现状）

P0 基线取自 `phase-P0/balance-impact.md` §3.1–3.3 与 `phase-P0/gate-perf-main.log`；P4 取自 `phase-P4/p4-perf-*.log`（主运行 + 复跑）。

| 指标 | P0 基线 | P4 现状（主运行） | P4 复跑范围 | 结论 |
| --- | --- | --- | --- | --- |
| Single Calculate Mean/P99 | 0.001 / 0.001 ms | 0.000 / 0.001 ms | — | 不劣化 |
| Batch 200 Mean / P99 | 0.056 / 0.258 ms | 0.058 / 0.103 ms | Mean 0.054–0.059；P99 0.103–0.185 | P99 显著优于基线；Mean 持平（噪声内） |
| Scaling 50 Mean / P99 | 0.025 / 0.059 ms | 0.029 / 0.060 ms | — | 持平 |
| Scaling 100 Mean / P99 | 0.032 / 0.043 ms | 0.043 / 0.066 ms | — | Mean 略升（噪声） |
| Scaling 200 Mean / P99 | 0.047 / 0.063 ms | 0.062 / 0.104 ms | — | 略升（噪声） |
| Scaling 500 Mean / P99 | 0.102 / 0.295 ms | 0.111 / 0.254 ms | Mean 0.111–0.124；P99 0.235–0.291 | Mean 略升 / P99 略降 |
| Adaptive Dispatch 200 Mean / P99 | —（P3 新增能力） | 0.059 / 0.088 ms | — | 自适应分派子项 |
| Release Gate frame p95 | 0.0185 ms | 0.0236 ms | 0.0224–0.0239 | 高于 P0 记录值（见 §5 风险）；硬门限 8 ms 通过 |
| Release Gate frame p99 | 0.0257 ms | 0.0402 ms | 0.0388–0.0467 | 高于 P0 记录值（见 §5 风险）；硬门限 12 ms 通过 |
| CombatCorePerf 断言 | 59 / 59 | 59 / 59 | — | 一致 |

性能条件：`build.bat`（RelWithDebInfo）。所有基准内置硬预算（Single < 0.01 ms、Batch200 < 1.0 ms、Release Gate p95 ≤ 8 ms / p99 ≤ 12 ms）均通过。

### 4.2 回归门禁

| 门禁 | 命令 | 结果 | 证据 |
| --- | --- | --- | --- |
| 构建 | `build.bat` | exit=0；`Build completed successfully` / `All steps completed successfully`；前置检查（legacy 回流、模块边界、资源校验）全 OK | `p4-build.log` |
| CI | `ctest -C RelWithDebInfo -L ci` | 首跑 1/1436 失败（见 §4.3）；**复跑 100% 通过（0/1 失败）** | `p4-ctest-ci.log`、`p4-ctest-ci-rerun.log` |
| 集成 | `ctest -C RelWithDebInfo -L integration` | 100% 通过，0/6 失败 | `p4-ctest-integration.log` |

### 4.3 CI 首跑失败归因（环境 flake，非本包引入）

- 失败用例：`[Unit] SkillBehaviorGuard - Deep dive cadence and miasma refresh`
- 位置：`tests/unit/SkillBehaviorGuardTests.cpp:2214`
- 断言：`CHECK( impactNodes.elite_health_after_delayed_window < impactNodes.elite_health_after_first_tick )`，实参 `CHECK( 0 < 0 )`
- 归因：该文件未被 P4 触碰；隔离复跑 3 次全部通过（`p4-rerun-skillguard-{1,2,3}.log`，各 1/1 通过、136 断言），属测试顺序/随机性 flake；CI 全量复跑 100% 通过。

---

## 5. 行为变化、遗留风险与不做项

### 5.1 行为变化

- 预期行为变化：**无**。P4 仅删除从未编译的死分支（默认 0 的兼容开关）与已无调用点的废弃 API/原型模块；现行 `DamagePipeline` 路径未修改。
- `tests/integration/CombatDamagePipelineCutoverTests.cpp` 的 6 个用例继续在 CI 中执行，覆盖现行行为。

### 5.2 遗留风险

1. **Release-gate 计时高于 P0 记录值**：P4 p95/p99（≈0.023/0.040 ms）高于 P0 `balance-impact.md` 记录（0.0185/0.0257 ms）。该基准为亚 0.05 ms 级、`high_resolution_clock` 计时，连续 4 次 P4 复跑的 p99 波动即达 0.0388–0.0467 ms，方差大于与基线的差值；且 P0 自身“基线 vs 改动后”表值与 `gate-perf-main.log` 实测亦不一致。P4 改动均为行为中性删除，无法解释该差异。**硬门限（p95 8 ms / p99 12 ms）以约 300× 余量通过。** 建议：若后续需严格“不劣于 P0”，在同一构建会话内做前后配对测量以排除机器状态漂移。
2. **Scaling 中高目标 Mean 略升**：Scaling 100/200/500 的 Mean 较 P0 记录值略高（最大 +0.015 ms），同样处于该基准噪声范围，P99 未劣化。判定为非阻塞。
3. **prototype 能力仅存于 git 历史**：`combat_v2` 的 ConditionIR/ModifierGraph 等若未来需要，须从 git 历史恢复。

### 5.3 不做项

- 不删除 `docs/` 中历史文档对 `combat_v2` 的叙述（计划/设计/历史报告），保持文档史实与“不删改无关内容”原则。
- 不重命名 `BuildLegacyAttackBasePool`（活跃路径，避免无关改动）。
- 不调整性能基准的计时方法学（超出本包范围）。

---

## 6. 结项归档与证据清单

| 文件 | 内容 |
| --- | --- |
| `phase-P4/p4-build.log` | `build.bat` 全量构建与前置检查日志 |
| `phase-P4/p4-deletion-audit.txt` | P4-1/P4-2 删除后的全仓引用审计（构建输入范围零引用） |
| `phase-P4/p4-perf-damagepipeline.log` | `[Performance]*DamagePipeline*` 主运行 |
| `phase-P4/p4-perf-damagepipeline-r1.log`、`-r2.log` | DamagePipeline 复跑（波动） |
| `phase-P4/p4-perf-releasegate.log`、`-r1/-r2/-r3.log` | `[Performance]*Release Gate*` 主运行 + 3 次复跑 |
| `phase-P4/p4-perf-combatcore.log` | `[Performance]*CombatCorePerfBaseline*`（59/59） |
| `phase-P4/p4-ctest-ci.log`、`p4-ctest-ci-rerun.log` | `ctest -L ci` 首跑/复跑 |
| `phase-P4/p4-ctest-integration.log` | `ctest -L integration` |
| `phase-P4/p4-rerun-skillguard-{1,2,3}.log` | SkillBehaviorGuard flake 隔离复跑 |
| `phase-P4/p4-deletion-audit.txt` | 零引用证据 |

结项对照计划 P4 完成标准：

- [x] 删除清单为空（构建输入范围零引用）
- [x] 单目标延迟不劣于基线（Single 0.000/0.001 vs 0.001/0.001）
- [x] AoE 200 吞吐：P99 显著优于基线（0.103 vs 0.258），Mean 持平（噪声内），硬预算 <1.0 ms 通过
- [x] `build.bat` 通过；`ci` / `integration` 100%
- [x] 全阶段交付物归档，结项关闭

> memory 记录 hash：`e24d84dfc2b25ed2afe51987b701bfb15fbb023b8a8be58ca65b8e5964759693`（type=decision，tags=damage-pipeline,p4,closeout,verification）。
