# UMR 技能专精承载底座扩充与技能 1 数值切片 — 实施审查报告

## 审查目标

「UMR 技能专精承载底座扩充与技能 1 数值切片」Phase 1..6 最终实施审查（独立审查，非实现方自审）。

## 结论

`提交`

判定依据：无 `Blocker` / `High`；设计批准范围内无缺失必备行为；全部 10 项待裁定事项均已给出裁定（见「质量与风险评估」）；构建、单测、ctest、canonical 契约与运行时生成器 `--check` 全部通过且由本次审查复跑确认。留下的问题均为 `Medium`/`Low`/`Best Practice` 级，已逐条记录为剩余风险。

## 审查轮次

首次审查。

## 输入

- 设计：`docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md`
- 计划：`docs/plans/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-plan.md`
- 审查标准：`docs/workflows/review.md`；代码规则：`conductor/code_standard.md`、`docs/workflows/implementation.md`；`AGENTS.md`
- 数据契约：`assets/data/modifier_v2/canonical/skill_spec_modifier_record.schema.json`
- 本次复跑的验证证据（全部通过）：
  - `.\bin\NoMoreDayTests.exe --test-case=*ModifierPoints*,*SkillSpecDelivery*,*EquipmentSlotFilter*,*SkillSpecModifier*,*SkillSpecializationBaker*,*EquipmentModifier*` → `test cases: 35 | 35 passed | 0 failed`，`assertions: 364 | 364 passed`。
  - `python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest` → `Ran 6 tests ... OK`。
  - `python scripts/gen_skill_spec_modifier_contract.py --check` → exit 0；`python scripts/gen_modifier_runtime_v2.py --check` → exit 0。
  - 未由本次审查复跑：`build.bat RelWithDebInfo`（由编排方执行，exit 0；本次审查仅运行其产物）。**此项按「未独立复核」记录**，不构成阻塞。

## 变更文件边界

`git status --short` 摘要：

```
 M assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json
 M assets/data/modifier_v2/skill_spec_modifiers.json
 M scripts/gen_modifier_runtime_v2.py
 M scripts/migrate_skill_spec_modifier_slice.py
 M src/game/systems/modifier/EquipmentModifierAdapter.cpp
 M src/game/systems/modifier/EquipmentModifierAdapter.hpp
 M src/game/systems/modifier/ModifierContext.hpp
 M src/game/systems/modifier/ModifierEvaluator.cpp
 M src/game/systems/modifier/ModifierEvaluator.hpp
 M src/game/systems/modifier/SkillSpecModifierAdapter.cpp
 M src/game/systems/modifier/SkillSpecModifierAdapter.hpp
 M src/game/systems/skill/SkillSpecializationBaker.cpp
 M tests/unit/SkillSpecModifierAdapterTests.cpp
 M tests/unit/SkillSpecializationBakerTests.cpp
?? docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md
?? docs/plans/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-plan.md
?? tests/unit/EquipmentSlotFilterTests.cpp
?? tests/unit/ModifierPointsScalingTests.cpp
?? tests/unit/SkillSpecDeliveryOpTests.cpp
```

边界清晰：无 `assets/generated/`、无渲染/存档/UI 文件、无 `conductor/` 写入。`tests/unit/EquipmentModifierAdapterTests.cpp` 未被修改（公开签名保持兼容，可继续编译）。未发现未声明的权限触及或禁用消费者改动。

## 范围对齐

| 计划任务 | 实现位置 | 结论 |
| --- | --- | --- |
| 1.1/1.2 点数模型 | `ModifierContext.hpp:110-152`、`ModifierEvaluator.cpp:152-167` | 对齐（两条路径共用） |
| 2.1/2.2/2.4 opcode+类别+统一分发 | `ModifierContext.hpp:11-83`、`ModifierEvaluator.cpp:170-298` | 对齐 |
| 2.3 ModifierDelta 容器/MergeFrom | `ModifierEvaluator.hpp`、`ModifierEvaluator.cpp:376-528` | 对齐（含全部 6 个怪物集合） |
| 3.1-3.3 逐槽求值/职业/武器掩码 | `EquipmentModifierAdapter.cpp:33-163` | 对齐 |
| 4.1-4.3 脚本注册与契约不变 | `gen_modifier_runtime_v2.py`、`migrate_skill_spec_modifier_slice.py` | 对齐（schema 未改） |
| 5.1-5.3 9 条 canonical + 适配器 + 重生成 | canonical/runtime JSON、`SkillSpecModifierAdapter.cpp:167-191` | 对齐（逐字段核对，见下） |
| 6.1-6.3 Bake 接入与老分支迁出 | `SkillSpecializationBaker.cpp:177-203, 267-306` | 对齐（顺序见裁定 4） |
| 7 门禁 | 本次复跑 | 对齐 |

逐字段核对：9 条新记录（2001010/2001020/2001030/2001100/2001101/2001110/2001111/2001340/2001341）的 `operation/stat_path/value/tags/conditions` 与 `runtime.{opcode,target,param_u32=1,param_f32,node_id_whitelist}` 与设计 §3.4 表**完全一致**（含 2001100 的 `-1.0`、2001101 的 `-0.15` 负系数与 `skill.cooldown_flat`/`skill.more_damage` 路径归属）。运行版 `assets/data/modifier_v2/skill_spec_modifiers.json` 与 canonical 转写结果一致。新 opcode 名称在 `assets/data` 下**仅**出现在 `skill_spec_modifiers.json`（9 处），装备/地图/怪物/天赋域均无 30..36。

## 质量与风险评估

### 10 项待裁定事项的裁定

