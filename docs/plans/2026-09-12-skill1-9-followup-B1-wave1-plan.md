# 技能1~9收尾 B1 第一波实施计划

- 日期：2026-09-12
- 关联清单：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`
- 范围：B1 第一波 = **无设计裁决依赖的 P1 项**：结算一致性（B1-08/B1-09）、数据卫生（B1-10/B1-11）、调查（B1-12）、测试护栏（B1-13/B1-14/B1-16/B1-17）、B1-18 结论落档、运行时验证（B1-21/B1-22/B1-23）。
- 修订（2026-09-12 审查）：B1-08 经复核改向为「口径落档 + 生产护栏测试」——`CalculateBatch` 已 `[[deprecated]]`（`DamagePipeline.hpp:42-54`），生产批量链路 `ResolveDamageBatch → CalculateBatchResults → 逐目标 Calculate` 中 990 已生效（`DamagePipeline.cpp:2246-2256`），无线上缺口；deprecated 入口清理登记入 B2。B1-09 方案修正为 StatsDirty 感知随 owner 迁移（原「保留检测」在删除调用后为死代码）。B1-14 路径更正 `tests/integration/`。B1-16 的 752 中心用例已存在（`MindBladeNodes.cpp:796`），改为核对补缺。
- 排除：依赖 RD 裁决的 B1-03/05/06（第二波）、大规模节点实现 B1-01/02/04（节点实现批次，另立计划）、P2 尾项 B1-07/15/19/20/24/25/26（第二波）。
- 前置调研（2026-09-12，只读）：伤害管线、数据行为、测试护栏三路并行调研已完成，关键事实见各工作流「现状」。

## 1. 实施思路/原理

### WS1 结算一致性

#### B1-08 990 冰增幅批量口径落档（原「接入 CalculateBatch」改向，2026-09-12 复核）
- 现状（复核结论）：`CalculateBatch`（`DamagePipeline.cpp:1697`，声明 `DamagePipeline.hpp:42-54`）已 `[[deprecated]]`，注释明示「生产批量路径统一走 ResolveDamageBatch → CalculateBatchResults → 逐目标 Calculate；本入口仅测试/基准调用」。生产批量链路为 `ResolveDamageBatch`（`DamageResolutionHooks.cpp:53-67`）→ `CalculateBatchResults`（`DamagePipeline.cpp:2246-2256`，逐目标调 `Calculate`），而 `Calculate` 已含 990（`:1417-1425`）——**生产批量路径 990 已生效，不存在线上缺口**。`ResolveDamageBatch` 在 src 下亦无生产调用者（仅 hooks 注册链 :2273-2276 消费 calculateBatch）。
- 原理（改向）：向 deprecated 入口插入 990 是给无人调用的死路径加逻辑，与 backlog B1-08 自身定性「潜在一致性缺口而非线上缺陷」冲突。本项收缩为**口径落档 + 生产护栏测试**：
  1. 在 backlog B1-08 销项时附调用链证据（`DamagePipeline.hpp:42-54` deprecated 注释、`DamagePipeline.cpp:2246-2256`、`DamageResolutionHooks.cpp:53-67`）。
  2. 补护栏测试（见 T1.2）：Cold 附魔攻击者打冻结/非冻结混合目标，单目标 `Calculate` 与生产批量链路逐目标结果一致、非冻结目标不增伤——防止未来批量路径重构时 990 回退。
- 遗留登记：deprecated 入口清理（`CalculateBatch` + `ResolveDamageBatch`，含测试/基准调用点 `tests/unit/DamageElementIndexTests.cpp:109/:167`、`tests/unit/EventConsistencyTests.cpp`、`tests/performance/DamagePipelineBenchmark.cpp`）涉及面超出本波，登记入 B2 随技能10~12迁移批次统一处置。

#### B1-09 ActiveEffects 双重衰减
- 现状（复核确认）：`EffectSystem.cpp:80`（`EffectSystem::update`，GameplayState.cpp:717 调用，后）与 `StatsSystem.cpp:556`（`UpdateBuffs`，GameplayState.cpp:356 调用，前）同帧、同 dt 各调一次 `effects.Update` → 每帧实际衰减 `2*dt`。两处还各自重复了一份 988 `swordStepDrainMult` 逻辑（EffectSystem.cpp:73-79 / StatsSystem.cpp:549-555）。
- 方案 A（推荐，EffectSystem 为唯一 owner）：删除 `StatsSystem.cpp:556` 调用及 `:549-555` 的 swordStepDrainMult 重复段。**注意（原方案矛盾点）**：删除 `:556` 后 `:547-560` 的 before/size 比较永远相等，`StatsDirty` 检测变为死代码，不能原样「保留」——将「到期删除 → `StatsDirty`」感知迁移到 `EffectSystem.cpp:80` 所在循环（Update 前后 size 比较，变化则 `registry.get_or_emplace<StatsDirty>(entity)`）；`UpdateBuffs` 保留 Freeze/Burn/Stun/Shock 粒子段（`:562` 起）。
- 风险与对策：`StatsDirty` 感知时点随 owner 迁移变化（EffectSystem 在 GameplayState 帧序中后执行），实施时核对 `StatsSystem::update` 的消费时序无「同帧更早消费」方；回归 Freeze/Chill/SwordStep 时长用例；`SkillBehaviorGuardTests.cpp:1774-1776`（同帧连调 UpdateBuffs + EffectSystem::update）断言语义随单次衰减复核调整。

### WS2 数据卫生

#### B1-10 非法标签清理
- 现状：`TagFromString`/`ParseTagList` 静默丢弃未注册串。命中：`skills.json:714`（技能2 `sword_skill`）、`:1456`（技能3 `sword_skill`）、`:6349`（技能10 `sword_skill`）、`:3552`（技能6 `Duration`）。
- 原理：前 3 处直接改为已注册的 `SwordSkill`（技能9/11/12 已采用）；`Duration` 先查设计与消费方再定夺（删除或注册），如属设计词汇缺失（`Emergency`/`Cooldown` 无枚举）另记设计词汇对齐项。

#### B1-11 FlowingThrust 字符串比较改 BuffKind
- 现状：`FlowingThrust.cpp:378-388`（DoHit，破阵流流血与护甲击碎前置探测）对 `b.id.find("Bleed"/"Ignite"/"Cold"/"Chill"/"Slow")` 子串匹配，并与 `b.type` 混用。
- 原理：沿用枚举化先例——`RendingWave.cpp:493-498` 创建时打 `kind=BuffKind::QiBrand`，热路径 `GetByKind`（`Buff.hpp:264-287`）；`InfiniteBlades.cpp:299/:312` 同型。
- 步骤：①审计相关 buff（Bleed/Burn/Freeze/SpeedDown/Ignite/Cold/Chill/Slow）的创建点，确认 `type`/`kind` 覆盖；②为缺口补 `BuffKind` 值并同步 `kBuffKindCount`（`DamageConditions.hpp:50-51` 当前=4）；③创建点设置 `.kind`；④改热路径为 `GetByKind`；⑤按审计结论保留/删除 `type` 检查。
- 伪代码：
```
victimHasBleed = effects->GetByKind(BuffKind::Bleed) != nullptr;
victimHasFire  = effects->GetByKind(BuffKind::Ignite) != nullptr;
victimHasCold  = effects->GetByKind(BuffKind::Chill) || effects->GetByKind(BuffKind::Freeze) || effects->GetByKind(BuffKind::Slow);
```

#### B1-12 N10 生成器副作用调查
- 核对技能1 契约新增 130/153/211/230/232 与 `mastery_skill_trees.json` 技能1 新增 1115 是否符合设计意图；确认 `gen_skill_contracts.py` 泛化规则的边界；产出调查记录，不一致则修脚本或回滚数据。

### WS3 测试护栏

- **B1-13**：`tests/unit/SkillCastConstraintServiceTests.cpp:30` 父用例下新增技能3（370+372 双点）拒绝 SUBCASE，照抄技能8 `:33-54` 模式；可选顺带 4/5/6/7/9（均 `max_transmuters=1`）。契约数据：`skill_contracts_compact.json:104-130`。
- **B1-14**：(a) `tests/integration/SkillContractRegistryTests.cpp:82-110` 升级为精确集合（count 相等 + 成员存在，参考技能9 `:181-198` 的 count 断言；`SkillContract` 运行结构无 `keystone_node_ids` 字段，断言对象为测试夹具/角色集合）；(b) `tests/unit/DamagePipelineP1Tests.cpp` 新增「生产批量链路（ResolveDamageBatch→逐目标 Calculate）与单目标 Calculate 反击一致性」用例（参考 `:81` 单体 Counter settles 与 `:44` Batch hook 模式；不使用 deprecated 的 `CalculateBatch`）。
- **B1-16**：`tests/functional/MindBladeNodes.cpp` 增补 775 Lightning×730 组合用例。**注意**：752 次级撕裂中心用例已存在——`:796` `TEST_CASE("[Functional] MindBlade - 730 次级撕裂以自身中心判定 775")`（修复 Major），实施时核对 `:558` 既有 775 中心门控用例与 backlog 诉求是否同义，缺口以补充清单所列场景为准，避免重复覆盖。
- **B1-17**：`SkillBehaviorGuardTests.cpp:228` 父用例下新增非转质 keystone 互斥过滤 SUBCASE（技能2 213/214，同 exclusion group 1），覆盖 `SkillSystem.cpp:2437-2443` 的 `role != Transmuter` 分支（现有覆盖仅转质过滤 `:2432-2435`；`GetEffectiveSkillTags` 现有断言 `:345-355` 均为技能8）。
- **B1-18**：核实不成立（`stats->crit_chance` 已归一化，直拷正确），仅更新清单与审查文档口径记录，无代码改动。

### WS4 运行时验证（出口项）

- **B1-21**：531 护甲 / 551 闪避实机观测（BuffEffect.modifiers 生命周期）。
- **B1-22**：元素路径与 AreaFieldDeliverySystem 生命周期运行验证。
- **B1-23**：772 区域伤害改由异常承担后的数值验证（总伤害对齐设计）。
- 三项若无运行环境则记录阻塞原因与复现步骤，不阻塞代码批次的提交，但需在复审报告中列为未验证项。

## 2. 原子任务拆分

### T1 结算一致性

- [x] T1.1 核对 990 生产批量口径并落档：确认 `Calculate`（`:1417-1425`）与 `CalculateBatchResults`（`:2246-2256`）一致性，backlog B1-08 销项附调用链证据（`DamagePipeline.hpp:42-54`、`DamageResolutionHooks.cpp:53-67`）。
- [x] T1.2 新增护栏测试：Cold 附魔攻击者打冻结/非冻结混合目标，单目标 `Calculate` 与生产批量链路（`ResolveDamageBatch`）数值一致（非冻结目标不增伤），防止未来重构回退（DamagePipelineP1Tests）。
- [x] T1.3 登记 deprecated 入口清理（`CalculateBatch` + `ResolveDamageBatch` 及其测试/基准调用点）入 backlog B2（随技能10~12迁移批次）。
- [x] T1.4 删除 `StatsSystem.cpp:556` 双重 Update 及 `:549-555` 重复 swordStepDrainMult 段；将「到期删除 → StatsDirty」感知迁移至 `EffectSystem.cpp:80` 循环（size 前后比较）；粒子段保留在 UpdateBuffs。
- [x] T1.5 复核 `SkillBehaviorGuardTests.cpp:1774-1776` 同帧断言按单次衰减语义调整。
- [x] T1.6 回归 Freeze/Chill/SwordStep 时长用例与相关 suite（含 BuffTests.cpp:37 ActiveEffects Update 用例）。

### T2 数据卫生

- [x] T2.1 `skills.json` 三处 `sword_skill`→`SwordSkill`（技能2/3/10）；技能6 `Duration` 调查后处置。
- [x] T2.2 数据一致性验证：`gen_skill_contracts.py --check`（+ idempotency/determinism）。
- [x] T2.3 BuffKind 审计：相关 buff 创建点、`kBuffKindCount` 影响面、`type` 覆盖确认。
- [x] T2.4 FlowingThrust `:378-388` 枚举化实现（含枚举补值与创建点打 kind）。
- [x] T2.5 回归技能1/2 行为测试（破阵流/护甲击碎）。
- [x] T2.6 N10 调查记录与处置（130/153/211/230/232、1115）。

### T3 测试护栏

- [x] T3.1 技能3 互斥双点拒绝 SUBCASE（SkillCastConstraintServiceTests）。
- [x] T3.2 keystone 角色精确集合断言（`tests/integration/SkillContractRegistryTests.cpp:82-110`）。
- [x] T3.3 生产批量链路与 Calculate 反击一致性用例（`tests/unit/DamagePipelineP1Tests.cpp`，与 T1.2 同一用例）。
- [x] T3.4 MindBlade 775×730 组合用例；核对 752 次级撕裂中心用例（`:796` 已存在）是否需补场景。
- [x] T3.5 keystone exclusion（role != Transmuter）过滤 SUBCASE（SkillBehaviorGuardTests）。
- [x] T3.6 B1-18 结论落档（清单已勾选；审查文档口径备注）。

### T4 验证与出口

- [x] T4.1 全量构建 + `ctest -L ci` 全绿。（`build.bat`/RelWithDebInfo 成功；`ctest -L ci` 100%，见复审报告）
- [x] T4.2 专项命令复核（见第 3 节）。（`ctest -L skill` 2/2；6 个新增/专项用例全过；`gen_skill_contracts.py --check` PASS）
- [x] T4.3 B1-21/22/23 运行时验证记录（或阻塞登记）。（阻塞登记：无交互运行环境，见复审报告与清单 §8）
- [x] T4.4 复审报告（`docs/reviews/`）+ 清单勾选与证据链接。（`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md` 第 3 轮结论 `提交`；F1/F7 已修复并补回归用例）

## 3. 测试方法

- 构建：`build.bat`（RelWithDebInfo，0 警告）。
- 全量：`ctest --test-dir build -C RelWithDebInfo -L ci`。
- 专项：
  - `ctest --test-dir build -C RelWithDebInfo -L skill`（技能 unit/integration）；
  - `bin/NoMoreDayTests.exe --test-case="[Unit] SkillCastConstraintService*"`；
  - `bin/NoMoreDayTests.exe --test-case="[Unit] DamagePipeline P1*"`；
  - `bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard*"`；
  - 技能1/2 行为：`bin/NoMoreDayTests.exe --test-case="[Unit]*Skill*"` 或对应功能套件。
