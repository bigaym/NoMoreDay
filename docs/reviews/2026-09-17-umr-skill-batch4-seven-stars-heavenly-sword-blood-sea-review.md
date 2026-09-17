# UMR-SKILL-BATCH-4 实施审查报告（技能 10/11/12 七星斩·天剑降临·血海）

- 审查日期：2026-09-17
- 审查对象：技能 10（七星斩 SevenStarSlash）/ 11（天剑降临 HeavenlySwordDescent）/ 12（血海 BloodSea）批次四迁移的**数据资产、离线门禁、Baker 步骤 1/2 与三行为层单源改造、测试面**
- 审查性质：独立对抗性审查（只读，未修改任何源码/资产/测试，未执行 git commit）
- 审查轮次：第 1 轮（首次审查）→ 第 2 轮（复审，见 §8）
- 结论：**提交**（第 2 轮复审结论。第 1 轮 F-01/F-02/F-03/F-04 均已修复且证据充分；第 2 轮新增 F-05 为 Low 潜伏项，不阻断验收，详见 §8）
- 审查依据：
  - 设计说明 `docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md`（v1.2，重点 §3.2 步骤 1、§3.3 行为改造、§4 数据契约、§5.1 门禁、§6 风险 R-01~R-07）
  - 实施计划 `docs/plans/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-plan.md`（v1.2，Task 1~7）
  - 审查标准 `docs/workflows/review.md`
  - 历史参照：`docs/reviews/2026-09-17-umr-skill-batch3-implementation-review-data-gate.md`、`docs/reviews/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-review.md`

---

## 1. 验证命令与实测结果

已由上游提供、本次审查采信（未重复执行高开销命令）：

| # | 命令 | 结果 |
|---|---|---|
| 1 | `cmd /c build.bat RelWithDebInfo` | RC=0，无 error/warning |
| 2 | `bin\NoMoreDayTests.exe -tc="*SkillBatch4*"` | 14 cases / 81 assertions，0 failed |
| 3 | 全量 doctest | 1758/1760（仅 2 项 flaky GPU 基准：MaterialVFX / ParticleTrail，隔离复跑通过） |
| 4 | `ctest -C RelWithDebInfo` | 17/19（同上 ParticleTrail flake + 已知环境项 `nmd.tests.gpu.hardware`） |

本次审查**独立复跑**的只读门禁：

| # | 命令 | 退出码 | 关键输出 |
|---|---|---|---|
| 5 | `python scripts/validate_skill_spec_modifiers.py` | 0 | `[OK]`×6：`id_decode` / `projectile_integer` / `skill_delivery_domain` / `migration_equivalence` / `retired_keys_absent` / `registry_reverse`；末行 `6/6 passed.` |
| 6 | `python tests/python/SkillSpecBatch4GateTest.py` | 0 | `Ran 20 tests ... OK` |

程序化只读核对（自建临时脚本，输出见正文）：

| # | 核对项 | 结果 |
|---|---|---|
| 7 | canonical 记录总数 / 重复 id | 60 条，`duplicates: []` |
| 8 | 9 条新记录逐字段 vs 设计 §4.2 | `field mismatches: NONE`（opcode/stat_path/operation/debug_name/value/param_f32/node/skill/stacks/param_u32/priority/profession_mask/weapon_class_mask/equip_slot_mask/exclusive_group/max_active/debug_source/min_player_level/tags 全等） |
| 9 | 退役键在 `src/ tests/ scripts/ tools/ assets/data` 的残留 | 仅命中门禁登记表与「负断言」用例（见 §3.3） |
| 10 | mastery 节点 1001/1101/1107/1119/1201/1207/1219/1015 的 `stat_modifiers` | 前 7 个均为 `[]`（1207 的 `damage_modifiers` 亦为 `[]`）；**1015 保留** `[{"type":35,"mode":1,"value":20.0}]` |
| 11 | 1207 `desc_key` | 「血海范围缩小 40%，但伤害总增 (More) 10%。」 |

---

## 2. 变更文件边界（`git status --short`）

```
 M assets/data/mastery_skill_trees.json
 M assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json
 M assets/data/modifier_v2/skill_spec_modifiers.json
 M assets/data/skill_mechanics.json
 M assets/data/skill_mechanics_schema.json
 M scripts/validate_skill_spec_modifiers.py
 M src/game/systems/skill/SkillSpecializationBaker.cpp
 M src/game/systems/skill/behaviors/BloodSea.cpp
 M src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp
 M src/game/systems/skill/behaviors/SevenStarSlash.cpp
 M tests/functional/BloodSeaNodes.cpp
 M tests/functional/HeavenlySwordDescentNodes.cpp
 M tests/unit/SkillMechanicsRegistryTests.cpp
 M tests/unit/SkillSpecializationBakerTests.cpp
?? docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md
?? docs/plans/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-plan.md
?? tests/python/SkillSpecBatch4GateTest.py
?? tests/unit/SkillBatch4DeliveryOpTests.cpp
?? tests/unit/SkillBatch4NullProfileFallbackTests.cpp
```

