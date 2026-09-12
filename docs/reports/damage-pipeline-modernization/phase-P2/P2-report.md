# 伤害管线重构 P2 阶段报告：转换与标签规范

- 计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` §3.3
- 设计（权威）：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md` §4.2/§4.3/§4.4
- 阶段：P2（依赖 P0/P1 已完成并验证）
- 结论：P2-1 ~ P2-4 全部完成；构建、CI、Integration 门禁通过，DamagePipeline 性能未退步。

---

## 1. 分项状态

| 任务 | 状态 | 说明 |
| --- | --- | --- |
| P2-1 纯百分比转换算法 | 完成 | 新增 `DamageConversion.hpp` 快照式纯函数，替换 `DamagePipeline.cpp` 原 ordered-chain 内联块 |
| P2-2 标签剥离 | 完成 | `TransformTagsOnConversion` 全量剥离旧元素位、仅赋目标元素位；新增防双享测试 |
| P2-3 多元素分池 | 完成 | 按元素槽独立聚合/转换/重建；新增多元素独立用例 |
| P2-4 暴击单位归一 0.0~1.0 | 完成 | 出口边界、payload 写入点、单/批路径统一为分数制，修复批量 `/100` 与快照二次暴击 |

---

## 2. 实现说明（文件 + 行锚）

### 2.1 新增 `src/game/systems/combat/damage/DamageConversion.hpp`

头文件内联纯函数，命名空间 `NoMoreDay::damage`：

- `kElementCount = 6`（:14）——与 `DamageType` 的 0..5 元素索引对齐。
- `kAllDamageTypeTags`（:19）——`Physical|Fire|Cold|Lightning|Shadow|Poison|Void`，用于标签剥离掩码。
- `struct ConversionRule { DamageType src_type; DamageType dst_type; float pct; bool is_gain_extra; }`（:27）。
- `TransformTagsOnConversion(Tag original, DamageType target)`（:36-40）：
  `return (original & ~kAllDamageTypeTags) | (1ULL << target)`。保留 `Melee/Projectile/Hit/Critical` 等非元素位。
- `ConversionOutput { std::array<float,6> values; std::array<Tag,6> tags; }`。
- `ApplyConversion(source_values, source_tags, std::span<const ConversionRule>)`（:57-95），严格逐条对应设计 §4.4：
  1. 逐源元素 `sum_convert_pct = Σ pct`（仅 Convert，不含 GainExtra）；
  2. `retained_pct = max(0, 1 - sum_convert_pct)`，源池保留 `initial * retained_pct`；
  3. `scale = (sum_convert_pct > 1) ? 1/sum_convert_pct : 1`；
  4. Convert 产物 `initial * pct * scale`；
  5. GainExtra 产物 `initial * pct`（不参与 scale、不从源扣减、无上限）；
  6. 目标槽标签经 `TransformTagsOnConversion` 重赋。

> 元素位说明：`DamageType`（Poison=4/Shadow=5）与 `TagRegistry` 位序（Shadow=bit4/Poison=bit5）在命名上相反。管线内部始终以“位索引”作为元素槽（`1ULL << i`），与 P0/P1 保持一致；本阶段不重命名以避免数据/存档兼容破坏（见 §6 遗留风险 R3）。

### 2.2 `src/game/systems/combat/DamagePipeline.cpp` 改造