1. **武器通配回归 — 无回归（已核实）。** `assets/data/modifier_v2/**` 全量普查：无任何记录使用 `weapon_class_mask: 0`（装备 65535×1；地图/怪物 4294967295；技能专精/天赋 65535）。改动前 `65535`/`0xFFFFFFFF` 已可匹配任意已装武器，改动仅新增了「ctx 掩码为 0（徒手）」时也匹配。实现新增 `0xFFFF` 到通配集合是对设计 `design.md:189`（只写 0/0xFFFFFFFF）的**必要修正**：技能 1 记录用 65535，而 `design.md:273` 明确要求「徒手 + 65535 掩码仍生效」，若严格按设计原文实现，徒手玩家会被全部 9 条记录过滤掉。裁定：修正正确，设计文档 §3.3 文字需回写同步（见发现项）。
2. **Legacy 回退 — 逐调用点确认位一致。** `ctx.node_points` 全仓库仅 `SkillSpecModifierAdapter.cpp:186` 一处填值；`EvaluateDamageMultiplier`（`SkillSpecModifierAdapter.cpp:206-209`）、`ApplyHeavyMomentum`、`EquipmentModifierAdapter.cpp:113`、`MapModifierAdapter.cpp:114`、`MonsterModifierAdapter.cpp:76`、`TalentModifierAdapter.cpp:101` 均只填 `active_node_ids` → `ResolveEffectivePoints` 走 `node_points.empty()` 分支返回 1，与改动前「节点命中即按 op.param_f32 生效一次」完全等价（`EvaluateDamageMultiplier` 的测试 `SkillSpecModifierAdapterTests.cpp:77-113` 未修改且通过，佐证等价）。**无应当使用点数却被留在 legacy 路径的调用者**：设计只授权交付类算子按点数缩放，装备/天赋/怪物/地图域本就无 node 白名单语义。
3. **Op 0..28 不变 — 已证明。** 与 `git show HEAD` 的旧静态 switch（旧 `:345-399`）和旧运行时 switch（旧 `:177-234`）逐支比对，`ApplyOp`（`ModifierEvaluator.cpp:220-298`）无 opcode 丢失、改号或语义变化：参数下标、`out.Add*` 目标、`static_cast<uint16_t>(opcode)` 行为、`effectivePercentMult` 仅作用于 `ADD_STAT_PERCENT_MULT` 全部一致。`CategoryOfOp`（`:170-214`）对 0..28 的归类与旧 `switch` 内联分类一致。`ModifierOpCategory::All` 由 `0x07` 变为 `0x0F`（`ModifierContext.hpp:59-66`），**全仓库无非具名枚举的字面量使用者**；`MonsterModifierAdapter.cpp:90,97,110` 使用具名 `Stats|Behavior` / `Events` 组合，天然排除 `SkillDelivery`，符合意图。唯一残留风险：3 参 `Evaluate`（`ModifierEvaluator.cpp:566-579`）以 `All` 求值，装备路径沿用它，未来若装备记录含 30..36 会被 `GetManaCostMultiplier` 折叠进装备法耗系数（见发现项）。
4. **节点 154 顺序 — 确定性成立。** 步骤 3 交付增量（`SkillSpecializationBaker.cpp:177-190`）→ 步骤 4 节点 154 覆盖（`:192-203`）→ 步骤 5 装备（`:206-243`）→ 步骤 6 装备法耗折叠（`:245-250`）。154 用 `spec->allocated_points.find(154)` 显式查找，**不依赖 `unordered_map` 迭代顺序**（对比同函数 `SkillSystem.cpp:2377` 存在对 `allocated_points` 的迭代，但与本路径无关）。`ApplyNodeModifiersToProfile` 中 101/102/103/110/111（`:267-269`）、134（`:300-301`）、154（`:305-306`）分支已清空赋值 → 无双重生效；其余 skill-1 节点（112/113/114/115/130/131/132/133/150/155/170/172）保留原实现，未被误迁。
5. **110+111 充能语义 — 实现方正确，编排方前提有误。** 设计 §3.4 明确：节点 111 行为 `effective_charges += N`（`design.md:212`），节点 110 只给 `cooldown_flat -1.0*N` 与 `more_damage *(1-0.15*N)`（`design.md:210-211`）；计划 Task 6.1 同样写 `out_profile.effective_charges += deliveryDeltas.GetSkillCharges(...)`（`plan.md:102`）。设计与计划**均未**写「110+111 → 充能等于基础值」；`design.md:21` 只说「110+111+154 组合最终为 1 充能」（由 154 覆盖达成）。技能 1 基础充能为 2（`SkillSpecializationBakerTests.cpp:642` 无加点基线断言）。故 1 点 111 时 `2 + 1 = 3` 正确，测试 `SkillSpecializationBakerTests.cpp:548-562`（CD = `(3-1)*1.15`、charges 3、more ×0.85）正确。
6. **槽位位/索引对应 — 与朴素逐条等值。** `EquipmentModifierAdapter.cpp:49` `slotBit = 1u << static_cast<uint32_t>(slotIndex)`，`slots` 为 `std::array<entt::entity, EquipmentSlot::Count>`（`EquipmentComponent.hpp:11`），索引与枚举一一对应。分组求值后 `MergeFrom`（`ModifierEvaluator.cpp:482-528`）对加性容器用 `+=`、乘性容器用 `*=`、集合用 `union`，三者均结合且交换 → 分组结果与逐条朴素求和/乘积**数学等值**（顺序无关，测试亦以「乘法可交换」佐证）。索引 0（`None`）由 `Set` 拒绝写入（永不进入采集循环），且位 0 与 `WeaponSubtype::None` 同位但掩码域不同，无冲突。`Ring`(12)/`Ring1`(10)/`Ring2`(11) 三别名并存存在潜在错配（见发现项）。
7. **职业语义 — 存在「未誓约 vs 职业 0」合并，设计已显式接受。** `ProfessionID`（`TalentData.hpp:18-25`）中 `BladeAscendant = 0` 是真实职业，`AstrolabeComponent::mainProfession` 默认 `-1`（`Progression.hpp:30`），实现把 `>=0` 才写 `ctx.profession_id`（`EquipmentModifierAdapter.cpp:133-138`）→ 未誓约与职业 0 都得到掩码 `1u<<0`。设计确已接受此行为（`design.md:20`、`design.md:197`「未誓约（< 0）维持 0，保持现状，与当前数据掩码 63/1 的行为一致」）。裁定：**不构成偏差**，但代码缺少注释，且未见设计预留的「区分位」骨架（见发现项）。
8. **Canonical/运行时一致性 — 一致。** 9 条记录与设计表逐字段相符；迁移契约映射（`migrate_skill_spec_modifier_slice.py:54-63`：`FLAT/CHARGES/CRIT→add`、`MORE/CD_MULT/AREA/MANA→mul`）与 `operation` 字段逐条吻合；两个生成器 `--check` 复跑 exit 0；schema 未修改。
9. **确定性与性能 — 可接受。** `GetPointsForNode`（`ModifierContext.hpp:128`）每次 `std::is_sorted` 为 O(n)、`ResolveEffectivePoints` 对每条白名单做一次 → O(k·n)；n = 已分配节点数（个位到十位），k 对 skill_spec 恒为 1，实际代价可忽略，且有序性校验是防 `lower_bound` UB 的正当保护。新增开销中量级更大的是 `EvaluateSkillDeliveryDeltas` 每次调用构造 `NodePointSnapshot` 两个 vector 与含 11 个 `unordered_map` 的 `ModifierDelta`，以及 `ResolveSkillDeliveryRecordIds` 的全注册表线性扫描；但 Bake 在 `DamageMitigationService.cpp:136,166` 仅为「baked profile 缺失」的兜底（`DamageMitigationService.cpp:150-151` 有显式热路径保护注释），非稳定逐帧路径。无新增字符串比较或字符串键（新算子全为整数键）。
10. **测试充分性 — 强，存在两处覆盖缺口。** 已覆盖：点数 0（跳过）/1/3/5 线性缩放（`ModifierPointsScalingTests.cpp:113-172`）、多节点取首个已加点（`:187`）、无序回退（`:207`）、7 个 opcode 单点数学含负系数（`SkillSpecDeliveryOpTests.cpp:95`）、充能截断与 mana 下限（`:171`、`:193`）、`MergeFrom` 全容器 + 自合并（`:236`、`:310`）、类别掩码值（`:203`）、槽位过滤/武器过滤/未誓约/徒手+65535（`EquipmentSlotFilterTests.cpp:115-253`）、5 个组合（`SkillSpecializationBakerTests.cpp:548-620`）。缺口：`weapon_class_mask == 0` 与 `== 0xFFFFFFFF` 两条通配分支无用例（见发现项）；`All` 含 `SkillDelivery` 后装备路径的隔离无用例。