边界与计划 Task 3.1/3.2/3.4 声明的数据改动集合一致：**无越界文件**；根目录 `settings.json` 未出现在 diff 中（计划 Task 7.4 满足）；新增测试文件由 `tests/CMakeLists.txt:4` 的 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 自动纳入，Task 6.2 隐式满足。

---

## 3. 逐项范围对齐结论

### 3.1 Canonical 9 条记录 — 通过

文件 `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`（9 条新记录行 `2201 / 2244 / 2287 / 2330 / 2373 / 2416 / 2459 / 2502 / 2545`）。程序化逐字段核对设计 §4.2，**零字段不匹配**：

| id | skill | node | opcode | value | stat_path | operation | debug_name |
|---|---|---|---|---|---|---|---|
| 2010010 | 10 | 1001 | SKILL_BONUS_CRIT | 0.02 | skill.bonus_crit | add | SevenStarSlash_Node1001_BonusCrit |
| 2010150 | 10 | 1015 | SKILL_DURATION_FLAT | 0.03 | skill.duration_flat | add | SevenStarSlash_Node1015_VoidTreadDuration |
| 2011010 | 11 | 1101 | SKILL_AREA_MULT | 0.08 | skill.area_mult | mul | HeavenlySword_Node1101_AreaMult |
| 2011070 | 11 | 1107 | SKILL_AREA_MULT | -0.30 | skill.area_mult | mul | HeavenlySword_Node1107_AreaPenalty |
| 2011190 | 11 | 1119 | SKILL_DURATION_FLAT | 0.50 | skill.duration_flat | add | HeavenlySword_Node1119_DurationFlat |
| 2012010 | 12 | 1201 | SKILL_MORE_DAMAGE_MULT | 0.06 | skill.more_damage | mul | BloodSea_Node1201_MoreDamage |
| 2012070 | 12 | 1207 | SKILL_MORE_DAMAGE_MULT | 0.10 | skill.more_damage | mul | BloodSea_Node1207_MoreDamage |
| 2012071 | 12 | 1207 | SKILL_AREA_MULT | -0.40 | skill.area_mult | mul | BloodSea_Node1207_AreaPenalty |
| 2012190 | 12 | 1219 | SKILL_DURATION_FLAT | 0.60 | skill.duration_flat | add | BloodSea_Node1219_DurationFlat |

通用头约束（设计 §4.1）全部满足：`priority=200`、`profession_mask=1`、`weapon_class_mask=65535`、`equip_slot_mask=0`、`exclusive_group=0`、`max_active=0`、`debug_source="skill_spec_node"`、`min_player_level=1`、`stacks==param_u32==skill_id`、`stat_path==runtime.target`、`tags=["skill"]`。60 条记录无重复 id、无孤儿；新增 9 条即批次 4 全部，无连带写入。

### 3.2 节点 1207 双记录（More then Area）— 通过

`2012070`（op_index 0，More +10%）与 `2012071`（op_index 1，Area -40%）`node_id_whitelist` 均为 `[1207]`；`(id-2000000)//10` 对两者均回解为 1207（`12071//10=1207`），`%10` 分别 0/1，符合设计 §4.1 的 id 编解码。门禁 `check_record_id_decode` 与 `SkillSpecBatch4GateTest.py:84-87` 双重覆盖。行为层双算子合成由 `tests/unit/SkillBatch4DeliveryOpTests.cpp:187-190`（点亮 1 点 → More 1.10 / Area 0.60）与 `tests/unit/SkillSpecializationBakerTests.cpp:2413-2418`（1.10 / 72.0）实证，R-02 结论成立。

### 3.3 8 个退役键 + schema 同步 + 无残留 — 通过

`assets/data/skill_mechanics.json` diff 精确删除：整节点 `10/1001`、`10/1015`、`11/1101`、`11/1119`、`12/1201`、`12/1219`，以及 `11/1107.field_radius_mult`、`12/1207.damage_mult`（含 `1107` 仅剩 `impact_damage_bonus`、`1207` 整节点移除）。`skill_mechanics_schema.json` diff 恰为 8 个 `(skill,node,key)` 五元组行，**无多删、无漏删**；`1220.damage_mult` 等机制层键原样保留。

