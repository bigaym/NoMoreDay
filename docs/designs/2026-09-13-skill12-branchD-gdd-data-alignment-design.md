# 技能12「血相改写」分支 D GDD 与权威数据漂移对齐设计

- 日期：2026-09-13
- 状态：**已落地**（2026-09-13，D1–D5 按推荐默认拍板执行）
- 落地说明：用户已拍板 **D1=a / D2=a / D3=a / D4=a / D5=a**。已按 §6.2 **R-A1** 对 GDD §5.3.2（全 25 节点）完成 `name_key` 与 `prerequisites` 对齐（数据为准），并同步 §5.3.1、§5.3.2 关键节点清单、§5.4 的交叉引用；**未改动** `assets/data/*`、引擎、backlog、plan，未提交 git、未跑构建/测试。1220/1223/1224 的 `desc_key` 与机制不符问题按 D5 由用户另立 backlog，本条目不处理。
- 复审追加（2026-09-13）：用户就子代理提出的 R1/R2 再拍——1220 的 desc 定为「持续伤害的物理占比降至约 45%，其余转为虚空伤害；并提升对物理 / 虚空抗性的侵蚀（约 4 点）」（`physical_ratio=0.45` 是物理占比，§6.3 原「45% 转为虚空」表述已弃用）；§5.3.2 全部节点英文括注去除（数据无英文字段，`skill_node_prompts.json` 仅为图标提示词、非显示源）。
- 范围：backlog `B1-28`——GDD §5.3.2「血相改写」分支 D 及全树与随包数据的漂移（全 25 节点命名/前置）
- 前置文档：`docs/workflows/design.md`；`docs/plans/2026-09-12-skill1-9-followup-backlog.md:99`（B1-28 原文）；`docs/plans/2026-09-13-skill-rd-rulings-plan.md:183-190,235-239`（C2 与未决项）；`docs/reviews/2026-09-13-skill-rd-rulings-review.md:94`
- 证据基线（设计时点）：`设计文档/职业设计草案_剑修.md` L1256-1262；`assets/data/mastery_skill_trees.json` L1797-2361；`assets/data/skill_mechanics.json` L1044-1137；`src/game/systems/skill/SkillSystem.cpp:2230-2268`
- 落地约束回顾：仅改 GDD 文档；不改 `assets/data/*`、引擎、backlog、plan；不编译、不跑测试、不提交 git。§6 的 before/after 已成为已执行的改动清单。

---

## 1. 问题陈述与事实核验

### 1.1 backlog 原始陈述

`docs/plans/2026-09-12-skill1-9-followup-backlog.md:99`：

> **B1-28** [P2] GDD §3.5「血相改写」分支除已修订的 1221/1222 前置外仍与数据漂移：L1260 虚蚀瘴幕前置写「污秽侵蚀 3/4 或 渴血回流 3/4」，数据为 `{1202:3 且 1209:3}`（且非或）；L1261 腐血蔓延前置「任意 Transmuter 或 虚蚀瘴幕」、L1262 滞腐深渊「腐血蔓延 2/3」，数据均为 `{1220:1}`；L1261/1262 名称（腐血蔓延/滞腐深渊）与数据 `name_key`（噬骨余烬/瘴海蚀骨）不一致。

### 1.2 关键纠正：多前置是 **OR**，backlog 的「且非或」是错误陈述

backlog 断言「数据为 `{1202:3 且 1209:3}`（且非或）」。**该断言与引擎实现相反。** 引擎对同一节点的多个 `prerequisites` 采用「任一满足即通过」的 OR 语义，三处消费者一致：

- 运行期加点校验：`src/game/systems/skill/SkillSystem.cpp:2230-2255`——`for` 遍历 `node.prerequisites`，命中 `pre_pts >= required_points` 即 `prereq_satisfied = true; break;`（`:2251-2254`）。只有全部不满足才拒绝（`:2256-2267`）。
- 天赋树 UI 预检：`src/game/application/ui/UISkillTalentTree.cpp:95-115`，函数名即 `IsPrerequisiteSatisfiedOr`，`return true` 于 `:111`。
- 专精 UI 连线可用性：`src/game/application/ui/UISkillSpecRenderer.cpp:45-66`，同名函数、同 OR 逻辑。

