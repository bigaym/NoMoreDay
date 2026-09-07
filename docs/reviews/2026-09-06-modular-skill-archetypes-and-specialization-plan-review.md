# 计划审查报告：模块化技能交付原型与专精系统实施计划

- 日期：2026-09-06
- 审查对象：`docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md`（下称"计划"）
- 对照设计：`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`（下称"设计"）
- 审查方式：逐条对照代码实况（rg 实测），核验计划中的事实声明、任务覆盖与验收标准
- 结论：**修改**（架构方向正确，但存在 3 项 P0 缺口与 4 项 P1 风险，修订后可批准执行）

---

## 1. 总体评价

计划的架构方向正确且与项目现状高度契合：

- **AOT 烘焙 + POD 组件 + 原型库**是 ARPG/ECS 的成熟模式（暗黑式 stat baking），与设计"无虚函数多态、零堆分配"约束一致。
- 烘焙器伪代码引用的 API 全部现成：`SkillRegistry::Get().GetNodeContract()`（`SkillSystem.cpp:143`、`StatsSystem.cpp:418/461`）、`SpecNodeRole::{Keystone,Trigger,Synergy,Transmuter}`（`SkillSystem.cpp:148-154`）、`ResolveElementalConversion`（各行为文件多处调用）。
- `BakedSkillProfile` 已有 `static_assert(std::is_standard_layout_v<...>)` 与默认 `operator==`（`SkillDefs.hpp:527-544`），扩展字段不破坏既有约束。
- 计划文档结构（rationale/伪代码/任务/测试/DoD）符合 planning 工作流要求。

但计划存在事实性偏差与覆盖缺口，直接执行将在 Phase 2/3 卡住。

## 2. 事实核验表