全仓 `rg` 反查 8 个退役键仅命中：门禁登记表 `scripts/validate_skill_spec_modifiers.py:104-112,167-168`、Python 门禁用例 `tests/python/SkillSpecBatch4GateTest.py:36-49`、以及**负断言**用例 `tests/functional/BloodSeaNodes.cpp:128-130,141-143,180-182`、`tests/functional/HeavenlySwordDescentNodes.cpp:127-129,145-148,181-183`、`tests/unit/SkillMechanicsRegistryTests.cpp:81-86`。新增 `tests/unit/SkillBatch4NullProfileFallbackTests.cpp` 无旧键引用。**无真实残留消费点**。

### 3.4 mastery 清理 — 通过

`assets/data/mastery_skill_trees.json`：`1001/1101/1107/1119/1201/1207` 的 6 处 `stat_modifiers` → `[]`；`1207` 的 `damage_modifiers` → `[]`；`1207` `desc_key` 改为 10%。**节点 1015 刻意保留** `[{"type":35,"mode":1,"value":20.0}]`（DodgeChance +20%，设计 §4.4 / R-04）。`1107` 的 `damage_modifiers` `[{...value:35.0,type:2}]` 保留，与设计行 322「技能 10/11/12 各节点 `damage_modifiers` 属机制层，仅 1207 因与 UMR 同源重复被清理」一致，未越界。

### 3.5 Baker 步骤 1/步骤 2 — 通过

`SkillSpecializationBaker.cpp:130-145` 新增 `case 10/11/12`，逐字对齐设计 §3.2：

```cpp
case 10: out_profile.area_radius = skillData->GetParam("radius", 96.0f);
         del.duration = skillData->GetParam("invulnerable_duration", 0.5f); break;
case 11: out_profile.area_radius = skillData->GetParam("field_radius", 140.0f);
         del.duration = skillData->GetParam("field_duration", 5.0f); break;
case 12: out_profile.area_radius = skillData->GetParam("field_radius", 120.0f);
         del.duration = skillData->GetParam("field_duration", 4.8f); break;
```

- **无禁止死写**：三个 case 未写 `del.range` / `del.sub_interval`（全仓确认三个行为文件无 `delivery.range`/`sub_interval` 消费点：`rg` 仅命中 `HeavenlySwordDescent.cpp:427` 一条描述技能 5 的注释）。
- 步骤 2（`SkillSpecializationBaker.cpp:1065-1071`）新增 `case 10/11/12: break;` 纯空分支，符合设计 §3.2。
- 步骤 3 通用合成（`:218` `more_damage_mult *=`、`:222` `area_radius *=`）位于 switch 之后，步骤 1 基准先于乘/加算，顺序正确。

### 3.6 三行为层单源改造与 R-01 — 通过

三个行为文件均引入 `ResolveBakedProfile(registry, owner, kSkillId, localProfile)` 标准基元；`SkillProfileResolve.cpp:9-39` 在无缓存档案且无同 ID 专精槽时返回 `nullptr`。**所有 `profile->` 消费点均为三目守卫**（`rg "profile->"` 仅 8 处，全部带 `profile ?` / `profile &&`）：`SevenStarSlash.cpp:424,553`、`HeavenlySwordDescent.cpp:603,651`、`BloodSea.cpp:428,435,488`。R-01 成立。

- 技能 10：删除 `critChancePerPoint` / `voidTreadDurationPerPoint`；`invulnerableDuration` 与 `critChanceBonus` 改单源，空档案回退 `0.5f` / `0.0f`。未引用 `more_damage_mult`。
- 技能 11：删除 `celestial_domain_per_point` / `sky_piercing_range_mult` / `enduring_heaven_per_point` / `base_field_duration`；`areaMult = (profile && base_field_radius>0) ? profile->area_radius/base_field_radius : 1.0f`，保留 `if (spec.skyPiercingFall) impact_damage_mult *= 1.0f + sky_piercing_damage_mult;`（机制层）。
- 技能 11 **未引用不存在的 `HeavenlySwordFieldComponent::bonus_damage_mult`**（该结构体确无此成员，见 `PersistentFieldComponents.hpp`；文件内 `beam->bonus_damage_mult` 属另一组件类型，编译通过佐证）。
- 技能 12：`duration`/`radius` 基础部分单源 + 行为层 1200/1202 加项保留；删除 `damage_per_point` / `bottomless_damage_mult` / `duration_per_point` 及 `if (spec.bottomlessPurgatory) { *= bottomless_damage_mult; }` 分支；`field.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);` 置于 `BloodSea.cpp:488`，先于 `:489` 的 1202 `+=`。