### 差异裁定汇总（编排方要求逐条裁定）

| 差异 | 裁定 |
| --- | --- |
| 节点 154：保留空分支而非删除 | 行为等价，可接受；建议收敛（Best Practice） |
| `CollectEquippedRecordIds` 改为去重 | **符合设计 §3.3「去重并排序」**；该函数现无生产调用者，仅测试调用，契约变化无外部影响 |
| 110+111 充能 = 3 | **正确**（编排方前提有误，见裁定 5） |
| `GetPointsForNode` O(n) | 可接受（裁定 9） |

### 重复轮子核查

已用图谱/检索确认：仓库内无既有的 `ModifierDelta` 合并实现（`MergeFrom` 引用仅出现在新代码与其测试），无既有的「按节点点数线性缩放」评测模型（`allocated_points` 是既有数据源，非竞争实现）。`ResolveDamageRecordIds`（`SkillSpecModifierAdapter.cpp:55-84`）与新 `ResolveSkillDeliveryRecordIds`（`:88-118`）结构相近但筛选条件与目标算子不同，且在 `:86-87` 有显式理由注释，判定为**不同契约、非重复轮子**。结论：无应复用/合并/删除的重复实现。

## 发现项

### Medium

1. **Medium** — `src/game/systems/modifier/ModifierEvaluator.cpp:51-57`（配合 `tests/unit/EquipmentSlotFilterTests.cpp:225`）
   最高风险改动「`weapon_class_mask == 0` 视为无过滤」**无测试覆盖**：现有徒手用例使用的是 65535 分支。这属于「缺失边界用例证明」（严重度定义 3）。若日后有人把 0 从通配集合去掉，测试仍全绿。
   修复建议：在 `EquipmentSlotFilterTests.cpp` 增加两例——filter 掩码 0 与 0xFFFFFFFF 分别在 `ctx.weapon_class_mask = 1u<<Sword` 与 `= 0` 下均命中。

2. **Medium** — `src/game/systems/modifier/EquipmentModifierAdapter.cpp:133-138`
   「未誓约（-1 保持 0）」与「真实职业 `BladeAscendant == 0`」被合并为同一上下文值（`TalentData.hpp:18-25`）。设计已接受该行为（`design.md:197`），但代码无任何注释，且设计提到的区分位骨架未落地；未来若给职业 0 注入新语义，技能 1 记录会静默对所有未誓约玩家生效或反之失效。
   修复建议：至少补一行中文注释说明「0 同时承担通配与 BladeAscendant」，并在 `ModifierContext.hpp` 附近定义 `kUnswornProfessionId`/注释保留位；如设计确认需要，落地区分位。

3. **Medium** — `src/game/systems/modifier/EquipmentModifierAdapter.cpp:145-160`
   ctx 徒手时 `weapon_class_mask = 0`，而 0 又被 `MatchesFilters` 定义为「无过滤」→ **「仅徒手生效」在数据层不可表达**（掩码 0 与位 0 都拿不到「徒手」语义：位 0 对应 `WeaponSubtype::None`，实现从不置位）。当前无记录需要此语义，属有界能力缺口。
   修复建议：徒手时置 `1u << static_cast<uint32_t>(WeaponSubtype::None)`，并将 0 从通配集合中移除（65535 含位 0，既有无需徒手过滤的记录不受影响）；或在设计文档明确「不支持仅徒手筛选」并保持现状。

### Low

4. **Low** — `src/game/systems/modifier/SkillSpecModifierAdapter.cpp:88-118`、`:167-191`
   每次 Bake 全注册表线性扫描；`EvaluateSkillDeliveryDeltas` 每次分配 2 个 vector + `ModifierDelta` 的 11 个 `unordered_map`。同切片内 `EquipmentModifierAdapter.cpp:223` 已用 `thread_local` scratch 规避同类开销，此处未做（§2.1 精神），只因 Bake 是兜底路径而不阻塞。
   修复建议：为节点→记录建立一次性索引，或复用调用方 scratch 缓冲。

5. **Low** — `src/game/systems/modifier/ModifierContext.hpp:128`
   `std::is_sorted` 每次调用 O(n)，`ResolveEffectivePoints` 每条白名单一次 → O(k·n)。当前规模可接受，但属于「同一不变量重复校验」。
   修复建议：由 `NodePointEntry` 构建端保证有序并在 debug 断言，或让快照缓存排序结果。

