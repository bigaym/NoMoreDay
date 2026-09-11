# 技能9「绝影绝剑」专精树实现审查报告

- 日期: 2026-09-10
- 审查对象: 技能9 绝影绝剑 (Phantom Trance, skill_id = 9) 专精树实现完全性
- 前序参照: `docs/reviews/2026-09-10-skill8-specialization-nodes-review.md`（格式与审查深度基准）
- 审查轮次: 第 1-2 轮（第 1 轮首次审查；第 2 轮独立核验准确性并增补发现项，见 §13。技能1-8 均已完成"审查→修复→提交"闭环，技能9 为下一个未审技能；技能9 无实施计划、无历史审查报告、无修复提交）

---

## 1. 审查目标

按技能7/8 审查报告的八层框架逐层核对：

| # | 层 | 关注点 |
|---|----|--------|
| 1 | 数据层 | `skills.json` 技能9 条目（树 + 内嵌契约）与 `skill_9_tree.json` 布局副本 |
| 2 | 契约层 | `skill_contracts_compact.json` 技能9 块与内嵌契约一致性、role/transmuter/trigger 配置 |
| 3 | 烘焙层 | `SkillSpecializationBaker.cpp` case 9 节点 → 交付参数/标签/触发烘焙 |
| 4 | 行为交付层 | `PhantomFlash.cpp` DoCast/Update 与 §3.9 基础机制 |
| 5 | 管线结算层 | ProcEngine/DamagePipeline/CombatSystem/StatsSystem/SkillSystem 中的技能9 特例 |
| 6 | 跨技能联动层 | 逆命反噬×御剑·回旋(技能8)、御剑化影×御剑步、时光逆流×全技能冷却、技能12 绝影共噬依赖 |
| 7 | 测试防线 | 单元/集成/矩阵/约束测试对技能9 的覆盖与断言语义 |
| 8 | 数据生成 | `skill_mechanics.json` 机制外置、`gen_skill_contracts.py`、布局合并脚本、文档同步 |

## 2. 结论表

| 轮次 | 结论 | 摘要 |
|------|------|------|
| 1 | **修改** | 技能9 当前实现的仍是**旧版「绝影闪/看破反击」**：数据树 26 节点 vs 设计 §3.9 的 28 节点（18 个设计节点零数据、10 个同名节点位置/角色/数值错位、3 节点死锁）；契约层 transmuter/trigger/synergy/resist/sword_step 全量错绑；行为层 `PhantomFlash.cpp` 是冲刺+0.5s 反击窗口，§3.9 的 3s 绝影/免死/逆脉/结束爆发/元素附魔**全部零实现**；管线层存在 6 处技能9 旧语义硬编码；测试防线整体锚定旧语义。结论「修改」，按 Phase 1→4 路线图整改。 |
| 2 | **修改** | 对第 1 轮 15 条发现逐条独立复核：**全部成立、仍开启**（本轮无实现变更）；补充 2 条纠偏（R1 第三处 scope 特判漏计、R2 CombatSystem 反击描述与门控偏差）与 5 条新发现（N1 触发链抑制 guard、N3 触发契约 schema 无法表达 20%/近战/逆脉条件等）。结论维持「修改」。 |

## 3. 输入