### 3.7 R-05 顺序偏差 — 实现符合「已接受」语义，但**验收覆盖缺失**（见 F-01）

`BloodSea.cpp:487-488` 的乘算确实先于 `:489` 的 1202 加算，与设计 262 行钉死的位置一致；偏差量 `0.1*K*ring`（`K = effective_consumed * bloodthirstEdgePoints * damage_per_point_per_bloodthirst`）已在 `BloodSea.cpp:486-487` 注释记录。但设计强制要求的「1202=0 / 1202>0 两组量化快照」未实现，详见 §4 F-01。

### 3.8 门禁与测试充分性 — 基本通过，含 2 项覆盖缺口

- 新增 Python 门禁 `tests/python/SkillSpecBatch4GateTest.py` 20 例**实际能捕获目标漂移**：ID 编解码（`:84-86`）、契约同步（`:161-166` 直接比对生成器产物）、值漂移（`:263-282` 注入 `value=0.11` 断言报错并指向 `2012010`）、死键回潮（`:296-305` 注入 literal 键断言被拦）、退役键回潮（`:307-318`）、保留键误删（`:214-224`）。**未发现恒真（tautological）断言**；负断言 `GetMech(...,-1.0f) == Approx(-1.0f)` 能真实区分「键缺失」与「键存在」，非恒真。
- C++ 断言值与计划 Phase 6.3/6.4 对齐（`SkillSpecializationBakerTests.cpp:2397-2420`：96 / 0.5 / 0.59 / 0.02·pts / 140 / 184.8 / 98.0 / 6.5 / 120 / 1.24 / 1.10 / 72.0 / 6.6），仅技能 12 空档案基准存在一处**已文档化的派生偏移**（见 F-03，判定为可接受）。
- 缺口：R-05 差分用例缺失（F-01）；BloodSea More 单源消费缺精确正向端到端锁定（F-02）。

---

## 4. 发现项（按严重度排序）

### F-01 · High · 设计强制要求的 R-05 顺序偏差验收快照缺失，且计划存在不实陈述

- **证据**：
  - 设计 §3.3（design 行 262）：「该偏差是**有意接受的迁移代价**（换取单一事实源），**须**在本设计 §6 R-05 与验收快照测试中显式覆盖。」
  - 设计 R-05（design 行 381）缓解措施：「在 §5.2 迁移等价性快照中固定“1202 = 0”与“1202 > 0”两组用例，**量化并记录该偏差**」。
  - 计划行 173 声称：「该偏差已被设计 R-05 显式接受并由 Task 6.3 快照覆盖。」
  - 实际：计划 Task 6.3（plan 行 243-246）列举的技能 12 快照**不含任何 1202 用例**；实现 `tests/unit/SkillSpecializationBakerTests.cpp:2400-2418` 只有 1201 单点满（1.24）、1207 点亮（1.10），无 1202；`tests/unit/SkillBatch4DeliveryOpTests.cpp:220-236` 只做 profile 级 1201+1207（1.364），无 1202。全仓 `rg "1202"` 于 tests 仅命中 `SkillBehaviorGuardTests.cpp:766`（`castField({{1200,2},{1201,2},{1202,2},{1203,2}})`），其断言仅为相对量 `CHECK(shapedField.bonus_damage_mult > baselineField.bonus_damage_mult);`（`:770`），**未量化**偏差。
- **为何成问题**：批次 4 的核心动机即消灭血海增伤多源重复（设计 §1.1 记录旧实现存在 `1.10 * 1.30 = 1.43x` 膨胀史）。`BloodSea.cpp:488` 是本批最高风险单源行，其唯一的行为变更（1201 乘算前移至 1202 加算之前）既无精确值锁定，也无「1202>0」差分用例量化；任何后续对乘/加顺序或 `more_damage_mult` 聚合语义的改动都不会被测试拦截。计划行 173 的「已被 Task 6.3 覆盖」与计划 Task 6.3 正文自相矛盾，属**未申报的设计/计划偏差**。
- **修复建议**：在 `tests/unit/SkillSpecializationBakerTests.cpp`（或行为层 DoCast 用例）补两组技能 12 用例：`{1201:4, 1207:1}` 与 `{1201:4, 1207:1, 1202:4}`，分别断言 `field.bonus_damage_mult` 的精确期望值（`effective_consumed` 需与断言口径一致），从而以数值固定 `0.1*K*ring` 偏差；并同步更正计划行 173 的覆盖声明。