| 计划声明 | 代码实况 | 判定 |
|---|---|---|
| `RebakeSkillProfiles` 待接入烘焙器 | 已存在且已接入生产链路：洗点 `SkillSystem.cpp:1868`（置 `StatsDirty`）→ `StatsSystem.cpp:525-531` → `StatsSystem.cpp:101` `AttributePipeline::Calculate` → `AttributePipeline.cpp:788` 调用 Rebake | **工作量被高估**：任务是"扩展 Rebake 内容"，非"新建接入" |
| BeamChannel 特例 2 处（行 71/89） | 实际 **5 处**：`BeamChannelDeliverySystem.cpp:39/71/89/206/318`，含行 39 的 `chan.skill_id == 5 && chan.burst_finisher` 语义 | **低估**，且漏了 finisher 语义迁移 |
| FlowingThrust 615 行 / RendingWave 559 行 | 实测 614 / 558 | 准确 |
| 引入 `CompactEntitySet<8>` | **全仓库无任何实现**（仅两份文档提及）；最近似物是 `Projectile.hpp:44` 的 SBO 注释（≤32 hits） | **工作量低估**：需从零新建+单测 |
| `TriggerRuleComponent::AddRule` / `ProcEngine` 已有 | `TriggerRuleComponent.hpp:50` / `src/game/systems/skill/ProcEngine.hpp` 均存在 | 准确 |
| `DeliveryArchetypes.hpp` 新建 | 不存在 | 准确 |
| 契约矩阵测试已存在 | `tests/unit/SkillBehaviorGuardTests.cpp`、`tests/unit/SkillKeyNodeMatrixTests.cpp`、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` 均存在 | 准确 |
| 基准测试 `tests/integration/SkillSystemBenchmark.cpp` | 实际在 **`tests/performance/SkillSystemBenchmark.cpp`** | 路径误写 |
| `skill_contracts_compact.json` 数据驱动 | `assets/data/skill_contracts_compact.json` 存在 | 准确 |

## 3. 关键缺口

### P0-1 Phase 2 覆盖缺口：装配了没人消费的组件

12 交付原型只安排了 3 个新 System（Task 2.1 Mobility、2.2 Boomerang、2.3 OrbitingSentinel）+ 2 个升级（Task 2.4 AreaField/BeamChannel）。但 Phase 3 的技能装配依赖其余原型，且**无任何消费方任务**：

| 原型 | 装配技能 | 现状 |
|---|---|---|
| DirectStrike | 技能 1 | 无 System、无任务 |
| PhantasmClone | 技能 1 | 无 System、无任务 |
| StickyDetonation | 技能 2 | 无 System、无任务 |
| ReactiveWard | 技能 4、9 | 无 System、无任务 |
| SkyfallImpact | 技能 5、6 | 无 System、无任务 |

装配后组件是死数据，行为不变，契约测试（`ExpectedKeyNodesBySkill`）将失败。**修订要求**：
1. 为每个原型标注处置方式："复用现有 System 扩展"（如 DirectStrike/StickyDetonation → `ProjectileSystem` 状态机扩展；SkyfallImpact → AreaField 扩展；ReactiveWard → `ProcEngine` 已有回调骨架）或"新建 System"。
2. Phase 2 每个系统任务必须带消费方验证测试（组件被至少一个 System 读取并产生可断言的行为）。

### P0-2 验证命令全部无效

`tests/CMakeLists.txt` 注册的 ctest 条目名为 `nmd.tests.ci.nonperf` / `nmd.tests.unit` / `nmd.tests.integration` / `nmd.tests.performance` / `nmd.tests.gpu.contract`。计划 4.2 与 Task 3.1-3.6 的 `ctest -R "SkillBehaviorGuard.*FlowingThrust"`、`ctest -R "SkillBehaviorGuard|SkillKeyNodeMatrix|..."` **均匹配不到任何条目**（ctest 正则作用于注册名，非源码内 TEST_CASE/SUBCASE）。**修订要求**：
- 套件级：`ctest -R "nmd.tests.(unit|integration)"` 或 `-L ci`。
- 用例级过滤：直接运行测试二进制并使用测试框架的 `-tc=` 参数（doctest 按 TEST_CASE 名过滤，不支持 SUBCASE）。
- 修正基准测试路径为 `tests/performance/SkillSystemBenchmark.cpp`。

### P0-3 `ChannelingComponent` → `BeamChannelComponent` 迁移无任务

现状：`BeamChannelDeliverySystem.cpp:24-32` 使用 `view<ChannelingComponent, Position>`（`ChannelingComponent` 定义于 `SkillDefs.hpp:1073`）。计划 Task 2.4 直接以"纯由 `BeamChannelComponent::mode` 参数驱动"为目标，但缺少：
1. 新建 `BeamChannelComponent`（mode/aim_assist/finisher_trigger_skill_id）的明确子任务（Task 1.2 的 12 组件定义未覆盖字段语义与迁移）。
2. `ChannelingComponent` 全部消费方盘点与迁移顺序（SkillSystem 引导状态机也在写该组件）。
3. 行 39 的 `burst_finisher`（万剑归宗收束）语义如何并入 `mode` / `finisher_trigger_skill_id`。
4. 行 206/318 的分支（计划未提及）同属待剔除特例。

## 4. 风险（P1）

### P1-1 设计与计划范围不一致：HeavenlySword/BloodSea 组件

设计 3.1 声称"彻底废弃 SwordArrayComponent、HeavenlySwordFieldComponent 与 BloodSeaFieldComponent"，但计划仅 Task 3.4 处置 `SwordArrayComponent`。后两者属天剑降临/血海（技能 11/12，**超出本次 9 技能范围**），且 `HeavenlySwordDescent.cpp:184/216/380-400`、`BladeMasteryService.cpp:51-88`、`SkillSystem.cpp:1080-1092`、`AreaFieldDeliverySystem.cpp:25` 深度依赖。上一轮重构刻意保留"双组件共存"（旧组件驱动行为逻辑、AreaField 为交付层）。
**修订要求**：设计范围声明收窄为"废弃 SwordArrayComponent"；Heavenly/BloodSea 迁移另立计划。否则 DoD"零特例"不可达。

### P1-2 `BakedSkillProfile` 新旧字段重叠

现有 `projectile_count`（int，默认 1）与 `area_radius`（float，默认 1.0f）与新 `BakedDeliveryParams.count` / `width_or_radius` 语义重叠，计划未定义处置；`tests/unit/ItemSkillModifierTests.cpp:60-136` 与 UI Tooltip 依赖现有字段。
**修订要求**：明确单一事实源——或保留旧字段由 Baker 写入、`delivery` 仅存行为标志；或列出全部消费方并一次性迁移。

### P1-3 验收标准自相矛盾

- DoD：9 行为文件总行数 **-50%** vs 设计 7.1：每文件 **-60%**。
- Task 3.1 目标 80 行 vs 设计"100 行以内"；Task 3.2 目标 70 行 vs 设计"100 行以内"。
- `InfiniteBlades.cpp` 现 159 行，-60% 即 <64 行，对 Barrage 装配模式偏紧。

**修订要求**：统一口径（建议以设计为准），逐文件给目标表并注明依据；对 159 行级别的文件改用绝对行数上限。

### P1-4 Rebake 频率与幂等成本

Rebake 已被属性重算链路触发（见核验表第 1 行）。扩展 Rebake 内容后，**每次 StatsDirty 重算都会全量重烘**。
**修订要求**：
1. 利用已有默认 `operator==` 先比较后写入，避免下游缓存失效扩散。
2. Task 4.2 基准必须包含"洗点+换装"压力场景，不能只测施法热路径。

## 5. 最佳实践建议（P2）

1. **CompactEntitySet 单列任务**：从零新建（含单测），Task 1.2 将其与 12 组件定义合并低估了工作量；且 8 槽位对 hit-once 语义够用、对穿透（`Projectile.hpp:44` 注释 ≤32 hits）不够——按原型区分容量或统一小容量+溢出策略。
2. **Phase 3 切片试点**：技能 5/7（BeamChannel）仅依赖组件迁移，可作为迁移模式试点先做 3.3，验证后再推其余技能。
3. **性能基线先行**："2.5x 吞吐"无当前基线，建议在 Phase 2 动工前用 `tests/performance/SkillSystemBenchmark.cpp` 记录基线，避免目标不可证。
4. **回滚策略**：组件迁移类任务保留"旧组件共存一个 Phase"的回滚窗口（参照 `AreaFieldDeliverySystem.cpp:25` 的 skip 模式），契约测试全绿后再删除。

## 6. 结论

**修改**。架构方向与既有决策一致、伪代码 API 现成、总行数 3298 行的瘦身空间真实存在——可行性成立。但必须先完成：P0-1（补 Phase 2 消费方任务）、P0-2（修正全部 ctest 命令与基准路径）、P0-3（补组件迁移任务），并处理 P1-1～P1-4 的一致性问题。修订后建议批准执行。

---

## 7. 复审记录（2026-09-06 第二轮）

修订由子代理完成后，复审对两份文档做了独立抽验（不依赖修订方自检）：

| 条目 | 抽验结果 |
|---|---|
| P0-1 | 通过。新增 Task 2.6：DirectStrike/StickyDetonation→`ProjectileSystem` 状态机扩展（`ProjectileSystem.hpp:13`，`OnProjectileDeath`+`DeathReason{Expired,Collision}` 扩展点已核实）；SkyfallImpact→AreaField 扩展；ReactiveWard→`ProcEngine`+`TriggerRuleComponent::AddRule`；PhantasmClone→MobilityDeliverySystem 扩展。每项带消费方验证测试；DoD 新增"消费方全覆盖" |
| P0-2 | 通过。全部命令改为 `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(unit|integration)"` / `-L ci`（计划 4.2 含禁止性说明与 doctest `-tc=` 二级过滤说明）；`SkillSystemBenchmark` 路径两文档统一 `tests/performance/` |
| P0-3 | 通过。Task 2.4 拆为 2.4a-d；`ChannelingComponent` 引用清单（22 处/11 文件）经本复审独立 rg 复核，文件集合完全吻合 |
| P1-1 | 通过。设计 3.1 收窄为仅废弃 `SwordArrayComponent`，HeavenlySword/BloodSea 注明属技能 11/12 另立计划；计划 2.4a/3.4/DoD 同步 |
| P1-2 | 通过。单一事实源落地（计划 Task 1.1"新旧字段单一事实源处置"、设计 4.2 注释），`BakedDeliveryParams` 不再定义 `count`/`width_or_radius`，ItemSkillModifierTests 兼容策略明确 |
| P1-3 | 通过。设计 7.1 逐文件绝对上限表（3298→≤900，行数与实测一致），废除 -60% 比率口径并说明其数学矛盾；计划 DoD 同口径 |
| P1-4 | 通过。Task 1.3 幂等要求（默认 `operator==` 先比较后写入）；Task 4.2 增加"洗点+换装"压力场景并附触发链 |
| P2a-d | 通过。Task 1.4（CompactEntitySet 从零新建+单测，8/32 槽分容量）、Task 1.5（基线前置门禁）、Phase 3 切片试点（先 3.3）、回滚策略（2.4c/2.4d/3.4/3.5 共存窗口，Task 4.1 统一删除） |

