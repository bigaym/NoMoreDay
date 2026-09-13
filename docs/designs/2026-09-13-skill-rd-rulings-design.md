# 技能 RD 裁决修订 — 设计文档

- 日期：2026-09-13
- 性质：修订 `docs/designs/2026-09-13-skill-followup-design.md`（下称「首版设计」，随 commit `31cc67e6` 提交）。本会话复核 backlog §1 RD 清单时，对其中若干项作出与首版相反的最终裁决。
- 关联：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（活清单，§1.1 同源裁决）、`docs/reviews/2026-09-13-skill-followup-review.md`
- 下游：`docs/plans/2026-09-13-skill-rd-rulings-plan.md`

## 1. 背景

首版设计把 2026-09-13 早间的 D1~D12 裁决固化并随实现提交。随后对 RD 清单的二次复核中，用户对 4 项改判、2 项补全。以本修订为准，首版对应条目作废。

### 1.1 覆盖矩阵

| RD | 首版设计裁定 | 本修订裁定 | 关系 |
|---|---|---|---|
| RD-01 Keystone 上限/互斥 | D1：撤销 Keystone 互斥，仅保留 Transmuter 互斥 | 角色分组互斥，每组选 1，每树 keystone 生效上限=组数；**全 12 树**补齐分组 | 覆盖 |
| RD-02 981+982 暴伤 | D2：不封顶，保持线性 | 暴伤加成总量封顶 **+200%** | 覆盖 |
| RD-08 153 饮血刃 | D6：保留全来源流血 tick 回血，比例 100%→30% | **仅流云刺造成的流血 tick 回血** | 覆盖 |
| RD-14 子投射物 ignore_resist | D10：不继承 | 接受子投射物继承（确认现状） | 覆盖 |
| RD-03 形态中重施法 | D3：禁止重置免死 | 确认；当前**未实现**，补实现 | 确认+补实现 |
| RD-06 技能4 455/435/452 | D5：按设计实现（455 一次性、435、452 专精版） | 455 改「下一次攻击 More」语义；435/452 已实装，确认 | 确认+细化 |

其余 RD-04/05/07/09/10/11/12/13/15/16/17/18 与首版一致，不重复。

## 2. 目标与非目标

**目标**

- 把上表 6 项固化为行为/数据/文本，并同步修订 GDD 与首版设计文档的口径。
- RD-01 建立可校验的「Keystone 角色分组」数据模型，覆盖全部 12 棵树（含 skill10~12，其节点在 `mastery_skill_trees.json`）。

**非目标**

- 性能类（Tracy/RenderDoc 基线、O-01/02/03/07、伤害管线计时噪声）。
- RD-18 UMR 并轨、A-01 抽象化（独立 Track）。
- 首版已确认的 RD-04/05/07/09/10/11/12/13/15 行为不改。

## 3. 设计修订

### 3.1 RD-01 Keystone 角色分组互斥（全 12 树）

**问题**：现数据只有技能2 把 4 个 keystone 全塞进 group1（实际生效上限=1，与「按角色取舍」的本意不符）；技能3~9/11/12 的 `keystone_exclusion_group` 几乎只装在 Transmuter 上，Keystone 默认无互斥、可全点；skill10 把 keystone 1007 与 transmuter 1021/1022 混在同组且组号从 3 起跳。

**规则**

1. 每棵树的每个 Keystone 节点必须归入且仅归入一个「角色组」；同一角色组内至多 1 个 Keystone 生效。
2. 每棵树可同时生效的 Keystone 上限 = 该树角色组数（组内至多 1 个即自然成立，无需另一套计数）。
3. 角色组按「形态/强度/循环/生存/机动」等职责划分；单节点组表示该角色无替代，合法。
4. 组号在**每棵树内**从 1 起顺序编号（不再跨树递增）。
5. Transmuter 互斥（每组二选一）与 Keystone 角色组使用**同一字段与同一校验**，但语义分治：默认各自成组；仅当设计明确要求「Keystone 与 Transmuter 互斥」（如 skill10）时才同组。

**机制落点**