### F-02 · Medium · BloodSea `more_damage_mult` 正向端到端消费缺精确断言

- **证据**：`src/game/systems/skill/behaviors/BloodSea.cpp:488` `field.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);`。对该行的精确断言仅存在于**空档案路径**：`tests/unit/SkillBatch4NullProfileFallbackTests.cpp:119-120`（`== 1.0f + 0.12f`，即 `more_damage_mult` 回退 1.0）。正向路径的唯一测试是 `tests/unit/SkillBehaviorGuardTests.cpp:721`（`castField({{1201, 2}})`），其断言 `:725` 为相对 `>`，无精确值。
- **为何成问题**：迁移前 1201 的值语义（每点 +6%）由行为层解析；迁移后由 `profile->more_damage_mult` 单源提供。Baker 快照（`SkillSpecializationBakerTests.cpp:2407`）只证明档案值 1.24，未证明行为层确实消费该字段——若该行被误写为其它 profile 字段（如 `area_radius`），现有测试全部通过。属「缺失边界用例证明」（严重度表 Medium）。
- **修复建议**：新增 DoCast 级用例（如 `castField({{1201, 4}})`、`castField({{1207, 1}})`）断言 `field.bonus_damage_mult` 的精确期望值（注意叠加 `1 + effective_consumed*0.12` 基准项），把「1201/1207 单源」钉在行为出口。

### F-03 · Low · 空档案用例中常量恒等断言近似恒真，且空分支未被真正隔离

- **证据**：`tests/unit/SkillBatch4NullProfileFallbackTests.cpp:111-112` `CHECK(skills::BloodSea::kFieldDurationDefault == Approx(4.8f)); CHECK(skills::BloodSea::kFieldRadiusDefault == Approx(120.0f));`，`:117-118` 断言 `4.8f + 0.2f` / `120.0f + 6.0f`。
- **为何成问题**：前两条是对编译期常量与自身字面量的比较（近似恒真，仅能防常量被改）；后两条因 `effective_consumed = max(1, consumed) ≥ 1` 必然叠加一层血欲系数，无法把 `profile == nullptr` 分支与零档案分支区分——若 `ResolveBakedProfile` 改为返回「非空但零 delta」档案，本用例仍全绿。该写法与既有的 `tests/functional/BloodSeaNodes.cpp:561-601`（`[Functional] BloodSea - DoCast constexpr fallback constants`，含相同的「无法直接观测裸常量」降级说明）数值重合，存在部分重复。
- **判定**：属可维护性/证明力的低级别问题，非阻断。空档案前提（`CreateCaster` 不 emplace `ActiveSkillsComponent`，见 `tests/SkillKeyNodeMatrixTestHelpers.hpp:321-329`）本身成立，R-01 验收证据有效。
- **修复建议**：保留常量固化断言作为快照锚，但补一条正向档案用例（见 F-02）以区分「nullptr 回退」与「零 delta 档案」；或对空档案用例直接断言 `profile` 解析结果为 nullptr 的间接证据。

### F-04 · Best Practice · Python 门禁用例存在冗余重复计算

- **证据**：`tests/python/SkillSpecBatch4GateTest.py:192` `index = _canonical_index()` 位于 `for record_id, skill, node, opcode, value in BATCH4_RECORDS` 循环体内，每轮重新加载并解析 canonical 文件（`:168-197`）。
- **为何成问题**：`n=9` 时无实际影响，但重复读盘/解析属可避免的轮子；作为门禁基线代码易被后续批次复制。
- **修复建议**：将 `index = _canonical_index()` 提升到循环外。

> 说明：门禁脚本 `check_record_id_decode` 仍未校验 `%10` 的 op_index（历史项 G-07），本批数据无问题，且 `SkillSpecBatch4GateTest.py:86` 已在 Python 侧补 `assertIn(%10,(0,1))`，不单列为发现项。

---

## 5. 最佳实践建议

1. **优先完成 F-01**：R-05 是设计显式「须…显式覆盖」的验收项，补两组快照成本极低（约 20 行断言），应作为提交前置。
2. **统一「空档案守卫」口径**：`SkillBatch4NullProfileFallbackTests.cpp` 与既有 `BloodSeaNodes.cpp:561-601` 共享同一降级说明，建议在 Batch 4 用例注释中显式引用既有用例，避免读者误判为重复覆盖。
3. **计划文本更正**：计划行 173 的「由 Task 6.3 快照覆盖」与 Task 6.3 正文不符，应在本轮修改中更正，避免后续审查继续采信错误声明。
4. 门禁侧可考虑把 `BATCH4_RETIRED_KEYS`/`BATCH4_LITERAL_DEAD_KEYS` 与 `validate_skill_spec_modifiers.DEAD_MECHANICS_KEYS` 做一致性断言（现 `:209-212` 仅断言 literal ⊂ DEAD），防止两表长期漂移。

