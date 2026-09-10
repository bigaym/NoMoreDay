# 技能8「御剑·回旋」专精树实现审查报告

- 日期: 2026-09-10
- 审查对象: 技能8 御剑·回旋 (Blade Boomerang, skill_id = 8) 专精树实现完全性
- 前序参照: `docs/reviews/2026-09-10-skill7-specialization-nodes-review.md`（格式与审查深度基准）
- 审查轮次: 第 1-2 轮（第 1 轮首次审查；第 2 轮独立核验准确性并增补发现项。技能2-7 均已完成"审查→修复→提交"闭环，技能8 为首个未审技能）

---

## 1. 审查目标

按技能7审查报告的八层框架逐层核对：

| # | 层 | 关注点 |
|---|----|--------|
| 1 | 数据层 | `skills.json` 技能8 条目（29 节点树 + 内嵌契约）与 `skill_8_tree.json` 布局副本 |
| 2 | 契约层 | `skill_contracts_compact.json` 技能8 块与内嵌契约一致性 |
| 3 | 烘焙层 | `SkillSpecializationBaker.cpp` case 8 节点 → 交付参数/标签/触发烘焙 |
| 4 | 行为交付层 | `BladeBoomerang.cpp` DoCast/DoHit 与 `BoomerangDeliverySystem.cpp` 状态机 |
| 5 | 管线结算层 | ProjectileSystem 回旋分支、流血/牵引/接剑消费端、抗性穿透通道 |
| 6 | 跨技能联动层 | 拔血流云(814)×流云刺(技能1)、巨剑共鸣(855)×灵剑决巨剑术(技能3)、御剑接踵(834)×御剑步 |
| 7 | 测试防线 | 单元/集成/矩阵/约束测试对技能8 的覆盖与正确性 |
| 8 | 数据生成 | `skill_mechanics.json` 机制外置、`gen_skill_contracts.py` 降级推断影响 |

## 2. 结论表

| 轮次 | 结论 | 摘要 |
|------|------|------|
| 1 | **修改** | 烘焙层与行为交付层存在**旧版树布局遗留的节点映射灾难性错位**（11 个被处理节点中 10 处语义错绑、18 个节点零实现）；契约层 roles/transmuter/trigger 大面积错绑；数据层存在 3 个**永久不可点**的死锁节点（814/833/835）；基础数值（法耗/冷却）与设计矛盾；存在**双重回旋状态机**架构冲突；Type A 条件穿透无引擎通道；测试防线整体锚定错误语义。结论「修改」，按 Phase 1→4 路线图整改。 |
| 2 | **修改** | 第 2 轮独立核验：第 1 轮 20 条发现**全部可复现**、核心结论成立（另确认 29 节点 `stat_modifiers` 全为空/缺失，通用属性管线无兜底生效）；纠偏 6 处（R1 `min_nodes` 语义、R2 缺失清单补 833/834、R3 H3 证据方法、R4 计数/清单一致性、R5 role 计数口径、R6 流血通道表述）；新增 6 项（N1 Baker 死写扩展、N2 组件零读取字段、N3 节点级互斥字段约束、N4 TypeA 改绑加载期校验约束、N5 组件技能2 化、N6 874/875 OR 语义澄清）。无实现变更，20 条全部仍开启；结论维持「修改」。 |

## 3. 输入