- 字段与加载：`src/game/foundation/data/SkillContract.hpp:67 uint8_t keystone_exclusion_group`；`src/game/foundation/data/SkillRegistry.cpp:379-382`。
- 生效结算：`src/game/systems/combat/DamagePipeline.cpp:826-849 is_keystone_excluded`（同组内按 `allocated_points` 取**最小 node_id** 生效，其余抑制）。
- 校验：`src/game/foundation/data/SkillRegistry.cpp:485-566`。
  - **须放宽**：现 `:558-566` 要求每个组 ≥2 节点，单节点角色组会报 `keystone_exclusion_group N has fewer than 2 nodes`。改为：Keystone 角色组允许 ≥1；Transmuter 组保留既有 `has_transmuter_exclusion` 豁免（`:521-536`）。
- UI：`src/game/application/ui/UISkillTalentTree.cpp:141-143`（互斥徽标）、`src/game/application/ui/GameUiSnapshotBuilder.cpp:951`、`src/game/application/ui/GameUiSnapshot.hpp:423/445`。
- 数据源：权威源是紧凑契约 `assets/data/skill_contracts_compact.json` 的 `skills[].keystone_exclusion_groups`；由 `scripts/gen_skill_contracts.py` 生成为 `assets/data/skills.json`（skill1~9）与 `assets/data/mastery_skill_trees.json`（skill10~12）内嵌 `skill_contract` 的节点 `keystone_exclusion_group`。改 compact 后须重跑生成器。

**分组方案（建议，待用户确认 §6 开放点）**

| 技能 | 组1 | 组2 | 组3 | 上限 |
|---|---|---|---|---|
| skill1 流云刺 | {113 风行者}机动 | {154 孤注一掷}强度 | — | 2 |
| skill2 裂空斩 | {213,214,234}形态 | {253 湮灭波}强度 | — | 2 |
| skill3 灵剑决 | {330,351}形态 | {313 神速}频率 | — | 2 |
| skill4 剑气护体 | {412,433,434}生存 | {470 剑气反震}反击 | — | 2 |
| skill5 万剑归宗 | {533,550}引导形态 | {534 天剑降世}终结 | — | 2 |
| skill6 剑阵·诛仙 | {633,634}阵内压制 | {613 千丝万缕}多阵联动 | {653 随身剑垒}阵体形态 | 3 |
| skill7 心剑·无影 | {711,732}引导形态 | — | — | 1 |
| skill8 御剑·回旋 | {810,854}飞剑形态 | {815 风眼}生存循环 | — | 2 |
| skill9 绝影绝剑 | {977,980}生存 | {981 逆脉}锁血爆发 | {954 时光逆流}冷却循环 | 3 |
| skill10 七星斩 | {1007,1021,1022}形态锁定 | {1013}循环强化 | {1025}机动循环 | 3 |
| skill11 天剑降临 | {1107,1113}输出取向 | {1120}元素身份 | — | 2 |
| skill12 血海 | {1207,1213,1220}主倾向三选一 | — | — | 1 |

**关键不变量**

- skill2 形态组 {213,214,234} 三选一、强度组 {253} 独立 → 上限 2（用户原话示例）。
- skill3 取上限 2（{330,351}形态 + {313}频率）；skill4 取上限 2（{412,433,434}生存 + {470}反击）。
- skill6/skill9/skill10 上限 3，skill7/skill12 上限 1。
- **skill8 定稿**：解除 815 风眼对 810 滞空切割的前置依赖；810/854 形态组 + 815 独立，上限 2，任意选择下 815 均可达。
- **skill10 定稿**：仅此一树保留 keystone1007 与 transmuter1021/1022 同组（形态锁定三选一）；其余树 keystone 与 transmuter 各自成组。
- **单节点角色组合法**：校验由「组内≥2 节点」放宽为「≥1」；组号在树内自 1 起连续。
- **分配交互保持现状**：允许分配，同组非最小 node_id 的效果被抑制（不在分配时拦截）。
- skill12 的 Transmuter 1221/1222 仍二选一，与 Keystone 主倾向组相互独立。
- skill5 基底形态、skill3 基底形态的定义不受本组划分影响。

### 3.2 RD-02 982「孤注一掷」暴伤封顶