| 行锚 | 内容 |
| --- | --- |
| :20 | `#include "game/systems/combat/damage/DamageConversion.hpp"` |
| :850-936 | 规则收集：`FixedVector<damage::ConversionRule,64> conversion_rules`（:854），统一三来源（global_mods、`SkillModifierComponent`、专精树节点）；保留 `source_tag==None`/越界/`IsValidConversion` 过滤与非法转换 `LOG_WARN`（:872 附近） |
| :938-957 | 按元素槽聚合快照（:940-952），调用 `damage::ApplyConversion`（:954-957） |
| :959-967 | 清空并按元素重建实例（同元素合并、`final_type = 1ULL<<i`） |
| :1264-1302 | 单目标暴击：payload 直接使用分数（:1270，不再 ×100）；`std::clamp(crit_chance, 0.0f, Cap::CRIT_CHANCE)`（:1281、:1296），不再 `clamp(0,100)/100` |
| :1512-1525 | 批量快照：`crit_applies = Hit && !DoT`；用 `expected_crit = 1 + P*(crit_damage-1)` 归一 `snap.base_damage`（消除快照模拟暴击 + 批量再次暴击的二次收益） |
| :1729-1731、:1804-1806 | 批量暴击判定移除 `/100.0f`，直接以分数比较 |

P0/P1 成果（`SettlementFrame`/`QueueDeferredAction`/`MoreAccumulator`/`kMaxReentrancyDepth`/`DamageOrigin`）未改动语义。

### 2.3 暴击单位归一涉及文件

| 文件 | 行锚 | 改动 |
| --- | --- | --- |
| `src/game/contracts/impl/StatsSystem.cpp` | :502-503 | `GetStatWithTags(CritChance)` 出口 `result *= 0.01f`，内部 `dynamic_calc.base` 保持百分点空间（:215） |
| `src/game/systems/skill/SkillSpecializationBaker.cpp` | :246/:270/:301/:474/:620 | `delivery.bonus_crit`、`riding_wind_bonus_crit` 统一为分数（0.02/0.08/1.0/…`*0.01f`） |
| `src/game/systems/skill/BeamChannelDeliverySystem.cpp` | :281-289/:378/:686/:1220/:1474 | `critChanceFor` 返回分数；payload 直接使用分数 |
| `src/game/systems/skill/behaviors/InfiniteBlades.cpp` | :115/:209 | `1.0f`（原 100.0f） |
| `src/game/systems/skill/behaviors/RendingWave.cpp` | :155/:221/:344/:444 | `crit_pct_per_intent` `/100`，payload 分数 |
| `src/game/systems/skill/behaviors/FlowingThrust.cpp` | :209-212/:306-308 | payload 分数 |
| `src/game/systems/skill/behaviors/PhantomTrance.cpp` | :322 | payload 分数 |
| `src/game/systems/skill/behaviors/BladeBoomerang.cpp` | :138 | 移除 `/100.0f` |
| `src/game/systems/skill/behaviors/SevenStarSlash.cpp` | :601 | `critChanceBonus = 0.02f * points` |

---

## 3. 审计表

### 3.1 转换逻辑审计

| 维度 | 改造前（`DamagePipeline.cpp` 原 :846-963） | 改造后（`DamageConversion.hpp` + :850-967） |
| --- | --- | --- |
| 遍历方式 | 按 `CONVERSION_ORDER` 有序、就地级联（Lightning 产物可再被 Lightning→Cold 转换） | 单遍快照、无级联 |
| 守恒 | 逐条 `amount_to_convert = original*value*scale` 扣源，浮点误差/级联导致总和不稳 | `retained = max(0,1-sum)`，守恒（含 >100% 比例缩放） |
| >100% | `conv_scale = 1/total` 仅对 Convert 生效，但源头已扣减 | `scale = 1/sum`，Convert 按比例、GainExtra 不受影响 |
| GainExtra | 与 Convert 混用同一 `total_conv_pct` 缩放 | 独立分支，不封顶、不扣源、不参与 scale |
| 标签 | 产物 `tags | target_tag`（旧元素位残留） | 剥离全部元素位后仅赋目标元素位 |
| 合法性 | `IsValidConversion` + 告警 | 保持一致 |
| 规则来源 | 三来源内联分散 | 统一收敛为 `ConversionRule` |

### 3.2 暴击单位审计表