---

## 6. 剩余风险（通过审查后接受）

1. **R-04（残余，已接受）**：节点 `10/1015` 的 `stat_modifiers` `type 35 = DodgeChance +20%` 保留（设计 §4.4 显式不清理），仍会经 `StatsSystem` 技能作用域产生全局闪避污染；本批以「不扩大范围」为准，属已登记遗留。
2. **R-05 偏差本体**：`0.1*K*ring` 的数值缩小为有意接受；即便补测，偏差本身仍存在（仅被量化锁定）。
3. **GPU 基准 flaky**：MaterialVFX / ParticleTrail 与 `nmd.tests.gpu.hardware` 的环境性失败与本批无关，但会在全量回归中持续产生噪声，建议另立收尾项。
4. **1202 与 1201/1207 的乘/加语义**：R-05 只处理顺序，未讨论 `GetSkillMoreDamageMult` 多记录聚合的乘性语义（1201×1207=1.364）；该语义与迁移前 `*= (1+0.06·pts)` 后再 `*= 1.1` 等价，本次无风险，但建议在设计中补一句显式语义说明。

---

## 7. 下一步动作

1. **必修（F-01）**：补 R-05「1202=0 / 1202>0」量化快照，并更正计划行 173 的覆盖声明。
2. **应修（F-02）**：补 BloodSea DoCast 正向用例，精确锁定 1201/1207 单源消费出口。
3. **可选（F-03/F-04）**：补充空档案分支隔离断言、提升门禁索引计算位置。
4. **改后复验**：重跑 `python scripts/validate_skill_spec_modifiers.py`、`python tests/python/SkillSpecBatch4GateTest.py`、`bin\NoMoreDayTests.exe -tc="*SkillBatch4*"` 与全量 doctest，再提交复审。

---

# 第 2 轮复审（复审结论：提交）

- 复审日期：2026-09-17
- 复审性质：只读；未修改源码/资产/测试
- 复审范围：针对第 1 轮 F-01~F-04 的修复验证，以及修复过程中并行落入的 C++ 侧改动（哨兵守卫、`std::max` 夹取、注释）之回归复核

## 8.1 复审输入与证据

上游声明（采信，未重跑编译/ctest）：`build.bat RelWithDebInfo` RC=0 clean；全量 `bin\NoMoreDayTests.exe` **1761/1761 cases、141849/141849 assertions、0 failed**（较第 1 轮 1760 增 1 例）；定向 `*SkillBatch4*` **15/15**；`*SkillBehaviorGuard*` 19/19；`ctest -L unit` 8/8。实现方并做了顺序敏感变异验证：把 `*= profile->more_damage_mult` 移到 1202 `+=` 之后会使 R-05 用例 4 条断言失败，回退即转绿。

第 2 轮新增/变更文件（`git status --short` 增量）：

```
?? docs/reviews/2026-09-17-umr-skill-batch4-implementation-review-cpp.md
?? tests/unit/SkillBatch4BloodSeaOrderingTests.cpp
```

`docs/plans/...plan.md`、`tests/python/SkillSpecBatch4GateTest.py`、`tests/unit/SkillBatch4NullProfileFallbackTests.cpp` 为已跟踪/已列出文件的就地修改。

**本次复审独立复跑（只读）**：`python scripts/validate_skill_spec_modifiers.py` → RC=0，`[OK]`×6，`6/6 passed.`；`python tests/python/SkillSpecBatch4GateTest.py` → RC=0，`Ran 20 tests ... OK`。

## 8.2 第 1 轮发现项关闭确认

