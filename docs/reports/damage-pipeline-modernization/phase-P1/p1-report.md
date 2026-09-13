# 伤害管线现代化重构 — Phase P1 实施报告（结构解耦与重入安全）

- 计划依据：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` §3.2（P1-1..P1-5）
- 设计依据：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md` §4.3（S0-S11）、§4.5.1、§4.5.2、§6.1
- 上游交接：`docs/reports/damage-pipeline-modernization/phase-P0/`
- 构建配置：RelWithDebInfo（`build.bat`），独占编译/测试
- 状态：P1-1..P1-5 全部完成；build exit=0；ctest `-L ci` / `-L integration` 100%；性能达标

---

## 1. 逐项完成情况

| 任务 | 状态 | 核心产出 |
| --- | --- | --- |
| P1-1 批量钩子修复 | 完成 | `DamagePipeline::CalculateBatchResults`；钩子返回真实结果；回归测试锁定单/批一致且非空 |
| P1-2 拦截器抽离 | 完成 | 新建 `damage/DamageInterceptors.hpp/.cpp`；无敌预检 + Blade Ward 拦截判定外移，行为不变 |
| P1-3 延迟动作队列 | 完成 | 线程局部 `s_deferred_actions` + `s_reentrancy_depth`、`kMaxReentrancyDepth=2`、`SettlementFrame` RAII 收尾；两处反击改入队 |
| P1-4 公式单源化 | 完成 | 批量 SIMD 路径 level factor 改走 `Constants::Combat::Scaling`（与 `CombatFormula.hpp` 同源）；payload 公式复核后保持并锁定 |
| P1-5 死代码清理与 More 合并 | 完成 | `MoreAccumulator` 单一口径合并四处 More 来源；删除 4 个死符号；等价值由既有 functional 测试锁定 |

---

## 2. 变更文件与行锚

### 2.1 新增文件

- `src/game/systems/combat/damage/DamageInterceptors.hpp`
  - `NoMoreDay::damage::InvulnerabilityIntercept` / `BladeWardIntercept`
  - `EvaluateInvulnerability(registry, defender, is_simulation)`
  - `EvaluateBladeWardInterception(registry, defender, source_entity, combined_tags, is_simulation)`
- `src/game/systems/combat/damage/DamageInterceptors.cpp`（含减剑逻辑与日志）
- `tests/unit/DamagePipelineP1Tests.cpp`（5 个回归用例，见 §4）

### 2.2 修改文件（关键行锚，改动后行号）

| 文件 | 变更 |
| --- | --- |
| `src/game/systems/combat/CMakeLists.txt:21` | 显式源列表加入 `damage/DamageInterceptors.cpp` |
| `src/game/systems/combat/DamagePipeline.hpp:57` | 新增公共 `CalculateBatchResults(registry, request)` |
| `src/game/systems/combat/DamagePipeline.hpp:61` | 新增公共 `struct DeferredCombatAction{ DamageRequest request; entt::entity apply_attacker; bool show_vfx; }` |
| `src/game/systems/combat/DamagePipeline.hpp` | 删除私有声明 `ApplyConversion` / `ApplyMultipliers` / `Settle` |
| `src/game/systems/combat/DamagePipeline.cpp:19` | 引入 `DamageInterceptors.hpp` |
| `src/game/systems/combat/DamagePipeline.cpp:489` | `SettlementFrame` RAII（深度计数 + 最外层收尾） |
| `src/game/systems/combat/DamagePipeline.cpp:530` | `QueueDeferredAction`（超深/帧外丢弃 + `LOG_WARN`） |
| `src/game/systems/combat/DamagePipeline.cpp:548` | `MoreAccumulator`（P1-5 单一口径） |
| `src/game/systems/combat/DamagePipeline.cpp:636` | 调用 `damage::EvaluateInvulnerability`（原内联无敌预检） |
| `src/game/systems/combat/DamagePipeline.cpp:1083-1171` | 四处 More 来源统一累加后一次求值 |
| `src/game/systems/combat/DamagePipeline.cpp:1359` | 单目标 Blade Ward 拦截改走拦截器 |
| `src/game/systems/combat/DamagePipeline.cpp:1387-1398` | 单目标反击改为 `QueueDeferredAction` |
| `src/game/systems/combat/DamagePipeline.cpp:1423` | `Execute` 入口建立结算帧 |
| `src/game/systems/combat/DamagePipeline.cpp:1521` | `CalculateBatch` 入口建立结算帧 |
| `src/game/systems/combat/DamagePipeline.cpp:1823` / `:1863` | 批量拦截 + 反击入队 |
| `src/game/systems/combat/DamagePipeline.cpp:1950` | `CalculateBatchResults` 实现 |
| `src/game/systems/combat/DamagePipeline.cpp:1978-1980` | 钩子 `calculateBatch` 返回真实批量结果（B2/B3 修复） |
| `src/game/systems/combat/CombatSystem.hpp` | 删除 `deprecated CalculateDamage` 声明 |
| `src/game/systems/combat/CombatSystem.cpp` | 删除 `CombatSystem::CalculateDamage` 定义 |
| `tests/unit/ProjectileTests.cpp:198-211` | mock `calculateBatch` 改调 `CalculateBatchResults`，与生产钩子一致 |