历史裁决先例（项目已多次核准该语义，非本次新解）：

- `docs/reviews/2026-09-10-skill7-specialization-nodes-review.md:82-84`：明确「引擎实际上原生支持 OR 语义」。
- `docs/reviews/2026-09-10-skill7-specialization-nodes-review.md:195`：更正「引擎不支持多前置 OR」的说法不成立。
- `docs/reviews/2026-09-09-skill5-specialization-nodes-review.md:188-189`：多前置 OR 与设计「任意 Transmuter」契合。
- `docs/reviews/2026-09-10-skill8-specialization-nodes-review.md:355,489`：874/875 双前置 OR 语义维持。

**结论**：`mastery_skill_trees.json:2243-2252` 中 1220 的 `prerequisites=[{1202:3},{1209:3}]` 在运行时的含义是「`1202` 满 3 点 **或** `1209` 满 3 点」，**不是** AND。GDD L1260 的「或」在逻辑连接词层面是**正确**的；backlog 的「（且非或）」需更正。

### 1.3 权威源是分层的，不是单一文件

玩家实际体验到的东西由三个随包数据文件分别供给，GDD 不在其中：

| 关注面 | 随包数据（权威） | 运行时加载证据 |
|---|---|---|
| 树拓扑 / 节点名 / 描述回显 / 解锁前置 | `assets/data/mastery_skill_trees.json` | `src/game/foundation/data/SkillRegistry.cpp:301-314`（`ResolveMasterySkillTreePath`）、`:316`（`LoadMasterySkillTrees`） |
| 机制数值（伤害/吸血/侵蚀比例等） | `assets/data/skill_mechanics.json` | `src/app/Game.cpp:261`；消费于 `src/game/systems/skill/behaviors/BloodSea.cpp:116-129,337-403,421` |
| 角色 / Keystone 互斥组 / Transmuter 合同 | `assets/data/skill_contracts_compact.json` + `mastery_skill_trees.json` 的 `skill_contract` | `skill_contract` 见 `assets/data/mastery_skill_trees.json:1612-1795` |

`设计文档/职业设计草案_剑修.md` 是**设计意图文档**，不参与运行时；其中形如 `*{需求: ...}*` 的文案**不是**玩家可见字符串（UI 只画连线与需求点数圆点，`src/game/application/ui/UISkillSpecRenderer.cpp:490-491,499-518`），因此它的作用是「准确描述数据图」，其自身准确性以数据为标尺。

---

## 2. 现状对照表（逐条 `文件:行`）

### 2.1 表 A：分支 D 节点（本次核心）

| id | GDD 名（`:行`） | GDD 前置文案（`:行`） | 数据 `name_key`（`:行`） | 数据 `prerequisites`（`:行`） | 数据 `desc_key`（`:行`） | 判定 |
|---|---|---|---|---|---|---|
| 1221 | 血潮奔流（`设计文档/职业设计草案_剑修.md:1258`） | 虚蚀瘴幕 1/1（同左） | 血潮奔流（`assets/data/mastery_skill_trees.json:2262`） | `[{1220:1}]`（`:2264-2267`） | 「血海改造成追猎血潮：移动速度提升 40%，范围提升 20%。」（`:2258`） | **已对齐**（C2.2 已修订，勿动） |
| 1222 | 血环噬身（`:1259`） | 虚蚀瘴幕 1/1（同左） | 血环噬身（`:2289`） | `[{1220:1}]`（`:2291-2294`） | 「血海压缩为血环：生命吸取效率提升 50%，但范围缩小 30%。」（`:2285`） | **已对齐**（勿动） |
| 1220 | 虚蚀瘴幕（`:1260`） | 污秽侵蚀 3/4 **或** 渴血回流 3/4（`:1260`） | 虚蚀瘴幕（`:2242`） | `[{1202:3},{1209:3}]`（`:2243-2252`，**OR**） | 「血海对物理与虚空抗性的侵蚀效果提升 5/10/15。」（`:2238`），`max_points=1`（`:2241`） | **漂移**：名一致；前置被引用节点不一致；desc 与机制不符 |
| 1223 | 腐血蔓延（`:1261`） | 任意 Transmuter 或 虚蚀瘴幕（`:1261`） | **噬骨余烬**（`:2316`） | `[{1220:1}]`（`:2318-2321`） | 「虚蚀状态延长 1s/2s/3s，且造成的持续伤害总增 (More) 15%/30%/45%。」（`:2312`） | **漂移**：名、前置、效果全不一致 |
| 1224 | 滞腐深渊（`:1262`） | 腐血蔓延 2/3（`:1262`） | **瘴海蚀骨**（`:2346`） | `[{1220:1}]`（`:2348-2351`） | 「血海对物理与虚空抗性的侵蚀量增加 4/8/12。」（`:2342`） | **漂移**：名、前置、效果全不一致 |