- 设计文档: `设计文档/职业设计草案_剑修.md:507-561`（§3.8 御剑·回旋，29 节点规格）
- 数据层: `assets/data/skills.json:4895-5618`（技能8 主体+内嵌契约）
- 布局副本: `assets/data/skill_8_tree.json:1-365`
- 紧凑契约: `assets/data/skill_contracts_compact.json:330-366`
- 机制外置: `assets/data/skill_mechanics.json`（技能8 零条目）
- 烘焙层: `src/game/systems/skill/SkillSpecializationBaker.cpp:105-110, 811-843, 867-921`
- 行为层: `src/game/systems/skill/behaviors/BladeBoomerang.cpp:1-97`
- 交付层: `src/game/systems/skill/BoomerangDeliverySystem.cpp:1-140`、`src/game/systems/skill/ProjectileSystem.cpp:215-348`、`src/game/systems/skill/SkillSystem.cpp:1276`
- 组件/数据: `src/game/foundation/components/DeliveryArchetypes.hpp:85-86`、`src/game/foundation/data/BuffIds.hpp:24-25`、`src/game/foundation/data/BuffRegistry.cpp:38`
- 跨技能: `src/game/systems/skill/behaviors/FlowingThrust.cpp:355-404`、`src/game/systems/skill/behaviors/RendingWave.cpp:387,392`、`src/game/systems/combat/DamageMitigationService.cpp:205`
- 测试: `tests/functional/SkillBehaviors.cpp:26-65`、`tests/integration/SkillSystemTests.cpp:941-1055`、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp:57-99,142-145,360-372`、`tests/integration/GameplaySystems.cpp:250-282`、`tests/unit/SkillCastConstraintServiceTests.cpp:30-54`、`tests/SkillKeyNodeMatrixTestHelpers.hpp:133,156,295`
- fixture: `tests/fixtures/skill_specialization_keynodes.json:40-41`

## 4. 变更文件边界

`git status`（HEAD = f0340b6e，技能7 修复提交）: 仅 `settings.json` 修改，无技能8 相关未提交变更。本次审查为纯基线审查（无 staged diff 可审），全部发现基于工作区 HEAD 内容。

## 5. 范围对齐

| 设计项 | 数据层 | 烘焙行为层 | 管线系统层 | 结论 |
|--------|--------|-----------|-----------|------|
| 基础: 跟随光标飞剑、最远端折返、双段伤害、穿透 | mana=20✗/CD=4s✗（设计 8/无）、tags 缺 Return/Dexterity✗ | BoomerangProjectile 原型✓，speed 500 硬编码✗ vs params 400 | 双状态机双重计时✗ | **错位** |
| Keystone 滞空切割(810) | max=3✗（应1） | 未实现（默认滞留 0.45s 行为发明） | hover 相位存在但周期切割缺失 | **未实现** |
| Keystone 风眼(815) | role✓ | 未实现 | 无流血→治疗通道 | **未实现** |
| Keystone 巨阙(854) | role✓ | 未实现 | 眩晕逻辑仅技能2 触达（stun_on_apex_end 唯一写点 RendingWave.cpp:392） | **未实现** |
| Trigger 巨剑共鸣(855) | 无契约条目✗（被 831 顶替错绑 trigger_skill_id=3） | 未实现 | SyncTriggerRules 会为错绑 831 写入无暴击要求触发 | **错位** |
| Synergy 拔血流云(814) | role=Keystone✗、prereq 811@4 不可达✗ | 未实现 | 技能1 仅检测流血不拔层；技能8 流血 Buff 通道与技能1 Ailment 通道不互通 | **错位+死锁** |
| 剑意交互 意随剑舞(852)/心剑合一(853) | 852 role✓/max✗、853 无条目 | 852 错绑撕裂 flag、853 未实现 | GainSwordIntent 通道存在但触发条件错 | **错位** |
| Transmuter 劫灰路径(870)/电磁回旋(872) 互斥 | 870 role✓、871 锚绑转质✗、872 标 Passive+TypeA✗、无互斥组✗ | 870 Fire✓、871 错绑 Lightning、872 跳过 | 互斥守卫缺失（对照技能6/9 有 exclusion_groups） | **错位** |
| Type A 条件穿透 灵根破壁(875) | 绑 872✗ | 未实现 | DamageMitigationService.cpp:205 仅 ArmorPenetration，无 TypeA 分支 | **未实现** |
| 元素路径系统 (870/872/873/874/875/876 依赖) | 876 max=5✗ | 未实现 | 无路径实体，`skill_mechanics.json` 零条目 | **未实现** |

## 6. 初审核验与纠偏

1. **「行为层节点常量存在性守卫不防语义错位」**: `tests/integration/GameplaySystems.cpp:250-282` 校验行为文件内节点常量必须存在于数据表——BladeBoomerang.cpp 的 812/813/830/831/832/833/850/851/852/870/871 全部存在于 skills.json（ID 合法），故守卫通过。该防线只查 ID 存在性，无法发现「813 剑鸣被当作幻影回旋掷侧翼剑」这类语义错位，解释了错位为何存活。
2. **「接剑(832) 存在三处独立实现且互不一致」**: ProjectileSystem.cpp:277-296（硬编码 831、回蓝 2/点、CD-0.35s/点）、BoomerangDeliverySystem.cpp:102-116（flag 8 门控、CD-1s、剑意+1）、BladeBoomerang.cpp（无）。设计只有一种：回复 2...6 点法力。
3. **「Baker del.pull_radius 是死代码」**: Baker L819/824/827 设置 100/150/200，但 BladeBoomerang.cpp:76 `bc.pull_radius = (pullStr > 0.0f) ? (p.radius * scale) : 0.0f` 从未读取 del.pull_radius，实际牵引半径恒为 40×scale。
4. **「del.speed=500 / del.range=300 同为死代码」**: 行为层 L39 读 `sd->GetParam("speed", 400)`；delivery.speed/range 无消费点，与 skills.json params（speed 400 / return_timer 0.45）来源分裂。

## 7. 发现项

### Blocker (C)

**C1 — 烘焙层与行为交付层的节点映射与数据层语义灾难性错位（旧版树布局遗留）**

证据: `src/game/systems/skill/behaviors/BladeBoomerang.cpp:20-31` 的 `BladeBoomerangNodes` 常量表与 `src/game/systems/skill/SkillSpecializationBaker.cpp:811-843` 的 case 8 均按**旧版节点排布**编写，而 `skills.json` 已更新为 §3.8 新版 29 节点。逐项对照:

| 节点ID | 数据层语义（§3.8） | 行为层当作 | 错误效果 |
|--------|-------------------|-----------|---------|
| 812 撕裂伤口 | 对流血敌人暴击倍率 +15-60% | BreakAir 类增伤 | more_damage_mult +10%/点，无条件 |
| 813 剑鸣 | 滞留切割音爆→护甲击碎 | PhantomSpin | 点「剑鸣」掷出 2 柄侧翼飞剑（sub_count=2 + flag 2） |
| 830 幻影回旋 | 额外 2 柄侧翼虚影剑 | MagnetField | 点「幻影回旋」无侧翼剑，反而获得磁力牵引 |
| 831 无尽刃舞 | 侧翼偏移角缩小、穿透不衰减 | CatchBlade | flag 8 接剑效果；另被契约错绑 Trigger（见 C2/H1） |
| 832 接剑 | 接住飞剑回复法力 | GravityField | pull_radius=150 + flag 16 |
| 833 连环劲 | 接剑后攻速窗口 | BlackHole | pull_radius=200 + flag 32 |
| 850 磁力场 | 折返途中牵引敌人 | HoverCut | hover_duration=1.0 + flag 64（滞空切割效果） |
| 851 重力网 | 牵引范围/判定体积 | Bleed | flag 128 → DoHit 施加流血 |
| 852 意随剑舞 | 折返命中几率叠剑意 | Tear | flag 256 → DoHit 中目标有流血才 GainSwordIntent（触发条件完全错误） |
| 871 燎原之势 | 燃烧路径火焰 More 增伤 | GuardQi(电磁) | Physical→Lightning 转质 + GuardQi 护体 |
| 870 劫灰路径 | 物理→火焰 + 路径残留 | PathResidue | **唯一绑定正确**（但路径残留未实现） |

为何成问题: 玩家投入的每个专精点都会获得与设计完全无关的效果；正确节点（810/811/812/813/830/831/832/833/834/835/851/852/853/854/855/872/873/874/875/876）零实现。整棵树的"点→效"映射失效。

修复: 按 §3.8 重写 `BladeBoomerangNodes` 常量表与 Baker case 8 逐节点分支（建议在 Phase 2 一并外置数值至 `skill_mechanics.json`）。

**C2 — Transmuter 契约错绑 [870,871] 且无互斥守卫**

证据: `skills.json:5387-5390` 与 `skill_contracts_compact.json:338-341` 的 `transmuter_node_ids: [870, 871]`——871 燎原之势是普通被动（More 增伤+降火抗），真正的 872 电磁回旋在契约里被标为 `Passive` + `resist_model TypeA_Penetration`（`skills.json:5601-5603`）。`max_transmuters: 2` 允许同时点亮两个"转质"，Baker L837-841 会先转 Fire（870）再转 Lightning（871），最终 tag 取决于 allocated_points 遍历顺序；BladeBoomerang.cpp:55-61 的 convTag 判定 Lightning 优先，GuardQi 护体只挂在 Lightning 路径。对照技能6（Baker L167-174 显式互斥守卫）与技能7/9（`keystone_exclusion_groups` 机制），技能8 既无守卫也无互斥组。

为何成问题: 双转质同时点=随机组合效果；872 的"无视地形障碍+自动索敌电弧"彻底丢失；互斥设计（§3.8 明确"与[电磁回旋]互斥"）未被契约表达。

修复: `transmuter_node_ids: [870, 872]`、`max_transmuters: 1` 并配置 `keystone_exclusion_groups {"870": 1, "872": 1}`；同步更新 `tests/unit/SkillCastConstraintServiceTests.cpp:33-54`（该测试锁定 [870,871] 折叠行为）。

**C3 — 数据树死锁：拔血流云(814)、连环劲(833)、回旋游步(835) 永久不可点**

证据: `skills.json` 内 max_points 与 prereq 矛盾——
- 811 放血 `max_points=3`（设计 4），而 814 拔血流云 prereq `{811, required_points 4}`（`skills.json:5051-5056`）→ 814 永不可达；
- 832 接剑 `max_points=1`（设计 3），而 833 连环劲 prereq `{832, required_points 2}`（`skills.json:5127-5132`）→ 833 永不可达；
- 835 回旋游步 prereq `{833, required_points 3}` → 随 833 一并锁死。

为何成问题: 分支 A 的核心 Synergy 节点与分支 B 的后段两个节点在任何加点路径下都无法点亮，玩家专精树出现"悬空出口"。契约 `min_nodes=29/max_nodes=29` 也因此实际不可达成。

修复: 按设计回填 max_points（811→4、832→3），并新增"全 29 节点可点性遍历"回归测试。

### High (H)

**H1 — 巨剑共鸣(855) Trigger 配置缺失且 831 被错绑为假触发**

证据: 设计 855 = Trigger{854@1}，暴击时无消耗触发灵剑决天降巨剑，3s 内置 CD。实际 `skills.json:5544-5556` 中 855 是 `Keystone` 且 trigger 全零；831 却携带 `role=Trigger` + `trigger_skill_id=3, effectiveness=0.5, internal_cooldown=3.0`（`skills.json:5456-5471`）。`SyncTriggerRules`（`SkillSpecializationBaker.cpp:887-919`）对 role==Trigger 且 trigger_skill_id≠0 的节点一律写入 `TriggerRule`（OnSkillHit/Victim/requires_crit=false）——点 1 级「无尽刃舞」即可让每次飞剑命中以 50% 效力无暴击要求施放技能3（御剑术），3s CD。

为何成问题: 真正的 Trigger（855）不可用；无尽刃舞附带设计外的召唤触发，战斗表现错乱。

修复: trigger_nodes 改为 `{node_id: 855, trigger_skill_id: 3, requires_crit: true, internal_cooldown: 3.0}`（effectiveness 按设计确认，可能保留 0.5）；831 恢复 Passive/功能 role。855 的 listen_event 需求与 Baker L900-912 的特判表对齐（当前仅 452/714 有特判，855 若需 OnCrit 命中监听需扩特判或复用 requires_crit 标志）。

**H2 — 六个关键 Keystone/Synergy/Trigger/功能节点零实现**

证据: grep 全 `src/` 中 814/815/854/855/875/876 仅命中 `SkillNodeAssetRegistry.hpp:218-238,554-574,885-905`（贴图注册），无任何逻辑。具体缺口:
- 814 拔血流云: 技能1 `FlowingThrust.cpp:372-401` 有流血检测但无"拔出全部流血层数→爆发性真实伤害"；且技能8 流血走 Buff 通道（`BladeBoomerangBleed`, ActiveEffectsComponent），技能1 流血走 AilmentEngine 通道（`AilmentType::Bleed`, stacks），双通道不互通。
- 815 风眼: 无"本次飞行流血伤害 100% 化治疗波"。
- 854 巨阙: 无巨大化/必打断+硬直/总护甲 5% 附加伤害；`BoomerangDeliverySystem.cpp:64-76` 的 stun_on_apex_end 眩晕分支唯一写点在 `RendingWave.cpp:392`（技能2 Node 233），技能8 永远 false。
- 834 御剑接踵: 无御剑步联动（契约无条目，`affects_sword_step` 全树仅 813 错置 true）。
- 835 回旋游步: 无解控+20% 移速爆发。
- 853 心剑合一: 无剑意→飞行速度/折返速度/判定体积增幅。
- 875 灵根破壁: `resist_models "872": "TypeA_Penetration"` 错绑节点，且 `DamageMitigationService.cpp:205` 仅存在 ArmorPenetration 通道，**引擎无 TypeA_Penetration 处理分支**——即使改绑 875 也无法结算，需先补抗性削弱通道（同技能7 报告的 TypeE 缺失问题）。
- 876 元素护体: 行为被错绑在 871（`BladeBoomerang.cpp:57-60` GuardQi = +10 ResistAll 2s，仅 Lightning），设计为站上元素路径获 15% 对应元素绝对减免、火雷双向。

**H3 — 元素路径系统整体缺失（分支 D 依赖地基）**

证据: `skill_mechanics.json` 中技能8 零条目（grep `"skill_id": 8` 无命中；`base_range` 键仅技能7 使用）。870 只做 tag 转化（Baker L837），无燃烧路径残留实体、无电弧索敌、无尾迹宽度/持续、无路径判定。871/872/873/874/875/876 六个节点全部依赖不存在的路径系统。

**H4 — 流血机制与设计不符且完全无伤害**

证据: 设计 811 放血 = 每次击中 25-100% 几率施加**一层**流血（叠层 DoT）。实际 `BladeBoomerang.cpp:88-91`: DoHit 中无几率判定、100% 施加、`AddOrRefresh`（刷新不叠层），modifiers 仅 `-10% MoveSpeed`——未挂载任何伤害 modifier（`BuffRegistry.cpp:38` 通用流血注册含 `flat_health` 模板但未被该 buff 使用）→ 技能8 流血零伤害纯减速，且层数概念缺失，撕裂伤口(812)/拔血流云(814)/风眼(815) 依赖的流血结算系统不存在。

**H5 — 双重回旋状态机冲突（架构级，波及技能2）**

证据: `BoomerangDeliverySystem.cpp:28-131`（`SkillSystem.cpp:1276` 调用）与 `ProjectileSystem.cpp:229-315`（SimulateProjectile lambda）**同时**消费 `BoomerangComponent`:
- Outward: 两处 `returnTimer -= dt` → 相位计时双倍流逝；
- HoverApex: 仅 BoomerangDeliverySystem 有牵引+眩晕（ProjectileSystem:249-255 只推进相位）；
- Returning 接剑阈值双实现（`kReturnCatchThreshold 32.0f` vs `PROJECTILE_RETURN_THRESHOLD`）：ProjectileSystem.cpp:277-296 回蓝+CD-0.35/点、BoomerangDeliverySystem.cpp:102-116 CD-1s+剑意+1 → 接住一柄飞剑两套效果叠加；
- returnTarget 失效时 ProjectileSystem.cpp:313 回退 Outward（潜在永动飞剑）vs BoomerangDeliverySystem.cpp:126 销毁。

技能2（RendingWave 230-235）共用该组件，同样受双重状态机影响。修复需选定单一权威状态机，接剑效果收敛到节点逻辑层。

**H6 — max_points 18/29 处偏离设计**

证据（设计→实际）: 800(4→5) 801(4→5) 803(4→5) 810(1→3) 811(4→3) 812(4→5) 813(3→5) 831(3→5) 832(3→1) 833(3→5) 851(4→5) 852(3→5) 870(1→5) 871(3→5) 872(1→5) 874(3→5) 875(4→5) 876(1→5)。Keystone/Transmuter max≠1 破坏单点定位语义。正确 11 处: 802/814/815/830/834/835/850/853/854/855/873。

**H7 — 磁力牵引行为错位**

证据: 设计 850 = **折返途中**牵引沿途轻/中型敌人。实际牵引仅 HoverApex 滞留阶段生效（`BoomerangDeliverySystem.cpp:46-61`），Returning 相位无牵引；不区分轻/中型（`any_of<EnemyTag>` 全量牵引）；牵引强度三档硬编码（300/500/×2，`BladeBoomerang.cpp:51-54`）为行为发明，设计无"黑洞/重力场"梯度。

### Medium (M)

**M1 — 基础数值与文案偏离**: `skills.json:4898-4899` mana_cost=20（设计 8）、cooldown=4.0s（设计无冷却）；`desc_key`「掷出飞剑并在一段距离后折返。」丢失"穿透敌人/双段伤害"关键信息（对照技能2-7 修复后均回填完整设计文案）。

**M2 — tags 缺失与大小写**: `skills.json:4900-4906` tags = [Physical, Attack, Projectile, Hit, sword_skill]，缺 [Return]、缺 [Dexterity]；`sword_skill` 小写与技能1 `SwordSkill` 大小写不一致（技能7 报告已确认 tags 匹配为大小写敏感使用）。

**M3 — 滞空切割语义偏离**: 设计仅 810 使飞剑最远端滞留 0.8s 且每 0.2s 周期切割，无 Keystone 时"立刻折返"。实际默认 `hover_duration=0.45s`（`BladeBoomerang.cpp:75`），无周期切割结算；850（错位映射）给 1.0s。折返时机为计时语义（returnTimer 0.45s），设计为最远端距离语义——`del.range=300` 死代码，"疾速(801) 等比例提高最远距离"无从挂接。

**M4 — 契约结构缺失与错绑**: 契约仅 14/29 节点（缺 800/801/802/803/810/811/812/851/853/873/874/875/876）；无 `keystone_node_ids`（触发 `gen_skill_contracts.py:432` 对 max_points==1 的降级推断，正是 Keystone 大面积错标 814/832/835/850/855 的机制来源）；`synergy_node_ids [830]`（应 814）、`sword_step_node_ids [813]`（应 834）、`resist_models/scope_policies` 绑 872（应 875）。`sword_intent_node_ids [852]` 正确。

**M5 — 侧翼飞剑参数偏差**: 设计单发基础伤害 -30%，实际 `p_scale=0.6`（-40%，`BladeBoomerang.cpp:78`）；偏移角 ±0.25 rad 硬编码，831 的偏移角缩小与穿透不衰减未实现（穿透当前默认 `kUnlimitedPiercing` 无衰减，部分等效成立）。

### Low (L)

**L1 — 机制键未外置**: 滞留时长/切割间隔/牵引强度/侧翼偏移角/流血几率/接剑回蓝等全部硬编码行为层，`skill_mechanics.json` 无技能8 键。
**L2 — Baker 硬编码与 params 冲突**: speed 500/range 300（Baker L107-108）vs params speed 400，来源分裂且后者为死代码。
**L3 — 无 profile 时接剑宽松回退**: `BoomerangDeliverySystem.cpp:105` `: true`——未专精 832 也默认有接剑返还。
**L4 — 功能测试断言恒真**: `tests/functional/SkillBehaviors.cpp:59` `CHECK(proj.snapshot.damage_multipliers[0] >= 1.0f)`（812=3 点应为 1.3，1.0 起步无区分度）。
**L5 — 测试覆盖缺口**: 815/814/854/855/834/835/853/873/874/875/876 零测试；既有测试（SkillSystemTests.cpp:944,1034 / SkillKeyNodeMatrixIntegrationTests.cpp:366 / SkillBehaviors.cpp:41）锚定错误旧映射，属"测试锁定错误实现"。

## 8. 逐节点核对矩阵

| 节点 | 设计名 | 设计max | 实际max | 设计prereq | 实际prereq | 契约role(实际) | 实现状态 | 缺陷摘要 |
|------|--------|---------|---------|------------|------------|----------------|----------|----------|
| 800 | 轻巧 | 0/4 | 5 ✗ | 根 | 根 ✓ | （缺） | 未实现 | 法耗/攻速无实现 |
| 801 | 疾速 | 0/4 | 5 ✗ | 根 | 根 ✓ | （缺） | 未实现 | 飞速/最远距离无实现 |
| 802 | 锋锐 | 0/5 | 5 ✓ | 根 | 根 ✓ | （缺） | 未实现 | 点伤/暴击率无实现 |
| 803 | 回力感应 | 0/4 | 5 ✗ | 根 | 根 ✓ | （缺） | 未实现 | returning_damage_mult 唯一写点在技能2 |
| 810 | 滞空切割 K | 0/1 | 3 ✗ | 801@3 | 801@3 ✓ | （缺） | 未实现 | 默认滞留 0.45s 行为发明，无周期切割 |
| 811 | 放血 | 0/4 | 3 ✗ | 810@1 | 810@1 ✓ | （缺） | 错位 | 流血行为被绑到 851；无几率/层数/伤害 |
| 812 | 撕裂伤口 | 0/4 | 5 ✗ | 811@2 | 811@2 ✓ | （缺） | 错位 | 被当作无条件 more_damage |
| 813 | 剑鸣 | 0/3 | 5 ✗ | 810@1 | 810@1 ✓ | Passive + step✗ | 错位 | 被当作幻影回旋掷侧翼剑；affects_sword_step 误置 |
| 814 | 拔血流云 S | 0/1 | 1 ✓ | 811@4 | 811@4 ✗不可达 | Keystone ✗ | 未实现+死锁 | 双流血通道不互通 |
| 815 | 风眼 K | 0/1 | 1 ✓ | 810@1 | 810@1 ✓ | Keystone ✓ | 未实现 | 无流血→治疗 |
| 830 | 幻影回旋 | 0/1 | 1 ✓ | 800@3 | 800@3 ✓ | Synergy ✗ | 错位 | 被当作磁力牵引 |
| 831 | 无尽刃舞 | 0/3 | 5 ✗ | 830@1 | 830@1 ✓ | Trigger ✗(绑3,eff0.5,icd3,无crit) | 错位+假触发 | 被当作接剑；每次命中以 50% 效力施放技能3 |
| 832 | 接剑 | 0/3 | 1 ✗ | 802@2 | 802@2 ✓ | Keystone ✗ | 三处不一致 | 回蓝/CD/剑意三实现互相矛盾 |
| 833 | 连环劲 | 0/3 | 5 ✗ | 832@2 | 832@2 ✗不可达 | （缺） | 未实现+死锁 | 攻速窗口无实现 |
| 834 | 御剑接踵 | 0/3 | 3 ✓ | 832@1 | 832@1 ✓ | （缺） | 未实现 | 无御剑步联动 |
| 835 | 回旋游步 | 0/1 | 1 ✓ | 833@3 | 833@3 ✗不可达 | Keystone ✗ | 未实现+死锁 | 无解控/移速爆发 |
| 850 | 磁力场 | 0/1 | 1 ✓ | 802@4 | 802@4 ✓ | Keystone ✗ | 错位 | 被当作滞空切割；牵引相位错 |
| 851 | 重力网 | 0/4 | 5 ✗ | 850@1 | 850@1 ✓ | （缺） | 错位 | 被当作放血 |
| 852 | 意随剑舞 | 0/3 | 5 ✗ | 850@1 | 850@1 ✓ | Passive + intent ✓ | 错位 | 被当作撕裂；剑意触发条件错 |
| 853 | 心剑合一 | 0/3 | 3 ✓ | 852@2 | 852@2 ✓ | （缺） | 未实现 | 无剑意→属性 |
| 854 | 巨阙 K | 0/1 | 1 ✓ | 830@1 | 830@1 ✓ | Keystone ✓ | 未实现 | 无巨大化/打断/护甲附加伤害 |
| 855 | 巨剑共鸣 T | 0/1 | 1 ✓ | 854@1 | 854@1 ✓ | Keystone ✗无trigger | 未实现 | 触发配置被错放 831 |
| 870 | 劫灰路径 T | 0/1 | 5 ✗ | 803@2 | 803@2 ✓ | Transmuter ✓ | 部分实现 | tag 转化✓；路径残留未实现 |
| 871 | 燎原之势 | 0/3 | 5 ✗ | 870@1 | 870@1 ✓ | Transmuter ✗ | 错位 | 被当作电磁转质+护体 |
| 872 | 电磁回旋 T | 0/1 | 5 ✗ | 803@2 | 803@2 ✓ | Passive ✗ + TypeA✗ | 跳过 | 转质/无视地形/电弧全缺 |
| 873 | 高压电弧 | 0/3 | 3 ✓ | 872@1 | 872@1 ✓ | （缺） | 未实现 | 电弧/电磁爆发全缺 |
| 874 | 元素尾迹 | 0/3 | 5 ✗ | 870∨872@1 | 870∨872@1 ✓ | （缺） | 未实现 | 依赖不存在的路径系统 |
| 875 | 灵根破壁 | 0/4 | 5 ✗ | 870∨872@1 | 870∨872@1 ✓ | （缺） | 未实现 | TypeA_Penetration 无引擎通道 |
| 876 | 元素护体 | 0/1 | 5 ✗ | 874@1 | 874@1 ✓ | （缺） | 错位 | 行为在 871 且仅雷路径/数值不符 |

统计: prereq 29/29 正确；max_points 11/29 正确；契约 roles 4/14 正确（815/852/854/870）；行为映射 1/11 正确（870）；可点性 26/29（814/833/835 死锁）；实现完全度 1/29（870 部分实现）。

## 9. 架构与跨系统偏差

1. **回旋交付双状态机**: BoomerangDeliverySystem（Phase 2 通用交付）与 ProjectileSystem 内嵌回旋分支并行运行，同一 `BoomerangComponent` 被双重驱动。技能2 也受影响，修复时必须以单一状态机为权威并回归技能2 用例（`tests/integration/RendingWaveNodes.cpp:275-326`、`SkillSpecializationBakerTests.cpp:58-70,221`）。
2. **流血双通道**: 技能1 走 `AilmentEngine`（AilmentType::Bleed/stacks/magnitude），技能8 走 `ActiveEffectsComponent` Buff（BladeBoomerangBleed）。拔血流云(814) 要求跨技能读层数——统一到 AilmentEngine 通道是唯一可行解（FlowingThrust.cpp:378 的 `"Bleed"` 字符串匹配已隐含此意图）。
3. **抗性削弱通道缺口**: `DamageMitigationService.cpp` 无 TypeA_Penetration 分支（技能7 报告亦记录 TypeE 缺失）。875 灵根破壁落地前置条件是补齐 TypeA（条件式、仅限本技能）通道。
4. **契约双份存储**: `skills.json` 内嵌契约与 `skill_contracts_compact.json` 并存，技能8 两份当前一致但同步错误；修复时应由 `gen_skill_contracts.py` 单源再生成，并显式提供 `keystone_node_ids: [810, 815, 854]` 避免降级推断。
5. **触发特判表**: `SyncTriggerRules` L900-912 仅对 452/714 特判事件类型。855 巨剑共鸣（需暴击 + 装备灵剑决巨剑术门控）需要新增特判或通用 `requires_crit` + `required_source_node` 语义扩展；"装备了【巨剑术】节点"的前置校验当前契约模型无对应字段。

## 10. 整改落地路线图

**Phase 1 — 数据契约（先行，无代码风险）**
1. `skills.json`: mana_cost→8、cooldown→0、desc_key 补全设计文案、tags 补 Return/Dexterity、`sword_skill` 大小写统一；18 处 max_points 回填（811→4、832→3 解除死锁；810/870/872→1；其余按设计）。
2. 重写契约: keystone_node_ids=[810,815,854]、transmuter_node_ids=[870,872]+max_transmuters=1+互斥组、synergy=[814]、sword_intent=[852]、sword_step=[834]、resist_models/scope_policies 绑 875、trigger_nodes={855, skill 3, requires_crit, icd 3.0}；补齐 15 缺失条目（800/801/802/803/810/811/812/851/853/873/874/875/876）。
3. 重生成 `skill_contracts_compact.json`；更新 `tests/fixtures/skill_specialization_keynodes.json:41`（技能8 key_nodes 按 §3.8 重选）与 `SkillCastConstraintServiceTests.cpp:33-54`。

**Phase 2 — 烘焙与行为层重写**
4. 重写 `BladeBoomerangNodes` 常量映射与 `BladeBoomerang.cpp` DoCast/DoHit 到新节点语义。
5. 重写 Baker case 8（`SkillSpecializationBaker.cpp:811-843`）: 800-803 数值、810 滞空+周期切割、811 几率流血、812 条件暴伤、813 音爆护甲击碎、830 侧翼剑、831 角度/穿透、832 接剑回蓝、833 攻速窗口、834 御剑步联动、835 解控、850-853 磁力/剑意、854 巨阙、855 触发、870-876 转质/路径/穿透/护体。
6. 清除死代码（del.pull_radius/del.speed/del.range 未消费问题）并对齐单一参数来源。

**Phase 3 — 管线与跨技能**
7. 收敛回旋状态机（保留 ProjectileSystem 或 BoomerangDeliverySystem 之一为权威），接剑效果统一，回归技能2。
8. 流血统一 AilmentEngine 通道（叠层+DoT 结算），在 FlowingThrust 消费端实现 814 拔层数爆发、815 风眼治疗波。
9. 实现 855 巨剑共鸣触发（requires_crit + 装备巨剑术校验）、834 御剑步交互、853 心剑合一属性增幅。
10. `DamageMitigationService` 补 TypeA_Penetration（条件式、SkillOnly），落地 875。
11. 元素路径实体系统（燃烧路径/电弧/尾迹/护体判定）+ `skill_mechanics.json` 技能8 机制键外置。

**Phase 4 — 测试防线**
12. 修正 `SkillBehaviors.cpp:59` 恒真断言（812 语义修正后改断 1.3）；补 29 节点逐点行为测试。
13. 更新 `SkillSystemTests.cpp:943-944,1018-1055`（813/831 旧映射用例）与矩阵测试 GuardQi 断言（改绑 876/872 语义）；新增全节点可点性遍历（死锁回归）。
14. 构建验证: `build.bat` RelWithDebInfo 全量编译 + 现有测试套件全绿。

## 11. 剩余风险

1. **灵剑决↔巨剑术对应关系待设计确认**: 855 触发的"天降巨剑"具体技能形态/效力系数需与 §3.3（技能3 御剑术）及 330 巨剑术节点的设计意图对齐；当前 trigger_skill_id=3 的 0.5 效力值是旧数据遗留，需在 Phase 2 前由设计确认。
2. **双状态机重构波及面**: 技能2 的 Boomerang 用例（230-235）与 ProjectileSystem 广泛交互，收敛状态机可能暴露隐藏的时序依赖，建议单独提交并附技能2 回归证据。
3. **互斥组机制复用**: 技能6/9 用 `keystone_exclusion_groups` 表达转质互斥，技能8 沿用即可，但 `ValidateContractCastConstraints` 的"折叠到首选节点"语义与互斥组语义并存时的优先级需在 Phase 1 明确（避免回归技能6 修复）。
4. **Type A 通道设计待定**: 条件穿透"仅限本技能+命中路径内敌人才生效"的作用域判定需与 DamageMitigationService 现有 SkillOnly 机制核对实现成本；若引擎改造超期，需向设计侧提出降级方案再实施。
5. **契约 29 节点全量补齐后的回归面**: 契约节点从 14 扩到 29 会改变 `ValidateContractCastConstraints` 的校验面，`SkillCastConstraintServiceTests` 现有用例需全部重跑。

---

## 12. 第 2 轮核验（准确性验证与新增发现）

- 本轮变更内容: 无实现变更，为对第 1 轮报告的独立复核：逐条复读代码/数据并验证 20 条发现（C1-C3/H1-H7/M1-M5/L1-L5），同时排查第 1 轮未覆盖的消费端与字段。
- 复查范围: `assets/data/skills.json:4895-5618`、`assets/data/skill_8_tree.json`、`SkillSpecializationBaker.cpp`（case 8 / 触发 / 节点常量）、`BladeBoomerang.cpp`、`BoomerangDeliverySystem.cpp`、`ProjectileSystem.cpp:215-348`、`SkillRegistry.cpp`（契约解析 / 加载校验）、`SkillSystem.cpp`（前置 / 互斥 / 转质折叠）、`StatsSystem.cpp`（节点属性 / 裁剪）、`FlowingThrust.cpp`、`RendingWave.cpp`、`BuffRegistry.cpp`、`DamageMitigationService.cpp`、`scripts/gen_skill_contracts.py` 及第 1 轮引用的全部测试。
- 第 1 轮发现项状态: 本轮无实现变更，20 条**全部仍开启**，无已解决项。

### 12.1 逐条核验判定

| 编号 | 判定 | 关键复核证据 |
|------|------|--------------|
| C1 | **成立** | `BladeBoomerang.cpp:20-31` 常量表、`SkillSpecializationBaker.cpp:811-843` case 8、行为层消费点（`:48` flag2→侧翼、`:49` flag8→接剑、`:50` flag64→滞留、`:52-54` 牵引三档、`:57-61` 871 转雷+GuardQi）逐项复现；另用数据脚本确认全 29 节点 `stat_modifiers` 为空或缺失（800/803/812/813/831/833/851/852/870-872/874-876 为 `[]`，其余字段缺失），通用属性管线无任何兜底生效，各节点确为零实现/错绑。 |
| C2 | **成立（补充折叠细节）** | 契约约束阶段会把已点亮转质折叠为 `transmuter_node_ids` 首选（870）并写入 `SkillContractRuntimeComponent::active_transmuter_node_by_skill`（`SkillSystem.cpp:2006-2011`），但该记录仅被 `StatsSystem.cpp:469-478` 用于跳过未选转质的**节点属性**；技能8 转质节点属性为空，无法纠正 tag——tag 仍由 Baker 双烘焙顺序决定，结论不变。 |
| C3 | **成立（表述纠偏见 R1）** | 三处死锁复现：811 max=3 vs 814 req4、832 max=1 vs 833 req2、835 req 833@3。 |
| H1 | **成立** | 855 契约 Keystone trigger 全零；831 `role=Trigger` + `trigger_skill_id=3,eff=0.5,icd=3.0,requires_crit=false`；`SyncTriggerRules`（`SkillSpecializationBaker.cpp:887-920`）默认 `OnSkillHit/Victim` 且 `requires_crit` 取契约值。 |
| H2 | **成立（计数纠偏见 R4、表述纠偏见 R6）** | 8 项关键节点缺口逐项复现；`RendingWave.cpp:392` 为 `stun_on_apex_end` 唯一写点；契约 `affects_sword_step` 全 14 条中仅 813 为 true。 |
| H3 | **成立（证据方法纠偏见 R3）** | `skill_mechanics.json` 顶层键仅 `"1"`..`"7"`，无 `"8"`；技能8 无机制键，870/872/873/874/875/876 依赖的路径系统不存在。 |
| H4 | **成立** | `BladeBoomerang.cpp:88-91` 100% 施加、`AddOrRefresh`、仅 `-10% MoveSpeed`；另确认 ActiveEffects 通道无流血 DoT 消费端——`StatsSystem.cpp:540-598` 仅对 Freeze/Burn/Stun/Shock 出粒子，`BuffType::Bleed` 逻辑仅存在于 `AilmentEngine.cpp:140/410/446/475`（另一数据栈）。 |
| H5 | **成立（补充）** | 双状态机同时消费 `BoomerangComponent`；接剑双实现叠加；失效回退 `ProjectileSystem.cpp:313`（回 Outward）vs `BoomerangDeliverySystem.cpp:126`（销毁）；另确认 Delivery 返还带 `bc.skill_id != 2` 排除技能2（`:103`）。 |
| H6 | **成立** | 18 处偏差与 11 处正确逐项无误。 |
| H7 | **成立** | 牵引仅 HoverApex 生效（`BoomerangDeliverySystem.cpp:46-61`），Returning 无牵引；`any_of<EnemyTag>` 全量；强度 300/500/×2 硬编码。 |
| M1 | **成立** | `mana_cost=20`/`cooldown=4.0` 复现；800-803 无属性兜底（见 C1 行），基础数值与文案偏差均成立。 |
| M2 | **成立** | 技能1 tags 含 `SwordSkill`（`skills.json:15`）确认大小写基准；技能8 缺 `Return`/`Dexterity`、`sword_skill` 小写。 |
| M3 | **成立** | 默认 `hover_duration=0.45s`、无周期切割；折返为 `return_timer` 计时语义；`del.range=300` 无消费。 |
| M4 | **成立（清单漏项纠偏见 R2）** | 契约实有 14 条；`gen_skill_contracts.py:432` 的 `max_points==1` 降级推断已复核。 |
| M5 | **成立** | `p_scale=0.6`、偏移角 ±0.25 rad 硬编码；`kUnlimitedPiercing` 等穿透现状复现。 |
| L1 | **成立** | 滞留/切割/牵引/偏移角/流血几率/接剑回蓝全硬编码，`skill_mechanics.json` 无技能8 键。 |
| L2 | **成立** | `del.speed=500`/`del.range=300` vs params `speed=400`，后者无消费。 |
| L3 | **成立** | `BoomerangDeliverySystem.cpp:105` 无 profile 时 `: true`。 |
| L4 | **成立** | `SkillBehaviors.cpp:59` 断言 `>= 1.0f`，812=3 点无区分度。 |
| L5 | **成立** | 测试锚点行号全部复现：`SkillSystemTests.cpp:941-944`（813 注释 "Boomerang split"）、`:982-1000`、`:1018-1054`（831=2 走 ProjectileSystem 回蓝/冷却断言）；`SkillKeyNodeMatrixIntegrationTests.cpp:142-144`（case8 要求 3 发）、`:360-372`（871→GuardQi）；fixture `:40-41` key_nodes `[813,830,831,852,870,871]`。 |

### 12.2 报告纠偏项（对第 1 轮表述的修正）

**R1（Medium）C3 对 `min_nodes/max_nodes` 的表述错误**
观测: 第 1 轮原文「契约 `min_nodes=29/max_nodes=29` 也因此实际不可达成」。实际 `SkillRegistry.cpp:235-236` 将 min/max 默认为树节点数，`:457-465` 仅在加载时比较 `tree->nodes.size()` 是否落在区间内，与节点可点/可达数量无关。
为何成问题: 该字段是数据一致性校验，不表达"29 点全可点"；按原文验收会误以为需要改契约数值。
修复: 改为「契约声明的 29 节点树中有 814/833/835 三个节点不可点」；契约字段本身无需修改。

**R2（Medium）M4 与 Phase 1 的缺失节点清单漏列 833/834**
观测: 契约实有 14 条（813/814/815/830/831/832/835/850/852/854/855/870/871/872），缺失 15 条；第 1 轮 M4 与 Phase 1 第 2 条均只列 13 条（漏 833/834），且 Phase 1 文案"补齐 15 缺失条目"数字与清单不符。
为何成问题: 833/834 位于 814（Synergy）与 835 的前置链上，漏补会让 Phase 1 后 814 前置链仍无契约条目。
修复: 清单补入 833、834，即 800/801/802/803/810/811/812/833/834/851/853/873/874/875/876。

**R3（Low）H3 的证据方法无效**
观测: 原文以「grep `"skill_id": 8` 无命中」证明零条目；`skill_mechanics.json` 是技能 id 顶层键 schema（`"1"`..`"7"`），不存在 `skill_id` 字段，任意技能用该方法都会"无命中"。
为何成问题: 证据不成立会削弱 H3 论证（实际结论成立）。
修复: 改为「顶层键仅 `"1"`..`"7"`，无 `"8"`（`"9"` 亦缺）」。

**R4（Low）H2 与 C1 的计数/清单不一致**
观测: H2 标题「六个关键节点零实现」但正文列 8 项（814/815/854/834/835/853/875/876）；C1 正文「18 个节点零实现」后括注了 20 个节点 ID。
为何成问题: 数字与清单不一致影响验收清单可信度。
修复: H2 标题改"八个"；C1 括注改为「其余 18 个未处理节点：800/801/802/803/810/811/814/815/834/835/853/854/855/872/873/874/875/876」。

**R5（Low）第 8 节 roles 正确数口径**
观测: 统计写「契约 roles 4/14 正确（815/852/854/870）」，把 813 排除；但 813 的 `role=Passive` 本身符合设计，错误在 `affects_sword_step=true`。
为何成问题: role 字段与 role+flags 两种口径混用，会让读者误以为 813 role 也错。
修复: 改为「role 字段正确 5/14（813/815/852/854/870）；若计入 flags 则 4/14」。

**R6（Low）H2 中「双流血通道不互通」需精确化**
观测: `FlowingThrust.cpp:372-388` 对目标流血状态的检测同时匹配 id 子串与 `BuffType::Bleed`；技能8 施加的 buff `id="blade_boomerang_bleed"`（小写）虽不匹配 `"Bleed"` 子串，但可被 `b.type == BuffType::Bleed` 分支识别。
为何成问题: "检测不互通"表述过强；实际问题是技能8 侧无叠层、无伤害结算。
修复: 表述改为「检测侧可经 `BuffType` 识别技能8 流血；层数、伤害结算与 814 拔层所需的 AilmentEngine 数据模型不互通」。

### 12.3 新增发现项

**N1（Medium）Baker case 8 死写范围大于第 1 轮记录**
观测: 除已记录的 `del.pull_radius`（`SkillSpecializationBaker.cpp:819/824/827`）、`del.speed`（`:107`）、`del.range`（`:108`）外，`:109` `del.duration=0.3f`、`:830` `del.duration=1.5f`、`:816` `del.sub_count=2` 亦无消费端：`BladeBoomerang.cpp` 仅读取 `profile->delivery.feature_flags/effective_tags/more_damage_mult`（`:44-47`）与 `skills.json` params（`:39`），侧翼数量由 flag 2 推导（`:48`）。
为何成问题: 死写会让人误以为数值已接线；Phase 2 重写若照抄会产生新的死代码。
修复: Phase 2 重写时同步决定这些字段接线或删除，并保留单一参数来源。

**N2（Medium）BoomerangComponent 存在零读取字段**
观测: 全 `src/game` 检索仅见声明与写入、无读取：`DeliveryArchetypes.hpp:72` `max_distance`、`:75` `return_speed_accel`；另 `SkillDefs.hpp:571` `primary_archetype` 由 Baker 多处写入但运行期无消费。
为何成问题: 与 L2 同类；"最远端距离"（801/810）若接 `max_distance` 才有数据落点，否则回旋镖永远依赖计时语义（M3）。
修复: Phase 2/3 判定：接线 `max_distance` 支撑 801/810 设计语义，或删除字段并移除 Baker 写入。

**N3（Medium，修复约束）转质互斥必须落"节点级"字段**
观测: 运行时互斥消费 `node_contract->keystone_exclusion_group`（`SkillSystem.cpp:2119-2142`；字段定义 `SkillContract.hpp:57`；由 `SkillRegistry.cpp:370-373` 从 skills.json 节点读取）。紧凑契约的 `keystone_exclusion_groups` 仅供 `gen_skill_contracts.py` 与测试辅助。
为何成问题: 若只改 `skill_contracts_compact.json` 与 `transmuter_node_ids` 而不给 skills.json 的 870/872 节点加 `keystone_exclusion_group`，C2 的互斥依旧不生效。
修复: Phase 1 同步在 skills.json 节点写 `keystone_exclusion_group`（参考技能7 `skills.json:4856/4873` 写法）。

**N4（High，修复约束）TypeA 改绑会触发加载期校验，必须与 875 绑定同批**
观测: `SkillRegistry.cpp:566-617` 的 `ValidateBladeAscendantResistCoverage` 要求技能1-9 契约中至少存在一个 `TypeA_Penetration` 桶；当前全树仅技能8 错绑在 872 上提供该桶。若修复时先清空 872 的 `resist_model` 而未同批把 TypeA 绑到 875，`LoadFromJson` 会因缺失桶而校验失败。
为何成问题: 属原子提交约束；拆开改会导致整个 `skills.json` 加载失败。
修复: 875 与 872 的 `resist_model/scope_policy` 调整必须同批落地，并跑 `SkillRegistry` 加载用例。契约先绑 875、引擎 `TypeA_Penetration` 分支在 Phase 3 补齐不冲突（加载校验只要求桶存在）。

**N5（Low）BoomerangComponent 字段注释为技能2语义**
观测: `DeliveryArchetypes.hpp:85-86` 字段注释只标注技能2 节点（Node230/231/233）；技能8 需要的折返伤害增幅（803）、路径残留、侧翼剑在组件中无对应字段。
为何成问题: 支持 H5/第 9 节判断——共享组件 schema 面向技能2 设计，是双状态机冲突的结构性根源之一。
修复: 收敛状态机时（Phase 3）同步定义技能8 所需字段或复用语义，避免再以行为层 flag 补丁承载。

**N6（Low，核验澄清，非缺陷）874/875 双前置与设计一致**
观测: 874/875 前置同时含 870@1 与 872@1，但前置判定为 OR 语义（`SkillSystem.cpp:2094-2115` 满足其一即 `prereq_satisfied=true`；UI 侧 `IsPrerequisiteSatisfiedOr` 同名），与设计"任意 Transmuter"一致；第 1 轮矩阵标注 ✓ 正确。
为何成问题: 无需修复；记录以防修复者误把双前置当作新缺陷改动。
修复: 无（保持现状）。

### 12.4 未独立核验项（保留为残余风险）

1. H2 中「814/815/854/855/875/876 仅命中 `SkillNodeAssetRegistry.hpp:218-238,554-574,885-905`」未逐项重跑 grep（本轮以行为/契约/组件证据交叉验证）。
2. 双接剑阈值 `PROJECTILE_RETURN_THRESHOLD` 与 `kReturnCatchThreshold(32.0f)` 的常量定义未取，数值后果未量化。
3. TypeA_Penetration 引擎通道的改造工作量未评估（承接第 1 轮剩余风险 4）。

### 12.5 第 2 轮结论

| 项 | 状态 |
|----|------|
| 第 1 轮 20 条发现 | 全部可复现、仍开启（无实现变更） |
| 纠偏项 R1-R6 | 新增，应在验收前修订报告表述（不影响结论方向） |
| 新增发现 N1-N6 | 新增；N3/N4 为 Phase 1 原子约束，N1/N2/N5 为 Phase 2/3 输入，N6 为核验澄清 |
| 结论 | **修改**（维持第 1 轮判定；实现零变化） |

下一步动作: 按第 1 轮 Phase 1→4 路线图整改；Phase 1 数据契约修复时须同时满足 N3（节点级互斥字段）与 N4（TypeA 绑定 875 与解绑 872 同批）。

---

## 13. 第 3 轮跟进审查（修复包实施核验）

- **审查目标**: 技能8「御剑·回旋」专精节点修复包（工作流 A Baker+行为层、B 回旋镖状态机、C 元素路径、D 跨技能联动 + 主线程数据/契约/测试修复）的最终实施审查。
- **结论**: **修改**
- **审查轮次**: 跟进审查（本文件第 3 轮；第 1 轮初审见 §7-§11，第 2 轮准确性核验见 §12）。

### 13.1 输入

1. 设计: `设计文档/职业设计草案_剑修.md` §3.8（:507-561，技能8 29 节点语义）。
2. 实施计划: `docs/plans/2026-09-10-skill8-specialization-nodes-implementation-plan.md`（架构决策 1-7、Phase 1 数据、WS A-D、Phase 4 验证）。
3. 审查标准: `docs/workflows/review.md`（本轮按其「报告必填字段」「发现项格式」「硬否决规则」执行）。
4. 前两轮报告: 本文件 §7-§12（20 条发现 + R1-R6 纠偏 + N1-N6）。
5. 数据与契约: `assets/data/skills.json`（技能8 `:4894-5633`）、`assets/data/skill_contracts_compact.json`、`assets/data/skill_mechanics.json`（技能8 块 `:727-757`）。
6. 验证证据（本次复查）:
   - `build\ws8_build6.log`（2026-09-10 18:21，RelWithDebInfo；legacy reintroduction gate、module boundaries、assets、build 全部通过）。
   - `python scripts\gen_skill_contracts.py --check` 本次重跑输出 `[OK] skill_contract blocks are up to date.`（EXIT=0）。
   - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` 本次重跑 1/1 passed，7.55s（EXIT=0）。
   - `bin\NoMoreDayTests.exe --success=false` 本次重跑：1506 cases / 129030 assertions / **2 failed**（详见 M3-4；与实施方提供的“0 失败”声明不符）。
   - `git status --short` 本次复核：42 行（34 modified + 8 untracked），与用户给定边界一致。