**抽验中排除的疑点**：`MobilityComponent::spawn_clone_node` 在 src 中无实现，经查为设计 3.1 规划的新建组件字段（设计 194 行定义、节点 130"留影"使用），属合法上游引用，非文档错误。

**遗留备注（不阻塞批准，实施期验证点）**：
1. Task 3.3 依赖 Task 2.6 SkyfallImpact"服务天剑降世终结技落地衍生"，与 P1-1"天剑降世（技能 11）超范围"的边界需在实施 Task 2.6 时书面澄清：`finisher_trigger_skill_id = 11` 指向的技能 11 本体不迁移，技能 5 落地衍生的标准 `AreaFieldComponent` 属范围内。
2. PhantasmClone 归入 MobilityDeliverySystem 为合理推断但未经行为验证，由 Task 2.1/2.6 消费方验证测试在实施期确认（残影延迟施放 `SkillSnapshot` 不超 Mobility 系统职责）。

### 复审结论

**提交**（修订后批准执行）。全部 P0/P1/P2 条目修订到位且经独立抽验，可进入 Phase 1。

### 核验证据索引

| 事实 | 位置 |
|---|---|
| Rebake 定义/唯一生产调用点 | `SkillSystem.cpp:2203` / `AttributePipeline.cpp:788` |
| 洗点触发链 | `SkillSystem.cpp:1868` → `StatsSystem.cpp:525-531,101` |
| BakedSkillProfile 现有字段 | `SkillDefs.hpp:527-544` |
| ChannelingComponent 定义与消费 | `SkillDefs.hpp:1073`；`BeamChannelDeliverySystem.cpp:24-32` |
| BeamChannel 特例 5 处 | `BeamChannelDeliverySystem.cpp:39/71/89/206/318` |
| RefundRendingWaveCooldown | 定义 `FlowingThrust.cpp:96`，唯一调用 `:538` |
| 双组件共存依赖面 | `SkillSystem.cpp:1074-1092`、`BladeMasteryService.cpp:51-88`、`HeavenlySwordDescent.cpp:184-400`、`AreaFieldDeliverySystem.cpp:25` |
| ctest 注册名 | `tests/CMakeLists.txt:46-99`（`nmd.tests.*` 系列） |
| 9 行为文件实测行数 | FlowingThrust 614 / RendingWave 558 / MindBlade 405 / SwordArray 436 / BladeFormation 355 / BladeBoomerang 315 / PhantomFlash 237 / BladeWard 219 / InfiniteBlades 159，合计 3298 |