| 发现项 | 状态 | 证据 |
|---|---|---|
| **F-01**（High）R-05 量化快照缺失 + 计划不实陈述 | **已关闭** | 新增 `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp`（147 行，1 个 TEST_CASE）：`:91` `castField({{1201, 4}, {1207, 1}})`（1202=0）与 `:93` `castField({{1201, 4}, {1207, 1}, {1202, 4}})`（1202>0）；`:104-106` 用同点配直接 Bake 取 `more`；`:125` baseline=1.48、`:126` `more==1.364`、`:128` `k1202==0.4`；`:132-133` 精确断言 `field1202Zero.bonus_damage_mult == 2.01872 == baseline*more`；`:137-138` 精确断言 `field1202Positive.bonus_damage_mult == 2.41872 == baseline*more+k1202`；`:141-144` 偏差 `== k1202*(more-1) == 0.1456`。计划 `:172-175` 已改为「由新增的 R-05 快照测试 tests/unit/SkillBatch4BloodSeaOrderingTests.cpp（“1202 = 0”与“1202 > 0”两组用例，精确锁定 final = baseline*more + K 与偏差 K*(more-1)）覆盖」，与实现一致。 |
| **F-02**（Medium）More 正向消费缺精确锁定 | **已关闭** | 同文件 `:132-133`/`:137-138` 为**非空烘焙档案**（`ConfigureSpecialization` 装配同 ID 专精槽 → `ResolveBakedProfile` 走烘焙路径）下的精确端到端断言，断言值来自独立 Bake 参考档案，可捕获「消费错字段/漏乘 more」类回归。 |
| **F-03**（Low）空档案常量近似恒真 / 分支未隔离 | **已关闭** | 技能 11 空档案断言改为字面量 `tests/unit/SkillBatch4NullProfileFallbackTests.cpp:90` `CHECK(field.header.duration == doctest::Approx(5.0f));`、`:92` `CHECK(field.header.radius == doctest::Approx(140.0f));`（不再与被测常量同源自证）；`:111-114` 注释显式说明兜底叠加层与既有 `BloodSeaNodes.cpp` 同口径。 |
| **F-04**（Best Practice）门禁索引循环内重复解析 | **已关闭** | `tests/python/SkillSpecBatch4GateTest.py:183-184` 注释「canonical 索引与循环无关，提取到循环外」+ `index = _canonical_index()` 已置于 `:185` `for` 循环之外；`self.assertEqual(index[...])` 仍在循环内使用同一索引。 |

## 8.3 并行落入的 C++ 侧改动复核

修复轮同时落入了 `docs/reviews/2026-09-17-umr-skill-batch4-implementation-review-cpp.md`（并行 C++ 复审）的修复，主要三处：

1. **哨兵档案守卫（该报告 F2）**：`BloodSea.cpp:431` `((profile && profile->delivery.duration > 0.0f) ? profile->delivery.duration : ...)`、`:440` `((profile && profile->area_radius > 0.0f) ? profile->area_radius : ...)`；`HeavenlySwordDescent.cpp:607` `(profile && profile->area_radius > 0.0f && base_field_radius > 0.0f)`、`:657` `(profile && profile->delivery.duration > 0.0f)`。
2. **技能 10 时长夹取（该报告 F6）**：`SevenStarSlash.cpp:423-425` 改为 `std::max(0.0f, profile ? profile->delivery.duration : skillData->GetParam("invulnerable_duration", 0.5f))`。
3. **注释补全（该报告 F3/F5）**：`SkillSpecializationBaker.cpp` 技能 10 case 注释注明 `area_radius` 非行为层权威；`HeavenlySwordDescent.cpp:598-600` 补记 `area_radius` 亦折叠装备 `area_radius_mult`。

上述改动的**既有路径行为未改变**（三行为层全部消费点仍为受守卫的单源回退，无新增裸解引用），全量 1761 例通过佐证无回归。

## 8.4 新发现项（本轮）

### F-05 · Low · `area_radius` 哨兵守卫无效（时长守卫有效），且注释与哨兵实际取值自相矛盾

- **证据链**：
  - 哨兵写入：`src/game/systems/skill/SkillSystem.cpp:2737-2741` `BakedSkillProfile p{}; p.skill_id = skill_id;`（`skillData == nullptr` 分支）。
  - 结构体默认：`src/game/foundation/components/SkillDefs.hpp:678` `float area_radius = 1.0f;`（**不是** 0.0；`delivery.duration` 默认 0.0）。
  - 返回路径：`src/game/systems/skill/SkillSystem.cpp:2774` `if (active->baked_profiles[i].skill_id == skill_id) return &active->baked_profiles[i];`（哨兵因 `skill_id` 被赋值而**被原样返回**，不校验幅值）。
  - 失效守卫：`src/game/systems/skill/behaviors/BloodSea.cpp:440` 与 `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:607` 均为 `profile->area_radius > 0.0f`，而哨兵值为 `1.0f` → 条件**恒真**，哨兵被当作有效档案消费。
  - 注释矛盾：`BloodSea.cpp:427-428`/`HeavenlySwordDescent.cpp:601-602` 自述「哨兵档案…area_radius=1.0…仅当 area_radius > 0 时才消费」——`1.0 > 0` 为真，故该守卫对半径不生效（时长守卫 `> 0.0f` 因哨兵 duration=0 而**有效**）。
  - 同源先例：所引 `src/game/systems/skill/behaviors/SwordArray.cpp:133` 亦为 `profile->area_radius > 0.0f`，即该潜伏写法在既有代码中已存在，本次「对齐先例」复制了同一盲点。