6. **Low** — `src/game/systems/modifier/EquipmentModifierAdapter.cpp:49`
   `slot_bit = 1u << slotIndex` 与 `EquipmentSlot` 的 `Ring(12)/Ring1(10)/Ring2(11)` 三别名并存；`ItemFactory.cpp:115-116,881-882` 把 Ring1/Ring2 查询归一为 `Ring`，说明戒指可能落在 bit 12。若未来有记录按 `Ring1`/`Ring2` 授权槽位，会与真实佩戴槽位错配。
   修复建议：规定戒指类掩码必须同时包含 `bit10|bit11|bit12`，或在采集时把戒指槽位归一化到单一约定位并在设计文档记录。

7. **Low** — `src/game/systems/modifier/ModifierEvaluator.cpp:575-576`（调用方 `EquipmentModifierAdapter.cpp:113`）
   装备求值使用 `ModifierOpCategory::All`，而 `All` 现含 `SkillDelivery`。当前装备记录无 30..36，无实际影响；但若未来装备记录写入 `SKILL_MANA_COST_MULT`，会被 `GetEquippedManaCostMultiplier` 折叠进装备法耗系数（语义越界）。
   修复建议：装备路径显式传 `Stats|Events|Behavior`，仅技能专精路径传 `All`。

8. **Low** — `src/game/systems/modifier/SkillSpecModifierAdapter.cpp:88-118`
   交付记录采集要求 `node_id_whitelist` 非空，而运行时 `MatchesFilters` 把空白名单视为通配 → 若未来出现「无节点条件的交付记录」，Bake 路径会漏算，而直接 `Evaluate`（预览/其它消费者）会计上，形成路径不一致。当前数据不存在此类记录（属假设性风险）。
   修复建议：统一前置条件（改为「白名单为空或与 active 相交」）或在离线条目中强制拒绝空白名单。

9. **Low** — `src/game/systems/modifier/EquipmentModifierAdapter.cpp:165-179`
   `CollectEquippedRecordIds` 由「仅排序」改为「排序 + 去重」，属公开契约变化。符合设计 §3.3，且现无生产调用者（仅 `EquipmentModifierAdapterTests.cpp:152,178`、`EquipmentSlotFilterTests.cpp:287` 调用），裁定为可接受；仅记录在案以便后续调用者知情。

10. **Low** — `docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md:189`
    设计 §3.3 写「掩码 0 或 0xFFFFFFFF 视为通配」，实现额外纳入 0xFFFF（必要修正，因 §3.4 数据用 65535 且 §4 要求徒手仍生效）。文档与实现已不一致。
    修复建议：把设计 §3.3 的通配集合回写为 `{0, 0xFFFF, 0xFFFFFFFF}` 并注明理由。

### Best Practice

11. **Best Practice** — `src/game/systems/skill/SkillSpecializationBaker.cpp:267-306`
    101/102/103/110/111/134/154 保留为空分支。计划 Task 6.2 措辞为「删除直接赋值代码」（赋值确已删除），行为等价；但空 `else if` 分支提高了后续误回填风险。
    建议：收敛为单条注释（或直接移除分支，未命中节点自然落空），减少两处真相。

12. **Best Practice** — `tests/unit/SkillSpecModifierAdapterTests.cpp:119`
    该用例依赖仓库内已生成产物路径 `assets/generated/modifier_runtime_v2.bin` 并显式 `Reload`，属良好实践（避免同进程内其它用例的合成 blob 残留）。建议在其它读取运行时注册表的用例中沿用同一模式，以彻底消除用例间的单例污染顺序依赖。

## 最佳实践建议

- 上述 1/2/3 三项 `Medium` 建议在**下一次触碰本模块时**一并修复（均为注释/测试/小范围语义收紧，无回归面）。
- 4/5 的缓存与排序不变量建议合并为一次「评测上下文构建端统一准备」改动，避免零散优化。
- 7 的类别收紧是**一行改动**且能封住未来越界，建议尽早做。
- 10/11 属文档与死代码收敛，可与后续技能 2..9 切片一并处理。

## 剩余风险

1. 已接受：未誓约与职业 0 语义合并（设计显式接受，`design.md:20,197`）。
2. 已接受：「仅徒手生效」筛选不可表达（当前无数据需求）。
3. 已接受：`weapon_class_mask == 0` / `0xFFFFFFFF` 通配分支无自动化断言保护。
4. 已接受：戒指槽位三别名（Ring/Ring1/Ring2）与位掩码可能错配（当前无戒指掩码数据）。
5. 已接受：装备路径以 `All` 求值，未来装备记录若写入交付算子会越界（当前无此类数据）。
6. 已接受：Bake 路径每次分配快照与增量容器（仅在兜底路径触发）。
7. 未独立复核：`build.bat RelWithDebInfo` 由编排方执行（exit 0）；本次审查仅复跑其产物与该产物上的 35 个用例。

## 下一步动作

`提交`。本切片合并；建议将发现项 1/2/3/7 登记为后续跟进（可在技能 2..9 切片或下一次触碰 UMR 模块时一并处理），发现项 10/11 随下一轮文档维护收敛。

## 修改后复审（follow-up）

本节为修复后的独立复审（第二轮），仅追加，不改写前文结论。复审对象为 FIX-1..FIX-5 及修复 delta 所列文件；复审人未参与修复实施。

### 复审输入与复跑证据