### 13.2 变更文件边界

- 修改（34）: `assets/data/skill_contracts_compact.json`、`assets/data/skill_mechanics.json`、`assets/data/skills.json`、`settings.json`；`src/game/application/states/GameplayState.cpp`；`src/game/foundation/components/{DeliveryArchetypes.hpp,SkillDefs.hpp,TriggerRuleComponent.hpp}`；`src/game/foundation/data/{BuffIds.hpp,TagRegistry.hpp}`；`src/game/systems/combat/{DamageMitigationService.cpp,DamagePipeline.cpp}`；`src/game/systems/skill/{BoomerangDeliverySystem.cpp,CMakeLists.txt,ProcEngine.cpp,ProjectileSystem.cpp,SkillSpecializationBaker.cpp,SkillSystem.cpp,SkillSystem.hpp}`；`src/game/systems/skill/behaviors/{BladeBoomerang.cpp,FlowingThrust.cpp}`；`tests/{SkillKeyNodeMatrixTestHelpers.hpp,fixtures/skill_specialization_keynodes.json,functional/FlowingThrustNodes.cpp,functional/SkillBehaviors.cpp,integration/DeliveryArchetypesTests.cpp,integration/SkillContractRegistryTests.cpp,integration/SkillKeyNodeMatrixIntegrationTests.cpp,integration/SkillSystemTests.cpp,unit/SkillBehaviorGuardTests.cpp,unit/SkillCastConstraintServiceTests.cpp,unit/SkillKeyNodeMatrixTests.cpp,unit/SkillPrerequisiteRequiredPointsTests.cpp,unit/SkillSpecializationBakerTests.cpp}`。
- 新增（untracked，8）: `src/game/systems/skill/ElementPathSystem.hpp/.cpp`、`src/game/systems/skill/StunApplication.hpp`；`tests/functional/{BladeBoomerangNodes.cpp,BladeBoomerangCatchTests.cpp,BladeBoomerangElementPathTests.cpp}`；`docs/plans/2026-09-10-skill8-specialization-nodes-implementation-plan.md`、本报告。
- 边界说明: `settings.json` 的 diff 仅为运行期基准字段（`benchmarkScore`/`updatedAtUtc`），与本包无关，不得随本包提交；`docs/reviews/` 历史文件、`conductor/` 均未被改动；未发现未声明的越界重构。