补充事实：`1223` 未出现在 `skill_contract.nodes` 列表（`assets/data/mastery_skill_trees.json:1628-1794` 列出 1202/1207/1209/1211/1213/1217/1220/1221/1222/1224），而 `tests/fixtures/skill_specialization_keynodes.json:61` 的 key_nodes 同样不含 1223——即 1223 是普通 Passive，无特殊角色。这不影响本次裁定，但解释「为何 1223 在合同里查不到」。

### 2.2 表 B：全树 25 节点的命名/前置一致性（证据，用于界定影响面）

数据侧以 `python` 解析 `assets/data/mastery_skill_trees.json` 的 `skills[skill_id==12].talent_tree` 得到（id / name_key / max / prerequisites）：

```
1200 血幕起势 4 [(0,0)]        1210 残命索回 3 [(1209,2)]      1220 虚蚀瘴幕 1 [(1202,3),(1209,3)]
1201 血压潮升 4 [(0,0)]        1211 鲜血回灌 1 [(1209,3)]      1221 血潮奔流 1 [(1220,1)]
1202 渴血成锋 4 [(0,0)]        1212 血浪复饮 3 [(1211,1),(1210,2)] 1222 血环噬身 1 [(1220,1)]
1203 血雾追身 4 [(0,0)]        1213 饮海而生 1 [(1212,2)]      1223 噬骨余烬 3 [(1220,1)]
1204 断生压迫 3 [(1201,2)]     1214 猎命回潮 3 [(1213,1)]      1224 瘴海蚀骨 3 [(1220,1)]
1205 追猎血径 3 [(1204,2)]     1215 追猎瘴衣 3 [(1202,2)]
1206 濒死刃口 3 [(1205,2)]     1216 刃雾共振 3 [(1215,1),(1203,2)]
1207 无间血狱 1 [(1205,3)]     1217 绝影共噬 1 [(1216,2)]
1208 断脉余波 3 [(1207,1)]     1218 猎血延压 3 [(1217,1)]
1209 饮血涨潮 4 [(1201,2)]     1219 久驻血雾 3 [(1215,1)]
```

与 GDD `设计文档/职业设计草案_剑修.md:1226-1262` 逐行比对：25 个节点中 **仅 7 个名称一致**（1207 无间血狱 `:1236`/`:1949`、1211 鲜血回灌 `:1243`/`:2050`、1213 饮海而生 `:1245`/`:2092`、1217 绝影共噬 `:1252`/`:2180`、1220 虚蚀瘴幕 `:1260`/`:2242`、1221 血潮奔流 `:1258`/`:2262`、1222 血环噬身 `:1259`/`:2289`），**其余 18 个名称不一致**。前置漂移同样普遍（例：GDD `:1233` 写 1204 前置「血雾凝炼 2/4 或 污秽侵蚀 2/4」，数据 `:1888-1892` 为 `[{1201:2}]`）。

**因此 B1-28 名为「分支 D 漂移」，根因是整棵 §5.3.2 的命名与拓扑早于数据现状。** 只改 L1260-1262 会留下「分支 D 引用了基础层未定义的节点名」这一内部矛盾（见 §6.2）。

---

## 3. 权威源判定与理由

### 3.1 判定

**建议以随包数据为准，据此修订 GDD；且权威在数据内部再分层**：