| 层 / 路径 | 位置 | 改造前单位 | 改造后单位 | 处理方式 |
| --- | --- | --- | --- | --- |
| 数据（JSON 词缀/天赋 `crit_chance` 等） | `SkillSpecializationBaker.cpp`、`ItemTemplateRegistry` | 百分点 % | 百分点 % | 加载/烘焙边界经 `/100` 转分数 |
| 属性内部计算 `dynamic_calc.base` | `StatsSystem.cpp:215` | 百分点 | 百分点 | 不变，保证词缀按百分点叠加 |
| 属性存储 `CombatStats.crit_chance` | `Stats.hpp`、`AttributePipeline.cpp:773` | 分数（已正确） | 分数 | 不变 |
| 查询出口 `GetStatWithTags(CritChance)` | `StatsSystem.cpp:502-503` | 百分点 | 分数 | `*0.01f` |
| payload `DamagePayloadContext.crit_chance` | `SkillDefs.hpp:757` 及各写入点 | 混用（部分分数、部分×100） | 分数 | 写入点统一 |
| 单目标路径 | `DamagePipeline.cpp:1270/1281/1296` | payload ×100 后 `clamp(0,100)/100` | 分数 `clamp(0,1)` | 修 |
| 批量快照 | `:1504` | 分数 | 分数 | 不变（但见下一行） |
| 批量暴击 roll | `:1729/:1804` | 分数却再 `/100` → 实际低 100 倍 | 分数 | 移除 `/100` |
| 批量二次暴击 | `:1512-1525` | 快照已含期望暴击 E，roll 成功再乘 → 双计 | 快照基底除以 E | 修 |

---

## 4. 行为与数值变化清单（含测试期望推导）

| 测试 | 旧期望 | 新期望 | 推导 |
| --- | --- | --- | --- |
| `tests/functional/DamagePipelineConversionTest.cpp`（Physical→Lightning→Cold） | P=50 / L=0 / C=50（级联） | P=50 / L=50 / C=0 | 单遍无级联：源 50% 转 Lightning，产物不再转；`retained=0.5`。子用例改名 `Physical -> Lightning (single-pass, no cascade)` |
| `tests/functional/InfiniteBladesNodes.cpp:344` | `bonus_crit_chance >= 100.0f` | `>= 1.0f` | 100% 归一为分数 1.0 |
| `tests/unit/SkillBehaviorGuardTests.cpp:423` | `>= 100.0f` | `>= 1.0f` | 同上 |
| `tests/unit/SkillSpecializationBakerTests.cpp:473` | `bonus_crit == Approx(2.0f)` | `== Approx(0.06f)` | node102：`0.02 * 3pt` |
| 同上 :536 | `riding_wind_bonus_crit == Approx(8.0f)` | `== Approx(0.24f)` | node114：`0.08 * 3pt` |
| 同上 :590 | `bonus_crit >= 100.0f` | `>= 1.0f` | node154 必暴归一 1.0 |
| `DamagePipelineConversionTest` 子用例 `Conversion Loop Prevention` | P/F 值 | 不变 | Cold→Fire 合法、Fire→Cold 非法仍被 `IsValidConversion` 拦截 |
| `DamagePipelineConversionTest` `Unified More Multipliers` | 180 | 不变 | More 累乘逻辑未改 |

> 说明：批量快照二次暴击修复仅在「快照模拟暴击 + 批量再次 roll」同时成立时改变数值；既有测试多用 `is_simulation=true`（确定性期望暴击）或物理必中/DoT 场景，故未出现连带期望变更。所有改动测试均已在源码内留下中文推导注释。

---

## 5. 验证证据