### 13.3 范围对齐

**29 节点逐节点核验**（语义以设计 §3.8 为准；代码引用为当前行号）:

| 节点 | 设计语义摘要 | 实现核验 | 判定 |
|------|--------------|----------|------|
| 800 | 法耗-1/点、攻速+4%/点 | `SkillSpecializationBaker.cpp:815-818` → `out_profile.effective_mana_cost`（消费于 `SkillSystem.cpp:1916`）+ `stat_modifiers` t17（应用管线 `StatsSystem.cpp:484`） | ✅ |
| 801 | 飞速/最远距离 +15%/点 | `Baker:819-823` → `BladeBoomerang.cpp:125`（speed）、`:233`（max_distance）→ `BoomerangDeliverySystem.cpp:325` | ✅ |
| 802 | 点伤+5/点、暴击率+2%/点 | `BladeBoomerang.cpp:155-161`（mech `added_physical_per_point`）+ t15 暴击 `stat_modifiers` | ✅ |
| 803 | 折返伤害+10%/点 | `Baker:826-828` → `BoomerangDeliverySystem.cpp:287-303`（折返时乘算 payload/snapshot） | ✅ |
| 810 | 顶点滞留 0.8s、每 0.2s 切割 | `Baker:829-831` → `BladeBoomerang.cpp:234` → 交付层滞空+周期切割派发 | ✅ |
| 811 | 25%/点概率施加流血 | `Baker:832-834` + `BladeBoomerang.cpp:318-345` + AilmentEngine；层数近似见剩余风险 2 | ✅ |
| 812 | 对流血目标暴击倍率+15%/点 | `DamagePipeline.cpp:1127-1146` | ✅（实现方式见 B3-2） |
| 813 | 切割 30%/点概率破甲+1s | `BladeBoomerang.cpp:347-375` + mech(8,813) | ✅（同类字符串问题见 B3-2） |
| 814 | 流云刺命中滞空 → 拔流血爆发 | `FlowingThrust.cpp:394-481`，真实伤害 `skip_mitigation=true` | ✅（实现方式见 B3-2） |
| 815 | 接剑时本次飞行流血伤害 100% 转治疗 | `BoomerangDeliverySystem.cpp:236-246` 读 `heal_bleed_pct` | ✅（近似，见剩余风险 2） |
| 830 | 额外 2 侧翼、单发-30% | `Baker:845-847` + `BladeBoomerang.cpp:263-271`（mech `side_damage_pct=0.70`） | ✅ |
| 831 | 侧偏角-10%/点、穿透不衰减 | `Baker:848-850` → 角度 `BladeBoomerang.cpp:266-269` | ✅ |
| 832 | 接剑回蓝 2/点 | `Baker:851-853` → `BoomerangDeliverySystem.cpp:198-215`（同 cast 其他刃清零） | ✅ |
| 833 | 接剑后 2s 攻速+15%/点 | `Baker:854-857` → `BoomerangDeliverySystem.cpp:216-224`（可叠层） | ✅ |
| 834 | 御剑步+0.5s/点、下次投掷免蓝 | 前段 `BoomerangDeliverySystem.cpp:225-234`；免蓝无消费 | ⚠️ 部分 → H3-2 |
| 835 | 折返击杀解减速定身+20%移速 | `BoomerangDeliverySystem.cpp:248-269` | ✅（实现方式见 B3-2） |
| 850 | 折返途中轻/中型牵引 | `BoomerangDeliverySystem.cpp:407-447`（重装/Boss 免疫） | ✅ |
| 851 | 牵引范围与判定体积+15%/点 | `Baker:865-867` → `BladeBoomerang.cpp:124` + `BoomerangDeliverySystem.cpp:128` | ✅ |
| 852 | 折返命中 15%/点得剑意 | `BladeBoomerang.cpp:385-400` | ✅（宽松回退见 L3-3） |
| 853 | 每层剑意速度/判定体积+2%/点 | `intent_scaling` 仅烘焙+拷贝，全仓无消费 | ❌ → H3-1 |
| 854 | 禁侧翼、护甲 5% 转物伤、硬直 | `Baker:874-877` + `BladeBoomerang.cpp:162-166,381-384` | ✅ |
| 855 | 巨剑暴击→触发技能3 巨剑 | 规则已生成但缺技能来源过滤 | ⚠️ 误触发 → B3-1 |
| 870/872 | 火/雷转质+残留路径 | `Baker:878-886` + `ElementPathSystem.cpp` | ✅ |
| 871 | 路径上火伤 More+15%/点 | `Baker:881-883` → `DamageMitigationService.cpp:226-256` | ✅ |
| 873 | 电弧频率+30%/点、接剑电磁爆发 | `Baker:887-889` → `ElementPathSystem.cpp:199-228,231-258` | ✅（键未外置 → M3-2） |
| 874 | 路径宽度/时长+15%/点 | `Baker:890-893` → 路径段生成 | ✅ |
| 875 | 本技能元素穿透+6%/点 | `Baker:894-896` → `DamageMitigationService.cpp:194-213`（SkillOnly、限路径内） | ✅ |
| 876 | 路径内 15% 元素绝对减免 | `ElementPathSystem.cpp:67-93,417-435` | ✅（烘焙字段无消费 → M3-1） |