- 设计文档: `设计文档/职业设计草案_剑修.md:564-616`（§3.9 绝影绝剑，28 节点规格）；旁证 `:101, :113`（专精定位）、`:713, :715`（剑圣/魔剑技能组）、`:1191, :1220, :1252`（技能12 绝影共噬依赖）、`:1397`（魔剑 MVP 状态声明）、`:1410`（绝影状态组件化要求）
- 数据层: `assets/data/skills.json:5636-5657`（基础）、`:5658-6060`（26 节点树）、`:6062-6288`（内嵌契约）
- 布局副本: `assets/data/skill_9_tree.json:1-325`
- 紧凑契约: `assets/data/skill_contracts_compact.json:380-423`
- 机制外置: `assets/data/skill_mechanics.json`（顶层键仅 `"1".."8"`，技能9 零条目）
- 烘焙层: `src/game/systems/skill/SkillSpecializationBaker.cpp:113-118`（基础原型）、`:904-919`（节点细化）、`:926-988`（`SyncTriggerRules`）
- 行为层: `src/game/systems/skill/behaviors/PhantomFlash.cpp:1-97`、`PhantomFlash.hpp:1-21`
- 组件: `src/game/foundation/components/SkillDefs.hpp:929-937`（`PhantomFlashComponent`）、`src/game/foundation/components/DeliveryArchetypes.hpp:265-275`（`ReactiveWardComponent`）
- 管线: `src/game/systems/skill/ProcEngine.cpp:36-57, 82-89, 101, 160-180`；`src/game/systems/combat/DamagePipeline.cpp:595-597, 1236-1250, 1730-1741`；`src/game/systems/combat/CombatSystem.cpp:553-570`；`src/game/contracts/impl/StatsSystem.cpp:395-398`；`src/game/systems/skill/SkillSystem.cpp:2414-2417, 1279-1294, 1340-1366`
- 跨技能: `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:23, 113`
- 测试: `tests/unit/SkillBehaviorGuardTests.cpp:452-487`、`tests/unit/TriggerRuleTests.cpp:417-470`、`tests/unit/SystemMechanics.cpp:63-76`、`tests/unit/EventConsistencyTests.cpp:222`、`tests/unit/CombatAntiMetaLayerTests.cpp:60-83`、`tests/integration/SkillSystemTests.cpp:886-929, 1015-1016, 1390`、`tests/integration/SkillContractRegistryTests.cpp:239-264`、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp:59-186`（尤其 `:149-152`）、`tests/SkillKeyNodeMatrixTestHelpers.hpp:115`、`tests/fixtures/skill_specialization_keynodes.json:44-48`、`tests/integration/GameplaySystems.cpp:267`
- 历史/旁证文档: `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md:362, 430-435, 444, 464`；`docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md:188`；`docs/reviews/2026-09-07-modular-skill-archetypes-implementation-review.md:47`；`conductor/archive/skill_tree_doc_alignment_20260223/spec.md:19, 107`；`conductor/specs/blade_ascendant_design_config_alignment_20260223.md:31`；`设计文档/特效和UI/BladeAscendant_VFX_Design_v3.md:910-987, 1147, 1174`
- 验证证据: 本轮为**纯静态审查**（设计符合性核对），未执行构建/测试；全部结论可由上述 `path:line` 直接复核。技术参考基线：`docs/reviews/2026-09-10-skill8-specialization-nodes-review.md`（技能8 已按同框架完成整改并提交 `9f9ad255`）
- 实施计划: **不存在**。`docs/plans/` 无 2026-09-10-skill9-*-implementation-plan.md（技能8 于其审查报告第 1 轮后创建 `docs/plans/2026-09-10-skill8-specialization-nodes-implementation-plan.md`，技能9 需补建）

## 4. 变更文件边界

`git status --short`（HEAD = `9f9ad255`）：仅 ` M settings.json`（历史脏文件，与本审查无关）。技能9 无 staged/未提交变更、无修复提交、无相关分支；本次为纯基线审查，全部发现基于工作区 HEAD 内容。

## 5. 范围对齐

### 5.1 基础行为

| 设计（§3.9:566） | 数据层 | 行为层 | 结论 |
|------------------|--------|--------|------|
| 散去形体进入绝影，持续 3s | 无 duration 参数（params 仅 dash_speed/dash_dist，`skills.json:5654-5657`） | 无 3s 状态（counter_window 0.5s，`PhantomFlash.cpp:43`） | **未实现** |
| 移动速度 +20% | 无 | 无（旧 930 影遁在反击窗口内 +20%，`PhantomFlash.cpp:64-68`） | **未实现** |
| 致命伤触发免死：锁 1 血并提前结束 | 无 | 无（旧实现为反击免伤一次，`DamagePipeline.cpp:1236-1240`） | **未实现** |
| 结束时周围爆发无形剑气 | 无 | 仅 VFX `BuffExit`（`SkillSystem.cpp:1350-1352`） | **未实现** |
| 法力 30 / 冷却 15s | mana 30 ✓ / **cooldown 1.0** ✗（`skills.json:5639-5640`） | — | **矛盾** |
| Tags: [Buff][Defensive][Emergency][Cooldown] | [Physical, Attack, Melee, Movement, Hit, sword_skill] ✗（`:5641-5648`） | — | **矛盾** |
| 无直接伤害（除结束爆发） | base_damage 50 / weapon_damage_mult 3.0 ✗（`:5649-5650`） | 反击即时伤害 ~100（`ProcEngine.cpp:174-179`） | **矛盾** |
| 名: 绝影绝剑（原绝影闪） | name_key 绝影绝剑 ✓ / desc_key 仍为旧文案"看破状态，受到攻击立即反击。" ✗（`:5637-5638`） | 旧反击语义 | **矛盾** |

### 5.2 设计 28 节点覆盖矩阵（设计节点 → 数据现状）

| 分支 | 设计节点 (max) | 前置要求 | 数据层对应 | 前置/角色 | 实现状态 |
|------|----------------|----------|------------|-----------|----------|
| 基础 | 延命 (4) | 根 | 无 | — | **零数据** |
| 基础 | 气旋爆发 (4) | 根 | 无 | — | **零数据** |
| 基础 | 身轻如燕 (3) | 根 | 902 (3) | 根 ✓ / role 无 | 数据在，行为零实现 |
| 基础 | 空明心境 (4) | 根 | 974 (4) | 误挂 `950@2 + 970@2` ✗ | 位置错，行为零实现 |
| A | 向死而生 K (1) | 延命 2/4 | 无 | — | **零数据** |
| A | 浴血重生 (4) | 向死而生 | 无 | — | **零数据** |
| A | 绝影护甲 (3) | 延命 4/4 | 无 | — | **零数据** |
| A | 虚灵之躯 K (1) | 绝影护甲 2/3 | 无 | — | **零数据** |
| A | 灵流穿透 (3) | 虚灵之躯 | 913 (3) | 误挂 `912@1` ✗，contract role=Passive + `affects_sword_step` ✗ | 位置/角色错，效果零实现 |
| A | 虚境馈赠 (3) | 绝影护甲 | 914 (3) | 误挂 `911@2` ✗ | 位置错，效果零实现 |
| B | 逆脉 K (1) | 气旋爆发 3/4 | 无 | — | **零数据** |
| B | 孤注一掷 (4) | 逆脉 | 无 | — | **零数据** |
| B | 死亡螺旋 (3) | 逆脉 | 无 | — | **零数据** |
| B | 嗜血本能 (1) | 逆脉 | 无 | — | **零数据** |
| B | 剑随心动 (3) | 孤注一掷 2/4 | 934 (3) | 误挂 `931@1`（不可达）✗ | 位置错+死锁，效果零实现 |
| B | 逆命反噬 T (1) | 孤注一掷 3/4 | 935 (1) | 误挂 `934@2`（不可达）✗，内嵌 contract role=**Keystone** ✗ | 位置/角色错+死锁，Trigger 零实现 |
| C | 破空一闪 M (1) | 气旋爆发 2/4 | 无 | — | **零数据** |
| C | 缩地成寸 (4) | 破空一闪 | 无 | — | **零数据** |
| C | 意随神行 (3) | 身轻如燕 2/3 | 无 | — | **零数据** |
| C | 御剑化影 (3) | 破空一闪 | 无 | — | **零数据** |
| C | 时光逆流 K (1) | 意随神行 3/3 | 954 (1) | 误挂 `953@1` ✗ | 位置错，效果零实现（旧 951 仅有反击返 1.5s 技能8 冷却） |
| C | 全神贯注 (3) | 时光逆流 | 955 (3) | 误挂 `953@1` ✗ | 位置错，效果零实现（无节点消费点） |
| D | 天山雪隐 T (1) | 空明心境 2/4 | 无 | — | **零数据** |
| D | 凛冬附魔 (3) | 天山雪隐 | 无 | — | **零数据** |
| D | 疾空惊雷 T (1) | 空明心境 2/4 | 972 (1) | 误挂 `971@1` ✗，内嵌 contract role=**Keystone** + exclusion 2 ✗ | 位置/角色错，效果零实现 |
| D | 过载护盾 (3) | 疾空惊雷 | 973 (3) | 误挂 `970@1` ✗ | 位置错，效果零实现 |
| D | 意念穿透 (4) | 任意 Transmuter | 无（contract TypeD 误挂 971） | — | **零数据** |
| D | 灵气反哺 (1) | 意念穿透 3/4 | 无 | — | **零数据** |

统计：**18/28 设计节点零数据**（延命、气旋爆发、向死而生、浴血重生、绝影护甲、虚灵之躯、逆脉、孤注一掷、死亡螺旋、嗜血本能、破空一闪、缩地成寸、意随神行、御剑化影、天山雪隐、凛冬附魔、意念穿透、灵气反哺）；**10/28 同名节点全部位置错位**（0 个前置/角色正确）；**有效实现 0/28**。

### 5.3 旧树 26 节点残留清单

旧版专用、设计中不存在的节点 16 个：900 识破、901 残心、903 神速反制、910 震慑剑压、911 气劲爆发、912 影杀连斩、930 影遁、931 幽影长存、932 致命奇袭、933 影之双生、950 灵动之躯、951 流光重置、952 气劲充盈、953 影之舞、970 元素护盾、971 天罚反震。

## 6. 初审核验与纠偏

1. **权威设计判定**: §3.9（`设计文档/职业设计草案_剑修.md:564-616`）为玩法规格权威；`设计文档/特效和UI/BladeAscendant_VFX_Design_v3.md:910-987` 已按新设计（免死、逆脉、冰雷附魔、时间倒转圆环）描述 VFX；技能12 绝影共噬（设计 `:1191, :1252`、`assets/data/mastery_skill_trees.json:2172`）依赖「逆脉/免死窗口」，与 §3.9 闭合。三者一致指向新设计。
2. **旧形态的来源已确认**: `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md:362, 430-435` 在架构层明确把技能9 映射为 `Mobility + ReactiveWard`（节点 910/913/930/951），并据此在 `docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md:188`（Task 3.6）完成模块化迁移；`conductor/archive/skill_tree_doc_alignment_20260223/spec.md:19` 与 `conductor/specs/blade_ascendant_design_config_alignment_20260223.md:31` 也已记录「runtime still uses legacy naming/shape」「Structural aligned, semantic effect pending」。即：**旧树是模块化阶段的临时形态，从未按 §3.9 做语义落地**，`conductor/*` 属旧文档（AGENTS.md：not use now）。技能9 整改需同时更新 `docs/designs` §5.9 与非目标条款（见 M1）。
3. **架构非目标冲突**: `docs/designs/2026-09-06-...design.md:444` 要求 `ExpectedKeyNodesBySkill` 等矩阵 100% 行为一致、零回归；但技能8 修复提交 `9f9ad255` 已修改 `tests/fixtures/skill_specialization_keynodes.json:41`（skill 8 行）与 `tests/SkillKeyNodeMatrixTestHelpers.hpp`，确立「按设计重写关键节点矩阵」的先例。技能9 属同类设计性整改，不受该临时非目标约束，但必须在技能9 计划中显式声明对矩阵/fixture 的更新。
4. **既有守卫为何失效**: `tests/integration/GameplaySystems.cpp:250-282` 只校验行为文件内节点常量 ID 存在于数据表——PhantomFlash.cpp 引用的 930/950/951/952/970 全部存在（旧树内），故守卫通过；该防线无法发现「整树语义错版」。与技能8 审查纠偏第 1 条同构。
5. **技能8 审查遗留衔接**: `docs/reviews/2026-09-10-skill8-specialization-nodes-review.md` 剩余风险第 5 条记录技能9 死锁（931 前置 930@2 而 930 max=1）属跨技能范围外待办；本轮确认该死锁为**连锁死锁**：930(max=1) → 931 不可达 → 934（需 931@1）不可达 → 935（需 934@2）不可达，共 3 节点不可点亮。
6. **规模纠偏**: 技能8 的问题是「数据已更新、烘焙/行为错位」；技能9 的问题是**数据层本身就是旧版**，故整改范围大于技能8：需要先在数据/契约层重排 28 节点，再落行为与管线。

## 7. 发现项

### Blocker (C)

**C1 — 数据层：整树仍为旧版「绝影闪/看破反击」26 节点，与 §3.9 的 28 节点规格脱节**

证据: `assets/data/skills.json:5658-6060` 的 `talent_tree` 为 26 节点（900-974），名称/前置/上限均按旧设计：900 识破（反击伤害 +20..100%）、901 残心（反击姿态时长）、903 神速反制、910 震慑剑压、911 气劲爆发、912 影杀连斩、930 影遁、931 幽影长存、932 致命奇袭、933 影之双生、950 灵动之躯、951 流光重置、952 气劲充盈、953 影之舞、970 元素护盾（`desc_key` 为空，`:5983-5984`）、971 天罚反震。§3.9:569-616 要求的 18 个节点零数据（见 §5.2），10 个同名节点前置/上限/角色全错位。

为何成问题: 玩家在技能9 专精树上的每一个加点都落在设计外的旧效果；新设计的免死/逆脉/附魔三条 build 轴完全不可用。契约 `min_nodes=26/max_nodes=26`（`:6065-6066`）也固化了错误的 26 节点规模（设计 28）。

修复: 按 §3.9 重排 28 节点（保留 902/913/914/934/935/954/955/972/973/974 的语义承接或改派新 ID），修正 `max_points` 与 `prerequisites` 至设计值，补齐 970 描述；同步更新 `skill_9_tree.json` 布局副本与契约 `min/max_nodes`。

**C2 — 契约层：Transmuter/Trigger/Synergy/SwordStep/剑意/穿透/互斥全量错绑**

证据（`assets/data/skill_contracts_compact.json:380-423` 与 `skills.json:6062-6288`）:

| 契约字段 | 现值 | §3.9 要求 | 问题 |
|----------|------|-----------|------|
| `transmuter_node_ids` | `[970, 950]` | 天山雪隐(冰)/疾空惊雷(雷) | 970 元素护盾（desc 空）、950 灵动之躯（旧闪避节点）被当作变质器；972 疾空惊雷反被标 Keystone |
| `max_transmuters` | 2 | 2（互斥） | 数量对，但互斥组挂在 `{971,972}`（`keystone_exclusion_groups`），971 是旧天罚反震 |
| `trigger_nodes` | `[951 → skill8, eff0.6, icd0.5]` | 935 逆命反噬 → skill8, 20%, 0.5s | 触发源节点错绑；`SyncTriggerRules`（`SkillSpecializationBaker.cpp:976-985`）会为 951 写 OnSkillHit 规则，而设计要求「逆脉期间近战命中 20% 触发」 |
| `synergy_node_ids` | `[930]` | 设计标注「缺，待补」（§3.9:571） | 旧影遁冒充协同节点；设计协同方向为回旋/灵剑决联动 |
| `sword_intent_node_ids` | `[952]` | 意随神行(每秒生剑意)/意念穿透(剑意→穿透) | 952 气劲充盈（旧「重置时回蓝」）与剑意交互无关 |
| `sword_step_node_ids` | `[913]` | 御剑化影 | 913 灵流穿透与御剑步无设计关联 |
| `resist_models` / `scope_policies` | `{971: TypeD_StatToPenetration}` / `{971: GlobalWhileBuffActive}` | 意念穿透 剑意→穿透 上限 40% | 错挂 971（旧反震 Keystone） |
| `cost_affixes` | `{971: GlassCannonCrit}` | 设计无此词缀要求 | 设计外代价词缀 |
| 内嵌 contract role | 935 Keystone、972 Keystone、953 Keystone | 935 Trigger、972 Transmuter | role 与设计相反 |

为何成问题: 即使行为层将来实现新节点，契约仍会把效果烘焙到错误节点；`GlobalWhileBuffActive` 的技能9 分支（`StatsSystem.cpp:395-398`、`SkillSystem.cpp:2414-2417`）以 `PhantomFlashComponent.counter_window` 作为「绝影激活」判据，与新设计的 3s 绝影状态无关。

修复: 契约整体按 §3.9 重建：`transmuter=[天山雪隐ID, 疾空惊雷ID]` + `max_transmuters=1`（设计明确互斥）+ 同组 `keystone_exclusion_groups`；`trigger_nodes=[逆命反噬ID→skill8, requires_melee_hit/逆脉期间, 20%, 0.5s]`；`sword_step=[御剑化影ID]`；`sword_intent=[意随神行ID, 意念穿透ID]`；TypeD 迁移至意念穿透；删除 971 的 cost_affix；`min/max_nodes=28`；synergy 待设计补齐或同步修改 §3.9:571。

**C3 — 行为交付层：`PhantomFlash.cpp` 为旧版冲刺+反击窗口，§3.9 基础机制零实现**

证据: `src/game/systems/skill/behaviors/PhantomFlash.cpp:25-84`——
- `:29-39` 读取 `dash_speed`/`dash_dist` 并向反向（`Vector2Subtract(self, target)`）冲刺，写 `MobilityComponent(Dash, has_invulnerability=true)`；
- `:42-44` `counter_window = 0.5 + 0.1×qiyao`，`:60-61` 写 `ReactiveWardComponent`；
- `:54-58` 读取旧节点位 `930/950/951/952/970`，`:77-83` 注册 `OnTakeDamage → 反击` 触发规则；
- `:86-93` Update 仅倒计时窗口。

新设计基础机制（3s 持续、+20% 移速、致命伤锁 1 血免死+提前结束、结束爆发无形剑气）**无任何代码**；`PhantomFlashComponent`（`SkillDefs.hpp:929-937`）字段全部为旧语义（`counter_window/knockback_bonus/triggered/flow_reset/synergy_shadow_hide/intent_overflow/enchant_tag`），无「绝影状态/免死标记/生命锁/附魔窗口」字段。

为何成问题: 技能9 的玩法身份（保命/低血爆发开关/节奏重置）完全缺失；`rg` 全 `src/` 对 `免死/CheatDeath/DeathSeal/逆脉` 零命中，确认并非命名差异而是功能不存在。硬否决对应「缺失必备行为」。

修复: 按 §3.9 重写行为：默认无位移（破空一闪为可选 Keystone 分支），建立 3s 绝影状态与免死判定（需 DamagePipeline 致命伤钩子）、结束爆发、元素附魔窗口；组件改为新状态字段并外置数值至 `skill_mechanics.json`。

**C4 — 管线结算层：6 处技能9 旧语义硬编码，与新设计冲突且将成为死代码/错误行为**

证据:
- `ProcEngine.cpp:36-57`：`OnTakeDamage` 时若存在 `PhantomFlashComponent` 自动创建 `rule_id=9` 反击规则；`:82-89` 反击窗口门控；`:101` 触发后写 999s 冷却闭锁；`:160-180` 直接构造 `DamagePool` 反击伤害（`max(25, damage_multipliers[0]×100)`，物理/附魔标签）。
- `DamagePipeline.cpp:1236-1250` 与 `:1730-1741`：`PhantomFlashComponent.counter_window>0 && !triggered` 时**完全免伤**（`total_final_damage=0`）并 `ShadowCast(..., 2, ...)`（裂空斩反击）。
- `CombatSystem.cpp:553-570`：另一条反击分支（后撤步位移+日志）。
- `StatsSystem.cpp:395-398`、`SkillSystem.cpp:2414-2417`：`GlobalWhileBuffActive` scope 对 `source_skill_id==9` 的特判，以 counter window 作为激活判据。
- `SkillSystem.cpp:1279-1294`（ReactiveWard 生命周期）、`1340-1366`（PhantomFlash 更新、`BuffExit` VFX、Cold/Lightning GainExtra 清理）。

为何成问题: 同一「反击」语义在三处独立实现（ProcEngine/DamagePipeline/CombatSystem），互不一致；新设计落地时这些特判必须先清理，否则新旧两套状态会叠加（例：新绝影期间被旧 counter window 逻辑误判）。若不移除，技能9 仍表现旧版「受击免伤+裂空斩反击」。

修复: Phase 3 中移除三处反击特判与 scope 特判，`rule_id==9` 的 ProcEngine 特殊分支改为通用 Trigger 语义；`ReactiveWardComponent` 保留给技能4（`BladeWard.cpp:112-113` 仍使用），但解除与技能9 的耦合；组件更新循环改挂新绝影状态。

**C5 — 跨技能联动层：新设计依赖的共享系统缺失，且技能12 联动引用悬空**

证据:
- 逆命反噬（§3.9:598）需「逆脉期间近战命中 20% 投掷技能8 御剑·回旋（0.5s ICD）」——当前技能8 触发是 `SyncTriggerRules` 为节点 951 生成的 OnSkillHit 规则（`SkillSpecializationBaker.cpp:976-985`），无逆脉门控、无近战命中过滤、无 20% 概率；`PhantomFlash.cpp:75-82` 的手写规则还会叠加。
- 御剑化影（§3.9:605）需读取御剑步状态并修改连击点流失——全 `src/` 无节点 605 对应实现，技能9 侧无 SwordStep 消费点；契约却把 `sword_step` 挂在 913。
- 时光逆流（§3.9:606）需「绝影结束时所有其他技能剩余冷却 -3s」——现有只有旧 951 在**反击触发**时返还技能8 1.5s（`ProcEngine.cpp:49-52, 151-158`），且 `SevenStarSlashShared.hpp:23,113` 反向由技能8 侧引用技能9 冷却（`RefundSkillCooldownPercent`），方向与触发时机均错。
- 全神贯注（§3.9:607）需「绝影期间其他技能法力消耗 -20..60%」——无系统支持；技能9 施法只写 `SkillModifierComponent` 的物理→元素 GainExtra（`PhantomFlash.cpp:70-74`）。
- 技能12「绝影共噬」（设计 `:1191, :1252`；`assets/data/mastery_skill_trees.json:2172`）要求「在绝影的逆脉/免死窗口中释放血海」，当前无逆脉/免死窗口，技能12 该节点联动不可达。

为何成问题: 技能9 是魔剑 build 的枢纽（设计 `:744, :1397`）与技能12 的前置依赖；跨技能契约落地前，技能9 无法承担设计职责。

修复: 在技能9 计划中为「绝影状态/逆脉/免死窗口」定义可查询组件标志（设计 `:1410` 已要求组件化），再以该标志重写逆命反噬触发与技能12 联动；冷却返还改为通用「按剩余冷却百分比/固定值返还」服务（复用 `RefundSkillCooldownPercent` 抽象）。

### High (H)

**H1 — 烘焙层：基础原型与节点映射仍按旧树，仅 5 个节点有分支**

证据: `SkillSpecializationBaker.cpp:113-118` case 9 写 `primary=Mobility, secondary=ReactiveWard, speed=600, duration=0.25`；`:904-919` 仅映射 930/950/951/952/970（分别写 feature_flags 1/2/4/8/16、sub_count、Tag 转质）。954/955 在数据树中存在但**无任何烘焙分支**；18 个缺失节点无分支；`PhantomFlash.cpp:54-58` 与 Baker 用位魔法 `&1/&2/&4/&16` 隐式耦合。

为何成问题: 即使数据层补齐，烘焙层不会把新节点写入交付参数；位魔法不可读且与技能8 审查建议（数值外置 `skill_mechanics.json`）不一致。

修复: 按 §3.9 重写 case 9 基础原型（无位移的 Buff/状态交付，破空一闪时切换 Mobility）与逐节点分支；参数外置 `skill_mechanics.json` 技能9 块；废弃 feature_flags 位魔法，改为明确字段。

**H2 — 测试防线：技能9 相关断言全面锚定旧语义，新设计零覆盖**

证据（全部为旧语义断言或旧配置锁定）:
- `tests/unit/SkillBehaviorGuardTests.cpp:452-487`：断言 930→`synergy_shadow_hide`、951→`flow_reset`、952→`intent_overflow==2`、970→`enchant_tag==Cold`；
- `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp:149-152`：断言施放技能9 后 `PhantomFlashComponent.flow_reset`；`tests/SkillKeyNodeMatrixTestHelpers.hpp:115` 与 `tests/fixtures/skill_specialization_keynodes.json:44-48` 将关键节点锁定为 `[913,930,950,951,952,970,971,972]`；
- `tests/integration/SkillSystemTests.cpp:886-929`（反击走管线并伤害攻击者）、`:1015-1016`（施放后 counter_window>0）、`:1390`；`tests/unit/TriggerRuleTests.cpp:417-470`（单窗口单次反击+规则清理）；`tests/unit/SystemMechanics.cpp:63-76`；`tests/unit/EventConsistencyTests.cpp:222`；`tests/unit/CombatAntiMetaLayerTests.cpp:60-83`（971 TypeD + counter window 激活）；
- `tests/integration/SkillContractRegistryTests.cpp:239-264`：结构性矩阵把 951 列为触发节点、且要求技能9 `max_transmuters==2`。

为何成问题: 按 §3.9 整改会立即打破这些测试；若不重写而弱化断言，将命中审查硬否决（弱化测试换取通过）。同时新设计的 28 节点无任何测试。

修复: Phase 4 按新设计重写上述断言（counter window→绝影状态、免死、逆脉、附魔窗口、结束冷却返还），更新 fixture/helper 关键节点集，并新增「28 节点全可点性遍历」「免死锁血/提前结束」「时光逆流结束返还」「冰/雷互斥」回归测试。

**H3 — 数据生成与机制外置缺失**

证据: `assets/data/skill_mechanics.json` 顶层键仅 `"1".."8"`（技能9 零条目），而 §3.9 的全部数值区间（0.25..1.0s、15..60%、10..30%、2..6 法力、1..4% 穿透等）无数据宿主；`assets/data/skill_9_tree.json:1-325` 是旧树布局副本，由 `scripts/merge_skill_tree_coords_and_deps.py:90-106` 合并回 `skills.json`，双源同步存在漂移风险；新节点无 `icon_id` 资源登记（`SkillNodeAssetRegistry` 未核查新 ID）。

修复: 建立 `skill_mechanics.json` 技能9 块作为节点数值唯一事实源；重写布局文件或改为由 `skills.json` 单向导出；为新节点分配 icon 并登记。

### Medium (M)

**M1 — 文档漂移：架构设计文档与玩法设计文档对技能9 的描述冲突**

- `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md:430-435` 仍写 `MobilityDelivery(后撤) + ReactiveWardDelivery(弹反反制)` 与节点 910/913/930/951；`:444` 的非目标条款会误导后续实现者认为旧语义受保护。
- `设计文档/职业设计草案_剑修.md:1397` 声明魔剑 MVP（含「绝影绝剑 的保险机制」）**[已实现]**，与运行时不符。
修复: 技能9 计划中同步修订 `docs/designs` §5.9 与非目标条款措辞，并把设计文档 `:1397` 状态改为「部分完成（技能9 保险机制待重做）」。

**M2 — 数据卫生：非法标签与设计标签词汇缺口**

证据: `skills.json:5647` 技能9 tags 含 `"sword_skill"`，`TagRegistry::TagFromString`（`src/game/foundation/data/TagRegistry.hpp:160-195`）只识别 `"SwordSkill"`，`ParseTagList` 会静默丢弃；同类问题存在于技能2/3/10（`skills.json:714, 1456, 6302`）与技能6 的 `"Duration"`。设计标签词汇 `[Buff][Defensive][Emergency][Cooldown]` 中 `Emergency`/`Cooldown` 在 TagRegistry 无枚举，`Defensive` 与现有 `Defense` 命名不一致。
修复: 新技能9 tags 使用注册表规范名（`Buff`/`Defense`），如确需 `Emergency`/`Cooldown` 语义须先扩展 TagRegistry 或改用既有字段表达；顺带清理跨技能非法标签（技能9 本轮仅记录，改动留给对应技能）。

**M3 — 死数据与空描述**

证据: 954/955 无 Baker 分支与运行时代码消费（`SkillSpecializationBaker.cpp:904-919` 之外无命中）；970 `desc_key` 为空（`skills.json:5983-5984`）；内嵌契约仅覆盖 13/26 节点（`skills.json:6075-6287`）。
修复: 随 C1/H1 重排一并清理；契约节点表与新树 28 节点一一对齐。

**M4 — 遗留副本文件**

证据: `assets/data/skills copy 2.json:4419+` 为技能9 同源旧树的整段副本（含部分新文案，如 `:4682, :4728`）。存在被误编辑/误当作事实源的风险。
修复: 删除或移入归档目录（不在本轮执行，列入技能9 计划清理项）。

### Low / Best Practice (L)

**L1 — 位魔法与隐式耦合**: `PhantomFlash.cpp:54-58` 的 `feature_flags & 1/2/4/16` 可读性差且无编译期约束；重写时改为语义字段（对应 `conductor/code_standard.md` §7.2 精神：避免脆弱分支）。

**L2 — VFX 资产已先行**: `设计文档/特效和UI/BladeAscendant_VFX_Design_v3.md:910-987, 1147, 1174` 已按新设计给出绝影/免死/逆脉/附魔/时间倒转表现与 `MAX_PHANTOM_OVERLAYS` 预算，运行时只有 `BuffExit` 一个事件（`SkillSystem.cpp:1350-1352`）。实施时同步接入 GPU 事件，避免二次返工。

**L3 — 复用与清理边界**: `ReactiveWardComponent` 同时服务技能4 与技能9（`BladeWard.cpp:112-113`）；解除技能9 耦合时保留组件与通用更新路径，防止误删技能4 功能。

## 8. 逐节点核对矩阵（旧树 26 节点）

| 节点 | 名称 | max | 实际前置 | 契约 role | 设计对应 | 缺陷 |
|------|------|-----|----------|-----------|----------|------|
| 900 | 识破 | 5 | 根 | — | 无 | 旧节点 |
| 901 | 残心 | 3 | 根 | — | 无 | 旧节点 |
| 902 | 身轻如燕 | 3 | 根 | — | 身轻如燕(基础) | 名对，效果无实现 |
| 903 | 神速反制 | 3 | 根 | — | 无 | 旧节点 |
| 910 | 震慑剑压 | 3 | 900@3 | — | 无 | 旧节点 |
| 911 | 气劲爆发 | 5 | 900@1 | — | 无 | 旧节点 |
| 912 | 影杀连斩 | 1 | 910@1 | Keystone | 无 | 旧节点+假 Keystone |
| 913 | 灵流穿透 | 3 | 912@1 ✗ | Passive+sword_step ✗ | 灵流穿透(虚灵之躯→) | 前置/角色错，效果无实现 |
| 914 | 虚境馈赠 | 3 | 911@2 ✗ | — | 虚境馈赠(绝影护甲→) | 前置错，效果无实现 |
| 930 | 影遁 | 1 | 902@1 | Synergy | 无（设计协同待补） | 旧节点+假 Synergy；死锁源头 |
| 931 | 幽影长存 | 3 | 930@2 ✗（930 max=1） | — | 无 | **不可达** |
| 932 | 致命奇袭 | 5 | 930@1 | — | 无 | 旧节点 |
| 933 | 影之双生 | 1 | 930@1 | Keystone | 无 | 旧节点+假 Keystone |
| 934 | 剑随心动 | 3 | 931@1 ✗ | — | 剑随心动(孤注一掷2/4→) | **不可达**+前置错 |
| 935 | 逆命反噬 | 1 | 934@2 ✗ | Keystone ✗ | 逆命反噬 Trigger(孤注一掷3/4→) | **不可达**+role 反 |
| 950 | 灵动之躯 | 5 | 903@2 | Transmuter ✗ | 无 | 旧节点+假 Transmuter（雷） |
| 951 | 流光重置 | 1 | 950@3 | Trigger(skill8) ✗ | 无 | 旧节点+假 Trigger |
| 952 | 气劲充盈 | 3 | 951@1 | Passive(intent) ✗ | 无 | 旧节点+假剑意 |
| 953 | 影之舞 | 1 | 950@2 | Keystone | 无 | 旧节点+假 Keystone |
| 954 | 时光逆流 | 1 | 953@1 ✗ | Keystone | 时光逆流(意随神行3/3→) | 前置错，效果无实现 |
| 955 | 全神贯注 | 3 | 953@1 ✗ | — | 全神贯注(时光逆流→) | 前置错，效果无实现 |
| 970 | 元素护盾 | 3 | 901@3 | Transmuter ✗ | 无 | 旧节点+假 Transmuter（冰）；desc 空 |
| 971 | 天罚反震 | 1 | 901@3 | Keystone+TypeD ✗ | 无 | 旧节点+假 TypeD/互斥/词缀 |
| 972 | 疾空惊雷 | 1 | 971@1 ✗ | Keystone+excl2 ✗ | 疾空惊雷 Transmuter(空明心境2/4→) | role/前置错，效果无实现 |
| 973 | 过载护盾 | 3 | 970@1 ✗ | — | 过载护盾(疾空惊雷→) | 前置错，效果无实现 |
| 974 | 空明心境 | 4 | 950@2 + 970@2 ✗ | — | 空明心境(基础根节点) | 位置错，效果无实现 |

## 9. 架构与跨系统偏差

1. **双重事实源**: 技能9 语义当前由「数据树 + 内嵌契约 + compact 契约 + docs/designs §5.9」四处描述且互不一致；整改必须先统一到 §3.9 单一事实源，再生成其余产物（对照技能2/8 的 `skill_mechanics.json` 外置模式）。
2. **管线特例债**: 技能9 是三处反击特判 + 两处 scope 特判的唯一技能；与技能8 审查中「双重回旋状态机」同类的架构债务，应在行为层重写时一并收敛。
3. **跨专精耦合**: 技能9 是技能12「绝影共噬」与魔剑 build 的枢纽；技能9 未落地前，技能12 该协同节点与魔剑低血玩法链条不可验证。
4. **临时非目标已到期**: `docs/designs` 的「关键节点矩阵零回归」非目标保护的是旧版临时形态；技能8 修复已开先例，本轮应按设计重写矩阵（见 §6.3）。

## 10. 整改落地路线图

| Phase | 范围 | 关键产出 |
|-------|------|----------|
| 1 | 设计与计划对齐 | 创建 `docs/plans/2026-09-10-skill9-specialization-nodes-implementation-plan.md`；明确 28 节点 ID 分配、synergy 节点是否补齐（§3.9:571「缺，待补」需产品决策）、Tag 词汇、免死/逆脉组件模型；修订 `docs/designs` §5.9 与 `:1397` 状态 |
| 2 | 数据/契约 | 重写 `skills.json` 技能9 树（28 节点）+ 基础数值（CD 15s、去直接伤害、tags）+ 内嵌契约；更新 compact 契约、`skill_9_tree.json`；新增 `skill_mechanics.json` 技能9 块；清理 M2/M3/M4 |
| 3 | 烘焙/行为/管线 | 重写 Baker case 9 与 `PhantomFlash.cpp`；新建绝影状态/免死/逆脉/附魔窗口；移除三处反击特判与 scope 特判；接入结束爆发、结束冷却返还、御剑化影×御剑步、逆命反噬×技能8 |
| 4 | 测试与回归 | 重写 H2 所列旧断言；更新 fixture/helper 关键节点；新增 28 节点可点性、免死锁血、逆脉禁疗、时光逆流返还、冰雷互斥、技能12 联动回归；`ctest -R "nmd.tests.(unit\|integration)"` 全绿 |

## 11. 剩余风险

1. **无构建/测试证据（本轮）**: 纯静态审查；「旧测试仍通过」为读码推断，未执行 `build.bat`/`ctest` 复核。若需回归基线，应在计划 Phase 1 前补跑一次全量测试。
2. **synergy 节点未定义**: §3.9:571 标注「缺，待补」，契约却已强行填 930；若 Phase 1 不决策，Phase 2 契约仍会留一个虚假协同节点。
3. **免死与 DamagePipeline 的耦合面**: 致命伤锁 1 血需要伤害结算末段的钩子（现仅 `InvulnerableComponent` 全免路径，`DamagePipeline.cpp:512-515`）；实现位置与优先级需在计划中定死，否则可能与技能10 七星斩无敌帧、怪物处决逻辑冲突。
4. **技能12 联动时序**: 逆命反噬触发技能8 与技能12 绝影共噬都依赖「逆脉/免死窗口」标记；标记的生命周期（进/出窗口事件）若设计不清，会出现触发丢失或重复。
5. **跨技能非法标签清理范围**: M2 涉及技能2/3/6/10；本轮不修改，存在后续各技能审查重复记录的可能（与技能8 审查剩余风险第 5 条同类的跨技能溢出）。
6. **VFX 与数值预算**: VFX v3 已定 `MAX_PHANTOM_OVERLAYS=4` 等预算，但技能9 新节点数量（28）与特效叠加上限的映射未验证，需在性能工作流下复核。

## 12. 下一步动作

1. **本报告结论: 修改**。在 Phase 1 完成前，不建议对技能9 做点状修补（任何单层修补都会被其他层的旧语义抵消）。
2. 创建技能9 实施计划 `docs/plans/2026-09-10-skill9-specialization-nodes-implementation-plan.md`（参照 `docs/plans/2026-09-10-skill8-specialization-nodes-implementation-plan.md` 结构），计划需先回答：28 节点 ID 分配、synergy 节点定义、绝影/逆脉/免死组件模型、Tag 词汇、`skill_mechanics.json` 数值表。
3. 计划评审通过后按 Phase 2→4 实施；每阶段完成后在本报告追加跟进轮次（不得删除本第 1 轮记录），出结论前须补跑构建与测试证据。

---

## 13. 第 2 轮核验（准确性验证与新增发现）

- 本轮变更内容: 无实现变更；对第 1 轮报告做独立复核：逐条复读数据/契约/烘焙/行为/管线/测试并验证 C1-C5、H1-H3、M1-M4、L1-L3 共 15 条发现，同时排查第 1 轮未覆盖的管线特判、触发契约 schema 与紧凑契约角色清单。
- 复查范围: `assets/data/skills.json`（技能9 树/内嵌契约/非法标签）、`assets/data/skill_contracts_compact.json:380-423`、`assets/data/skill_mechanics.json`、`assets/data/skill_9_tree.json`、`assets/data/mastery_skill_trees.json:2172`、`SkillSpecializationBaker.cpp:113-118/904-919/926-988`、`PhantomFlash.cpp/.hpp`、`ProcEngine.cpp:36-57/82-101/148-192`、`DamagePipeline.cpp:512-515/594-598/1234-1255/1729-1744`、`CombatSystem.cpp:552-594`、`StatsSystem.cpp:395-398`、`SkillSystem.cpp:822-834/1279-1294/1340-1366/2414-2417`、`SkillContract.hpp:41-48`、`scripts/gen_skill_contracts.py:240-369`、`TagRegistry.hpp:160-195`、`BladeWard.cpp:112-117`、`设计文档/职业设计草案_剑修.md:101/113/564-616/713/715/744/1191/1220/1252/1397/1410`、`设计文档/特效和UI/BladeAscendant_VFX_Design_v3.md:910-987/1147/1174`，以及第 1 轮引用的全部测试（`SkillBehaviorGuardTests.cpp:452-487`、`TriggerRuleTests.cpp:417-470`、`SystemMechanics.cpp:63-76`、`EventConsistencyTests.cpp:222`、`CombatAntiMetaLayerTests.cpp:60-83`、`SkillSystemTests.cpp:886-929/1004-1016/1390`、`SkillContractRegistryTests.cpp:239-301`、`SkillKeyNodeMatrixIntegrationTests.cpp:149-152`、`SkillKeyNodeMatrixTestHelpers.hpp:115`、`tests/fixtures/skill_specialization_keynodes.json:44-48`、`GameplaySystems.cpp:250-282`）。
- 第 1 轮发现项状态: 本轮无实现变更，15 条**全部成立且仍开启**；无已解决项。

### 13.1 逐条核验判定

| 编号 | 判定 | 关键复核证据 |
|------|------|--------------|
| C1 | **成立** | 26 节点名称/前置/上限逐一复现（930 max=1、931 需 930@2 → 934 需 931@1 → 935 需 934@2 三节点连锁不可达）；18 个设计节点零数据与 §5.2 清单一致；`skills.json:6065-6066` min/max=26、`:5983-5984` 970 空 desc 复现；`skill_9_tree.json` 恰 325 行（旧树布局副本）。 |
| C2 | **成立（补充 schema 约束，见 N3）** | `skill_contracts_compact.json:380-423` 全部字段复现；内嵌契约 13/26 节点、912/933/935/953/954/971/972 Keystone 角色复现；`SyncTriggerRules`（`SkillSpecializationBaker.cpp:976-985`）确会为 951 生成 `OnSkillHit/Victim` 规则；互斥约定复核：技能3-8 在双 Transmuter 互斥时 `max_transmuters=1`（`SkillContractRegistryTests.cpp:258-264`），故 C2 修复建议 `max_transmuters=1` 与既有约定一致。 |
| C3 | **成立** | `PhantomFlash.cpp:29-39/42-44/46-58/60-61/63/64-69/70-74/77-83/86-93` 逐行复现；组件字段 `SkillDefs.hpp:929-937` 全为旧语义；`rg 免死\|CheatDeath\|DeathSeal\|逆脉` 全 `src/`（693 文件）0 命中，确认并非命名差异而是功能不存在。 |
| C4 | **成立（计数/表述纠偏见 R1、R2；遗漏见 N1、N2）** | 所有文件:行引用复现；另确认 `DamagePipeline.cpp:1729-1744` 所在 `CalculateBatch`（`:1419` 起）无 `is_simulation` 参数，批量路径只在真实命中调用，故"缺 `!is_simulation` 门控"不成立为独立缺陷。 |
| C5 | **成立** | 951 触发由契约生成（无近战过滤/概率/逆脉门控），且 `PhantomFlash.cpp:77-83` 另叠加手写规则；旧返还是"反击触发时返还技能8 1.5s"（`ProcEngine.cpp:44-55/151-158`）；`SevenStarSlashShared.hpp:23,110-114` 技能8 侧反向引用技能9 冷却；技能12 依赖 `mastery_skill_trees.json:2172`（节点 id 1217「绝影共噬」）与 `设计文档:1191/1220/1252` 一致。 |
| H1 | **成立** | case 9 基础原型 `:113-118`、节点分支 `:904-919`（930/950/951/952/970）、954/955 零分支、位魔法与 `PhantomFlash.cpp:46-58` 隐式耦合全部复现。 |
| H2 | **成立（补充测试锁定项，见 N5）** | 全部引用断言逐行复现；另 `SkillContractRegistryTests.cpp:243-244/296-300` 还锁定技能9 触发节点=951、`transmuter_count==2`、`keystone_count>=2`。 |
| H3 | **成立** | `skill_mechanics.json` 顶层键仅 `"1".."8"`；`merge_skill_tree_coords_and_deps.py:88-106` 的双源合并逻辑（`--skills`/`--tree`/`--tree-glob` → skills.json）复现。 |
| M1 | **成立** | `docs/designs`:362「技能9 → Mobility + ReactiveWard」、`:430-435` 节点 910/913/930/951、`:444` 「100% 行为一致、零回归」非目标条款、`设计文档:1397` 魔剑 MVP **[已实现]**（含绝影绝剑保险机制）、`:1410` 状态组件化要求全部复现。 |
| M2 | **成立** | `skills.json:5647` 小写 `sword_skill` 被 `TagRegistry.hpp:166`（仅识别 `"SwordSkill"`）静默丢弃；技能2/3/10 同问题 `:714/1456/6302`；技能6 的 `"Duration"` 在 `:3552`；`Emergency`/`Cooldown` 无枚举、设计 `Defensive` 与注册表 `Defense` 命名不一致。 |
| M3 | **成立** | 954/955 无 Baker 分支与消费点、970 desc 空、内嵌契约 13/26 复现。 |
| M4 | **成立** | `skills copy 2.json:4419` 起为技能9 同源副本，`:4462/4527/4536/4601/4610/4682/4691/4728` 混有新设计文案（移速/穿透诅咒/回蓝/逆脉攻速/逆脉近战 20% 投掷/结束 -3s CD/法力 -20..60%/落雷与天山雪隐互斥）。 |
| L1 | **成立** | `PhantomFlash.cpp:46-58` `feature_flags & 1/2/4/16` 位魔法复现。 |
| L2 | **成立** | VFX `:910-987`（免死 `:925`、逆脉 `:964`、时间倒转 `:975`、逆脉近战投掷 `:979`）、`:1147`、`:1174` `MAX_PHANTOM_OVERLAYS=4` 复现；运行时仅 `SkillSystem.cpp:1350-1352` BuffExit 一个事件。 |
| L3 | **成立** | `BladeWard.cpp:112-117` 仍以 `ReactiveWardComponent`（`ward_duration=10`/`counter_window=0.8`/`counter_skill_id`=技能4）服务技能4。 |

### 13.2 报告纠偏项（对第 1 轮表述的修正）

**R1（High）C4 的 scope 特判数量：两处 → 三处**
观测: 第 1 轮 §2 摘要与 §7-C4、§9.2 均写"两处 scope 特判（StatsSystem + SkillSystem）"，但 §3 输入清单已列 `DamagePipeline.cpp:595-597`。实际 `DamagePipeline::Calculate` 内的 `can_apply_scope` lambda（`DamagePipeline.cpp:594-598`）同样对 `source_skill_id==9` 特判 `PhantomFlashComponent.counter_window > 0 && !triggered`，且该点决定"绝影期间的伤害能否套用 Global 修饰"，是与 StatsSystem/SkillSystem 并列的第三处。
为何成问题: Phase 3 清理清单若按"两处"执行，`DamagePipeline.cpp:594-598` 会被漏改，旧 counter_window 语义将残留于伤害作用域判定，新绝影状态接入后形成双重判据。
修复: 第 1 轮 §10 Phase 3 的"移除三处反击特判与 scope 特判"补入该点（改为全部 scope 特判 + 触发链抑制，见 N1）。

**R2（Low）C4 对 CombatSystem 分支的描述不准确**
观测: 第 1 轮写"另一条反击分支（后撤步位移+日志）"。实际 `CombatSystem.cpp:552-594`：免伤并返回 false（`commitApplyResult(0,0,true)`）、瞬移至攻击者身后 20px（`Vector2Normalize(target-attacker)` 方向 ×20.0f）、`ShadowCast(..., 1, ...)` 以流云刺反击、施加 2s `stealth` buff；且门控仅 `!pf->triggered`，无 `counter_window > 0` 判断（与 `DamagePipeline.cpp:1237/1731` 不一致，见 N2）。另 `ProcEngine.cpp` 反击路径实际延伸到 `:189`（含 `CombatSystem::ApplyDamage` 调用），第 1 轮括注 `:160-180` 偏窄。
为何成问题: 反击效果与门控均被低估，影响 Phase 3 清理范围与"三处反击互不一致"的论证强度。
修复: 按上述行为改写 C4 条目（不改结论）。

### 13.3 新增发现项

**N1（Medium）`SkillSystem.cpp:822-834`：反击窗口期间静默抑制触发链派发（第 1 轮未记录的第 4 处技能9 特判）**
观测: `InitHooks` 注册的 OnSkillHit 触发派发回调中，若施法者存在 `PhantomFlashComponent` 且 `counter_window > 0 && !triggered`，直接 `continue` 并记 `LogGuardBlocked(kDiagTriggerDepth, ..., "counter window suppresses trigger chain")`。
为何成问题: 该逻辑让旧反击窗口在 0.5s 内全局屏蔽该实体的**所有** OnSkillHit 触发（不限技能9），是行为层之外的隐藏联动；Phase 3 若只删反击伤害而不处理该 guard，新绝影状态下会出现"触发被旧窗口抑制"或"递归触发"两种风险之一。
修复: Phase 3 重写时明确新触发派发规则（绝影/逆脉期间是否允许触发链），删除该 counter_window 专用 guard。

**N2（Low）`CombatSystem.cpp:553-594` 第三处反击实现门控不一致**
观测: 该分支仅检查 `!triggered`；窗口到期时 `PhantomFlash::Update`（`PhantomFlash.cpp:86-93`）返回 true 并经 `SkillSystem.cpp:1343-1365` 移除组件，通常掩盖差异；但组件存在期间若 `counter_window` 已为 0，该分支仍会触发瞬移+流云刺+隐身，且其反击交付与 Pipeline/ProcEngine 两路均不同。
为何成问题: 是 C4"三处反击互不一致"的具体证据；现有测试只覆盖管线路径，无法覆盖直伤路径分歧。
修复: 并入 C4/R2 清理；Phase 3 前若需保留，至少统一判定条件与反击交付。

**N3（Medium，Phase 2 原子约束）触发契约 schema 无法表达"近战命中 / 逆脉期间 / 20% 概率"**
观测: `TriggerContract`（`SkillContract.hpp:41-48`）字段仅 `trigger_skill_id/effectiveness/range_mult/internal_cooldown/consumes_mana/requires_crit`；紧凑契约 `trigger_nodes` 生成器（`gen_skill_contracts.py:328-355`）与 `SyncTriggerRules`（`SkillSpecializationBaker.cpp:976-985`）均不写 `base_chance`；`TriggerRuleComponent::base_chance` 默认 1.0，`ProcEngine.cpp:97` 对 `base_chance >= 1.0f` 走必中分支。第 1 轮 C2 修复建议的"20%、requires_melee_hit/逆脉期间"当前无任何数据落点。
为何成问题: 若 Phase 2 仅改 `skill_contracts_compact.json` 的 `trigger_nodes`，20% 概率与近战/逆脉门控会在生成链（compact → gen → `SkillContract` → Baker → ProcEngine）中丢失，逆命反噬将退化为"任意命中 100% 触发"。
修复: Phase 2 扩展触发契约 schema（建议 `base_chance` + `requires_melee_hit` 或通用窗口条件字段），同步改 `gen_skill_contracts.py`、`SkillContract.hpp`、`SkillRegistry` 解析、`SyncTriggerRules` 与 ProcEngine 判定；或在 Phase 1 决策该节点改为行为层手写规则并记录理由。

**N4（Low）紧凑契约技能9 未声明 keystone/passive 角色清单**
观测: 技能9（及技能3）紧凑配置无 `keystone_node_ids`/`passive_node_ids`，技能12 有；`gen_skill_contracts.py:279-288` 接受显式清单，未见清单时按默认规则推断，技能9 内嵌 7 个 Keystone 均为推断产物。
为何成问题: 28 节点重建时若不显式声明角色，推断结果可能与设计（Keystone/Trigger/Synergy/Transmuter 分布）不符，而 H2 类测试只校验计数。
修复: Phase 2 在紧凑配置显式声明全部非 Passive 节点角色，生成后复核内嵌契约。

**N5（Low）H2 测试锁定项超出第 1 轮清单**
观测: `SkillContractRegistryTests.cpp:243-244` 锁定 `expected_trigger_nodes[8]=951`；`:296-300` 锁定 `trigger_count==1`、`transmuter_count==2`、`keystone_count>=2`（对 skills 1..9 全体）。第 1 轮 H2 仅记 951 与 `max_transmuters==2`。
为何成问题: 新设计的触发节点/Keystone 分布变化会同时打破这些断言，Phase 4 重写清单需完整覆盖，避免遗漏导致"同一测试反复修改"。
修复: 在 H2/Phase 4 清单补入上述断言（触发节点 ID、`transmuter_count==2`、`keystone_count>=2`、`synergy_count>=1`）。

### 13.4 未独立核验项（保留为残余风险）

1. 本轮仍为纯静态核验，未执行 `build.bat`/`ctest`；"旧测试仍通过"未复跑（承接第 1 轮剩余风险 1）。
2. `gen_skill_contracts.py` 角色推断函数未完整追踪（N4 依据为接口与产物差异）；28 节点重建时以显式声明规避。
3. `SkillNodeAssetRegistry` 对新节点 icon 的登记情况未核查（承接 H3）。
4. VFX `MAX_PHANTOM_OVERLAYS=4` 与 28 节点特效叠加的性能映射未验证（承接第 1 轮剩余风险 6）。

### 13.5 第 2 轮结论

| 项 | 状态 |
|----|------|
| 第 1 轮 15 条发现 | 全部成立、仍开启（本轮无实现变更） |
| 纠偏项 R1-R2 | R1 影响 Phase 3 清理清单（必须补第三处 scope 特判）；R2 为描述修正 |
| 新增发现 N1-N5 | N1 并入 Phase 3 管线清理；N3 为 Phase 2 原子约束（schema 扩展）；N2/N4/N5 为 Phase 2/4 输入 |
| 结论 | **修改**（维持第 1 轮判定；技能9 零实现变化） |

下一步动作: 维持第 1 轮 §12 路线图；创建技能9 实施计划时须纳入 R1（第三处 scope 特判）、N1（触发链抑制 guard）、N3（触发契约 schema 扩展决策）。

---

## 14. 第 3 轮核验（实施完成与回归验证）

- 本轮变更内容: 按 `docs/plans/2026-09-10-skill9-specialization-nodes-implementation-plan.md` 完成 WS0-WS7 全部实施：29 节点数据树重建（含产品决策新增协同节点 993）、契约与生成器 schema 扩展、Baker/行为/管线重写、测试重写与新增、文档与资产同步。
- 复查范围: `assets/data/skills.json` 技能9、`skill_contracts_compact.json`、`skill_9_tree.json`、`skill_mechanics.json`、`scripts/gen_skill_contracts.py`、`src/game/systems/skill/behaviors/PhantomTrance.*`、`SkillSpecializationBaker.cpp`、`ProcEngine.cpp`、`DamagePipeline.cpp`、`CombatSystem.cpp`、`StatsSystem.cpp`、`RegenerationSystem.hpp`、`AilmentEngine.cpp`、`AISystem.cpp`、`SkillSystem.cpp`、`tests/**`、`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`、`设计文档/职业设计草案_剑修.md`。

### 14.1 第 1/2 轮发现项闭环

| 编号 | 状态 | 处置与证据 |
|------|------|------------|
| C1 | 已闭环 | 旧 26 节点树重写为 29 节点（设计 28 + 协同节点 993）；930→931→934→935 死锁链与假角色全部消除；`skills.json` 技能9 min/max=29、970 空 desc 节点删除；`skill_9_tree.json` 与内嵌树一致 |
| C2 | 已闭环 | 契约按新树重新生成：`max_transmuters=1`、transmuter=[989,972] 同互斥组、trigger=935（base_chance 0.2/近战/DeathSeal 窗口）、synergy=993、sword_intent=[987,991]、sword_step=[988]、TypeD+GlobalWhileBuffActive 移至 991；971 cost_affix 删除 |
| C3 | 已闭环 | `PhantomFlash` 全量重写为 `PhantomTrance`（绝影形态/免死/逆脉/附魔窗口）；`PhantomFlashComponent`/`counter_window`/`triggered` 在 `src/` 与测试零残留 |
| C4/R1/R2 | 已闭环（措辞于第 4 轮修正） | 三处反击特判（ProcEngine/DamagePipeline/CombatSystem）+ N1 触发链抑制 guard 全部删除；DamagePipeline 的 scope 特判删除；`SkillSystem`/`StatsSystem` 两处 `GlobalWhileBuffActive` 的技能9分支为窗口承载的通用判定改写（非删除，详见 §15.2-M-C）；免死/窗口语义统一由 `PhantomTranceComponent` 承载；技能4 `ReactiveWard` 保留 |
| C5 | 已闭环 | 935 经契约→`SyncTriggerRules`→ProcEngine 触发技能8（20%/近战/0.5s ICD/逆脉窗口）；`御剑化影` 御剑步减耗/闪避接入；`时光逆流` 结束返还其它技能 3s；旧技能8 反向返还删除；技能12 绝影共噬依赖的窗口标记已存在 |
| H1 | 已闭环 | Baker case 9 全节点分支 + 数值外置 `skill_mechanics.json`；位魔法改为语义字段分支 |
| H2/N5 | 已闭环 | 所列测试全部重写（SkillSystemTests/TriggerRuleTests/SystemMechanics/EventConsistency/CombatAntiMetaLayer/SkillContractRegistry/SkillKeyNodeMatrix×2/GameplaySystems/fixture/helper） |
| H3 | 已闭环（占位图标为已知近似） | `skill_mechanics.json` 新增技能9 块；`skill_9_tree.json` 与 `skills.json` 单源；图标 975-993 复用旧 PNG 并重生成注册表（347 项），prompts 补 19 条 |
| M1 | 已闭环（第 4 轮补齐） | `docs/designs` §5.9 重写为 PhantomTrance；`设计文档:1397` 状态改 `[部分完成]`；`:571` 协同节点定义补齐；残留 `PhantomFlash.cpp`/精确弹反引用于第 4 轮清理（见 §15.2-M1） |
| M2 | 技能9 已闭环（跨技能遗留） | 技能9 tags 改为注册表规范名 `Buff/Defense/Movement/SwordSkill`；技能2/3/6/10 非法标签不在本轮范围 |
| M3 | 已闭环 | 954/955 实现，970 删除，内嵌契约与 29 节点一一对齐 |
| M4 | 已闭环 | `assets/data/skills copy 2.json` 已删除（无脚本引用） |
| L1 | 已闭环 | 分支语义化 |
| L2 | 部分（性能复核为残余风险） | 结束事件沿用 BuffExit；`MAX_PHANTOM_OVERLAYS` 叠加性能未复核，留待性能工作流 |
| L3 | 已闭环 | `ReactiveWardComponent` 及技能4 用例保留 |
| N1 | 已闭环 | guard 删除，测试中 counter window 抑制断言移除 |
| N2 | 已闭环 | 见 C4 |
| N3 | 已闭环 | `TriggerContract`/`TriggerRule` 新增 base_chance/requires_melee_hit/required_window，生成器→SkillRegistry→Baker→ProcEngine 全链透传 |
| N4 | 已闭环 | 契约按显式角色生成并逐节点复核（Keystone 954/977/980/981，Trigger 935，Transmuter 989/972，Synergy 993） |

### 14.2 实施偏差记录

1. 产品决策：协同节点补齐为 993（29 节点），`min/max_nodes=29`，与计划一致。
2. `991` 元素穿透：运行时无「施法者元素穿透」属性，改为附魔窗口内按剑意层数对命中目标施加对应元素抗性削减（TypeD 契约 + 行为层 debuff，`source_skill_id=0` 以通过 scoped 过滤）；与设计的全局穿透语义为等效近似。
3. 图标 975-993 为旧节点 PNG 占位，非最终美术。
4. 数值近似集中到 `skill_mechanics.json`：附魔近战附加触发（0.3/冻结 1.5s/感电 15%·4s）、973 过载链（0.3 概率/0.4 系数/0.5s ICD）、990 冰增幅等。
5. `990` 仅在 `DamagePipeline::Calculate` 单目标路径生效，`CalculateBatch` 未接入（技能9 伤害当前不经过批量路径）。

### 14.3 验证证据（第 3 轮实跑）

- `./build.bat`（RelWithDebInfo，含 ALL_BUILD 测试目标）：成功。
- `ctest --test-dir build -C RelWithDebInfo -E "nmd.tests.performance|nmd.tests.gpu"`：17/17 套件通过，0 失败；doctest 过滤用例 1439 个。
- `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`：`[OK] skill_contract blocks are up to date.`
- `python scripts/sync_skill_node_icon_ids.py --check`：0 更新 / 76 未变 / 0 缺失。
- `python scripts/gen_asset_registries.py`：`SkillNodeAssetRegistry.hpp` 347 项（+59 行），其余注册表无差异。

### 14.4 残余风险

1. `CalculateBatch` 路径未接入 990 冰增幅（见 14.2-5）。
2. `EffectSystem` 与 `StatsSystem::UpdateBuffs` 均调用 `ActiveEffectsComponent::Update`，存在既有双倍衰减隐患（非技能9 引入，未修改）。
3. 图标占位与 `skill_node_prompts` 生成物需美术后续替换；VFX `MAX_PHANTOM_OVERLAYS` 叠加性能未复核。
4. 跨技能非法标签（技能2/3/6/10）与技能12 绝影共噬节点行为未在技能9 实现中改动。

### 14.5 第 3 轮结论

| 项 | 状态 |
|----|------|
| 第 1 轮 15 条 + 第 2 轮 R1-R2/N1-N5 | 全部闭环（L2 性能项与跨技能项为范围外残余） |
| 构建与测试 | 全绿（17/17 套件，1439 用例） |
| 结论 | **提交**（技能9 实现与审查结论一致；残余风险已记录） |

下一步动作: 技能12 联动回归与跨技能非法标签清理在对应技能审查轮次处理；建议后续在性能工作流中复核绝影 VFX 叠加上限。

---

## 15. 第 4 轮：独立复审发现项修复

独立审查子代理对第 3 轮工作区改动做静态复审（真实核对文件、只读运行 `gen_skill_contracts.py --check`），结论为 `修改`，提出 3 条阻断项与若干建议项；本轮已全部修复并回归。

### 15.1 阻断项修复

| 编号 | 问题 | 修复 |
|------|------|------|
| C-A | 981 逆脉锁血被每帧属性重算复位（`AttributePipeline` 无条件 `hp->max = stats.max_health`），同时使 983 死亡螺旋伤害恒为 0；功能测试因绕过 `StatsSystem::update` 而漏检 | `AttributePipeline.cpp` 同步上限时尊重 `IsDeathSealActive`，保留 `stats.max_health * death_seal_hp_cap_pct`；981/983 用例加入真实帧序 `StatsSystem::update` 断言，983 额外断言螺旋弹 payload 伤害非零 |
| H-A | 980 虚灵之躯的 `PhaseTag` 在下一帧被御剑步相位清理剥离，形态结束后又可能残留 | `SkillSystem.cpp` 相位清理对 `params.void_body && remaining > 0` 的形态豁免；形态结束后由同一清理收回；980 用例改为推进 `SkillSystem::Update` 后断言相位持续/收回 |
| H-B | 973 过载护盾实现在 `OnSkillHit`（输出命中）触发，与设计 §3.9 的「受击」语义相反 | 迁移到 `OnTakeDamage` 处理器：受击者=玩家、反击目标=攻击者，ICD 标记仍挂受击者；新增用例断言命中不触发、受击可触发 |

### 15.2 建议项修复

1. **M-A 982 数值口径**：原 `per_point * points * 0.01` 使收益缩小约两个数量级；改为每点每缺失 1% 提供 1 个百分点（rank4 = 4），`skill_mechanics.json` 键值 4→1，新增暴伤 delta 断言（50% 缺失 × rank4 ⇒ PercentAdd 200）。
2. **M-B 禁疗缺口**：`CombatEventDispatcher`（`life_on_hit`）、`BoomerangDeliverySystem`（815 风眼）、`CombatSystem::ApplyDamage`（生命偷取）三条回血路径接入 `IsDeathSealActive` 门控；984 嗜血/978 浴血按设计在结算期绕过禁疗，保持不变。
3. **L-A**：`TriggerCast` 的 `active_nodes` 位序号与 `PopulateActiveNodesFromSpecialized` 统一为 `node_id % 100`。
4. **L-C**：删除死字段 `PhantomTranceComponent::original_max_hp`。
5. **L-D**：附魔尾附清理仅移除本形态写入的 `GainExtra(Physical→当前元素)`，并在 `enchant_duration=0` 的提前返回路径兜底清理。
6. **M-C 声明更正**：见 14.1 C4/R1/R2 行修正说明。
7. **M1 文档残留**：`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md` 中 `PhantomFlash.cpp`、精确弹反等引用已更新。

### 15.3 验证证据（第 4 轮实跑）

- `./build.bat` RelWithDebInfo：成功（`build/ws10_fix_build3.log`）。
- `ctest --test-dir build -C RelWithDebInfo -E "nmd.tests.performance|nmd.tests.gpu" --output-on-failure`：17/17 套件通过，0 失败（`build/ws10_fix_ctest2.log`）。
- `tests/functional/PhantomTranceNodes.cpp` 新增/加强：981 属性重算 + `life_on_hit` 禁疗、983 非零伤害、980 相位生命周期、973 受击语义、982 数值 delta。
- 契约/图标生成物未在本轮改动，14.3 的 `--check` 结论保持有效。

### 15.4 保留偏差（不阻断）

1. 981+982 满损血时暴伤收益很大（设计原文为线性百分比），如需上限由策划确认。
2. 形态中重施法会重新武装免死/覆盖附魔窗口（设计未规范，L-E）。
3. 免死判定位于 BladeFormation 无敌之前（L-F）。
4. 990 冰增幅仅接入 `DamagePipeline::Calculate`，`CalculateBatch` 未接入（见 14.2-5）。
5. `EffectSystem` 与 `StatsSystem::UpdateBuffs` 双调用 `ActiveEffectsComponent::Update` 为既存隐患（非本轮引入）。

### 15.5 第 4 轮结论

| 项 | 状态 |
|----|------|
| C-A / H-A / H-B 阻断项 | 已修复且有回归测试 |
| M-A / M-B / M-C / L-A / L-C / L-D / M1 | 已修复 |
| 构建与测试 | 全绿（17/17 套件） |
| 结论 | **提交**（第 4 轮残余均为已登记偏差或范围外项） |