- **问题**：`src/game/systems/skill/behaviors/PhantomTrance.cpp:340-341`
  `missing_pct = (1 - hp.cur/hp.max) * 100`；`value = missing_pct * last_stand_crit_pct`；`:351-353` 以 `StatType::CritDamage` + `PercentAdd`（0.5s）应用。rank4 时 `last_stand_crit_pct=4.0`，满损血可达 +400%，且 981 锁血使缺血稳定拉满，无上限。
- **修订**：最终加成的**总量**封顶 `+200` 个百分点（等价于 rank4 时最多按 50% 缺血计入）。
  - 落点：`PhantomTrance.cpp:340` 之后对 `value` 取 `min(value, 200.0f)`（或在 `:341` 计算后 clamp）；保持 `last_stand_crit_pct` 数据与烘焙不变（`SkillSpecializationBaker.cpp:971-978`、`assets/data/skill_mechanics.json:819-820`）。
  - 981 锁血 33% / 禁疗 / +33% More 不变（`assets/data/skills.json:5806`）。
- **理由**：极端 build 下 +400% 暴伤不可控；封顶不改线性手感，仅切掉尾部。

### 3.3 RD-03 形态中重施法不得重置免死

- **问题**：`src/game/systems/skill/behaviors/PhantomTrance.cpp:716-733 DoCast` 每次都 `emplace_or_replace<PhantomTranceComponent>`，`:723 pt.lethal_triggered=false`、`:730 pt.last_stand_buff_value=0`，并 `:747-750` 重新 `ApplyDeathSeal`；无「已在形态中」守卫，重施法可反复武装免死。
- **修订**：仅在「未处于形态」→「进入形态」时授予并武装免死。已在形态中时重施法：
  - 不重置 `lethal_triggered`；
  - 不刷新/不重复 `ApplyDeathSeal` 的免死窗口；
  - 其余形态刷新语义（时长等）保留：重施法可刷新形态时长。
- **落点**：`PhantomTrance.cpp:716-750`。正常 skill9 CD(15s) > 形态时长(3s)（`assets/data/skills.json:5640`），但代码层须显式防护。
- **不变量**：`CombatSystem.cpp:546-559` 的免死触发与 977「免死不再提前结束」语义不变。

### 3.4 RD-06 技能4 节点 455「以攻代守」

- **问题**：数据要求「闪避成功后 2s 内**下一次攻击** More+20% 并获得 1 层剑意」（`assets/data/skills.json:2491-2504`、`assets/data/skill_mechanics.json:350-354`）；实现却是 2s 全程的 `PhysicalDamage PercentMult 20` buff（`src/game/systems/skill/SkillSystem.cpp:1131-1134`、`:1164-1182`），既不是「下一次攻击」，也错误地限定为物理伤害而非全局 More。
- **修订**：改为一次性挂载、下次**攻击行为**的伤害结算时消耗：
  - 闪避成功后挂一个 pending「+20% More（全局）」标记（不直接结算为定时 buff）；
  - 下一次由该角色发起的攻击在伤害计算时消费该标记，结算后清除；
  - 获得 1 层剑意保留（`SkillSystem.cpp:1181`）。