**数据/契约互洽核验**（全部通过）:
- 基础数值: `mana_cost=8.0`、`cooldown=0.0`、`tags=[Physical,Attack,Projectile,Hit,Return,Dexterity,SwordSkill]`（`skills.json:4894-4940` 区段），与设计一致。
- 29 节点 `max_points` 全部与设计一致（含 811=4、832=3、870/872=1 等），前置链无死锁（`SkillPrerequisiteRequiredPointsTests` 全树可达回归）。
- keystone=[810,815,854]、transmuter=[870,872]+`max_transmuters=1`、节点级互斥 `keystone_exclusion_group`（`skills.json:5581/5598`）、synergy=[814]、`affects_sword_step`=[834]、`affects_sword_intent`=[852]、875 `resist_model=TypeA_Penetration`+`scope=SkillOnly`、855 trigger（`cast_skill_id=3, effectiveness=0.5, icd=3.0, requires_crit=true`）在三处（compact、生成契约、官方节点）一致。
- `gen_skill_contracts.py:415-479` 只为非默认节点发条目，技能8 生成 14 条为生成器既定口径（第 2 轮 R2 的“15 条”清单表述按此核正），角色/旗标/引用均正确。

**计划偏差**:
- `ElementPathComponent` 定义于 `src/game/systems/skill/ElementPathSystem.hpp`，而计划写作 `foundation/components/ElementPath.hpp`；仅为位置简化，无行为影响（可接受）。
- 计划要求“机制键外置”，872/873/876 仍未落 `skill_mechanics.json`（M3-2）。
- 计划要求 853/834 全量接线，实际分别为缺失与部分（H3-1/H3-2）。

### 13.4 质量与风险评估

1. **状态机归一已达成**：`ProjectileSystem.cpp:229-232` 的旧回旋分支（原 `:226-315`，含 returnTimer 双衰减、悬停清零、旧 831 回蓝/冷却返还、接刃销毁）已删除，注释承接；`BoomerangDeliverySystem` 成为唯一权威：技能2 保留 `returnTimer` 与顶点牵引/眩晕（`:365-397`），技能8 改按 `bc.max_distance`（`:325`）折返。技能2 回归用例通过。唯一遗漏是顶点 GPUParticle 冲击波未迁移（M3-3）。
2. **共享字段消费完整性**：新增 18 个 `BakedDeliveryParams` 字段中，16 个有真实消费；`element_shield_pct` 无运行时消费（876 走点数+mech 键），`intent_scaling` 无消费（H3-1）。`BoomerangComponent` 的 `return_speed_accel`、`SkillDefs.hpp:571 primary_archetype` 仍零读取（M3-1）。
3. **伤害管线**：875 在减伤阶段扣穿透（SkillOnly+路径内敌人，`DamageMitigationService.cpp:194-213`）；871 在伤害后乘 `(1+path_amp)`（`:226-256`）；876 减免仅同元素（`:246-256`）；812 在暴击结算段（`DamagePipeline.cpp:1127-1146`）。本次顺带修复的 dodge/block 100% 概率保护（`:113-114`、`:138`）语义正确；动态暴击 `:1177` 等 6 处同类写法未统一（属既有代码，见剩余风险 6）。
4. **855 触发链**：`TriggerRule.base_chance` 默认 1.0（`TriggerRuleComponent.hpp:29`），ProcEngine 对 100% 规则即时命中（`ProcEngine.cpp:97-100`），`required_source_node_id` 门控实现正确；但缺技能来源过滤造成跨技能误触发（B3-1）。
5. **814 与技能1 耦合**：复用 AilmentEngine 的 Bleed 效果与 `BuffType::Bleed` 识别抛异常口径合理，爆发走 `DamagePipeline::Execute` 真实伤害，未另造流血栈。
6. **测试质量**：L4 恒真断言已按承诺删除并换成实质断言；新增 3 个功能测试文件覆盖 27/29 节点行为路径（853/834-免蓝无行为可测）；概率型 813/852 采用 40 次迭代，统计上可复现；未发现既有用例被注释/删除/弱化（`DeliveryArchetypesTests` 对 832 语义变化的删行属计划内语义迁移）。新瑕疵：`BladeBoomerangCatchTests.cpp:193` 恒真（L3-1）。
7. **code_standard 硬规则**：未发现裸 `new/delete`、`dynamic_cast`/C 式转换、裸 `std::thread`、跨组件增删持有 EnTT 指针、宏观越界。热路径字符串分支命中硬否决（B3-2）；`src/` 新增标识符无 `\bfallback\b` 命中（legacy gate 通过），但 `kHoverZoneRadiusFallback`/`kBleedTickIntervalFallback` 与任务口径不符（L3-4）。

### 13.5 第 1/2 轮发现项状态