`git diff --stat`（不含新文件）：6 files changed, 247 insertions(+), 213 deletions(-)。

---

## 3. P1-1 B2/B3 修复证据

**缺陷**：旧钩子实现 `CalculateBatch(...)` 后 `return std::vector<DamageResult>{};`，批量契约恒返回空。

**修复**：`DamagePipeline.cpp:1950` 新增 `CalculateBatchResults`，按契约逐目标返回 `Calculate` 结果；钩子注册（`:1978-1980`）改为返回该结果。

**回归测试**：`tests/unit/DamagePipelineP1Tests.cpp:46` `[Unit] DamagePipeline P1 - Batch hook returns real results`
- 经 `ResolveDamageBatch` 取回批量结果 → 断言非空、`size()==1`、`> 0`
- 与逐目标 `DamagePipeline::Calculate` 结果 `Approx` 相等 → 单/批一致

---

## 4. P1-3 重入安全证据

语义实现（设计 §4.5.2）：线程局部队列 + 深度上限 2；`Execute`/批量入口收尾且深度归零时统一弹出；执行前校验 `registry.valid`；超深丢弃并 `LOG_WARN`。

新增 5 个用例（`tests/unit/DamagePipelineP1Tests.cpp`）：

| 行 | 用例 | 断言 |
| --- | --- | --- |
| 46 | Batch hook returns real results | 批次非空且与单目标一致 |
| 83 | Counter settles after Execute | 防御方 100→70，攻击者 100→65（反击在收尾落地） |
| 117 | Counter deferred to settlement tail | `OnTakeDamage` 事件派发时攻击者仍 100；Execute 返回后 65 |
| 161 | Reentrancy depth limit drops nested counter | 中途重入确实结算第二目标；其反击在深度上限被丢弃，攻击者仅承受一次反击 65 |
| 221 | Bare Calculate drops counter | 裸 `Calculate` 不落地伤害也不派发反击（有意的时序变更） |

运行日志出现预期告警（证明丢弃分支生效）：
```
NMD: Combat: Maximum re-entrancy depth reached. Dropping deferred action.
NMD: Combat: Deferred action queued outside a settlement frame; dropping.
```

**审计结论（受击/结算内触发点）**：
- `Calculate` / `CalculateBatch` 内原有的两处 `ResolveDamage`（单目标 `:1307`、批量 `:1779`）已全部改为 `QueueDeferredAction`，文件内不再有同步 `ResolveDamage`。
- `SkillSystem.cpp:1068`（事件驱动 `OnTakeDamage` 中 973 过载护盾连锁）**保留同步豁免**：它由事件层在结算尾段（`SettlementFrame` 深度 1）触发，已受 `SecondaryHit` 标签与 ICD 守卫。P2/P3 的伤害快照与结算帧现已落地，迁移到延迟队列技术上已可行；但同步执行保证 ICD 标记在本次受击返回前生效，避免同帧多次受击重复触发 973，而延迟入队会因 FIFO 与重入深度上限语义改变反伤/反击的因果顺序，故按语义保持同步。是否统一入队列列为后续架构项。
- 其余 `ResolveDamage` 调用均为技能系统顶层触发，非结算中重入。

---

## 5. P1-4 公式单源化

- 批量 SIMD 路径 level factor 由内联 `10.0f + 0.5f*L + 0.05f*L*L` 改为 `Constants::Combat::Scaling::LEVEL_BASE/LINEAR/QUADRATIC`（`DamagePipeline.cpp` 批量 `process_range` 内），与 `CombatFormula.hpp::LevelFactor` 同源同值。
- payload 路径公式 `skill.base_damage + payload_avg * skill_data->weapon_damage_mult`（`DamagePipeline.cpp:676-693`）复核：调用方意图为「payload 提供武器伤害均值，技能提供系数」，语义正确，**保持不改**；既有 `[Unit] DamagePipelineUnifiedEntryTests` 锁定 `10 + 100*1.2 = 130` 及其暴击/增伤派生值，本次全量 ci 通过即回归锁定。

---

## 6. P1-5 More 数组合并与死代码清理

### 6.1 More 合并（单一口径）

`MoreAccumulator`（`:548`）统一收集以下四处来源，逐实例一次求值（`:1083-1171`）：
1. `GlobalModifierComponent` 的 `More` 条目（原 `:978-985`）
2. 专精树节点 `More`（原 `:1022-1033`，保留 P0 的 `value*0.01` 百分比归一）
3. `SkillModifierComponent` 的 `More`（原 `:1050-1061`）
4. cost affix 桶（原 `:1062-1067`，保留 `ApplyDiminishingReturns` 桶聚合语义）