- **落点**：`SkillSystem.cpp:1131-1182`；是否需要新增组件字段或复用现有 on-next-hit 通道由计划定。`assets/data/skills.json:2491-2504` 文本不再修改。
- **确认项**：435 `crit_bonus` 已实装（`BladeWard.cpp:245-247`、`SkillSystem.cpp:1247-1266`）；452 已实装（`SkillSpecializationBaker.cpp:134-136/:525-526/:1067-1069`），本批不改。
- **定稿**：闪避后未命中的挥击也消耗该「下一次攻击」加成（命中与否都算一次攻击）。
- **补裁（2026-09-13，AoE/多段范围）**：一次「攻击行为」指**一次施法**或**一次普攻挥击动作**；该动作产生的**全部伤害实例**（AoE 全目标、同一施法的多段/多次命中、投射物爆炸波及的多目标）共享同一份 +20% More，而非只有首个结算目标获得加成。
  - **行为标识**：技能伤害沿用引擎既有 `cast_id`（`ResolveCastIdFromSourceEntity`，`DamagePipeline.cpp:460-479`；同一次施法的全部目标/段共享同一 `cast_id`）作为攻击行为标识；普攻挥击无施法 id，由攻击发起侧每挥一次分配一个挥击标识，经 `DamageRequest` 新增字段承载（默认 0 = 无标识）。
  - **标记生命周期**：首个合格伤害实例将标记置为「已消费」并记录当前攻击行为标识；同一标识的后续实例继续享受加成；出现**不同**标识时清除标记且新攻击不吃加成；2s 窗口到期仍兜底清除。标识缺失（均为 0，如直接调用管线的测试/工具路径）时退回「首个实例消费即清除」的逐实例语义，保持兼容。
  - **门控不变**：模拟、DoT 跳伤、荆棘反伤、地面危险区、异常 tick 既不消耗也不享受加成。
  - **落点**：消费点仍在 `DamagePipeline.cpp:808-846`（闪避早退之前）；攻击行为标识与「已消费」状态为运行期瞬时数据，不改变持久化格式。

### 3.5 RD-08 153「饮血刃」限定流云刺来源

- **问题**：`AilmentEngine.cpp:838-858` 只要 source 的技能1 专精点了 153，就对**任意来源**（含血海、回旋、引导等，`AilmentEngine.cpp:828-831` 注释明示）施加的 Bleed tick 回血；`HasFlowingThrustBloodDrinker`（`:48-62`）只查技能1 是否点 153，不看流血由谁施加。数据 `skill_mechanics.json:14-16` 当前 `lifesteal_ratio=0.30`。
- **修订**：153 只对**由技能1「流云刺」（及其 DoT 151）施加的流血** tick 回血；其它来源流血不回血。
  - 机制：为流血效果记录来源技能。`AilmentApplyRequest`（`src/game/foundation/components`/`AilmentEngine.hpp:48-54`）当前只有 `source`，需补来源技能/节点字段并在存储的 effect 上保留；`AilmentEngine.cpp:838` 门控增加 `source_skill_id==1`。
  - 施加点须传入来源：`FlowingThrust.cpp:504-511`（151）、`BladeBoomerang.cpp:328-330`、`BeamChannelDeliverySystem.cpp:340`（后者不传即不回血）。
  - GDD L156 文案改为「仅流云刺造成的流血回血」。
- **定稿**：`lifesteal_ratio` 由 0.30 回到 GDD 原值 **1.0**（`assets/data/skill_mechanics.json:14-16` 0.30→1.0）；即仅流云刺流血 tick 按实际伤害 100% 回血。

### 3.6 RD-14 子投射物继承 ignore_resist（确认现状）

- **现状**：`ProjectileSystem.cpp:896-961 SpawnSplitProjectiles`（`:920` 拷贝父组件、仅重置 `on_death/pierce`）与 `:963-1004 SpawnExplosionProjectiles`（`:982`）**未重置** `ignore_resist`；`Projectile.hpp:137 bool ignore_resist=false`，消费于 `DamageMitigationService.cpp:262-269`。
- **裁定**：接受子投射物继承父的 `ignore_resist`（及父的伤害标签/快照上下文），与首版 D10 相反。首版设计 §3 D10 改记为「继承」。
- **落点**：仅文档口径与测试；不改代码。补一条断言固化「子投射物继承 ignore_resist」。

## 4. 影响面与回退

- **数据**：权威源 `assets/data/skill_contracts_compact.json`（keystone 分组，手改）；生成产物 `assets/data/skills.json`（skill1~9 内嵌契约）与 `assets/data/mastery_skill_trees.json`（skill10~12）由 `scripts/gen_skill_contracts.py` 重生成；另 `assets/data/skill_mechanics.json`（982 参数、153 比例）。须过 `--check --check-idempotency --check-determinism` 与图标同步脚本校验。
- **存档/序列化**：RD-08 若给流血 effect 增字段，须评估 Ailment 状态是否入存档；RD-06 pending 标记若用新组件字段同样评估。无版本迁移需求时保持默认值兼容。
- **GDD**：`设计文档/职业设计草案_剑修.md`（153 文案；如分组结论影响 §5.1/§5.4.4 表述则同步）。
- **测试**：`SkillRegistry` 校验放宽用例、每树分组数据完整性用例、982 封顶用例、RD-03 重施法用例、455 一次消耗用例、153 来源门控用例、RD-14 继承断言。
- **性能**：`is_keystone_excluded` 每命中遍历已分配点，分组增补不改变其复杂度；无热路径结构变化。
- **回退**：按 RD 独立可回退；数据改动可 revert 文件，GDD 为文本 diff。