1. 玩家可见的节点名与效果描述：以 `mastery_skill_trees.json` 的 `name_key` / `desc_key` 为准（它们被直接上屏：`src/game/application/ui/UISkillTalentTree.cpp:195`（名）、`:1038`（desc）；`src/game/application/ui/AstrolabeController.cpp:439`）。
2. 实际生效的机制数值：以 `skill_mechanics.json` 为准（`src/app/Game.cpp:261` 加载；`src/game/systems/skill/behaviors/BloodSea.cpp:119-129,337-338,400-403,421` 读取）。当 `desc_key` 与机制冲突时，**以机制为准**，并把 `desc_key` 登记为显示缺陷（见 §3.3）。
3. 解锁前置与拓扑：以 `mastery_skill_trees.json` 的 `prerequisites` 为准（`SkillSystem.cpp:2233-2255`）。
4. GDD 修订目标 = 用上述数据把设计文档改写为「与运行时一致 + 保留设计意图」。

### 3.2 理由

- **数据对玩家实际生效**：技能12 的树**只存在于** `mastery_skill_trees.json`——`assets/data/skills.json` 中检索不到 `1220`（skill12 树未被 merge 进 `skills.json`），故该文件是 skill12 的唯一树源，经 `SkillRegistry.cpp:301-314` 在运行时叠加加载。
- **名称即上屏文案**：`name_key` 虽名为「key」，实为字面中文（如 `assets/data/mastery_skill_trees.json:2316` 的 `"噬骨余烬"`），并被 `UISkillTalentTree.cpp:195` 原样绘制。GDD 里的「腐血蔓延」在整个运行期资产中不出现。
- **最小改动与风险对称**：改 GDD 只影响文档、零运行时风险、可 git 回退；改数据会改变玩家解锁条件/显示，须连带重跑 `scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`、`scripts/sync_skill_node_icon_ids.py --check`，并更新 `tests/functional/BloodSeaNodes.cpp:182-213`、`tests/integration/SkillContractRegistryTests.cpp:294-330,670` 等断言。
- **backlog 的「且非或」已被证伪**（§1.2），说明 B1-28 的原始描述本身精度不足，不应作为裁定依据。

### 3.3 数据侧自身的两处缺陷（须同时登记，但不阻塞 B1-28）

- **1220 `desc_key` 与机制不符**：`assets/data/mastery_skill_trees.json:2238` 写「抗性侵蚀效果提升 5/10/15」，但该节点 `max_points=1`（`:2241`），且机制为固定值 `resist_shred_bonus=4.0`、`damage_mult=1.18`、`physical_ratio=0.45`、`pulse_damage_mult=1.08`（`assets/data/skill_mechanics.json:1111-1116`）。玩家可见 desc **漏报**了核心效果「约 45% 伤害转为虚空伤害」（`BloodSea.cpp:118-129`），且 5/10/15 三档与单点节点矛盾。
- **1223/1224 `desc_key` 与机制档位不符**：1223 写 `1s/2s/3s`、`15%/30%/45%`（`:2312`），机制为 `miasma_duration_per_point=0.25`、`void_damage_per_point=0.06`（`skill_mechanics.json:1130-1132`）；1224 写 `4/8/12`（`:2342`），机制为 `resist_shred_per_point=2.0`（`:1134-1135`）。二者最大点 3，数值语义需核对。

> 这两条属**数据文案债务**，与 GDD 漂移同源。是否随 B1-28 一并处理由用户定（§7 D4）。

---

## 4. AND/OR 语义裁定

### 4.1 结论

- **数据语义不改**：多前置数组在引擎中就是 OR（§1.2 三处消费者 + 历史先例）。1220 的运行时读法是「`渴血成锋(1202)` 达 3 点 **或** `饮血涨潮(1209)` 达 3 点」。
- **GDD 的连接词「或」正确**，无需为语义改 GDD 或改数据。
- 真正的分歧不是 AND/OR，而是**被引用的第二个节点是谁**：GDD 写「污秽侵蚀」（GDD 命名，映射到 GDD 的 id 1203），数据引用的是 `1209`。

### 4.2 影响面

- 若沿用 OR（推荐）：1220 解锁只需两条血渴核心被动之一满 3 点，Build 路径更宽、与「Keystone 三选一」的互斥设计（`assets/data/mastery_skill_trees.json:1660,1709,1742`，`keystone_exclusion_group=1`）配合合理。
- 若改成 AND（即「两条都必须满 3 点」）：需**同时改数据与引擎**——把 `SkillSystem.cpp:2251-2254`、`UISkillTalentTree.cpp:95-115`、`UISkillSpecRenderer.cpp:45-66` 三处从 OR 改为「全部满足」，且会**改变全项目所有多前置节点**（如 skill5 574、skill4 476、skill7 774、skill8 874/875）的行为，属跨技能回归。**强烈不建议**在 B1-28 内做。