- **影响**：若哨兵档案可被消费，技能 12 半径退化为 `1.0 + 层数项`、技能 11 半径退化为 `base*1.0/base ≈ 1.0`（`areaMult = 1.0/140`），与设计期望的 `120/140` 基准严重偏离。
- **可达性**：与并行 C++ 复审 F2 的判定一致——当前被 `SkillSystem::TryCast`（技能数据缺失即拒施）与技能 10 提前返回阻断，属**潜伏项**；且相对修复前无行为回归（修复前半径同样消费哨兵值）。故评为 **Low，不阻断**。
- **修复建议**（二选一）：① 用哨兵唯一可判别特征做守卫——`duration > 0.0f` 同时用于半径消费（哨兵 duration 恒 0），或在 `GetBakedSkillProfile`/`ResolveBakedProfile` 层统一不返回「`skill_id != 0` 但从未成功烘焙」的哨兵档案；② 写入哨兵时使用 `area_radius = 0.0f`（与 `SkillSpecializationBaker.cpp:51-54` 的合法下限 1.0 区分开），并保留现有 `> 0.0f` 守卫。同时修正两处注释中对哨兵取值的表述，避免后续维护者误信守卫已生效。

## 8.5 恒真/弱断言复核（针对新增测试）

- `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp` **非恒真**：`more` 来自独立 `SkillSpecializationBaker::Bake`（`:104-106`）、`baseline` 来自 skills.json 运行时参数（`:113-117`）、`k1202` 来自 `GetMech`（`:119-122`），被断言的 `field.bonus_damage_mult` 来自真实 DoCast 出口，两侧非同源；`:143` 的 `deviation` 由「旧顺序公式 − 实测值」构成，一旦顺序翻转即失败（实现方变异验证 4 断言失败佐证）。`:127` 与 `:39`（`more == 1.24f*1.10f` 与 `kMoreDamageExpected`）为轻微冗余，非恒真。
- 空档案用例经 F-03 修复后，技能 11 改为字面量锚定，证明力提升；技能 12 的常量固化 + 叠加行为断言组合仍属「近似恒真但可接受」，其正向隔离由 F-02 的新增用例补足。

## 8.6 剩余风险（复审后）

1. **F-05（Low，潜伏）**：`area_radius` 哨兵守卫无效；当前被施法前置校验阻断，建议后续收口（见 §8.4）。
2. **装备 `area_radius_mult` 交互（并行 C++ 复审 F3，潜伏）**：`SkillSpecializationBaker.cpp:299-301` 将装备范围词缀折叠进 `profile->area_radius`，技能 11/12 迁移后因此新增隐藏输入；`rg -n "area_radius_mult" docs/designs/2026-09-17-umr-skill-batch4-* docs/plans/2026-09-17-umr-skill-batch4-*` **无命中**，即设计/计划尚未记录此项；当前 `assets/` 无该词缀、`ItemFactory.cpp:643` 随机词缀技能池不含 10/11/12，故潜伏。
3. **R-04（残余，已接受）**：`10/1015` DodgeChance +20% 保留（设计 §4.4）。
4. **R-05 偏差本体**：`0.1456`（本用例参数）已被精确锁定，偏差本身按设计接受。
5. **R-07（信息）**：节点 1207 的 `-40%` 半径由 `stat_modifiers` 污染转为真实 UMR `AREA_MULT`，半径 `120 → 72` 为有意数值变更（设计 §5.2 已接受）。
6. **环境 flaky**：GPU 基准 MaterialVFX/ParticleTrail 与 `nmd.tests.gpu.hardware` 与本批无关。

## 8.7 复审结论

第 1 轮 F-01（High）/F-02（Medium）/F-03（Low）/F-04（Best Practice）**全部关闭**，设计强制的 R-05 验收项与 More 单源正向锁定均已具备可复现证据；`SkillBatch4GateTest.py` 20/20、`validate_skill_spec_modifiers.py` 6/6、全量 1761/1761 逐步通过；未发现新恒真断言；唯一新增 F-05 为 Low 潜伏项且非回归。

**结论：提交。** F-05 建议作为后续低优先级加固项（可并入哨兵档案统一治理），不作为本批阻塞。