| 编号 | 状态 | 本轮关键证据 |
|------|------|--------------|
| C1 | 已解决 | `SkillSpecializationBaker.cpp:813-900` 逐节点落地，行为/交付层消费（见 13.3 矩阵） |
| C2 | 已解决 | transmuter [870,872]+max1+节点级 `keystone_exclusion_group`；双转质用例拒绝 |
| C3 | 已解决 | `max_points` 全量修正；`SkillPrerequisiteRequiredPointsTests` 全树无环可达 |
| H1 | 部分解决 | 855 规则与 831 假 trigger 删除已完成；来源过滤缺失 → B3-1 |
| H2 | 部分解决 | 814/815/854/835/875/876 已实现；834 缺免蓝 → H3-2；853 无行为 → H3-1 |
| H3 | 已解决 | `ElementPathSystem.hpp/.cpp` + 技能8 机制块；键缺项 → M3-2 |
| H4 | 已解决（近似） | 概率+AilmentEngine+池化接线；2 层上限近似见剩余风险 2 |
| H5 | 已解决 | `ProjectileSystem.cpp:229-232` 去分支；技能2 回归通过；VFX 丢失 → M3-3 |
| H6 | 已解决 | 29 节点 `max_points` 与设计一致 |
| H7 | 已解决 | 折返牵引 + 重/轻判定 + 半径倍率 |
| M1 | 已解决 | mana 8 / CD 0 / 文案 |
| M2 | 已解决 | Return/Dexterity/SwordSkill |
| M3 | 已解决 | 0.8s 滞空 + 0.2s 切割 + `max_distance` 距离语义 |
| M4 | 已解决（口径已核） | 契约角色/旗标/桶绑定正确；生成器非默认节点口径见 13.3 |
| M5 | 已解决 | 侧刃 0.70 数据驱动、角度外置、831 生效 |
| L1 | 部分解决 | 技能8 机制块存在；872/873/876 键缺（M3-2） |
| L2 | 已解决 | `del.speed/del.range` 均被消费（`BladeBoomerang.cpp:125/233`） |
| L3 | 部分解决 | 交付层旧宽松回退已收敛；852 处仍宽松（L3-3） |
| L4 | 已解决 | `SkillBehaviors.cpp` 恒真断言已替换为实质断言 |
| L5 | 已解决 | 测试锚点重写、fixture 更新、新增 3 个行为测试文件 |
| N1 | 已解决 | `del.duration/range/speed/sub_count` 均有消费 |
| N2 | 部分解决 | `max_distance` 已接线（`BoomerangDeliverySystem.cpp:325`）；`return_speed_accel`/`primary_archetype` 仍零读（M3-1） |
| N3 | 已解决 | 节点级互斥字段落地 |
| N4 | 已解决 | 875 绑定、872 解绑同批；`SkillRegistry` 加载校验通过 |
| N5 | 已解决 | 技能8 新字段注释补齐 |
| N6 | 维持（非缺陷） | 874/875 双前置 OR 语义未改 |

### 13.6 发现项

**B3-1（Blocker）855 触发缺技能来源过滤，任意技能暴击都会误触发技能3**
- 位置: `src/game/systems/skill/SkillSpecializationBaker.cpp:968-981`、`src/game/systems/skill/ProcEngine.cpp:69-79`。
- 观测: 855 规则只设 `required_source_node_id=330`、`required_source_skill_id=3`（用于校验玩家已点技能3 节点330），未设 `required_skill_id`；`ProcEngine.cpp:79` 仅在 `required_skill_id!=0` 时比较 `event.skill_id`，因此玩家只要点了 855 且技能3 点了 330，任何技能（技能1/2/4…）的暴击命中都会触发技能3 的“天降巨剑”（effectiveness 0.5、ICD 3s）。对照同函数 `:967`（714 规则）显式设置 `required_skill_id=7`，口径不一致。
- 违反依据: 设计 §3.8 855 限定“掷巨剑暴击→触发”；计划 WS-D 要求 855 仅由技能8 巨阙链路驱动。
- 修复建议: 在 `SyncTriggerRules` 的 855 分支设置 `rule.required_skill_id = 8`；同步修正 `TriggerRuleComponent.hpp:42-43` 注释；在 `tests/unit/SkillBehaviorGuardTests.cpp` 的 855 用例中增加“技能1 暴击命中 + 已点 330/855 → 不触发”反例。

**B3-2（Blocker）新增战斗/技能核心逻辑使用字符串比较做分支，违反硬否决 7**
- 位置: `src/game/systems/combat/DamagePipeline.cpp:1127-1147`（812，判定 `effect.id.find("Bleed")` `:1135`）；`src/game/systems/skill/behaviors/FlowingThrust.cpp:394-481`（814，`b.id.find("Bleed")` `:434`、`:455`）；`src/game/systems/skill/BoomerangDeliverySystem.cpp:248-269`（835，`find("Slow"/"slow"/"Root"/"Chill"/"Cold")` `:255-259`）。
- 观测: 三处均为本包新增代码，运行时按效果 id 子串分支；同文件邻近的 QiBrand 逻辑（`DamagePipeline.cpp:1116-1125`）明确因 code_standard 改用 `GetByKind`。
- 违反依据: `docs/workflows/review.md` 硬否决 7（热路径 Update/Render 或核心玩法技能/战斗/AI 字符串分支，且设计未显式授权，得修改并按 Blocker 处理）；`conductor/code_standard.md` §2.1/§7.2。
- 修复建议: 812/814 改为 `effect.type == BuffType::Bleed`（`Buff.hpp:35`，AilmentEngine 对受管流血统一写该 type）；835 改为 `b.type == BuffType::SpeedDown || b.type == BuffType::Freeze || b.type == BuffType::Root`（`Buff.hpp:27/31/46`，契合“减速定身”语义），删除字符串回退。若未来需细分同类 buff，扩展 `BuffKind`（当前仅 QiBrand/FateMark，`Buff.hpp:59-63`）。补“同名不含关键字的减速类不受影响/不同名但同 type 正确命中”的单元测试。

**H3-1（High）853 心剑合一无任何行为实现**
- 位置: `src/game/systems/skill/SkillSpecializationBaker.cpp:871-873`、`src/game/systems/skill/behaviors/BladeBoomerang.cpp:252`、`src/game/foundation/components/SkillDefs.hpp:600`。
- 观测: `intent_scaling` 仅烘焙并经组件拷贝，`rg` 全仓（src/）无读取；设计 §3.8 853 要求“每层剑意→飞速/折返速度/判定体积+2..6%”。
- 修复建议: 在 `BladeBoomerang` 掷出/交付层按当前 `SwordIntent` 层数将 speed、radius、returnSpeed 乘 `1 + intent_scaling * stacks`（层数从既有 `SwordIntentComponent`/SkillSystem 查询取，勿新建并行状态）；补行为测试（同点数下有/无剑意层数的速度或半径差异）。

**H3-2（High）834“下次投掷不耗蓝”未实现**
- 位置: `src/game/systems/skill/BoomerangDeliverySystem.cpp:228-234`（创建 `BladeBoomerangFreeCast`，3s，无消费者）。
- 观测: `rg` 全仓无读取该 BuffId；`SkillSystem.cpp:1916` 统一按 `effective_mana_cost` 扣费，投掷侧无免费判定；设计 §3.8 834 明确“下次投掷不耗法”。
- 修复建议: 在施放扣费前检查并消耗该 buff（若扣费只在 SkillSystem 统一路径，需能在施放时访问施法者效果列表；或在 `BladeBoomerang::DoCast` 预检并回补法力，但要保证只对技能8 生效）；补“接剑后下一次投掷法力不减且 buff 被移除”的测试。

**M3-1（Medium）死字段与双权威来源**
- 位置: `src/game/foundation/components/SkillDefs.hpp:608`（`element_shield_pct`，写于 `SkillSpecializationBaker.cpp:898`，仅测试断言 `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp:389`，运行时由 `ElementPathSystem.cpp:87-88` 直读点数+mech 键）；`src/game/foundation/components/DeliveryArchetypes.hpp:75`（`return_speed_accel` 零读写）；`src/game/foundation/components/SkillDefs.hpp:571`（`primary_archetype` 多处写入、无读取）。
- 违反依据: 计划“共享接口确定性/单一数据来源”意图；第 2 轮 N1/N2 的清理要求。
- 修复建议: `element_shield_pct` 删除或让路径减伤读烘焙值（并统一数值来源）；`return_speed_accel`/`primary_archetype` 删除写入方，或接线为消费者（若属其它技能依赖，需给读取方）。

**M3-2（Medium）872/873/876 机制数值未外置，缺失被代码默认值掩盖**
- 位置: `src/game/systems/skill/ElementPathSystem.cpp:87-88`（876 默认 15.0）、`:202`（电弧间隔 0.5）、`:208-210`（半径 180/伤害 30）、`:236-240`（爆发 60/眩晕 0.5）；`assets/data/skill_mechanics.json:727-757` 无这些键。
- 观测: 设计 §3.8 872/873/874/875/876 依赖路径系统；测试注释甚至声明“不依赖尚未烘焙的 skill_mechanics 技能8数据”，意味着数据缺失无法通过测试暴露。
- 修复建议: 向 `skill_mechanics.json` 技能8 块补齐 `arc_interval/arc_radius/arc_damage/burst_damage/burst_stun_duration/element_shield_pct`（与代码兜底同值），并考虑缺失键时告警或失败，避免静默漂移。

**M3-3（Medium）回旋体顶点冲击波 VFX 未迁移**
- 位置: 被删代码位于 `src/game/systems/skill/ProjectileSystem.cpp` 原 `:236-248`（现 `:229-232` 注释处）；`BoomerangDeliverySystem.cpp`、`BladeBoomerang.cpp`、`RendingWave.cpp` 均无 `GPUParticle` 发射（rg 证实）。
- 观测: 旧分支对所有回旋体顶点发射 Glow 粒子，新交付层未补发；技能2/8 的顶点视觉反馈丢失。
- 修复建议: 在 `enterReturning`/HoverApex 入口按技能与元素发射等效粒子（沿用 `GPUParticleSystem`），或取得设计确认后记录为有意移除。

**M3-4（Medium）完整测试二进制“0 失败”证据不可复现**
- 位置: `tests/performance/MaterialVFXBenchmark.cpp:281`（`stats.mean_ms < 0.3`）、`tests/performance/ParticleTrailBenchmark.cpp:205`（`dispatchOverheadMs < 0.2`）。
- 观测: 本次重跑 `bin\NoMoreDayTests.exe --success=false` 为 1506 cases / 129030 assertions / 2 failed（与声明的 129031/0 不符）；ParticleTrail 隔离重跑稳定失败；MaterialVFX 过滤运行测试全过后进程以 `0xC0000005` 退出。两个文件不在本包变更边界，且被 ci label 显式排除（`tests/CMakeLists.txt:46-48`），`ctest -L ci` 本次重跑 1/1 通过（7.55s）。
- 违反依据: `docs/workflows/review.md` 第 4 条硬否决针对“隐藏失败/伪造证据”；此处不判定为故意，但引用证据必须更正，避免以不实声明通过验收。
- 修复建议: 在安静环境复跑并保存日志；报告改为引用 ci 门禁结果 + 专项测试；性能阈值用例单独跟踪（是否环境相关、是否需要放宽阈值或隔离运行）。

**L3-1（Low）恒真断言** — `tests/functional/BladeBoomerangCatchTests.cpp:193` `CHECK(enemy != entt::null)` 构造后恒真，无证明力（同一测试其余断言有实质覆盖，未构成硬否决 1）。建议删除或替换为实际行为断言。
**L3-2（Low）命名空间污染** — `src/game/systems/skill/behaviors/BladeBoomerang.cpp:76` `using namespace BladeBoomerangNodes;`（文件内使用），建议改限定名或局部 using。
**L3-3（Low）852 折返判定宽松回退** — `src/game/systems/skill/behaviors/BladeBoomerang.cpp:385-393`：取不到 `BoomerangComponent` 时 `isReturning=true`，使 852 可能在非回旋体上触发。建议缺组件即视为不触发。
**L3-4（Low）标识符命名不符任务口径** — `src/game/systems/skill/behaviors/FlowingThrust.cpp:405` `kHoverZoneRadiusFallback`、`:428` `kBleedTickIntervalFallback` 虽因词边界不触发 legacy gate（`scripts/four_pillars_phase0_inventory.py:17-25`），但与“新增标识符不含 fallback”要求冲突，建议改为 `Default`/`Base` 等。
**L3-5（Low）测试预期清单漏 875** — `tests/SkillKeyNodeMatrixTestHelpers.hpp:111` 与 `tests/fixtures/skill_specialization_keynodes.json` 的 key_nodes 不含 875（该节点已有 `BladeBoomerangElementPathTests.cpp` 直接覆盖）。建议纳入矩阵并在 cast smoke 中配置 870/872 之一作为前置。
**L3-6（Low）注释/冗余标记** — `SkillSpecializationBaker.cpp:969` 注释“巨阙(源节点330)”把技能3 节点 330 与技能8 巨阙 854 混写；case8 的多数 `feature_flags` 位除 bit4/16/20/25（`BladeBoomerang.cpp:177-178`、`DamageMitigationService.cpp:182`）外无消费者，属冗余写入。建议修正注释并逐步收敛到单一字段来源。

### 13.7 最佳实践建议