### 4.3 对 backlog 的纠错建议（仅提案，不改文件）

`docs/plans/2026-09-12-skill1-9-followup-backlog.md:99` 的「`{1202:3 且 1209:3}`（且非或）」应改为「`[{1202:3},{1209:3}]`（数组内两条，引擎按 **OR** 解析）」。

---

## 5. 命名取舍（腐血蔓延/滞腐深渊 vs 噬骨余烬/瘴海蚀骨）

### 5.1 玩家可见文案以数据 `name_key` 为准

- 显示路径唯一：`UISkillTalentTree.cpp:195`（`hoveredNode.name_key`）、`:1038`（`hoveredNode.desc_key`）、`AstrolabeController.cpp:439`。GDD 名不上屏。
- 全仓检索（排除 GDD/conductor/build/bin）仅在 `assets/data/mastery_skill_trees.json` 与 `docs/{plans,reviews}` 中出现这些中文名，**没有独立的本地化表**，即 `name_key` 就是成品文案本身。

### 5.2 `skill_node_prompts.json` 的定位

`assets/data/skill_node_prompts.json` 是**AI 生成节点图标用的英文提示词**，不是玩家可见 UI 文案，且自身口径混杂：

- `:347` 1223 = `'Corrupt Embers'`（与数据 `噬骨余烬` 一致）
- `:348` 1224 = `'Corrosion Path'`（数据 `瘴海蚀骨`，无官方英文名可比）
- `:326` 1202 = `'Hungry Blade'`（数据 `渴血成锋`，非 GDD `渴血回流/Hungry Reflux`）
- `:333` 1209 = `'Debt Repaid'`（却是 GDD 1209 `血债回生` 的英文，数据 1209 为 `饮血涨潮`）
- `:324` 1200 = `'Blood Mist Density'`（是 GDD `血雾凝炼` 的英文，数据 1200 为 `血幕起势`）

**结论**：`skill_node_prompts.json` 不能作为命名权威；它只影响图标美术，不参与显示逻辑。

### 5.3 建议

**以数据 `name_key` 为准**，即 GDD 改用 `噬骨余烬` / `瘴海蚀骨`。（注：任务描述中的「噬骨猎烬」为笔误，数据实际是 `噬骨余烬`，见 `assets/data/mastery_skill_trees.json:2316`。）

英文括注（GDD 特有的中英并列）在数据中无对应字段，属装饰性内容；建议与图标提示词对齐时用 `Corrupt Embers` / `Corrosion Path`，或直接去掉，避免再造第二套命名。

---

## 6. GDD 逐行 before/after（保留 id）

### 6.1 分支 D 立即修订三行（推荐方案 R-A：采用数据名 + OR）

> 说明：`1221`/`1222`（`:1258`/`:1259`）已按 C2.2 修订，**不动**。

**L1260 id:1220**

- before（`设计文档/职业设计草案_剑修.md:1260`）：
  `- **<Keystone> [虚蚀瘴幕 (Void Miasma)] (0/1)**: *{需求: 污秽侵蚀 3/4 或 渴血回流 3/4}* 血海更偏虚空侵蚀领域，持续伤害的一部分转为虚空伤害，并更擅长压制异常或减益目标。id:1220`
- after：
  `- **<Keystone> [虚蚀瘴幕 (Void Miasma)] (0/1)**: *{需求: 渴血成锋 3/4 或 饮血涨潮 3/4}* 血海更偏虚空侵蚀领域，持续伤害的一部分转为虚空伤害，并更擅长压制异常或减益目标。id:1220`
- 变更点：仅前置的两个节点名，`或` 保留（与引擎 OR 一致）。desc 保留（见 §6.3：该 desc 与机制一致，反而优于数据 `desc_key`）。

**L1261 id:1223**

- before（`:1261`）：
  `- **[腐血蔓延 (Corrupt Spread)] (0/3)**: *{需求: 任意 Transmuter 或 虚蚀瘴幕}* 改写后的血海范围增加 10%...30%，并对流血 / 衰弱 / 异常状态目标额外造成 6%...18% 伤害。id:1223`