- 数据：`python scripts/gen_skill_contracts.py --check`（如脚本带幂等/确定性开关一并执行）。
- 注意：测试工作目录须为源码根（相对 `assets/data` 加载），二进制位于 `bin/`。

## 4. 任务完成定义与批次退出标准

- 单任务完成：勾选清单条目 + 证据（命令输出摘要 / 测试用例名 / file:line）。
- 批次出口：
  1. `build.bat` 成功且 0 警告；
  2. `ctest -L ci` 100% 通过；
  3. 数据生成器检查 PASS（涉及 skills.json 改动）；
  4. WS4 三项验证有记录（完成或明确阻塞）；
  5. 复审报告结论「提交」后，方可启动第二波（RD 依赖项与节点实现批次）。
- 主要风险登记：
  - T1.4 StatsDirty 感知迁移（EffectSystem 帧序在后，需核对 `StatsSystem::update` 消费时序）；
  - T1.5 `SkillBehaviorGuardTests.cpp:1774-1776` 同帧断言语义漂移；
  - T2.3/T2.4 `kBuffKindCount` 数组同步（新增枚举值）；
  - T2.1 `Duration` 语义待查（可能触发设计词汇对齐）；
  - T3.4 752 用例可能已覆盖（`:796`），实施前先核对避免重复。