1. 元素路径寻敌与爆发改用项目既有 `SpatialHashGrid`（交付层 `BoomerangDeliverySystem.cpp:367` 已示范），替换 `ElementPathSystem.cpp:178-197`、`:231-258` 的全量线性扫描，避免电弧/爆发在敌群下的 O(N) 抬升。
2. `ElementPathSystem.cpp:127-145` 的眩晕实现与新建的 `src/game/systems/skill/StunApplication.hpp` 重复，应统一复用后者（单一实现，便于后续眩晕抗性规则集中）。
3. 812 块参照同文件 QiBrand 路径（`DamagePipeline.cpp:1116-1125`）的类型/枚举判定模式，与 B3-2 一并落地。
4. 855 触发用例补充非技能8 反例与召唤物/多来源场景（B3-1）。
5. 报告证据链整改：验收引用 `ctest -L ci` + 专项测试 + 安静环境完整运行日志；性能 label 单独执行、单独结论。
6. `settings.json` 在最终提交前恢复或单列，明确不属于技能8 包。

### 13.8 剩余风险

1. **855 effectiveness=0.5 与“无消耗”语义待设计确认**（沿用第 1 轮风险 1）：当前只沿用旧值，未与 §3.3 技能3 对齐。
2. **815 治疗与 811 层数为近似口径**：AilmentEngine Bleed 契约为最多 2 层、叠加时 Additive 合并（`AilmentEngine.cpp:408-410`），而设计/文案为“每击一层”；814 拔层与 815 池量按近似实现，需设计确认是否接受。
3. **元素路径与 `AreaFieldDeliverySystem` 生命周期/占位冲突**未做运行验证（计划风险 4）。
4. **性能阈值用例不稳定**（M3-4）且不在 ci 门禁；若长期失败需独立 triage。
5. **技能9 既存数据问题**（节点 931 前置 930@2 而 930 max_points=1）未修，属跨技能、评审范围外。
6. **100% 概率判定保护不完整**：本包仅补 dodge/block（`DamagePipeline.cpp:113-114/138`），动态暴击 `:1177` 及 `:1267/:1610/:1684/:1756`、`BeamChannelDeliverySystem.cpp:323/845` 仍为 `GetFloat01() <`；属既有代码，但同类问题会在 100% 概率配置下复现。
7. **互斥过滤新增 `role != Transmuter` 条件**（`SkillSystem.cpp:2330-2337`）为全局改动，技能3-7 的转质互斥仅由现有用例覆盖，无专项回归证据。

### 13.9 下一步动作

1. 修复 B3-1、B3-2（阻塞项）：855 触发范围收口；三处核心逻辑去字符串分支。
2. 修复 H3-1、H3-2；清理 M3-1 死字段；补齐 M3-2 机制键；处理 M3-3 视觉回归；更正 M3-4 证据链。
3. Low 项与最佳实践建议按需在修复批中一并处理（优先 L3-1/L3-3/L3-5）。
4. 修复后重跑 `build.bat`（RelWithDebInfo）、`ctest -L ci`、`gen_skill_contracts.py --check` 与完整二进制（安静环境），更新本报告为第 4 轮跟进审查。
5. 最终提交前排除非本包文件（`settings.json`），保持 `docs/reviews/` 历史轮次只追加。

---

## 14. 第 4 轮跟进审查（整改复验）

- **审查目标**: 复验第 3 轮 12 项整改（B3-1/B3-2、H3-1/H3-2、M3-1~M3-4、L3-1/L3-3/L3-4/L3-5）是否真实落地、是否正确、是否引入新问题；本轮不重新编译，仅做代码/数据检视与定向测试。
- **结论**: **修改**（唯一需修复项为 N4-1；其余 11 项已解决、M3-1/M3-4 部分闭合，均已记录残余）。
- **审查轮次**: 第 4 轮跟进审查。
- **输入**: 第 3 轮报告 §13；主线程整改声明；设计 `设计文档/职业设计草案_剑修.md:540`（834 条文）；实施计划；当前工作区代码与数据。
- **变更文件边界**（按 mtime ≥ 20:11 复核，17 个文件，全部在声明范围内）: `src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/combat/DamagePipeline.cpp`、`src/game/foundation/components/DeliveryArchetypes.hpp`、`src/game/foundation/components/Buff.hpp`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/skill/BoomerangDeliverySystem.cpp`、`src/game/systems/skill/behaviors/BladeBoomerang.cpp`、`src/game/systems/skill/behaviors/FlowingThrust.cpp`、`src/game/systems/skill/ElementPathSystem.cpp`、`assets/data/skill_mechanics.json`、`assets/data/skill_contracts_compact.json`、`tests/functional/BladeBoomerangCatchTests.cpp`、`tests/functional/BladeBoomerangElementPathTests.cpp`、`tests/functional/BladeBoomerangNodes.cpp`、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`、`tests/fixtures/skill_specialization_keynodes.json`、`tests/SkillKeyNodeMatrixTestHelpers.hpp`。`git status --short` 仍为 34 modified + 8 untracked，无新增越界文件；`settings.json` 本轮无新改动（仍为历史脏文件）。

### 14.1 逐条结论

| # | 问题 | 状态 | 证据（file:line） |
|---|------|------|-------------------|
| 1 | B3-1 855 缺 `required_skill_id` | 已解决 | `SkillSpecializationBaker.cpp:969-973` 设置 `rule.required_skill_id = 8`；消费侧 `ProcEngine.cpp:79` 对非 0 值执行 `event.skill_id != rule.required_skill_id → continue`；测试 `tests/functional/BladeBoomerangNodes.cpp:590` 断言 ==8，且该用例通过 |
| 2 | B3-2 核心逻辑字符串分支 | 已解决 | 812 `DamagePipeline.cpp:1134-1135` 仅 `effect.type == BuffType::Bleed`；814 `FlowingThrust.cpp:432-433,453` 仅 `type == BuffType::Bleed`；835 `BoomerangDeliverySystem.cpp:253-257` 仅 `SpeedDown/Freeze/Root`。rg 全仓复查：`src/` 中残留的 `.find("` 均为既有代码（`DamagePipeline.cpp:1099`、`FlowingThrust.cpp:382/385/388`、`BladeFormation.cpp:442/482/487`），非本包第 3/4 轮新增 |
| 3 | H3-1 853 无消费 | 已解决 | `BladeBoomerang.cpp:127-135` 按 `1 + intent_scaling*stacks` 放大 speed 与半径；`:245` `returnSpeed = speed*1.5` 一并继承；测试 `BladeBoomerangNodes.cpp:290-307` 断言 speed 448 / radius 44.8（2 层 + 853=3） |
| 4 | H3-2 834 免蓝缺失 | 已解决（获赠但门控存疑，见 N4-1） | `Buff.hpp:63` 新增 `BuffKind::FreeCast`、`:257-280` GetByKind/RemoveByKind；`BoomerangDeliverySystem.cpp:228-238` 施加（kind+source_skill_id=8）；`SkillSystem.cpp:1917-1925` 匹配来源后 `raw_mana_cost=0` 并移除；集成测试 `SkillKeyNodeMatrixIntegrationTests.cpp:397-430`（实验组不扣蓝+buff 消失；对照组正常扣蓝） |
| 5 | M3-1 876 双权威/死字段 | **部分解决** | 已解决部分：`ElementPathSystem.cpp:66-74` 改读 `GetBakedSkillProfile(...)->delivery.element_shield_pct`，Baker `:898` 为唯一写入；`return_speed_accel` 已从 `DeliveryArchetypes.hpp` 删除（rg 无命中）；测试侧已补 `RebakeSkillProfiles`（`BladeBoomerangElementPathTests.cpp:77`）。**残余**：`primary_archetype` 仍为写死字段，见 N4-3 |
| 6 | M3-2 872/873/876 机制键 | 已解决 | `skill_mechanics.json:752-755`（872: arc_interval/arc_radius/arc_damage）、`:757-759`（873: burst_damage/burst_stun_duration）；876 按声明保持“烘焙为权威”，`ElementPathSystem.cpp` 已无 mech(8,876) 读取（仅注释引用） |
| 7 | M3-3 顶点冲击波 VFX | 已解决 | `BoomerangDeliverySystem.cpp:336-350` 在 Outward→apex 转换处发射 GPUParticle（位置=顶点、`scale=proj->radius`、Glow、0.4s），不区分 `isBladeBoomerang`，与旧 `ProjectileSystem` 行为一致（技能2/8 共用） |
| 8 | M3-4 测试证据 | **部分解决/环境性** | 本次复测：ParticleTrail 仍稳定失败（`ParticleTrailBenchmark.cpp:205`，0.262<0.2，性能 label，ci 排除）；MaterialVFX `:281` 断言本轮通过（4/4、15/15）但进程退出 `0xC0000005`；非性能子集直跑 1415/1415、107700/107700、0 failed、EXIT=0；`ctest -L ci` 8 次中首跑 EXIT=8、随后 7 连跑 EXIT=0。包内无新增失败，但证据存在间歇性，见 N4-5 |
| 9 | L3-1 恒真断言 | 已解决 | `BladeBoomerangCatchTests.cpp:163` 改为 `REQUIRE(registry.valid(enemy))`，810 用例保留实质行为断言 |
| 10 | L3-3 852 宽松回退 | 已解决 | `BladeBoomerang.cpp:403-406`：要求 `registry.valid(boomerang) && phase == BoomerangPhase::Returning`，不再缺省 true |
| 11 | L3-4 fallback 命名 | 已解决 | `FlowingThrust.cpp:405` `kHoverZoneRadiusDefault`、`:428` `kBleedTickIntervalDefault`（rg 无 `*Fallback` 新增标识符） |
| 12 | L3-5 875 未入矩阵 | 已解决 | `skill_contracts_compact.json:361` `passive_node_ids:[875]`；`tests/SkillKeyNodeMatrixTestHelpers.hpp:114` 与 `tests/fixtures/skill_specialization_keynodes.json:41` key_nodes 含 875；`gen_skill_contracts.py --check` 通过 |

### 14.2 新发现问题

**N4-1（Medium，需修复后方可提交）834 免蓝未按设计门控于“御剑步状态”**
- 位置: `src/game/systems/skill/BoomerangDeliverySystem.cpp:220-238`。
- 观测: FreeCast 的施加（`:228-237`）位于 `if (bc.step_extend_sec > 0.0f)` 内、但在 `if (auto *step = ...SwordStep)` 之外——只要玩家点出 834 且发生接刃，即便当前不处于御剑步也会获得“下次投掷免蓝”；剑步延长本身是有门控的。
- 违反依据: 设计 `设计文档/职业设计草案_剑修.md:540`：“**若你在‘御剑步’状态时接住飞剑**，不仅保留上述所有效果，御剑步的持续时间延长 0.5s...1.5s，**并使下一次投掷不消耗法力**”——条件从句覆盖“免蓝”分句；计划 WS-D 仅写“834 御剑步延长对接”，未授权无条件施加。
- 修复建议: 将 FreeCast 施加代码块移入 `if (step != nullptr)` 分支内（约 2 行移动）；在 `BladeBoomerangCatchTests.cpp` 的 834 用例补一条“无 SwordStep 时接刃不得获得 FreeCast”的反例断言。若设计确认无条件施加，应更新设计文本并记录豁免。

**N4-2（Low）`RemoveByKind` 不区分来源技能**
- 位置: `src/game/foundation/components/Buff.hpp:277-280`；消费判断 `SkillSystem.cpp:1919-1923` 只校验 `GetByKind` 返回的首个 FreeCast 的 `source_skill_id`。
- 观测: 移除操作会删除所有 `BuffKind::FreeCast`，理论上跨技能多枚时会误删；当前仅技能8 使用该 kind，属预防性风险。
- 修复建议: 按 `source_skill_id` 过滤删除（或断言同 kind 唯一），并在注释中限定单一使用者前提。

**N4-3（Low）`primary_archetype` 仍为只写字段**
- 位置: `src/game/foundation/components/SkillDefs.hpp:571`；写入方 `SkillSpecializationBaker.cpp:64-114,336-355`、`BladeBoomerang.cpp:109`；测试断言 `SkillSpecializationBakerTests.cpp:44/70/221`。
- 观测: `rg`（src/tests/tools/scripts）无运行时读取者，属 M3-1 未清完的死字段。
- 修复建议: 删除字段与写入/测试，或补真实消费者（如交付层路由）。

**N4-4（Low/最佳实践）855 缺跨技能反例**
- 位置: `tests/unit/SkillBehaviorGuardTests.cpp:1017-1038`。
- 观测: 该用例手工构造的规则**未设置** `required_skill_id`，且只派发技能8 命中，因此不能锁死“其它技能暴击不触发”的回归；行为保障目前依赖 `ProcEngine.cpp:79` 的既有实现与 `BladeBoomerangNodes.cpp:590` 的字段断言。
- 修复建议: 在 `runScenario` 增加“技能1 暴击 + 已点 330”反例（预期不触发），并让手工规则带 `required_skill_id=8`。