- after：
  `- **[噬骨余烬 (Corrupt Embers)] (0/3)**: *{需求: 虚蚀瘴幕 1/1}* 虚蚀状态延长 1/2/3 秒，且血海造成的持续伤害总增 15%/30%/45%。id:1223`
- 变更点：名 → 数据 `name_key`；前置 → 数据 `[{1220:1}]`（写 `虚蚀瘴幕 1/1`）；效果文案 → 对齐数据 `desc_key`（`:2312`）。**注意**：`任意 Transmuter` 的旧需求在数据中不存在，属被数据取代的设计。

**L1262 id:1224**

- before（`:1262`）：
  `- **[滞腐深渊 (Stagnant Abyss)] (0/3)**: *{需求: 腐血蔓延 2/3}* 被改写血海命中的敌人会额外受到 4%...12% 的移速降低，且离开血海后 1...3 秒内继续承受小额侵蚀伤害。id:1224`
- after：
  `- **[瘴海蚀骨 (Corrosion Path)] (0/3)**: *{需求: 虚蚀瘴幕 1/1}* 血海对物理与虚空抗性的侵蚀量增加 4/8/12。id:1224`
- 变更点：名 → 数据 `name_key`；前置 → 数据 `[{1220:1}]`；效果文案 → 数据 `desc_key`（`:2342`）。
- **附加提示**：`:2342` 的 `4/8/12` 与机制 `resist_shred_per_point=2.0`（`skill_mechanics.json:1135`）不一致，属数据文案债务（§3.3），建议 GDD 采用数据 `desc_key` 后**单独登记**核对项，不要在本轮自造数值。

### 6.2 R-A 的一致性前提（必须一并处理，否则 GDD 自相矛盾）

R-A 让分支 D 引用了 `渴血成锋` / `饮血涨潮` 等数据名，但 GDD §5.3.2 基础层（`:1226-1229`）仍把它们写作 `血雾凝炼` / `猩红涌动` / `渴血回流` / `污秽侵蚀`。若只改分支 D，读者会在 §5.3.2 找不到「渴血成锋」的定义。因此 R-A 需要二者之一：

- **R-A1（推荐，但扩范围）**：把 §5.3.2（`:1226-1262`，25 节点）整体对齐数据 `name_key` 与 `prerequisites`。这是唯一自洽解；映射表见附录 A。
- **R-A2（折中）**：仅改分支 D，并在 §5.3.2 顶部加一条注记，声明「节点名以随包数据 `mastery_skill_trees.json` 的 `name_key` 为准，本文旧名为别名」。风险：文档内两套名并存，后续易再漂移。

**不推荐**保留 GDD 旧名（腐血蔓延等）：那等于维护一套玩家永远看不到、且与运行时前置引用错位的命名。

### 6.3 关于 desc 的可选对齐（决策项，非强制）

- **L1260 的 desc 建议保留 GDD 原文**：GDD 写「持续伤害的一部分转为虚空伤害」——这由 `physical_ratio=0.45` 实现（`BloodSea.cpp:118-129`），比数据 `desc_key` 更完整。若要更精确，可改为「约 45% 持续伤害转为虚空伤害，并提升物理/虚空抗性侵蚀」。
- **L1261/L1262 的 desc 建议采用数据 `desc_key`**（见 §6.1），因为 GDD 旧文案描述的机制在数据中已不存在。
- 以上 desc 改动会改变「设计文档记录的机制」，故列入 §7 决策点。

---

## 7. 需要用户拍板的决策点

| 编号 | 决策点 | 选项 | 建议 | 依据 |
|---|---|---|---|---|
| **D1** | 权威源 | (a) 数据为准改 GDD；(b) 以 GDD 为准改数据 | **(a)** | §3.2；数据是运行时唯一源 |
| **D2** | 多前置语义 | (a) 沿用引擎 OR；(b) 改为 AND（须改引擎+全项目回归） | **(a)** | §1.2、§4.2；既有先例一致 |
| **D3** | 1220 第二个前置节点身份 | (a) 接受数据 `{1202, 1209}`，改 GDD 文案为「渴血成锋 或 饮血涨潮」；(b) 认为设计本意是「虚空侵蚀线」，改数据 prereq | **(a)** | 数据已生效且两条血渴核心被动主题自洽；改数据风险高（§3.2）。**但若设计本意确为「污秽侵蚀/治疗」两条线，则选 (b) 并重生成契约/改断言** |
| **D4** | 命名范围 | (a) 全 §5.3.2（25 节点 18 处）对齐数据名；(b) 仅分支 D + 别名注记 | **(a)** | §6.2；仅 D 会自相矛盾 |
| **D5** | 数据文案债务是否并入 | (a) 本轮仅 GDD；(b) 同时登记/修 1220/1223/1224 的 `desc_key` 与机制不一致 | **(a) 本轮只改 GDD，(b) 另开条目** | §3.3；避免把显示缺陷与文档对齐混在一次改动 |