## 5. 可观察验收标准

- 每棵树 keystone 分组数据齐全、组号树内自 1 起连续，校验脚本与生成器全 PASS。
- 任意同组选 2 个 keystone 时，仅最小 id 生效（现有 `is_keystone_excluded` 语义），有断言覆盖；skill2 上限为 2、skill7/skill12 上限为 1。
- 982 满损血 + rank4 时暴伤加成恰为 +200%，不再线性到 +400%。
- 形态中重施法不改变 `lethal_triggered`、不刷新免死窗口，有断言。
- 455 在闪避后下次攻击结算 More+20% 且消耗，之后攻击无加成，有断言。
- 455 同一次攻击行为（AoE 多目标 / 同施法多段）的全部伤害实例均获得 +20%，不同攻击行为不再加成，有断言。
- 非流云刺来源流血 tick 不再触发 153 回血，有断言。
- 出口：`build.bat`（RelWithDebInfo，0 警告）→ `ctest -C RelWithDebInfo -L ci` 全绿 → 数据校验脚本 PASS。

## 6. 裁决落定与剩余风险

### 6.1 本会话已拍板（2026-09-13）

| # | 事项 | 定稿 |
|---|---|---|
| 1 | RD-01 单节点组 | 允许单节点角色组，校验放宽为「组内≥1」，不加 `max_keystones` 字段 |
| 2 | RD-01 skill3 | 上限 2：{330,351}形态 + {313}频率 |
| 3 | RD-01 skill4 | 上限 2：{412,433,434}生存 + {470}反击 |
| 4 | RD-01 skill8 | 解除 815 对 810 的前置依赖；810/854 互斥 + 815 独立，上限 2 |
| 5 | RD-01 skill10 | 仅此树保留 keystone/transmuter 同组，其余树各自成组 |
| 6 | RD-01 分配交互 | 保持「可分配、同组非最小 id 被抑制」现状 |
| 7 | RD-01 skill12 id 漂移 | 以数据为准修订 GDD 3.5 的 1220/1221/1222 标注 |
| 8 | RD-08 比例 | `lifesteal_ratio` 0.30 → 1.0 |
| 9 | RD-03 语义 | 允许刷新形态时长，但不重置 `lethal_triggered`、不重刷免死窗口 |
| 10 | RD-06 455 时机 | 闪避后未命中的挥击也消耗加成 |
| 11 | RD-06 455 范围（补裁） | 一次攻击行为=一次施法或一次普攻挥击；其全部伤害实例共享 +20%，按 `cast_id`/挥击标识聚合，换行为即清除 |

### 6.2 剩余风险

- **skill8 依赖**：解除数据前置后，须复核 `风眼`（815）机制是否在运行时依赖 `滞空切割`（810）形态状态；若依赖，须改为自持条件，否则选了 810 才有意义的情形会与「815 独立可达」冲突。
- **skill12 GDD id 漂移**：修订 GDD 3.5 后须顺带核对全文对 skill12 节点的 id 引用（含图标/契约生成配置）。
- **RD-08 字段扩展**：`AilmentApplyRequest` 增来源技能字段若进入存档/快照，须保证默认值（0=未知来源）向后兼容；未知来源按不回血处理。
- **契约重生成**：12 树分组数据补齐后必须重跑契约生成器 `--check --check-idempotency --check-determinism`，并复核 `SkillContractRegistryTests` 校验放宽不误报。
- **运行时采证**：B1-21/22/23、772 数值、§11.9 仍受「无交互运行环境」阻塞，保持未验证风险，不阻塞本批数据/行为改动。