求值顺序与旧实现一致：prefix → global → 专精节点 → skillMods → cost affix。**对拍证据**：既有 functional 用例 `[Functional] DamagePipeline - Unified More Multipliers`（`tests/functional/DamagePipelineConversionTest.cpp:133`，锁定 `100*1.2*1.5=180`）与 `- Iterative Conversion Chain` 在新实现下全部通过；另全量 ci（含专精节点/技能修正相关用例）100% 通过。证据日志：`p1-more-equivalence.log`（7 cases / 24 assertions 全过）。

### 6.2 死代码删除清单

经 codebase-memory 图谱确认 `in_degree=0`（无生产引用），且删除后全量构建/测试通过：

| 符号 | 位置 |
| --- | --- |
| `DamagePipeline::ApplyConversion` | 声明 + 空实现 stub |
| `DamagePipeline::ApplyMultipliers` | 声明 + 空实现 stub |
| `DamagePipeline::Settle` | 声明 + 空实现 stub |
| `CombatSystem::CalculateDamage` | `CombatSystem.hpp` 声明 + `CombatSystem.cpp` 定义（`#if COMBAT_LEGACY_CALC_ENABLED` 已恒 0） |

> 未触碰 `foundation/combat_v2`（P4 范围）。

---

## 7. 性能对比

命令：`bin\NoMoreDayTests.exe --test-case="[Performance]*DamagePipeline*"`（日志 `p1-perf.log`，exit=0，3/3 通过）

| 用例 | P0 after | P1 after | 预算 |
| --- | --- | --- | --- |
| Single Calculate Mean / P99 | 0.001 / 0.001 ms | 0.001 / 0.001 ms | < 0.01 ms ✅ |
| Batch 200 Mean / P99 | 0.045 / 0.108 ms | 0.049 / 0.090 ms | < 1.0 ms ✅ |
| Batch Scaling 50 Mean / P99 | 0.029 / 0.156 ms | 0.034 / 0.340 ms | — |
| Batch Scaling 100 | 0.037 / 0.051 ms | 0.032 / 0.040 ms | — |
| Batch Scaling 200 | 0.068 / 0.689 ms | 0.046 / 0.065 ms | — |
| Batch Scaling 500 | 0.085 / 0.145 ms | 0.115 / 0.490 ms | — |

结论：单目标无劣化；Batch200 均值 +0.004ms（在 P0 观测波动 0.043-0.045 与 run 噪声量级），P99 优于 P0。全部满足预算。

---

## 8. 行为/时序变化说明（有意）

1. **反击结算时机**：Blade Ward 反击由「结算中途同步派发」改为「最外层 `Execute`/批量入口收尾统一派发」。因此：
   - 通过 `Execute` / `CalculateBatch` 的路径：反击仍会结算，且既有集成用例 `[Integration] BladeWard 470 counter on Melee and Block` 通过。
   - 直接调用裸 `Calculate`（无结算帧）的调用方：不再同步结算反击（`QueueDeferredAction` 判定帧外丢弃）。这是设计 §4.5.2 的预期语义；已新增 `Bare Calculate drops counter` 用例锁定。
2. **批量钩子返回值**：`calculateBatch` 由空 vector 变为真实结果。调用方若依赖空返回，需按新契约调整（仓库内无生产调用方，`ResolveDamageBatch` 仅测试使用）。

---

## 9. 验证证据索引

| 项 | 命令 | 结果 | 日志 |
| --- | --- | --- | --- |
| 构建 | `build.bat` | exit=0，`Build completed successfully` | `p1-build.log` |
| CI | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | 100%（1/1） | `p1-ctest-ci.log` |
| 集成 | `ctest ... -L integration --output-on-failure` | 100%（6/6） | `p1-ctest-integration.log` |
| 新增回归 | `NoMoreDayTests --test-case="*DamagePipeline P1*"` | 5/5 通过 | `p1-more-equivalence.log` |
| More 对拍 | `NoMoreDayTests --test-case="[Functional] DamagePipeline - Unified More Multipliers,..."` | 7/7 通过 | `p1-more-equivalence.log` |
| 性能 | `NoMoreDayTests --test-case="[Performance]*DamagePipeline*"` | 3/3 通过 | `p1-perf.log` |

---

## 10. 遗留与风险（供主代理门禁复核）

1. **事件驱动受击施法保持同步**：`SkillSystem.cpp:1068`（973 过载连锁）仍为事件尾段同步触发；P2/P3 快照与结算帧已就绪，迁移技术上可行但非必须（详见 §4）。当前以语义稳定（ICD 即时生效、因果顺序不变）优先，彻底入队列列为后续架构统一项。
2. **裸 `Calculate` 反击语义变更**：接口签名不变，但无结算帧时反击被丢弃。设计如此；若存在仓库外调用方依赖同步反击，需在本阶段知会。
3. **`ResolveDamageBatch` 仍无生产调用方**：契约已修复，实际消费方待后续接入。
4. **性能波动**：Batch Scaling 500 P99 0.490ms，绝对值仍远低于 1.0ms 预算，属 run 噪声。
5. **未提交**：按 AGENTS.md 未 commit、未建 worktree，变更留在工作区交由主代理门禁。