> D3 与 D5 是仅有的、可能触及数据/引擎的点；其余均为文档层。

---

## 8. 可逆性与影响面

- **只改 GDD（推荐路径，D1=a / D2=a / D4=a 或 b）**：
  - 影响面：`设计文档/职业设计草案_剑修.md` 单文件；零运行时、零存档、零测试影响。
  - 可逆性：完全可逆（git）。不触发 `assets/` 校验脚本。
  - 若采纳 D4=a，GDD §5.3.2 与 §5.4 审计表（`:1264-1274`）的名称/计数表述需同步检查（名称变、结构不变）。
- **改数据（仅当 D3=b 或 D5=b）**：
  - 会改变玩家实际解锁条件/显示，属行为变更；需重跑 `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`、`python scripts/sync_skill_node_icon_ids.py --check`、`python scripts/validate_json.py`。
  - 需更新断言：`tests/functional/BloodSeaNodes.cpp:182-213`、`tests/integration/SkillContractRegistryTests.cpp:294-330,670`、`tests/SkillKeyNodeMatrixTestHelpers.hpp:118`。
  - 可逆但仍属运行时变更，建议独立计划管理。
- **改 backlog 描述（§4.3）**：文档层，可逆。

---

## 9. 验收标准与验证方法（供后续实施/复审使用）

文档层（D4 落地后）：

- `rg -n "污秽侵蚀|渴血回流|腐血蔓延|滞腐深渊" "设计文档/职业设计草案_剑修.md"`：分支 D 段（`:1256-1262`）应无残留（D4=a 时全文应无残留）。
- `rg -n "渴血成锋|饮血涨潮|噬骨余烬|瘴海蚀骨" "设计文档/职业设计草案_剑修.md"`：应命中分支 D 对应行。
- 人工核对：GDD 分支 D 每条的 `id` 与数据 `id` 一致；前置文案的节点名与点数与 `assets/data/mastery_skill_trees.json` 的 `prerequisites` 一致；`或` 表述与引擎 OR 一致。

若触及数据（仅 D3=b/D5=b）：契约三连校验 + `sync_skill_node_icon_ids.py --check` PASS；`ctest -C RelWithDebInfo -L ci` 全绿；上述断言同步更新。

> 本次提案不执行以上命令（受「不跑构建/测试」约束）。

---

## 10. 风险、依赖与未决

- **风险 R1（中）**：若只做 §6.1 三行而不做 §6.2，GDD 会引用未定义的节点名，留下二次债务。
- **风险 R2（低-中）**：D3 若被裁定为「改数据」，将同时改动解锁行为并触发跨测试回归；建议独立计划。
- **风险 R3（低）**：`skill_node_prompts.json` 与数据名混用（§5.2），若有人误当命名源会再引入漂移；建议在数据/文档侧标注其为「图标提示词，非显示文案」。
- **未决 U1**：1220 的 `desc_key` 与机制不符（§3.3）的修复条目归属。
- **未决 U2**：`1223` 不在 contract.nodes 与 key_nodes 中是否为有意（普通 Passive）——本次裁定不受影响，但审查时值得确认。
- **依赖**：`docs/plans/2026-09-13-skill-rd-rulings-plan.md:187`（C2.2）已建立的「GDD 对齐数据」先例；本提案沿用该方向。

---

## 附录 A：全树 25 节点 GDD ↔ 数据 映射（供 D4=a 使用）