| 项 | 命令 / 来源 | 结果 |
| --- | --- | --- |
| 构建 | `./build.bat`（RelWithDebInfo） | exit=0（`logs/build-p2e.log`，`[Build] All steps completed successfully`） |
| CI | `ctest --test-dir build -C RelWithDebInfo -L ci` | **100% passed, 0 failed, 1/1**（`logs/ctest-ci-p2-try1.log`，8.98s） |
| Integration | `ctest ... -L integration` | **100% passed, 6/6**（`logs/ctest-integration-p2.log`，6.03s） |
| 性能 | `bin\NoMoreDayTests.exe --test-case="[Performance]*DamagePipeline*"` | Single Mean=0.000ms / P99=0.001ms（目标 <0.01ms）；Batch200 Mean=0.049ms / P99=0.084ms（目标 <1.0ms）；批量缩放 50/100/200/500 均达标（`logs/perf-p2-damage.log`） |
| 新增 P2 测试 | `--test-case="*DamagePipeline P2*"` | **4 test cases / 30 assertions 全通过**（`logs/tests-p2-new-only.log`） |
| 转换测试集 | `--test-case="*DamagePipeline*"` | 27 cases / 91 assertions 全通过（`logs/tests-conversion3.log`） |
| 全量（非性能） | `--test-case-exclude=*Performance*,*GPU-Diagnostic*` | 1543 cases，P2 相关 0 失败；仅剩既有偶发（见 §6 R1/R2，`logs/tests-nonperf-p2.log`） |

新增/扩展测试清单：

1. `[Functional] DamagePipeline P2 - Conversion Conservation`（4 子用例）：部分转换守恒、>100% 比例缩放、GainExtra 不封顶不扣源。
2. `[Functional] DamagePipeline P2 - Tag Strip Prevents Double Dip`：源元素 More 转换后不再生效、目标元素 More 正常生效。
3. `[Functional] DamagePipeline P2 - Multi Element Independent Pools`：Physical→Fire 与 Lightning→Cold 独立分池、互不串扰（使用无固有元素标签的 `skill_id=999999` 构造纯净基准）。
4. `[Functional] DamagePipeline P2 - Crit Unit Fraction Consistency`：`GetStatWithTags` 分数出口、单目标期望暴击、单/批暴击一致（`Calculate` 与 `CalculateBatch` 同输入同结果）。

---

## 6. 遗留风险

- **R1（既有偶发，非 P2 引入）**：`tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235`（S1a GPU timer query）在整包运行中偶发 `QueryState::Valid` 失败，单独运行稳定通过；与渲染/GPU 状态相关。CI 需重试才可稳定绿（本阶段重试 1 次即通过）。
- **R2（既有失败）**：`tests/performance/ParticleTrailBenchmark.cpp:205` `dispatchOverheadMs < 0.2` 在性能全量中偶发超限（基线 `tests-before.txt` 同样存在）；`SkillSystemTests` BladeWard counter 用例存在 RNG 依赖偶发，均与 P2 无关。
- **R3**：`DamageType` 与 `TagRegistry` 在 Poison/Shadow 位序命名上相反；当前管线以位索引为准，未在本阶段重命名，后续若引入“按名字取元素位”的代码需显式映射。
- **R4**：`SummonCombatProfile.bonus_crit` 经 `BladeFormation.cpp:248`（分数）与 `SummonCombatBridge.cpp:87` 消费，假定一致；本次未改动，建议后续补充召唤物暴击单位用例。
- **R5**：部分历史测试/基准以 `crit_chance = 10.0f/25.0f` 强制暴击，依赖 `clamp` 兜底；虽仍可用，语义上应改为 `1.0f`，留待清理。

---

## 7. 日志索引（`logs/`）

`build-p2e.log`（构建）、`ctest-ci-p2-try1.log`（CI 通过）、`ctest-integration-p2.log`（Integration）、`perf-p2-damage.log`（性能）、`tests-p2-new-only.log`（新增 P2 测试）、`tests-conversion3.log`（转换测试集）、`tests-nonperf-p2.log`（非性能全量）、`tests-before.txt`（P2 前基线）、`ctest-ci-p2.log`/`-retry.log`（S1a 偶发失败留档）。