**N4-5（Medium，环境/证据）测试证据间歇不稳定**
- 观测（本轮独立复跑）: ① `ctest --test-dir build -C RelWithDebInfo -L ci` 首次 **FAILED（EXIT=8）**，因输出被截断未保留失败详情；随后 7 次连续运行全部 EXIT=0、100% passed。② `MaterialVFX*` 过滤运行两次（第 3、4 轮）均出现断言全过但进程退出 `0xC0000005`。③ `ParticleTrail*` 稳定失败（性能 label，ci 外）。④ 直接运行非性能子集（与 ci 同过滤）EXIT=0、0 failed。
- 影响: 不影响本包功能结论，但“ctest EXIT=0 / 全量 0 失败”的验收证据不具备完全可复现性；建议以多次运行日志与安静环境为验收依据，并对 0xC0000005 的退出崩溃单独 triage（疑似 GPU/粒子 teardown，非技能8 包引入）。

### 14.3 本轮验证命令与结果

- `git status --short`：34 modified + 8 untracked；`settings.json` 仍为历史脏文件。
- 定向测试（均 EXIT=0）: `.\bin\NoMoreDayTests.exe --test-case=*BladeBoomerang*`（29/29、372 断言）；`*SkillKeyNodeMatrix*`（8/8、702）；`*SkillBehaviorGuard*`（18/18、404）；`*FlowingThrust*`（8/8、67）；`*SkillSpecializationBaker*`（7/7、113）；`*SkillCastConstraint*`（1/1、24）。
- 非性能子集（等价 ci 过滤）: `--test-case-exclude=*Performance*,*GPU-Diagnostic*` → 1415/1415、107700/107700、0 failed、EXIT=0。
- `ctest --test-dir build -C RelWithDebInfo -L ci`：首跑 EXIT=8，其后 7 连跑 EXIT=0。
- `python scripts\gen_skill_contracts.py --check` → `[OK] skill_contract blocks are up to date.` EXIT=0。
- 性能定向: `--test-case=*ParticleTrail*` → 3/4、1 failed（0.262<0.2，EXIT=1）；`--test-case=*MaterialVFX*` → 4/4、15/15 通过但 EXIT=0xC0000005。
- 静态审计: `rg` 检查新增字符串分支（已清零）、`return_speed_accel`（已删除）、`primary_archetype`（无读取）、876 读取路径（仅烘焙）、round-4 文件 mtime（17 个，无越界）。

### 14.4 剩余风险

1. N4-1 修复前，834 在非御剑步接刃时会额外获得免蓝（设计偏离，待修复/确认）。
2. N4-5：过滤运行偶发 `0xC0000005` 退出与 ci 单次不稳定，根因未定位；可能干扰后续验收与 CI。
3. 上轮遗留（未变）：855 effectiveness=0.5 待设计确认；815 治疗/811 层数为近似口径（AilmentEngine Bleed 上限 2 层）；元素路径与 `AreaFieldDeliverySystem` 生命周期未做运行验证；技能9 节点 931 前置数据问题（范围外）；100% 概率保护仅 dodge/block；`GetEffectiveSkillTags` 的 `role != Transmuter` 过滤缺技能3-7 专项回归。
4. 旧存档兼容（低）：`BuffEffect.kind` 为可选反序列化字段（`Buff.hpp:155-156`），旧存档中已存在的 FreeCast buff 载入后 kind=None，需重新接刃获取；影响有限，记录即可。

### 14.5 最终结论与下一步动作

**结论：修改**。第 3 轮 12 项整改中 11 项已完整解决、M3-1 与 M3-4 部分闭合（残余为 Low 级死字段与测试环境稳定性），未发现新的字符串分支、双重乘区（851/853 各自单次生效，803 payload/snapshot 约定未变）、FreeCast 误消费（所有前置校验先于消费；FreeCast 下 `total_cost=0`，shadow 附加费按 `base_cost*0.5=0` 并在钩子内预检、恶魔剑分支因 `total_cost` 非正而跳过）或粒子补发位置问题。唯一需修复的是 N4-1（834 门控，1 处代码移动 + 1 条测试）。

下一步：
1. 修复 N4-1：FreeCast 施加移入 SwordStep 分支，补反例测试；同步补 N4-4 的 855 跨技能反例。
2. 可选清理 N4-2/N4-3（Low）。
3. 复跑 `build.bat` + 定向测试 + `ctest -L ci`（多次）+ `gen_skill_contracts.py --check`；对 N4-5 的退出崩溃与 ci 抖动单独 triage 并留日志。
4. 提交前排除 `settings.json`；完成后更新本报告第 5 轮结论。

---

## 15. 第 5 轮跟进审查（N4 收尾复验）

### 15.1 审查目标 / 输入 / 轮次 / 结论

- 审查目标：复验第 4 轮新发现 N4-1（Medium）与 N4-4（Low）的修复真实性与设计对齐性；评估 N4-2/N4-3 延迟处理的可接受性；在不重编译、不全量重跑的前提下做定向验证并给出最终结论。
- 输入：`docs/workflows/review.md`、本报告第 1–4 轮、`设计文档/职业设计草案_剑修.md` §3.8（`:540` 御剑步条件）、本轮 `git status --short` 边界。
- 审查轮次：第 5 轮（第 4 轮结论为“修改”，唯一阻塞项为 N4-1）。
- **结论：提交。**

### 15.2 发现项：N4-1（原 Medium，834 免蓝门控）——已解决

- 代码证据：`src/game/systems/skill/BoomerangDeliverySystem.cpp:220-239`。`if (bc.step_extend_sec > 0.0f)`（`:220`）内先取 `if (auto *step = skills::seven_star_shared::FindBuff(registry, owner, BuffIdToString(BuffId::SwordStep)))`（`:222`）；剑步延长（`:224-226`）与 FreeCast 施加（`BuffId::BladeBoomerangFreeCast`，`:228-237`）均位于该 `if` 内部，`:238` 闭合。未处于御剑步时既不延长剑步也不获得免蓝。
- 设计对齐：`设计文档/职业设计草案_剑修.md:540` “若你在‘御剑步’状态时接住飞剑……下一次投掷不消耗法力”；修复后的门控与设计条件一致。
- 测试证据：正向用例 `tests/functional/BladeBoomerangCatchTests.cpp:244-270` 未被弱化（`REQUIRE(step != nullptr)`、`remaining == Approx(2.5f)`、`FindEffect(FreeCast) != nullptr`）；新增反例 `:272-294` 在相同 `CreateBlade`/`Update` 布局下不授予 SwordStep，断言 `FindBuff(SwordStep) == nullptr` 且 `FindEffect(FreeCast) == nullptr`。因正向用例在同一布局下证明接刃必然发生（rem 2.0→2.5、FreeCast 存在），反例不是空转，也无恒真断言。
- 判定：N4-1 关闭。

### 15.3 发现项：N4-4（原 Low，855 跨技能反例）——已解决

- `tests/unit/SkillBehaviorGuardTests.cpp:1006-1053`：`runScenario` 新增 `source_skill_id` 参数（`:1006-1007`，默认 `8u`）；规则显式设置 `rule.required_skill_id = 8u`（`:1026`）；原三场景（未点 330 暴击不触发 / 点 330 非暴击不触发 / 点 330 暴击触发）保持于 `:1046-1050`；新增第 4 场景 `CHECK_FALSE(runScenario(true, true, 1u))`（`:1052`，技能1 暴击命中不触发）。
- 该反例真实覆盖 `ProcEngine.cpp:79` 的 `required_skill_id` 过滤路径，正是 B3-1 的回归保护；断言为真实行为判定，非空转。
- 判定：N4-4 关闭。

### 15.4 N4-2 / N4-3 延迟处理评估——可接受（Low，转为包外跟进）

- N4-2（`src/game/foundation/components/Buff.hpp:277-280` `RemoveByKind` 不区分来源）：当前全仓仅技能8 写入并消费 FreeCast（`BoomerangDeliverySystem.cpp:228`、`SkillSystem.cpp:1917-1925`），无其他写入者，属理论风险。可接受延迟。最小方案（后续任一次清理包执行）：为 `RemoveByKind` 增加带 `source_skill_id` 的过滤重载，并在 `SkillSystem.cpp:1923` 传入 8u。
- N4-3（`src/game/foundation/components/SkillDefs.hpp:571` `primary_archetype` 只写不读）：属本修复包之前的历史字段，所有交付原型的 Baker 写入且被 `tests/unit/SkillSpecializationBakerTests.cpp` 断言；删除将跨多个技能与测试，超出本包范围。接受延迟；建议后续先确认无外部工具/存档消费者（注意 `BakedDeliveryParams` 序列化兼容）后单独清理。
- 两者均为 Low、非本包新增缺陷（一为潜在风险、一为历史字段），不影响本轮结论。

### 15.5 定向验证与证据复核（范围对齐 / 变更文件边界）

本轮实际执行（未编译、未做全量重跑）：

| 命令 | 关键观测 | 结果 |
| --- | --- | --- |
| `.\bin\NoMoreDayTests.exe --test-case="*834*" --success=false` | `4 | 4 passed | 0 failed`，`49 | 49 passed`，EXIT=0 | 通过（符合预期 4/4） |
| `.\bin\NoMoreDayTests.exe --test-case="*855 requires skill3*" --success=false` | `1 | 1 passed`，`8 | 8 passed`，EXIT=0 | 通过（符合预期 8 断言） |
| 由上两条 doctest 摘要汇总的全套用例数 | 1509 cases（第 4 轮为 1508，+1 即新增 834 反例） | 一致 |
| `.\bin\NoMoreDayTests.exe --test-case="*GPUTimerQueryRing*" --success=false`（对应 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:248` P0-6） | `2 | 2 passed`，`23 | 23 passed`，EXIT=0 | 未复现 GPU Timer 抖动 |

- 采纳并复核的主线程参考证据：build11 EXIT=0；`ctest -L ci` 最近一次 EXIT=0（100% passed）；ci 等价过滤最近一次 1416 例仅 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp(235)` 的 GPU Timer 环境抖动失败（与本包无关）。本轮对该文件 P0-6 的定向复现通过，进一步支持“环境抖动”判断（第 4 轮记录的 ci 首跑失败/7 连跑通过与 MaterialVFX 退出 0xC0000005 同属环境类，见 15.7）。
- 变更文件边界：本轮仅 3 个文件变更（mtime 20:33-20:34）——`src/game/systems/skill/BoomerangDeliverySystem.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp`、`tests/functional/BladeBoomerangCatchTests.cpp`，二进制 `bin/NoMoreDayTests.exe` 20:35:01 晚于源码（已重编）。`git status --short` 共 43 项 = 35 modified + 8 untracked；第 35 个 modified 为第 4 轮加入的 `src/game/foundation/components/Buff.hpp`，在既定边界内；无未声明越界。`settings.json` 仍为历史遗留脏文件，提交时必须排除。
- 范围纪律：未发现对 `docs/reviews/` 历史轮次或无关文件的修改；本轮改动均落在 N4-1/N4-4 声明范围内。

### 15.6 质量与风险评估

- 硬否决复查：#1 假测试——新反例均为真实状态断言，无恒真；#2 弱化既有用例——正向 834 用例与 855 原三场景断言未削弱；#7 热路径字符串分支——本轮 3 文件无新增（`BoomerangDeliverySystem` 改动为块移动，标识符仍用 `BuffIdToString` 常量）；#9 裸 new/delete、#10 C 式转换/dynamic_cast、#11 裸线程——本轮无新增。
- 修复质量：N4-1 为纯门控移动（+1 条件），语义与设计 `:540` 一致，未引入副作用；N4-4 为测试增强，提升 B3-1 的跨技能回归覆盖。

### 15.7 剩余风险（承接第 3/4 轮，均不阻塞）

1. N4-5（环境稳定性，Medium 记录）：`ParticleTrailBenchmark` 性能阈值（0.262<0.2）稳定失败；`MaterialVFXBenchmark` 过滤子集运行退出 0xC0000005；`ctest -L ci` 存在 GPU Timer 随机抖动。均与本修复包无因果，建议单独 triage 并保留日志。
2. 既有设计/口径待确认项：855 `effectiveness=0.5` 待设计确认；815 治疗与 811 层数为近似口径（`AilmentEngine` Bleed 上限 2 层、Additive 合并）；元素路径与 `AreaFieldDeliverySystem` 生命周期未做运行验证；技能9 节点 931 前置数据问题（范围外）；100% 概率保护仅覆盖 dodge/block；`GetEffectiveSkillTags` 的 `role != Transmuter` 过滤缺技能3-7 专项回归。
3. N4-2 / N4-3（Low，见 15.4）。

### 15.8 最终结论与下一步动作

**结论：提交（Submit）。** 第 4 轮唯一阻塞项 N4-1 已修复，并经代码走查与定向测试双重核验；N4-4 已解决；N4-2/N4-3 为 Low 且经评估可延迟（附最小方案）；N4-5 属环境稳定性问题，与修复包无因果。本包第 1–4 轮发现项至此全部关闭，或降级为已记录的剩余风险。

下一步动作：
1. 正常提交本包；提交前排除 `settings.json`（历史遗留脏文件）。
2. 将 N4-2/N4-3 记入后续清理待办（最小方案见 15.4）。
3. N4-5 的环境抖动与 `MaterialVFXBenchmark` 退出码单独 triage（建议安静环境、单实例运行并留日志）。
4. 终验命令留档：`build.bat` → `ctest -L ci` → `python scripts\gen_skill_contracts.py --check` → 性能用例单独运行。