| id | GDD 名（`设计文档/职业设计草案_剑修.md`） | 数据 `name_key`（`assets/data/mastery_skill_trees.json`） | 一致 |
|---|---|---|---|
| 1200 | 血雾凝炼 `:1226` | 血幕起势 `:1803` | ✗ |
| 1201 | 猩红涌动 `:1227` | 血压潮升 `:1827` | ✗ |
| 1202 | 渴血回流 `:1228` | 渴血成锋 `:1849` | ✗ |
| 1203 | 污秽侵蚀 `:1229` | 血雾追身 `:1871` | ✗ |
| 1204 | 裂肉分潮 `:1233` | 断生压迫 `:1887` | ✗ |
| 1205 | 绝命追咬 `:1234` | 追猎血径 `:1909` | ✗ |
| 1206 | 溅血沸鸣 `:1235` | 濒死刃口 `:1925` | ✗ |
| 1207 | 无间血狱 `:1236` | 无间血狱 `:1949` | ✓ |
| 1208 | 屠潮 `:1237` | 断脉余波 `:1979` | ✗ |
| 1209 | 血债回生 `:1241` | 饮血涨潮 `:2001` | ✗ |
| 1210 | 赎罪之潮 `:1242` | 残命索回 `:2023` | ✗ |
| 1211 | 鲜血回灌 `:1243` | 鲜血回灌 `:2050` | ✓ |
| 1212 | 过量脉冲 `:1244` | 血浪复饮 `:2066` | ✗ |
| 1213 | 饮海而生 `:1245` | 饮海而生 `:2092` | ✓ |
| 1214 | 还命余烬 `:1246` | 猎命回潮 `:2108` | ✗ |
| 1215 | 血步追魂 `:1250` | 追猎瘴衣 `:2132` | ✗ |
| 1216 | 血幕护身 `:1251` | 刃雾共振 `:2154` | ✗ |
| 1217 | 绝影共噬 `:1252` | 绝影共噬 `:2180` | ✓ |
| 1218 | 嗜战不退 `:1253` | 猎血延压 `:2202` | ✗ |
| 1219 | 猎场主宰 `:1254` | 久驻血雾 `:2218` | ✗ |
| 1220 | 虚蚀瘴幕 `:1260` | 虚蚀瘴幕 `:2242` | ✓ |
| 1221 | 血潮奔流 `:1258` | 血潮奔流 `:2262` | ✓ |
| 1222 | 血环噬身 `:1259` | 血环噬身 `:2289` | ✓ |
| 1223 | 腐血蔓延 `:1261` | 噬骨余烬 `:2316` | ✗ |
| 1224 | 滞腐深渊 `:1262` | 瘴海蚀骨 `:2346` | ✗ |

小结：25 节点中 7 一致、18 不一致；其中分支 D 占 2 处（1223/1224）。

## 附录 B：证据索引

- 运行时前置 OR：`src/game/systems/skill/SkillSystem.cpp:2230-2255`；`src/game/application/ui/UISkillTalentTree.cpp:95-115,737`；`src/game/application/ui/UISkillSpecRenderer.cpp:45-66,550`。
- OR 历史先例：`docs/reviews/2026-09-10-skill7-specialization-nodes-review.md:82-84,195`；`docs/reviews/2026-09-09-skill5-specialization-nodes-review.md:188-189`；`docs/reviews/2026-09-10-skill8-specialization-nodes-review.md:355,489`。
- 数据加载：`src/game/foundation/data/SkillRegistry.cpp:301-314,316`；`src/app/Game.cpp:261`。
- 文案上屏：`src/game/application/ui/UISkillTalentTree.cpp:195,1038`；`src/game/application/ui/AstrolabeController.cpp:439`。
- 机制消费：`src/game/systems/skill/behaviors/BloodSea.cpp:116-129,337-338,400-403,421`；断言 `tests/functional/BloodSeaNodes.cpp:182-213`。
- GDD：`设计文档/职业设计草案_剑修.md:1226-1262,1264-1274`。
- 数据：`assets/data/mastery_skill_trees.json:1612-1795,1797-2361`（talent_tree 1200-1224）；`assets/data/skill_mechanics.json:1044-1137`；`assets/data/skill_node_prompts.json:324-348`。
- backlog/plan/review：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:99,261`；`docs/plans/2026-09-13-skill-rd-rulings-plan.md:183-190,235-239`；`docs/reviews/2026-09-13-skill-rd-rulings-review.md:94`。