- 修复 delta 文件：`src/game/systems/modifier/EquipmentModifierAdapter.hpp/.cpp`、`src/game/systems/modifier/ModifierEvaluator.hpp/.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`tests/unit/EquipmentSlotFilterTests.cpp`、`tests/unit/EquipmentModifierAdapterTests.cpp`、`docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md`。
- 独立复跑（本轮直接调用 `bin/NoMoreDayTests.exe`，非编排方代跑）：
  - `*EquipmentSlotFilter*,*EquipmentModifier*` → 12/12 通过，56 断言。
  - `*SkillSpecializationBaker*,*SkillSpecModifier*,*ModifierPoints*` → 18/18 通过，262 断言。
- 静态核对：`ModifierEvaluator.cpp/.hpp`、`ModifierContext.hpp`、`EquipmentModifierAdapter.cpp/.hpp`、`SkillSpecializationBaker.cpp`、`SkillSpecModifierAdapter.cpp`、两份单测、`assets/data/modifier_v2/**`。

### 逐项修复裁定

**FIX-1 — `weapon_class_mask == 0` / `== 0xFFFFFFFF` 通配覆盖：已修复**
- `tests/unit/EquipmentSlotFilterTests.cpp:254-295` 新增用例：掩码 0 在持械（Sword，`:285`）与徒手（`:286`）均命中；掩码 0xFFFFFFFF 在持械（`:289`）与徒手（`:290`）均命中；并额外覆盖掩码 1 = 仅徒手（`:293-294`）。与「持械 + 徒手两种上下文」要求一致。
- 独立复跑通过。前文 Medium#1 闭环。

**FIX-2 — 徒手置 `WeaponSubtype::None`(bit 0)、职业哨兵、戒指掩码常量：已修复**
- `EquipmentModifierAdapter.cpp:176-177`：`unarmedMask = 1u << static_cast<uint32_t>(WeaponSubtype::None)`；`weaponMask != 0 ? weaponMask : unarmedMask`。
- `WeaponSubtype::None == 0` 已核实（`src/game/foundation/components/ItemComponent.hpp:38`：`None = 0, Sword, ...`）。
- `EquipmentModifierAdapter.hpp:15` 定义 `kUnswornProfessionId = -1`，在 `EquipmentModifierAdapter.cpp:146` 使用；`:143-145` 补充「未誓约与真实职业 0 同映射」的中文注释（前文 Medium#2 闭环）。
- `EquipmentModifierAdapter.cpp:19-25` 新增 `kRingSlotMask`（bit10|bit11|bit12）文档常量，`:57` 交叉引用；仅文档化，未做归一化（见「新增发现」）。
- 引擎侧通配判定 `ModifierEvaluator.cpp:54-57` 维持 `{0, 0xFFFF, 0xFFFFFFFF}`，两处重复实现 `:84-87` 与 `:125-128` 同步。与 FIX 声明一致。

**FIX-3 — record-ids `Evaluate` 增类别掩码 + 装备路径收紧：已修复**
- 签名：`ModifierEvaluator.hpp:78-81` 新增 `ModifierOpCategory categories = ModifierOpCategory::All`。
- 掩码确实下传并逐算子生效（非「接收即忽略」）：透传至 `EvaluateRuntimeRecord`（`ModifierEvaluator.cpp:306` 形参、`:576` 调用点），循环内 `:326-328` 执行 `if (!HasOpCategory(categories, CategoryOfOp(opcode))) continue;`。
- 装备路径收紧：`EquipmentModifierAdapter.cpp:124-128` 传 `Stats | Events | Behavior`。
- 回归测试：`tests/unit/EquipmentSlotFilterTests.cpp:297-321` 用 `SKILL_MANA_COST_MULT`(36) 记录断言 `GetEquippedManaCostMultiplier == 1.0`。该断言有效：`SKILL_MANA_COST_MULT`(36) 与 `MANA_COST_MULT`(4) 写入同一 `mana_cost_mult` 容器（`ModifierEvaluator.cpp:237-239` 对比 `:294-296`），若类别掩码失效结果将为 `1 - 0.15 = 0.85`。
- 全量调用点核查（rg）：record-ids 重载仅 4 处生产调用——`TalentModifierAdapter.cpp:101-103`、`SkillSpecModifierAdapter.cpp:188-190`、`SkillSpecModifierAdapter.cpp:211-213` 三者均用默认 `All`（语义不变），`EquipmentModifierAdapter.cpp:124-128` 显式收紧。`MapModifierAdapter.cpp:114`、`MonsterModifierAdapter.cpp:76` 走 Request 重载，不受影响。**无调用点静默丢失类别。** 前文 Low#7 闭环。

**FIX-4 — 空分支收敛 + 设计文档通配集合回写：已修复**
- `SkillSpecializationBaker.cpp` 中 `node_id == 101/102/103/110/111/134/154` 的 `if/else if` 分支已全部移除（`rg "node_id == (101|102|103|110|111|134|154)"` 无命中），收敛为 `:265-267` 单条说明注释。
- 控制流核对：该链为 `if/else if`，移除空分支后这些节点不匹配任何分支并落至 `:313 break`，不穿透、不影响其它节点分支（`:263-313` 逐行核对）。
- 节点仍被交付：`assets/data/modifier_v2/skill_spec_modifiers.json` 含节点 101/102/103/110(×2)/111(×2)/134(×2) 共 9 条交付记录；154 仍由 Bake 步骤 4（`SkillSpecializationBaker.cpp:194-203`，`find(154)` 在 `:195`）赋值式覆盖。
- 设计文档 `design.md:198-200` 已回写：徒手置 `WeaponSubtype::None`（bit 0）而非 0；通配集合 `{0, 0xFFFF, 0xFFFFFFFF}`；`weapon_class_mask = 1` = 仅徒手。与实现逐项一致。前文 Medium#10 / BestPractice#11 闭环。

**FIX-5 — 陈旧注释更新：已修复**
- `tests/unit/EquipmentModifierAdapterTests.cpp:94` 注释改为「ctx.equip_slot_mask 现按槽分组填充，记录与主手物品槽位匹配而生效」，与 `EquipmentModifierAdapter.cpp` 按槽分组实现一致。

### 已发布数据净中性（net-neutrality）声明

复 grep `assets/data/modifier_v2/**` 结果：

| 域文件 | `weapon_class_mask` 取值 | 条数 |
|---|---|---|
| `equipment_modifiers.json` | 65535 | 1 |
| `talent_modifiers.json` | 65535 | 1 |
| `skill_spec_modifiers.json` | 65535 | 10 |
| `monster_modifiers.json` | 4294967295 | 25 |
| `map_modifiers.json` | 4294967295 | 12 |

- 全部取值仅 `{65535, 4294967295}`，**无 `0`，无其它值**（canonical 目录无该字段）。
- 二者均落在引擎通配集合 `{0, 0xFFFF, 0xFFFFFFFF}` 内，`MatchesFilters`（`ModifierEvaluator.cpp:84-85`、`:125-126`）对其短路、**从不读取 `ctx.weapon_class_mask`**。故 FIX-2 的「徒手 0 → bit0」对任何已发布记录的武器匹配结果无影响；持械上下文不含 bit0，也不会因该改动失配。
- opcode 30..36：`rg '"opcode"\s*:\s*"SKILL_' assets/data/modifier_v2/` 仅命中 `skill_spec_modifiers.json`；equipment/talent/map/monster 算子仅为 `ADD_STAT_*`/`ADD_SKILL_LEVEL`/`MANA_COST_MULT` 与 `MONSTER_*`（全部归入 `Stats|Events|Behavior`）。故 FIX-3 的类别收紧对已发布装备记录无影响。
- 结论：**全部 5 项修复对已发布数据为净中性行为变更**；`skill_spec` 路径仍用默认 `All`，其 9 条新记录行为不变。引擎通配集合在修复前后一致（FIX-4 仅回写文档，未改代码）。

### 新增发现

- 无 `Blocker` / `High` / `Medium` 级新增问题。
- 遗留（非本轮引入，前文已登记）：`Low` —— 前文第 6 项戒指三别名（`Ring1(10)/Ring2(11)/Ring(12)`）本轮仅新增 `kRingSlotMask` 文档常量与注释（`EquipmentModifierAdapter.cpp:19-25`），**未实现槽位归一化**；当前无戒指掩码数据，维持「已接受」。建议下一轮实现归一化或在设计文档固化约定。

### 修复后复审结论

`提交`。5 项修复全部落地，且经代码核对与独立复跑双重验证；已发布数据净中性成立；无新增 `Blocker`/`High`/`Medium`。剩余风险沿用前文，并保留本轮确认的第 6 项 `Low`。

## 第二轮修改复审

本节为第三轮独立复审（仅追加，不改写前文）。复审对象为第二轮评审提出的 R-1..R-13 修复；复审人未参与修复实施，结论仅依据当前工作区代码与独立复跑证据，不采信修复报告的自述。**注意**：委托说明称「14 项发现」，但实际枚举为 R-1..R-13 共 13 项；本报告按 13 项逐条裁定，数量差异登记为 N-4。

> 说明：前文「修改后复审（follow-up）」FIX-2 记录「`kRingSlotMask` 仅文档化，未做归一化」。当前代码已实现归一化（R-2），该句已过时，以本节为准；前文其它结论不受影响。

### 复审输入与复跑证据

- 复审基线：`HEAD = d56d61a9`；目标文件均处于工作区未提交状态（`git diff` 即本轮修复 delta）。
- 独立复跑（本轮直接调用 `bin/NoMoreDayTests.exe`，非编排方代跑）：
  - `*EquipmentSlotFilter*,*ModifierPointsScaling*,*SkillSpecModifier*,*SkillSpecDelivery*,*SkillSpecializationBaker*,*EquipmentModifierAdapter*,*ModifierEvaluator*` → 54/54 用例通过，473/473 断言通过。
- 静态核对：`ModifierContext.hpp`、`ModifierEvaluator.hpp/.cpp`、`EquipmentModifierAdapter.hpp/.cpp`、`SkillSpecModifierAdapter.hpp/.cpp`、`SkillSpecializationBaker.cpp`、四份单测、`assets/data/modifier_v2/**`。
- 编译期配置核对：`CMakeLists.txt:140` 为 RelWithDebInfo 追加 `/DNDEBUG`，故 `assert` 在受测配置中不参与编译。

### 逐项修复裁定

**R-1 — 职业负向用例：已修复**
- `tests/unit/EquipmentSlotFilterTests.cpp:229-230`：`multForPlayer(true, static_cast<int>(ProfessionID::Mage)) == 1.0f`，注释明确「若 profession_id 从未被填充，此断言会失败」。
- `ProfessionID::Mage == 1` 核实于 `src/game/foundation/data/TalentData.hpp:20`；`#include "game/foundation/data/TalentData.hpp"` 见 `EquipmentSlotFilterTests.cpp:9`，符号解析成立。
- 反例有效性：`profession_id` 唯一填充点为 `EquipmentModifierAdapter.cpp:154-160`（`mainProfession >= 0` 才写入）。若该填充缺失，`ctx.profession_id` 保持默认 0，`MatchesFilters`（`ModifierEvaluator.cpp:60-68`）以 `1<<0` 命中 `profession_mask = 1`，记录生效返回 `1-0.5 = 0.5`，与期望 `1.0` 冲突 → 用例必然失败。属真实反例，非恒真断言。

**R-2 — 戒指槽位归一化：已修复**
- `EquipmentModifierAdapter.cpp:22-25` 定义 `kRingSlotMask = (1u<<10)|(1u<<11)|(1u<<12)`；`:61-64` 对 `Ring1/Ring2/Ring` 置 `slotBit = kRingSlotMask`，其余槽位保持 `1u << slotIndex`（`:59`）；去重/排序仍以 `(slot_bit, record_id)` 为键（`:74-88`）。
- 匹配侧为掩码**相交**而非相等：`ModifierEvaluator.cpp:89-92`（静态）与`:130-133`（运行时）均判 `(filter.equip_slot_mask & ctx.equip_slot_mask) != 0`。故 `ctx.equip_slot_mask = kRingSlotMask` 时，限定 `Ring1` 的记录可命中戴在 `Ring2` 的物品。
- 回归测试 `EquipmentSlotFilterTests.cpp:372-411`：`Ring1 掩码记录 × Ring2 物品 → 0.5`（`:404-405`）、`Ring` 位 × `Ring1` 物品 → `0.5`（`:407-408`）、`Head` 掩码 × `Ring2` 物品 → 中性 `1.0`（`:410-411`）。三条断言方向互补，非单侧自证。

**R-3 — `groupRecordIds` 线程局部复用：已修复（重入确实不可能）**
- `EquipmentModifierAdapter.cpp:118` 为 `static thread_local std::vector<uint32_t>`，逐组 `clear()`（`:122`）后填满即用尽。
- 调用链核对：`EvaluateEquippedRecordRefs` → `ModifierEvaluator::Evaluate` → `EvaluateRuntimeRecord` → `ApplyOp`，全链无任何回边进入 `EvaluateEquippedRecordRefs`；同 TU 内 `ApplyEquippedSkillLevelBonuses`（`:213-241`）与 `GetEquippedManaCostMultiplier`（`:243-262`）为顺序调用而非嵌套。故同一线程内不存在对 `groupRecordIds` 的并发改写，重入安全成立。

**R-4 — `IsSkillDeliveryOpCode` 取代区间常量：已修复**
- 声明 `ModifierEvaluator.hpp:68`；定义 `ModifierEvaluator.cpp:349-351`（位于匿名命名空间之后，因而可调用 `CategoryOfOp`），语义为 `CategoryOfOp(op) == SkillDelivery`。
- `SkillSpecModifierAdapter.cpp:40-48` 的 `HasSkillDeliveryOp` 改为调用该函数；`rg '30|36'` 确认适配器内已无区间常量残留。

**R-5 — 空 `node_id_whitelist` 视为通配：已修复**
- `SkillSpecModifierAdapter.cpp:99-103`：仅当白名单非空且与 `activeNodeIds` 无交集时才 `continue`，空白名单被采集。
- 与求值侧一致：`ContainsAnyNode` 空输入返回 `true`（`ModifierEvaluator.cpp:39-41` 运行时 / `:25-27` 静态），`ResolveEffectivePoints` 空白名单取 `1`（`:152-167`）。即「采集为通配」与「求值为单点生效」两端口径统一，不再出现「采集跳过 / 求值通配」的分叉。

**R-6 — 复用 `ReloadModifierRuntimeFromAsset()`：已修复**
- `tests/unit/SkillSpecModifierAdapterTests.cpp:1` 引入 `TestCommon.hpp`，`:179` 调用 `ReloadModifierRuntimeFromAsset()`；`TestCommon.hpp:25-45` 为该资产重载的唯一路径来源，含缺失/解析失败告警。

**R-7 — `GetPointsForNode` 去 `is_sorted`：已修复**
- `ModifierContext.hpp:129-144`：`lower_bound`（比较器 `entry.node_id < id`）命中即返回；未命中再线性精确扫描兜底；否则 0。`#include <algorithm>` 已随新增（diff 新增行），无缺头文件风险。
- 分支推演：
  - (a) 有序命中：`lower_bound` 落在该节点（重复时为首个），返回其 `points`；与旧「`is_sorted` + `lower_bound`」一致。
  - (b) 有序未命中：`lower_bound` 落在后继或 `end`，命中判定失败，线性扫描亦无匹配 → 0；与旧实现一致。
  - (c) 乱序命中：`lower_bound` 可能落在任意位置，若恰为匹配项则返回正确值，否则线性扫描补正 → 结果正确。乱序下 `lower_bound` 结果未指定但**非 UB**（随机访问迭代器满足要求）。
  - (d) 重复 node_id：有序时新旧均返回首个匹配；乱序且重复时新代码可能落到后一个重复项，返回其 `points`，与旧「首个匹配」不保证相同——但该输入在生产路径不可达：`BuildNodePointSnapshot` 源为 `slot.allocated_points`（`node_id → points` 映射，键唯一），且快照按 `node_id` 排序（`SkillSpecModifierAdapter.cpp:135-138`）。
- 综上：对「唯一 node_id」表，新旧实现完全一致；「乱序 + 重复」为契约外输入，不构成回归。附带说明：未命中路径仍为 O(n)，热点收益仅体现在命中路径，属可接受折衷。

**R-8 — 乘性交付算子刻意不截断：已修复**
- `ModifierEvaluator.cpp:276-278` 注释显式引用「设计 §3.1」并说明截断会掩盖加点越界；`:279-300` 七个交付算子语义与设计 §3.2.2 逐项吻合；法耗下限 0 见 `:298-299`（`std::max(0.0f, 1.0f - paramF32*pts)`）。
- 线性外推锁定用例 `tests/unit/ModifierPointsScalingTests.cpp:239-247`：20 点 × 0.1 → `3.0f`，与「无隐藏截断」一致。

**R-9 — 技能烘焙测试复用重载助手：已修复（子声明不准确）**
- `tests/unit/SkillSpecializationBakerTests.cpp:30-33` 新增 `EnsureModifierRuntimeForSkillSpec()`，内部 `REQUIRE(ReloadModifierRuntimeFromAsset())`，并保留「资产缺失/解析失败即硬失败」语义。
- 「冗余 include 已移除」不可证：`git diff --numstat` 该文件为 `98 0`（零删除），当前 include 列表（`:1-21`）本就不含 `ModifierRuntimeRegistry.hpp`。实质诉求（复用单一资产路径）已满足，描述偏差不影响裁定。

**R-10 — 调试期一致性断言 + 跳过用例：已修复，但断言为恒真且被裁掉**
- 断言位于 `SkillSpecModifierAdapter.cpp:145-152`（`#ifndef NDEBUG`）。用例 `tests/unit/ModifierPointsScalingTests.cpp:249-261`：`node_points` 非空且不含白名单节点 100 → delta 为空，属真实行为回归护栏。
- 但断言本身**恒真**：`activeNodeIds` 就在 `:140-143` 由 `nodePoints` 逐元素派生（`push_back(entry.node_id)`），两集合同源，size 与逐元素相等永不可能失败；且 `CMakeLists.txt:140` 为 RelWithDebInfo 定义 `/DNDEBUG`，受测配置下该块根本不编译。故该断言既不能失败、也不提供独立交叉校验；作为「防未来分叉」的哨兵无害，但不应计入测试覆盖。

**R-11 — 移除未用 `<cstring>`：已修复**
- `tests/unit/ModifierPointsScalingTests.cpp:1-9` 的 include 列表为 doctest + 三个 modifier 头 + `<cstdint>/<span>/<vector>`，无 `<cstring>`。

**R-12 — 交付求值限定为 `SkillDelivery`：已修复**
- `SkillSpecModifierAdapter.cpp:194-199` 传入 `ModifierOpCategory::SkillDelivery`；类别在 `EvaluateRuntimeRecord` 内**逐算子**生效：`ModifierEvaluator.cpp:330-332` 的 `if (!HasOpCategory(categories, CategoryOfOp(opcode))) continue;`，非「整条记录级」跳过。
- 混合算子回归用例 `SkillSpecModifierAdapterTests.cpp:231-247`：记录含 op36（交付，`1-0.15=0.85`）与 op4（`MANA_COST_MULT`，每点 -50%）。二者写入同一 `mana_cost_mult` 容器，若类别未生效结果为 `0.85*0.5=0.425`；断言 `0.85` 能精确区分，用例有效。

**R-13 — 头文件改为包含求值器声明：已修复（子声明措辞不准确）**
- `SkillSpecModifierAdapter.hpp:4` 新增 `#include "game/systems/modifier/ModifierEvaluator.hpp"`，位于 include 区块（`:1-9`）内，在 `namespace NoMoreDay`（`:11`）之前，未落入命名空间。
- 「前置声明被替换」措辞不精确：`git diff --numstat` 该文件为 `6 0`（零删除），实为直接新增包含；要求（不把 include 放入命名空间）已满足。

### 强制检查项结论

**1. R-12/R-4 正确性（类别下传与逐算子生效）**
`CategoryOfOp`（`ModifierEvaluator.cpp:170-214`）分类为：`0..4 → Stats`（含 `MANA_COST_MULT = 4`）、`5..7 → Events`、`8..28 → Behavior`、`30..36 → SkillDelivery`、其余 `None`。类别掩码由 `EvaluateRuntimeRecord` 形参接收并在算子循环内逐条判定（`:330-332`）。**无任何原属 `Stats/Events/Behavior` 的算子被移入 `SkillDelivery`**，`All = bits0..3` 仅新增 bit3。

**2. 已发布数据净中性（net-neutrality）**
- `weapon_class_mask` 全量取值仍仅 `{65535, 4294967295}`：`equipment` 65535×1、`talent` 65535×1、`skill_spec` 65535×10、`monster` 4294967295×25、`map` 4294967295×12；**无 0，无其它值**。两者均落引擎通配集合 `{0,0xFFFF,0xFFFFFFFF}`（`ModifierEvaluator.cpp:54-57`），`MatchesFilters` 对其短路，不读 `ctx.weapon_class_mask`。
- `equip_slot_mask` 非零值仅 `equipment_modifiers.json:16` 的 `2`（主手，非戒指）；其余全部为 0。故 R-2 的戒指归一化对已发布记录**零影响**；主手掩码 2 的分组/匹配路径与修复前一致。
- opcode 30..36：`assets/data/modifier_v2/**` 中技能交付算子字符串**仅**出现在 `skill_spec_modifiers.json`（`SKILL_MORE_DAMAGE_MULT`×3、其余各×1，共 9 条交付记录加 1 条 `ADD_STAT_PERCENT_MULT` 的 213 记录）；equipment/talent/map/monster 仅用 `ADD_STAT_*`/`ADD_SKILL_LEVEL`/`MANA_COST_MULT` 与 `MONSTER_*`，全部归入 `Stats|Events|Behavior`。故 R-4/R-12 的类别收紧对非技能域记录无影响。
- 结构证据：`git status` 显示 `equipment_modifiers.json`、`talent_modifiers.json`、`map_modifiers.json`、`monster_modifiers.json` **均未改动**，本轮数据变更仅限 `skill_spec` 及其 canonical 与两个生成脚本。
- 结论：**R-2 / R-4 / R-12 对已发布数据为净中性**；技能交付记录的求值结果不因类别收紧而变化。

**3. R-7 正确性**：见 R-7 裁定 (a)~(d)；唯一差异场景（乱序 + 重复 node_id）在生产路径不可达，判定为不构成回归。

**4. R-2 戒指归一化合并语义**
- 同一 `record_id` 同时出现在两枚戒指：两槽均产出 `(kRingSlotMask, id)`，`std::unique`（`:82-88`）折叠为一条 → **不会重复计数**。
- 两枚戒指分别佩戴不同记录：产出 `(kRingSlotMask, idA)` 与 `(kRingSlotMask, idB)`，去重后同组，分组求值（`:120-137`）对两条各施加一次 → **两枚戒指的记录均生效**。
- 分组后 `ctx.equip_slot_mask = kRingSlotMask`，任一戒指位限定的记录都会命中；这是「戒指互为别名」的既定语义（`EquipmentModifierAdapter.hpp:28-30` 注释一致）。

**5. 新增问题**：见下节。

**6. 变更边界**：`git status --porcelain` 与预期集合一致——源码 7 文件、测试 3 文件、数据 2 JSON + 1 canonical、脚本 2 个；未跟踪新增为设计/计划/评审三份文档与 3 个新单测。**无预期外文件被修改**。

### 新增发现

- `Low`（N-1，残留学界）—— `ModifierOpCategory::All` 现包含 bit3 `SkillDelivery`（`ModifierContext.hpp:65`），但本轮仅收紧了装备路径（`EquipmentModifierAdapter.cpp:135-136`）。`TalentModifierAdapter.cpp:101-103`、`MapModifierAdapter.cpp:114-117`、`SkillSpecModifierAdapter.cpp:220-222` 仍用默认 `All`。当前净中性（非技能域无 30..36 记录；且这些消费者只读各自关心的容器），但今后若天赋/地图记录携带 30..36，将被静默并入其 delta。建议对这三处显式传入类别掩码，或把默认值收窄为非交付集合。
- `Low`（N-2，行为语义变化）—— 戒指归一化后，**同一 `record_id` 同时装备于 Ring1 与 Ring2 时只施加一次**（修复前两槽 `slot_bit` 不同，会各施加一次）。该变化与设计 §3.3「按 `(slot_bit, record_id)` 去重」一致，且当前无戒指掩码数据，故无现存影响；建议在设计文档中显式固化「戒指视为同一槽组、同记录不叠加」的约定，避免后续被当作缺陷。
- `Low`（N-3，覆盖有效性）—— R-10 的 `assert`（`SkillSpecModifierAdapter.cpp:145-152`）恒真且被 `/DNDEBUG` 裁掉，不提供实际保护（详见 R-10 裁定）。不影响正确性，但不应计入测试覆盖。
- `Info`（N-4，数量差异）—— 委托说明称 14 项发现，实际枚举 13 项（R-1..R-13）；若确有 R-14 未列出，请补充后另行裁定。
- `Info`（N-5，描述偏差）—— R-9「冗余 include 已移除」与 R-13「前置声明被替换」均与 diff 不符（两文件删除行数为 0）；实质诉求已达成，仅描述需更正。

### 修复后复审结论

`提交`。R-1..R-13 共 13 项逐条核实**全部落地**（R-10 附断言恒真的说明，R-9/R-13 附描述偏差说明）；独立复跑 54/54 用例、473/473 断言通过；三项修复对已发布数据净中性成立；源码变更边界与预期一致。无新增 `Blocker`/`High`/`Medium`，新增问题均为 `Low`/`Info`，登记为 N-1..N-5 供后续维护收敛（可在技能 2..9 切片或下一次触碰 UMR 模块时一并处理）。

复审人：独立复审（未参与修复实施）
