# 技能5「万剑归宗」专精树实现审查报告（深度复核与增补版）

- **日期**: 2026-09-09
- **审查对象**: `assets/data/skill_5_tree.json` + `assets/data/skills.json`（id=5）+ `assets/data/skill_contracts_compact.json`（skill 5 契约）+ `assets/data/skill_mechanics.json` + 专精相关代码（烘焙层 / 行为层 / 交付系统 / 消费系统 / 测试）
- **审查基准**: `设计文档/职业设计草案_剑修.md` §3.5「万剑归宗 (Infinite Blades)」（L338–393，共 28 节点：4 基底 + 6 分支A + 6 分支B + 6 分支C + 6 分支D）
- **复核说明**: 本报告在 2026-09-09 初版审查基础上，进行了针对整个源码库、契约管道、交付管线、机制数值表及测试矩阵的全量深度复核，核实纠正了初版 2 处事实性偏差，并增补了 1 项 CRITICAL、4 项 HIGH、1 项 MEDIUM 的系统性隐患。

## 结论

**修改（Block 级）**。

技能5「万剑归宗」专精树当前处于「数据骨架脱节、契约全面错标、机制数值空白、实现完全度 0%（语义层面）、测试护栏深度固化旧错误」状态。28 个设计节点中：

- 0 个按 §3.5 语义正确实现；
- 8 个 ID（512/513/530/533/551/552/570/571）存在**旧版占位语义**的错位实现（节点名与效果全部对不上 §3.5）；
- 19 个节点无任何代码实现；
- 数据层存在**多重不可达死锁**（分支A 510/511/512/515 连环前置断裂）与**分支C 前置拓扑矛盾**（550 prereq 指向 503 但描述指向 500）；
- 契约源头缺失 `keystone_node_ids` 与 `keystone_exclusion_groups`，导致契约生成器启发式规则全面误标；
- 架构层面因 `ChannelingComponent` 与 `BeamChannelComponent` 双组件脱节，导致 `HeavenlySwordDescent`（天剑降世）的回鞘联动加成**静默失效**（初版误记为符合项，现予纠偏）；
- 现有 4 处测试套件与测试夹具深度固化了错误的旧键节点与契约断言，直接阻碍按设计实现。

汇总评估结果：**7 项 CRITICAL、10 项 HIGH、6 项 MEDIUM**。

### 初版事实性纠偏说明
1. **初版 C4 纠偏**：初版断言点出 533 后弹幕命中会“扣全额 100 法力（L862-870）”。经查验 `skill_contracts_compact.json:218` 及 `skills.json:3313`，`consumes_mana` 当前配置为 `false`，故 `SkillSystem.cpp:860` 的 `if (node_contract->trigger.consumes_mana)` 判定为假，实机**不会扣减法力**。但其通过触发链向 `InfiniteBlades::DoCast` 派发深度为 1 的完整施法、导致 ChannelingComponent/BeamChannelComponent 每 2 秒被完全重置、引导被打断的运行时事故级事实成立。
2. **初版“符合项”纠偏（转入 C7）**：初版将 `HeavenlySwordDescent 对 skill 5 的回鞘联动独立且不冲突` 列为符合项。经深度代码追踪，`HeavenlySwordDescent.cpp:514-519` 的 `CastInfiniteBladesWithHeavenlyFollowUp` 仅修改了 `ChannelingComponent`（`bonus_damage_mult` 与 `is_empowered`），而 `InfiniteBlades::DoCast` 早已先一步创建了 `BeamChannelComponent`；随后在 `BeamChannelDeliverySystem.cpp:164` 中，存在 `BeamChannelComponent` 的实体直接跳过 `ChannelingComponent` 循环——导致**回鞘带来的伤害倍率与强化状态 100% 丢失**，属于严重的静默失效缺陷。

## 输入

| 输入 | 路径 | 说明 |
| --- | --- | --- |
| 设计基准 | `设计文档/职业设计草案_剑修.md` §3.5（L338–393） | 28 节点定义 |
| 基底技能定义 | `assets/data/skills.json` id=5 | tags/mana/cd/talent_tree/内嵌契约 |
| 专精树数据 | `assets/data/skill_5_tree.json` | 28 节点独立文件 |
| 契约配置（源头） | `assets/data/skill_contracts_compact.json` skill 5 | trigger/synergy/intent/transmuter/resist |
| 契约生成器 | `scripts/gen_skill_contracts.py` | 角色推导规则（原报告路径笔误为 tools/） |
| 机制数值表 | `assets/data/skill_mechanics.json` | 技能机制数值表（技能5条目完全空白） |
| 烘焙层 | `src/game/systems/skill/SkillSpecializationBaker.cpp` | case 5（L528–556）+ SyncTriggerRules |
| 行为层 | `src/game/systems/skill/behaviors/InfiniteBlades.cpp` | 引导/弹幕行为与旧占位节点枚举 |
| 交付系统 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp` | 弹幕发射/锁敌/终结技（双组件冗余） |
| 触发/标签/法力 | `src/game/systems/skill/SkillSystem.cpp` | 触发链/GetEffectiveSkillTags/mana |
| 消费系统 | `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp` | 天剑降世回鞘联动（双组件导致加成丢失） |
| 标签枚举表 | `src/game/foundation/data/TagRegistry.hpp` | Tag 枚举与 TagFromString 解析表 |
| 测试夹具/辅助类 | `tests/fixtures/skill_specialization_keynodes.json`、`tests/SkillKeyNodeMatrixTestHelpers.hpp` | 固化旧键节点矩阵 [530, 533, 552, 570, 571] |
| 契约/单元测试 | `tests/integration/SkillContractRegistryTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp` | 锁定 533 触发、max_transmuters=2、552/571 旧参数 |

## 范围对齐表

| 层 | 文件 | 与 §3.5 对齐结果 |
| --- | --- | --- |
| 节点数据 | `skill_5_tree.json` / `skills.json` talent_tree | 28 节点 ID 齐全；**17 处 max_points 与设计不符**；分支A 存在 4 重前置死锁/断裂；550 前置 ID 与描述矛盾；`sword_skill` 标签大小写错误 |
| 契约 | `skill_contracts_compact.json` skill 5 | **全部角色字段错标**；缺失 `keystone_node_ids` 与 `keystone_exclusion_groups` 配置；max_transmuters=2 违背火冰互斥 |
| 机制参数 | `assets/data/skill_mechanics.json` | **完全缺失**（技能1/2/4已纳管，技能5为空白，违反 UMR 规范） |
| 烘焙 | `SkillSpecializationBaker.cpp:528-556` | 仅 8 节点、全部旧占位语义；572 零烘焙；effective_tags 转化被下游丢弃 |
| 行为 | `behaviors/InfiniteBlades.cpp:16-78` | 枚举 9 节点旧占位名；基底行为不符（单次 0.5s 脉冲 vs 持续引导 drain） |
| 交付 | `BeamChannelDeliverySystem.cpp` | 扇形水平直射（应屏幕随机落雷）；effective_tags 硬编码 Physical；双组件维护且跳过 Channeling |
| 消费联动 | `HeavenlySwordDescent.cpp` | 包装函数修饰 ChannelingComponent，但交付系统消费 BeamChannelComponent，回鞘加成丢失 |
| 测试 | 矩阵/夹具/Guard/Baker 测试 | 4 处测试套件与夹具硬编码锁定旧占位语义与错标契约；无 `InfiniteBladesNodes.cpp` |

## 质量风险

1. **分支A 全线瘫痪与多重死锁**：
   - 500 max_points=1（设计 4），但 510 要求 500×2；
   - 510 设计为 1 点，但 511 要求 510×2；
   - 511 设计为 3 点，但 512 要求 511×4；
   - 512 max_points=1（设计 5），但 515 要求 512×3；
   整条分支A 处于四重锁死的永久不可达状态。
2. **运行时施法自毁循环**：C4 错误触发规则使玩家点出 533 后，每次弹幕命中（2s ICD）均通过触发引擎重新以 depth=1 派发万剑归宗施法，强行重置并打断玩家当前正在进行的引导状态。
3. **回鞘联动静默失效（C7）**：天剑降世的 ConsumeReturnToSheathBonus 无法将增伤与 is_empowered 传递给 BeamChannelComponent，形成隐蔽的无收益暗坑。
4. **火冰流派非法双持与标签截断**：
   - 缺失 `keystone_exclusion_groups` 使得玩家在专精分配界面可同时点亮 570 与 572；
   - 交付层硬编码 `Tag::Physical`，即便转质成功也无法生效对应元素抗性与伤害缩放。
5. **测试套件反向拦截风险**：
   - `SkillContractRegistryTests.cpp` 强制断言技能5触发节点必须为 533、max_transmuters 必须为 2；
   - `skill_specialization_keynodes.json` 固化旧 5 键节点；
   一旦修复数据，CI 将发生大面积红屏阻断。

---

## 发现项

### CRITICAL

- **C1（数据·分支A四重不可达死锁）** `skills.json` 与 `skill_5_tree.json`：
  1. 500「剑雨绵绵」`max_points=1`（设计 0/4），下游 510「神识锁定」前置要求 `{node_id: 500, required_points: 2}` → 第一重死锁；
  2. 510「神识锁定」设计为 0/1 点 Keystone/Passive，但 511「无处遁形」前置要求 `{node_id: 510, required_points: 2}`（若按设计将 510 纠偏为 1 点，511 依旧不可达）→ 第二重死锁；
  3. 511「无处遁形」设计为 0/3 点，但 512「天降命印」前置要求 `{node_id: 511, required_points: 4}`（4 > 3，设计要求为 2/3）→ 第三重死锁；
  4. 512「天降命印」`max_points=1`（设计 0/5），下游 515「万剑归阵」前置要求 `{node_id: 512, required_points: 3}` → 第四重死锁；
  5. 513「天诛」前置要求 `{node_id: 512, required_points: 1}`，而设计明确要求 `{需求: 天降命印 4/5}`。
  分支A 彻底沦为数据死路。
- **C2（契约·源头配置严重缺失与角色错标）** `skill_contracts_compact.json` skill 5：
  - **缺失关键配置字段**：
    - 完全缺失 `keystone_node_ids`：导致 `scripts/gen_skill_contracts.py` 中 `explicit_keystone_ids` 为空，触发兜底规则 `not explicit_keystone_ids and max_points == 1 -> Keystone`，将 500/512/555 等普通单点节点全误标为 Keystone；
    - 完全缺失 `keystone_exclusion_groups`：未能配置 `{"570": 1, "572": 1}`，导致 `SkillSystem.cpp:2059-2080` 无法执行跨转质互斥退还；
  - **角色错标**：
    - `trigger_nodes: [{node_id: 533,...}]` → 应为 **513 天诛**（533 是巨剑术 Keystone）；
    - `synergy_node_ids: [530]` → 应为 **[515] 万剑归阵**（530 是气定神闲普通被动）；
    - `sword_intent_node_ids: [552]` → 应为 **[553, 554, 555]**（552 是随影圆形落点，与剑意无关）；
    - `transmuter_node_ids: [570, 571]` → 应为 **[570, 572]**（571 是末日余烬燃烧，572 才是冰霜转质）；
    - `resist_models/scope_policies` 仅有 572，且挂错（应为 574 灵根感应挂 `TypeD_StatToPenetration`）；
    - `max_transmuters: 2` → 应为 **1**（火/冰互斥）。
  `skills.json` 内嵌契约因同步生成而全面污染，8 处 key-nodes 角色错标（500/512/555 误标 Keystone，513 误标 Keystone，530 误标 Synergy，533 误标 Trigger，572 误标 Keystone，571 误标 Transmuter）。
- **C3（实现·覆盖为零且语义错位）** Baker case 5（`SkillSpecializationBaker.cpp:528-556`）仅处理 8 个 ID，语义全部属于旧占位符：512→快速引导 / 513→终结技 / 530→全屏索敌 / 533→穿心 / 551→剑意爆发 / 552→意念加成 / 570→转元素 / 571→穿透。行为层 `behaviors/InfiniteBlades.cpp:16-26` 枚举对应完全过时的节点名。§3.5 设计的 28 节点语义**实现度为 0%**。
- **C4（契约错标→运行时自毁打断）** 契约 `trigger_nodes` 指向 533 且 `trigger_skill_id=5`（自触发）+ ICD 2.0s：
  当玩家点出「巨剑术」533 后，`SyncTriggerRules`（Baker L645-695）向玩家注册 OnSkillHit 触发规则。弹幕每 2 秒命中敌人，即通过 `SkillSystem.cpp:874-901` 生成 `trigger_exec.skill_id = 5`。触发施法派发进入 `InfiniteBlades::DoCast`，直接调用 `emplace_or_replace<BeamChannelComponent>` 与 `emplace_or_replace<ChannelingComponent>`，**强行重置并中断玩家当前正在进行的引导状态**。真正的天诛 513 没有任何触发配置。
  *(注：初版关于 L862-870 扣除 100 蓝的描述已查明，因 consumes_mana=false 暂未扣蓝，但引导重置自毁事实确凿)*。
- **C5（冰霜流转质全链路失效）**
  1. 572 在 Baker case 5 中完全缺失 `else if (node_id == 572)` 分支；
  2. `skills.json` 中 572 缺失 `add_tags: ["Cold"]` 与 `remove_tags: ["Physical"]`；
  3. 契约 `transmuter_node_ids` 未列入 572，且其角色被误标为 Keystone，导致 `SkillSystem::GetEffectiveSkillTags`（L2248-2260）查找 `selected_transmuter` 时完全忽略 572；
  4. 最终导致玩家即便点出 572，技能标签依然为物理，下游 573/574/575 冰霜分支全灭。
- **C6（火流星伤害标签断裂与属性截断）**
  1. 570 虽然在 Baker 写入 `out_profile.effective_tags`，但在 `BeamChannelDeliverySystem.cpp:110` 中，投射物 payload 的标签被**硬编码为 `ctx.effective_tags = Tag::Physical;`**；
  2. `skills.json` 中 570 同样缺少 `add_tags: ["Fire"]`；
  3. 伤害管线 `ResolveDamage` 接收到 Physical 标签，导致玩家受到火抗削弱或穿透增益无法生效，天火流星在数值结算上依旧是物理伤害。
- **C7（回鞘联动穿透静默失败，初版纠偏项）**
  `HeavenlySwordDescent.cpp:502-520` 定义的 `CastInfiniteBladesWithHeavenlyFollowUp` 试图在万剑归宗施放后应用回鞘强化：
  ```cpp
  auto *chan = registry.try_get<ChannelingComponent>(owner);
  chan->bonus_damage_mult *= 1.0f + bonus_mult;
  chan->is_empowered = true;
  ```
  但 `s_originalInfiniteBladesCast`（`InfiniteBlades::DoCast`）在之前已经构造了 `BeamChannelComponent`，并且其 `beam.is_empowered` 与 `beam.bonus_damage_mult` 取自修饰前的原始值。更致命的是，`BeamChannelDeliverySystem.cpp:164` 规定：
  ```cpp
  if (registry.any_of<BeamChannelComponent>(entity)) {
    continue; // 只要有 BeamChannelComponent，完全忽略 ChannelingComponent 循环！
  }
  ```
  这导致回鞘逻辑对 `ChannelingComponent` 所做的所有增伤与强化修改**被交付系统 100% 忽略**，回鞘联动实质处于死代码状态。

---

### HIGH

- **H1（基底行为与资源模型根本冲突）**
  - **交付形式**：实机为玩家面朝方向 ±20° 扇形直射投射物（速度 1000）；设计为**屏幕内随机位置轰击天降剑气**（落地式）；
  - **发射参数**：实机每 tick(0.5s) 发射 2 枚（强化时 4 枚），每枚 35% 物理；设计为**每 0.3s 发射 3 枚，每枚 40% 物理**；
  - **资源与引导模型**：实机为一次性扣除 100 法力，固定引导 0.5s（`max_channel_time=0.5` 单次脉冲）；设计为**持续引导技能，引导期间每秒消耗 20 法力 (drain)**。
- **H2（分支A 自动索敌与集中 零实现）**
  510 神识锁定（光标索敌 + 法耗+30%）、511 无处遁形（锁定半径 + 下落加速）、512 天降命印（命印叠层 + 5层溅射）、513 天诛（满层命印 2s ICD 触发 300% 必爆穿透主剑）、514 剑刃风暴（击杀分裂 30-90%）、515 万剑归阵（剑阵范围落剑集中 + 剑阵计时暂停）全部未实现。`BeamChannelDeliverySystem` 的 aim_assist 存在但 DoCast 从未开启。
- **H3（分支B 引导深度与防御 零实现）**
  530 气定神闲（引导减伤 6-24%）、531 不坏剑身（每秒 15-45 护甲叠层，上限 10 层）、532 剑气充盈（引导每秒回复 10-30 Ward）、533 巨剑术（数量减半、体积+100%、伤害+150%、范围重击）、534 天剑降世（引导>=2s 结束时召唤 800% 范围巨剑+击飞）、535 余波（天剑范围+20-60%、必晕普通怪）全部未实现。
- **H4（分支C 意动乾坤与移动设定 零实现）**
  550 御剑行（引导移动、移速-40%、禁位移技能）、551 御剑风雷（御剑步引导无视惩罚、闪避+50-150）、552 随影（自身周围固定半径环形落剑）、553 剑意回流（击杀/10连击回复 1-4 剑意）、554 意气爆发（满层剑意启动耗尽→全剑气+100% 暴击率）、555 意念合一（满层爆发暴伤+20-80%）全部未实现。
- **H5（分支D 天地异象 零实现）**
  571 末日余烬（陨石落地 3s 燃烧 DoT，每秒 10-30% 命中伤害）、573 绝对零度（暴雪冻结延长 0.1-0.3s，击碎伤害+15-45%）、574 灵根感应（Int 转对应元素穿透，上限 30%，Type D）、575 天灾（对应异常几率+30-90%）全部未实现。
- **H6（基底四节点无效果）**
  500（法耗减免）、501（引导时长增频至+75%）、502（伤害增加+微小溅射）、503（移动惩罚降低）在 Baker 无处理且 `stat_modifiers` 全空，为纯死节点。
- **H7（技能根级标签大小写缺陷与规范不符，新增）**
  `assets/data/skills.json` line 2780 中，技能5的 tags 字段写入了蛇形 `"sword_skill"`。
  根据 `src/game/foundation/data/TagRegistry.hpp` 中的 `TagFromString` 实现：
  ```cpp
  if (name == "SwordSkill") return Tag::SwordSkill;
  ```
  对 `"sword_skill"` 返回 `std::nullopt`。导致在游戏启动解析时，**万剑归宗完全丢失了 `Tag::SwordSkill`**，无法享受任何御剑类被动、装备及词缀的全局加成（此为技能1同款漏洞复现）。此外，skills.json 配有 `Area` 与 `Hit`，但缺少 `Channeled` 语义对齐（设计为 `[Spell], [Channeling], [Physical], [Global]`）。
- **H8（机制参数表完全空白，违反 UMR 规范，新增）**
  `assets/data/skill_mechanics.json` 专为外置节点机制参数而设（技能 1/2/4 均已建立严密字典）。但该文件中**完全没有技能5的任何条目**。若直接在 C++ 编码中硬编码数值，将严重违反项目规范（架构规范要求：机制数值由运行时动态读取，禁止 C++ 写死）。
- **H9（分支C 前置拓扑与描述文本冲突，新增）**
  `assets/data/skills.json:3037` 及设计草案 §3.5 中，550「御剑行」描述文本均为：
  `*{需求: 剑雨绵绵 3/4}*`（剑雨绵绵为 500 号节点）。
  然而在 `skills.json:3043` 与 `skill_5_tree.json:205` 的实际拓扑中，其配置为：
  `prerequisites: [{"node_id": 503, "required_points": 3}]`（503 为灵动引导）。
  拓扑逻辑要求 4 个基底节点各自引申一条分支（500→A，501→B，502→D，503→C），503（移速惩罚降低）引申 550（移动施法）在逻辑上完全自洽，但描述文本产生直接误导，需统一规范修正。
- **H10（测试护栏深度固化错误状态，新增）**
  除 `SkillBehaviorGuardTests.cpp` 外，代码库中存在多处测试套件与测试夹具深度硬编码并锁定了旧版错误：
  1. `tests/fixtures/skill_specialization_keynodes.json:26`：配置 `"key_nodes": [530, 533, 552, 570, 571]`；
  2. `tests/SkillKeyNodeMatrixTestHelpers.hpp:111`：硬编码 `{5, {530, 533, 552, 570, 571}}`；
  3. `tests/integration/SkillContractRegistryTests.cpp:244`：触发节点断言硬编码 `expected_trigger_nodes[4] = 533`（应为 513）；
  4. `tests/integration/SkillContractRegistryTests.cpp:259-260`：互斥断言硬编码 `CHECK(contract->max_transmuters == ((skill_id == 3 || skill_id == 4) ? 1 : 2));`（技能5收束为 1 后此处必崩）；
  5. `tests/unit/SkillSpecializationBakerTests.cpp:227-240`：单测显式测试 552 `bonus_crit` 与 571 `armor_pen` 旧字段。
  任何数据修复若不同步迁移上述测试，将导致持续构建红屏。

---

### MEDIUM

- **M1（契约生成源头配置缺陷与路径笔误）**
  契约生成脚本实际路径为 `scripts/gen_skill_contracts.py`（初版报告笔误写为 `tools/`）。脚本内部推导规则依赖 `explicit_keystone_ids`，若 compact 不提供 `keystone_node_ids`，脚本会将所有 1 点节点误推为 Keystone。整改必须以修正 compact 配置为首要源头。
- **M2（功能测试缺口）**
  目前完全缺失 `tests/functional/InfiniteBladesNodes.cpp`（对标 `BladeFormationNodes.cpp` 20+ 用例模式）；现有测试全是针对旧占位语义的伪防护。
- **M3（注释误导性残留）**
  `SkillDefs.hpp:1197-1216` 中 `ChannelingComponent` 的结构体字段注释仍旧保留：
  `extra_projectiles = false; // Talent 551`、`burst_finisher = false; // Talent 513`、`full_screen_lock = false; // Talent 530`，与 §3.5 编号完全冲突。
- **M4（双组件维护冗余与时序隐患）**
  `ChannelingComponent` 与 `BeamChannelComponent` 同时被 `InfiniteBlades::DoCast` 写入，但在 `BeamChannelDeliverySystem` 中二者互斥执行。双组件并存不仅造成内存与计算浪费，还直接诱发了 C7 的回鞘联动丢失缺陷，重写时必须彻底统一。
- **M5（旧占位代码内部 Bug 留档）**
  旧 552（MindUnify）在 551 消耗剑意之后才读取剑意层数，导致 stacks 恒为 0。该逻辑应随旧代码废除而直接移除。
- **M6（前置校验 OR 语义核准）**
  核实 `SkillSystem.cpp:2034-2051` 的前置检查逻辑，对于拥有多个前置条目的节点（如 574 灵根感应前置为 570 与 572），当前引擎代码采用只要任意一个前置满足（`pre_pts >= required_points`）即通过检查的 OR 语义。这与设计草案「需求: 任意 Transmuter」完全契合，数据层可安全保留双前置数组。

---

## 逐节点核对矩阵（28 节点）

图例：✅ 按 §3.5 实现｜⚠️ 部分实现/数据偏差｜✖️ 错位实现（旧占位语义）｜❌ 未实现｜🚫 不可达死锁

| 节点 | 设计（点数/效果） | skills.json 现状 | 契约角色 | 代码实现 | 判定与缺陷定位 |
| --- | --- | --- | --- | --- | --- |
| 500 剑雨绵绵 | 0/4 引导法耗 -10..40% | max=1 ❌ | Passive | 无 | ❌ H6, J9(max=1), J10(死锁头) |
| 501 剑意共鸣 | 0/5 引导提频 +15..75% | max=5 ✓ | Passive | 无 | ❌ H6 |
| 502 陨铁 | 0/5 伤害+10..50%+溅射 | max=5 ✓ | Passive | 无 | ❌ H6 |
| 503 灵动引导 | 0/4 移速惩罚 -10..40% | max=4 ✓ | Passive | 无 | ❌ H6 |
| 510 神识锁定 | 0/1 鼠标锁敌+法耗+30% | max=5 ❌，需500×2 ❌ | Passive | 无 | 🚫 C1, H2, J9, J10 |
| 511 无处遁形 | 0/3 半径+15..45%+落速 | max=5 ❌，需510×2 ❌ | Passive | 无 | 🚫 C1, H2, J9, J16(需510×2) |
| 512 天降命印 | 0/5 命印叠层+5层溅射 | max=1 ❌，需511×4 ❌ | Keystone ❌ | ✖️ 旧「快速引导」 | 🚫 C1, H2, J9, J16(需511×4) |
| 513 天诛 | 0/1 Trigger 满层主剑 300% | max=1 ✓，需512×1 ❌ | Keystone ❌ | ✖️ 旧「终结技」，无触发链 | 🚫 C1, C2, C4, H2, J16(需512×4) |
| 514 剑刃风暴 | 0/3 击杀分裂 30..90% | max=5 ❌，需510×1 ✓ | Passive | 无 | 🚫 C1(受510不可达影响), H2, J9 |
| 515 万剑归阵 | 0/1 Synergy 剑阵轰击+暂停 | max=5 ❌，需512×3 ❌ | Passive ❌ | 无 | 🚫 C1, C2, H2, J9, J3 |
| 530 气定神闲 | 0/4 引导减伤 6..24% | max=5 ❌，需501×3 ✓ | Synergy ❌ | ✖️ 旧「全屏索敌」 | ✖️ C2, H3, J9, J3 |
| 531 不坏剑身 | 0/3 护甲叠层 (max 10) | max=3 ✓，需530×2 ✓ | Passive | 无 | ❌ H3 |
| 532 剑气充盈 | 0/3 引导回复 10..30 Ward | max=3 ✓，需530×2 ✓ | Passive | 无 | ❌ H3 |
| 533 巨剑术 | 0/1 Keystone 数量减半+巨剑 | max=5 ❌，需502×4 ✓ | Trigger ❌ | ✖️ 旧「穿心」暴击+自打断施法 | ✖️ C2, C4, H3, J9, J2 |
| 534 天剑降世 | 0/1 Keystone 2s引导终结巨剑 | max=1 ✓，需530×3 ✓ | Keystone ✓ | 无（仅被回鞘包装，且回鞘失效） | ❌ C7, H3 |
| 535 余波 | 0/3 天剑范围+必晕普通怪 | max=5 ❌，需534×1 ✓ | Passive | 无 | ❌ H3, J9 |
| 550 御剑行 | 0/1 Keystone 移动施法/移速-40% | max=1 ✓，需503×3 ⚠️ | Keystone ✓ | 无（无移动引导逻辑） | ❌ H4, H9(503 vs 500矛盾) |
| 551 御剑风雷 | 0/3 御剑步免罚+闪避 | max=3 ✓，需550×1 ✓ | Passive | ✖️ 旧「剑意爆发」额外投射物 | ✖️ H4 |
| 552 随影 | 0/1 环形落剑区域 | max=5 ❌，需550×1 ✓ | Passive(intent=true❌) | ✖️ 旧「意念加成」增伤暴击 | ✖️ C2, H4, J9, J4 |
| 553 剑意回流 | 0/4 击杀/连击回剑意 | max=5 ❌，需550×1 ✓ | Passive | 无 | ❌ C2, H4, J9, J4 |
| 554 意气爆发 | 0/1 满剑意消耗→100%暴击 | max=5 ❌，需553×2 ✓ | Passive | 无 | ❌ C2, H4, J9, J4 |
| 555 意念合一 | 0/4 意气爆发暴伤+20..80% | max=1 ❌，需554×1 ✓ | Keystone ❌ | 无 | ❌ C2, H4, J9, J4 |
| 570 天火流星 | 0/1 Transmuter 转火/低频强力 | max=5 ❌，需502×2 ✓ | Transmuter ✓ | ✖️ 烘焙转质但交付硬编码物伤 | ✖️ C2, C6, J9, J13 |
| 571 末日余烬 | 0/3 燃烧火毯 DoT | max=5 ❌，需570×1 ✓ | Transmuter ❌ | ✖️ 旧「穿透 armor_pen」 | ✖️ C2, H5, J9, J5 |
| 572 凛冬暴雪 | 0/1 Transmuter 转冰/高频寒冷 | max=1 ✓，需502×2 ✓ | Keystone ❌ | 无（Baker无分支，标签断裂） | ❌ C2, C5, J5, J13 |
| 573 绝对零度 | 0/3 冻结延长+击碎增伤 | max=5 ❌，需572×1 ✓ | Passive | 无 | ❌ H5, J9 |
| 574 灵根感应 | 0/4 Type D 属性转穿透 | max=5 ❌，需570/572×1 ✓ | Passive | 无（Type D 未接线） | ❌ C2, H5, J9, J7 |
| 575 天灾 | 0/3 异常几率 +30..90% | max=3 ✓，需574×2 ✓ | Passive | 无 | ❌ H5 |

**统计汇总**：
- **实现判定**：✅ 按设计实现 0 ｜ ✖️ 错位实现 8 ｜ ❌ 零实现 20 ｜ 🚫 不可达死锁 5（510/511/512/513/515）；
- **点数偏差**：17 处 `max_points` 与设计不符；
- **前置拓扑错误**：510 需 500×2、511 需 510×2、512 需 511×4、513 需 512×1（设计为4）、515 需 512×3、550 需 503×3（描述为500×3）；
- **数值外置**：全 28 节点在 `skill_mechanics.json` 中为 0 记录，`stat_modifiers` 全空。

---

## JSON 矛盾清单 (J1 - J16)

| # | 矛盾项 | 发生位置 | 根因与影响 |
| --- | --- | --- | --- |
| J1 | 28节点结构一致性 | `skill_5_tree.json` ≡ `skills.json.talent_tree` | 两侧数据同步错误，同病相怜 |
| J2 | Trigger 错配与自触发 | `skill_contracts_compact.json` + 内嵌契约 | trigger=533（巨剑术）且 trigger_skill_id=5，形成自打断死循环 |
| J3 | Synergy 角色错配 | `skill_contracts_compact.json` + 内嵌契约 | 误标 530（气定神闲），真实 Synergy 节点 515（万剑归阵）被标为普通被动 |
| J4 | 剑意交互节点遗漏 | `skill_contracts_compact.json` + 内嵌契约 | 误标 552（随影），真实剑意节点 553/554/555 全被忽略 |
| J5 | Transmuter 节点配置错乱 | `skill_contracts_compact.json` + 内嵌契约 | 错标 [570, 571]，571（燃烧DoT）被当成转质，572（冰霜转质）被当成 Keystone |
| J6 | max_transmuters=2 | `skill_contracts_compact.json` + 内嵌契约 | 破坏火/冰互斥核心规则，允许双持转质 |
| J7 | Type D 抗性削弱挂错 | `skill_contracts_compact.json` + 内嵌契约 | 仅 572 配置了 TypeD_StatToPenetration，570 缺失，真实应挂 574 |
| J8 | 内嵌 key-nodes 角色 8 处错标 | `skills.json.skill_contract.nodes` | 由生成器启发式错误推导出的次生污染 |
| J9 | 17 处 max_points 与设计脱节 | `skills.json` / `skill_5_tree.json` | 机械化填表，严重扭曲技能成长曲线 |
| J10 | 510 死锁：500 max=1 vs req=2 | `skills.json` / `skill_5_tree.json` | 分支A 起点即锁死 |
| J11 | 标签拼写错误：`sword_skill` | `skills.json.tags` | 蛇形小写导致 TagRegistry 丢弃 Tag::SwordSkill |
| J12 | compact 缺失 `keystone_node_ids` | `skill_contracts_compact.json` | 导致脚本将所有单点节点全误算为 Keystone |
| J13 | compact 缺失 `keystone_exclusion_groups` | `skill_contracts_compact.json` | 导致 570 与 572 在加点阶段无法自动退还互斥 |
| J14 | 机制数值表完全缺位 | `assets/data/skill_mechanics.json` | 违反 UMR 规范，机制数值无法外置配置 |
| J15 | 550 前置 ID 与描述冲突 | `skills.json` / `skill_5_tree.json` | prereq 指向 503，描述写 500 |
| J16 | 511/512/513 前置点数与上限逻辑崩坏 | `skills.json` / `skill_5_tree.json` | 511 需 510×2（上限1）；512 需 511×4（上限3）；513 需 512×1（设计需4） |

---

## 架构偏差

1. **行为层与交付层遗留化石严重**：
   `InfiniteBlades.cpp` 与 `BeamChannelDeliverySystem.cpp` 充满早期旧版方案的临时枚举与字段，与 §3.5 毫无交集，必须彻底重写而非局部补丁。
2. **双组件脱节导致联动静默丢失（C7）**：
   当前架构对技能5同时挂载 `ChannelingComponent` 与 `BeamChannelComponent`，而外部修饰（如 `HeavenlySwordDescent`）仅更新前者，交付系统却独占式消费后者，导致修饰逻辑完全断链。必须统一由单一引导组件驱动。
3. **交付载荷截断元素转化链路**：
   Baker 仅把元素转化写到 `profile.effective_tags`，交付系统未读取 profile 的有效标签，反而硬编码 `ctx.effective_tags = Tag::Physical`，导致转质在结算层全灭。
4. **脉冲施法与持续引导 Drain 资源模型脱节**：
   万剑归宗设计为持续引导技能（每秒消耗 20 法力），其攻速提升（501）、护甲叠加（531）、剑意回复（553）均深度依赖持续引导的时长；现行代码为单次 0.5s 脉冲施法（一次性扣 100 蓝），无法承载设计机制。
5. **契约生成器启发式规则脆弱**：
   `scripts/gen_skill_contracts.py` 过于依赖 `max_points == 1` 作为 Keystone 判别条件，在源头 compact 缺乏显式列表时会产生毁灭性的角色漂移。
6. **机制参数未接入 SkillMechanicsRegistry**：
   违反《设计文档》与架构规约，缺少在 `skill_mechanics.json` 中的参数注册，阻碍后续数值平衡与热更。

---

## 符合项

- 28 个节点的 ID、命名、树状位置坐标在 `skill_5_tree.json` 与 `skills.json` 间保持一致；
- `ResolveElementalConversion` 已支持 570→Fire / 572→Cold（颜色与元素底座完备）；
- 引擎底层的 TriggerRuleComponent、Synergy、剑意管理、Keystone 互斥、TypeD 穿透消费框架均已在技能1/2/3/4跑通，数据修正后即可直接接通；
- 多前置的 OR 判定语义（`SkillSystem.cpp:2034-2051`）原生支持 574 任意转质前置校验；
- 当前构建 `build.bat RelWithDebInfo` 零报错，无死编译错误。

---

## 整改路线（建议顺序）

### 阶段一：数据层与契约源头校准（消除 C1 / C2 / H7 / H8 / J 系）
1. **修正 `skill_contracts_compact.json` (skill 5)**：
   - 显式配置 `keystone_node_ids: [533, 534, 550]`；
   - 修正 `trigger_nodes: [{node_id: 513, trigger_skill_id: 5, effectiveness: 3.0, internal_cooldown: 2.0, consumes_mana: false, requires_crit: false}]`（注：天诛由 513 触发，主剑下落可走特化技能/伴生调用）；
   - 修正 `synergy_node_ids: [515]`；
   - 修正 `sword_intent_node_ids: [553, 554, 555]`；
   - 修正 `transmuter_node_ids: [570, 572]` 与 `max_transmuters: 1`；
   - 增补 `keystone_exclusion_groups: {"570": 1, "572": 1}`；
   - 修正 `resist_models: {"574": "TypeD_StatToPenetration"}` 与对应 `scope_policies`；
2. **运行生成脚本同步**：
   - 执行 `python scripts/gen_skill_contracts.py`，同步刷新 `assets/data/skills.json` 内嵌契约；
3. **校准 `skills.json` 与 `skill_5_tree.json` 节点拓扑与点数**：
   - 修正 17 处 `max_points`（如 500 改为 4，510 改为 1，511 改为 3，512 改为 5 等）；
   - 修复分支A 前置死锁链：510 需 500×2、511 需 510×1、512 需 511×2、513 需 512×4、515 需 512×3；
   - 统一 550 前置：确认走 503×3 并将描述文本修正为 `*{需求: 灵动引导 3/4}*`；
   - 修正技能5根级标签：`"sword_skill"` → `"SwordSkill"`，补充转质节点 570 与 572 的 `add_tags`/`remove_tags`；
4. **纳管机制数值**：
   - 在 `assets/data/skill_mechanics.json` 中补全技能 5（500-575）全部专精数值节点。

### 阶段二：测试护栏与夹具同步解除（消除 H10 / M2）
1. 更新 `tests/fixtures/skill_specialization_keynodes.json`，将技能5的 `key_nodes` 纠偏为 `[513, 515, 533, 534, 550, 553, 554, 555, 570, 572, 574]`；
2. 更新 `tests/SkillKeyNodeMatrixTestHelpers.hpp:111` 与上述对齐；
3. 修正 `tests/integration/SkillContractRegistryTests.cpp`：
   - L244 expected_trigger_nodes 中技能5改为 `513`；
   - L259-260 互斥断言纳入技能5：`((skill_id == 3 || skill_id == 4 || skill_id == 5) ? 1 : 2)`；
4. 调整 `SkillBehaviorGuardTests.cpp` 与 `SkillSpecializationBakerTests.cpp` 中锁定旧 552/571 占位行为的单测。

### 阶段三：引导管线与行为层重写（消除 C3 / C7 / H1 / M4）
1. **统一引导交付组件**：
   彻底清理双组件混乱，确保 `HeavenlySwordDescent` 能正确作用于万剑归宗生效中的交付载荷；
2. **重写 `InfiniteBlades.cpp`**：
   - 实现每秒 20 点持续法力 Drain（蓝尽引导中断）；
   - 基底设为每 0.3s 3 枚天降落剑，基础伤害 40% 物理；
   - 接入 553/554/555 剑意消耗与加成机制；
   - 接入 550 移动引导与 552 圆形降剑。

### 阶段四：交付层与转质贯通（消除 C5 / C6 / H2 / H3 / H5）
1. `BeamChannelDeliverySystem.cpp` 改造：
   - 区分 Barrage 直射与 Skyfall 天降落剑模式；
   - 载荷 `ctx.effective_tags` 严格继承 Baker 产出的 effective_tags，使 570 火流星与 572 凛冬暴雪正确结算元素抗性与伤害；
   - 接入 510/511 光标锁敌（aim_assist）；
   - 接入 512 命印（FateMark）叠层组件与 513 主剑下落调用；
   - 接入 534 天剑降世（引导>=2s 结束时轰击）与 535 余波必晕；
   - 接入 571 燃烧地表与 574 Type D 属性转穿透。

### 阶段五：功能测试套件建设
- 新建 `tests/functional/InfiniteBladesNodes.cpp`，编写 28 节点的完整功能集成测试用例，覆盖引导耗蓝、前置加点、转质互斥、命印天诛触发、剑阵暂停联动等全场景。

---

## 证据命令摘要

```powershell
# 1. 验证 Tag 大小写缺陷
rg -n "sword_skill" assets/data/skills.json
rg -n "SwordSkill" src/game/foundation/data/TagRegistry.hpp

# 2. 验证机制数值表缺位
rg -n '"5":' assets/data/skill_mechanics.json

# 3. 验证测试套件中的硬编码阻碍
rg -n "533" tests/integration/SkillContractRegistryTests.cpp
rg -n "max_transmuters" tests/integration/SkillContractRegistryTests.cpp
rg -n "530,\s*533" tests/fixtures/skill_specialization_keynodes.json

# 4. 验证回鞘联动断链
rg -n "ChannelingComponent" src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp
rg -n "BeamChannelComponent" src/game/systems/skill/BeamChannelDeliverySystem.cpp -A 5

# 5. 编译与测试运行（RelWithDebInfo 模式，过滤输出）
.\build.bat
bin\NoMoreDayTests.exe "--test-case=*Contract key nodes map to runtime state*"
```

---

# 跟进审查（第二轮 · 2026-09-09）

- **审查目标**: 复核工作区未提交变更对第一轮 23 项发现（7 CRITICAL / 10 HIGH / 6 MEDIUM）的整改情况。
- **变更边界**: 21 个文件修改 + `tests/functional/InfiniteBladesNodes.cpp` 新增（+1232 / -175），覆盖数据层（skills/skill_5_tree/skill_contracts_compact/skill_mechanics/mastery_skill_trees）、生成脚本、烘焙层（Baker）、行为层（InfiniteBlades）、交付层（BeamChannelDeliverySystem）、触发层（SkillSystem InitHooks）、消费联动（HeavenlySwordDescent、DamagePipeline、DamageMitigationService、Buff.hpp）、测试 6 文件。
- **设计基准**: `设计文档/职业设计草案_剑修.md` §3.5（L338–393）。

## 第二轮验证证据

```powershell
.\build.bat                                   # EXIT=0，RelWithDebInfo 全量构建成功
ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(skill|combat|ci.nonperf)" --output-on-failure
# 7/7 passed：ci.nonperf、combat.unit/integration/parity/perf-baseline、skill.unit/integration
bin\NoMoreDayTests.exe "--test-case=[Functional] Skill 5*"
# 14 cases / 141 assertions 全部通过（含新增 InfiniteBladesNodes.cpp 12 用例）
```

## 第一轮发现整改对照

| 发现 | 状态 | 依据 |
| --- | --- | --- |
| C1 前置死锁 | ✅ 已修复 | `skill_5_tree.json`：511 req 510×2→×1、512 req 511×4→×2、513 req 512×1→×4；500 max=4、511 max=3、512 max=5；`InfiniteBladesNodes.cpp` 拓扑用例（L95–153）覆盖 |
| C2 契约源头缺配置 | ✅ 已修复 | `skill_contracts_compact.json` skill5：keystone `[533,534,550]`、exclusion `{570:1,572:1}`、synergy `[515]`、sword_intent `[553,554,555]`、transmuter `[570,572]`/max=1、resist_models 574、trigger 513 eff=3.0 ICD=2.0；内嵌契约同步 |
| C3 实现度 0% | 🟡 大部分修复 | Baker case 5 重写 28 节点分支，行为/交付/管线贯通；残留见 N7 |
| C4 自打断死循环 | ✅ 已修复 | `InfiniteBlades.cpp` DoCast `exec.trigger_depth > 0` 分支生成独立主剑（velocity (0,1200)、radius 70、crit=1.0、more=3.0×eff/3），不触碰引导组件；533 降级 Keystone；功能用例断言引导存活 |
| C5 572 转质断链 | 🟡 部分修复 | 转质链贯通（Baker→effective_tags→Convert 修饰器）+ `add_tags Cold`；高频/寒冷强度/冻结几率未实现（N7） |
| C6 标签硬编码 | ✅ 已修复 | 交付层 `ctx.effective_tags` 继承 profile 与 conversion_tag；测试断言 Fire 投射物且无 Physical |
| C7 回鞘加成丢失 | ✅ 已修复 | `HeavenlySwordDescent.cpp:512–520` 同时更新 Channeling 与 BeamChannel 双组件；测试断言两者均被赋值 |
| H1 交付形式/资源模型 | 🟡 部分修复 | 0.3s/3 枚/40% 伤害/5s 上限/按秒 drain 模型建立；drain 数值 5 倍偏差（N1）；基底形态仍扇形直射（N13） |
| H2 分支A 零实现 | 🟡 大部分修复 | 510/511/512/513/514/515 全链实现并有测试；残留溅射基数与门控问题（N2、N12） |
| H3 分支B 零实现 | 🟡 大部分修复 | 530/531/532/533/534/535 实现；531 护甲数值缺失（N4） |
| H4 分支C 零实现 | 🟡 大部分修复 | 551/552/553/554/555 实现；550 御剑行整套未实现（N7）、551 闪避生命周期缺陷（N5） |
| H5 分支D 零实现 | 🟡 大部分修复 | 571/573/574/575 实现；571 数值 2 倍偏差（N8）、574 系数硬编码（N9） |
| H6 基底 4 节点死置 | 🟡 部分修复 | 500 法耗减免/501 提频/502 增伤已接；502 溅射无消费、503 无逻辑（N7） |
| H7 标签蛇形拼写 | 🟡 部分修复 | `sword_skill`→`SwordSkill` 已修；Channeled 标签未见补充 |
| H8 机制数值表空白 | 🟡 大部分修复 | `skill_mechanics.json` 技能5 28 节点全参数化；但 532/534/551/553/573/575/574 消费端硬编码不读数据（N9） |
| H9 550 前置冲突 | ⚠️ 修复方向存疑 | desc 改为"灵动引导 3/4"对齐 prereq 503×3，与上轮报告阶段一第 3 条建议一致；但该建议与设计 §3.5 L376（御剑行需**剑雨绵绵 3/4** = 500×3）冲突——**上轮报告建议有误，本轮更正**（N6） |
| H10 测试固化旧错 | ✅ 已修复 | fixtures/helpers/ContractRegistry/Guard/Matrix/Baker 六处全部更新为新节点集 |
| M1 生成脚本启发式 | ✅ 已处理 | `explicit_passive_ids` + `max_points==1 被动`泛化；副作用见 N10 |
| M2 缺功能测试 | ✅ 已修复 | `tests/functional/InfiniteBladesNodes.cpp` 630 行 12+ 用例，GLOB_RECURSE 自动纳入并真实执行 |
| M3 注释残留 | ❌ 未整改 | `SkillDefs.hpp:1197-1216` ChannelingComponent 注释仍写 Talent 551/513/530 旧语义 |
| M4 双组件冗余 | 🟡 部分收敛 | 生命周期同步删除（交付结束/蓝尽中断双清）；结构冗余仍在 |
| M5 旧 552 bug | ✅ 随重写消除 | — |
| M6 前置 OR 语义 | ✅ 已核准 | — |

## 本轮新发现

### HIGH

- **N1 法力 Drain 5 倍偏差 + 弱断言**：`BeamChannelDeliverySystem.cpp` 中 `mana_rate = 20.0f * (profile->effective_mana_cost / 100.0f)`，而 Baker 对技能5 写死 `effective_mana_cost = 20.0f`（绝对每秒法耗）→ 实际 drain 4/s（500 减免后 2.4–3.6/s），设计要求每秒 20（上轮阶段三第 2 条明确）。新测试 `InfiniteBladesNodes.cpp` L214 断言使用 `epsilon(0.5f)`（doctest 相对容差 50%，容差带 ±97 点法力），4/s 消耗同样通过——**断言未起护栏作用**。修复：`mana_rate = profile->effective_mana_cost`，断言改为直接校验消耗量并收紧容差（epsilon 0.01）。
- **N2 513 天诛门控漏洞（测试驱动生产逻辑）**：`SkillSystem.cpp` InitHooks 中 node_id==513 检查为「有命印且 stacks<5 → 阻断；**无命印 → 放行**」（注释言明为契约矩阵测试假人放行）。生产中打无印小怪每 2s ICD 触发 300% 必爆主剑，满层命印条件被架空，与设计「拥有满层命印的目标被命中时触发」不符。修复：收紧为 `!fateMark || stacks < 5 → blocked`；测试假人场景由测试显式构造满层命印（守护测试已是此做法）或引入专用 bypass 开关。
- **N3 530 减伤不校验技能**：`DamageMitigationService.cpp` 530 块判定 `any_of<BeamChannelComponent>(defender) || any_of<ChannelingComponent>(defender)`，未校验组件 `skill_id == 5` → 玩家引导**任意**技能时均享受 6–24% 减伤（只要拥有技能5 的 530 加点）。修复：读取对应组件 skill_id 校验。
- **N4 531 不坏剑身护甲数值缺失**：交付层每 tick 添加 `SteeledBodyArmor`（DefenseUp，max 10 层）但 `BuffEffect` 无标量数值字段且 `modifiers` 向量未填充；`skill_mechanics.json` 的 `armor_per_sec_per_point: 15.0` 无任何消费；功能测试仅断言 buff 存在与层数，无法暴露缺陷。修复：为 buff 填充 `StatModifier(Armor)`（15×pts/层）。
- **N5 551 闪避直接改 CombatStats 字段**：`InfiniteBlades.cpp:187` `st->dodge_rating += 50.0f * pts_551`。`AttributePipeline.cpp:718-719` 的属性重算会整体覆盖 `dodge_rating` → 加成要么被重算清除、要么（不触发重算时）永久残留并随多次引导无限叠加，5s buff 语义无法保证。修复：改走 `BuffEffect.modifiers`（DodgeRating StatModifier）+ StatsDirty。

### MEDIUM

- **N6 550 前置与设计冲突（更正上轮建议）**：设计 §3.5 L376 御剑行需「剑雨绵绵 3/4」（500×3）；现数据 503×3 + 描述"灵动引导 3/4"。系上轮报告阶段一第 3 条建议（"确认走 503×3 并修正描述"）与设计文档冲突，**本轮更正：以设计文档为准**，prereq 改回 500×3 并同步 desc_key。
- **N7 实现残留**：502 微溅射（`pull_radius=30` 无消费）；503 移速惩罚无逻辑；550 御剑行整套未实现（移动引导/移速-40%/禁位移，上轮阶段三明确要求接入）；570 低频（`frequency_penalty_pct: 0.60` 未消费）；572 高频（`frequency_bonus_pct: 0.50`）与寒冷强度/冻结几率未消费。
- **N8 571 DoT 数值 2 倍偏差**：`pulse_interval=0.5s` × `value_mult=0.10×pts` → 每秒 20%×pts（3 秒合计 60%×pts），设计「**每秒**造成本次命中伤害 10%..30%」。修复：value_mult 按每秒折算（每 pulse 0.05×pts）或 pulse_interval 改 1.0s。
- **N9 UMR 消费残留**：532 ward 速率（10.0）、534（2.0/8.0/120/300）、551（50.0）、553（hits 10）、573（0.10/0.15）、575（30）、574 分段系数（1/12、1/18、1/24、1/30、基准 6%、上限 30）均硬编码于 C++，`skill_mechanics.json` 存在同名参数但无消费——改数据不生效，违反 UMR 精神（当前数值一致，属维护风险）。
- **N10 生成器副作用与代码重复**：gen 脚本泛化规则使**技能1** 内嵌契约新增 130/153/211/230/232 五条目、`mastery_skill_trees.json` 技能1 新增 1115——范围外变更，需确认影响面；`BeamChannelDeliverySystem.cpp` 内 `allocated_points.find(NODE)` 循环重复 6+ 次应提取 helper；Baker 置的多枚 feature_flags（2/4/128/16384/32768/262144/4194304/16777216/67108864）无消费，行为层查 allocated_points、交付层查 flags 的**双源语义分叉**应统一。

### LOW / 风险备忘

- **N11 热路径 Bake 隐患**：`DamageMitigationService.cpp` 574 块在 profile 缺失时于伤害结算路径 fallback `Bake`（全树重烘 + 字符串键 `GetFloat`）；建议保障 bake 生命周期或缓存。
- **N12 512 溅射递归链待确认**：DoHit 5 层溅射经 `ResolveDamage` 携带 `Tag::SecondaryHit`，需确认 DoHit 触发链对 SecondaryHit 的过滤，否则相邻 5 层目标可能连锁。另：535 对**所有**受波及敌人击晕（设计仅普通怪，偏强）；552 落点为随机圆盘（r=10..150）非严格环形；蓝尽中断也触发 534 终结（语义可辩，建议确认）；554 满层硬编码 10；532 `max_barrier=0` 时上限 1000 硬编码；533 `area_radius=70` 直接赋值有覆盖其它来源风险。
- **N13 基底交付形态与设计不符**：§3.5 L340「引导时持续向**屏幕内随机位置**轰击**天降**剑气」；实现为以玩家为中心 ±20° 扇形直射（仅 552 改周围落点、510 锁敌）。上轮阶段四"区分 Barrage 直射与 Skyfall 天降模式"未落地，属视觉/手感层偏差，建议作为后续迭代项显式跟踪。

## 第二轮结论

**修改（Block 合入）**：第一轮 7 项 CRITICAL 全部实质性消除，数据/契约/测试护栏整改扎实（C1/C2/C4/C6/C7/H10/M2 质量高，测试真实执行）。但本轮发现 5 项 HIGH（N1–N5）属新引入或新暴露的实现缺陷，其中 N1 直接违背上轮报告阶段三「每秒 20 点持续法力 Drain」的明确要求且被弱断言掩盖。**不建议在 N1–N5 修复前合入**。

### 修复顺序建议

1. N1：drain 公式一行修复 + 收紧断言（`InfiniteBladesNodes.cpp` L214 epsilon 0.5→0.01，改为断言消耗量本身）；
2. N2：门控收紧（无命印阻断）+ 契约矩阵测试假人策略调整；
3. N3：530 补 skill_id 校验；
4. N4/N5：统一改走 `BuffEffect.modifiers`（同时修复 531 护甲数值与 551 闪避生命周期）；
5. N6：550 prereq 回归设计（500×3）并同步 desc_key；
6. N7/N8/N9：残留实现与 UMR 收尾（可开后续任务，不阻塞本轮）；
7. N10–N13：记录为风险跟踪项。

## 第三轮记录（修复验证，2026-09-09）

第二轮 N1–N6、N7（502/503/550/551 免罚/570/572）、N8、N9、N11 已由 4 个并行子代理 + 主线程串行收尾完成修复。主线程收尾项：

1. **501 复利除法缺陷（修复过程中新发现并当场修复）**：交付层提频若写成相对式 `tick_interval /= (1+freq)` 会随 tick 指数缩小突破频率上限。已改为幂等公式（每次从 Baker 组合基准 `sub_interval` 重算），并移除 Baker 501 的静态满额因子（避免静态×动态双重计入）。
2. **`BuffEffect.modifiers` 通用消费端接线（`AttributePipeline.cpp`）**：调查确认全项目 buff modifiers（Armor/MoveSpeed/DodgeRating/DodgeChance/BlockChance/ResistAll 等）在属性重算中**无任何消费端**——SupportShield(+30% 护甲)、幻影闪(+20% 移速/+10% 闪避)、不坏剑壁、Chill 减速等现役 buff 全部为死数据，属既有缺口。已在 `AttributePipeline::Calculate` 聚合 `ActiveEffectsComponent` modifiers（跳过 ResistFire/Cold/Lightning——三者已在 `DamageMitigationService::ApplySkillScopedResistEffects` 按伤害类型消费，防双重计算）。N4/N5 修复由此真正闭环，并顺带修复上述现役缺口。
3. **550 禁位移确认**：`SkillSystem::HandleSkillInput` 引导中忽略其他技能输入（SkillSystem.cpp:2009-2011），位移技能拦截已由现有结构天然满足；dash 禁用在 GameplayState.cpp:538 已实现。无需额外修改。
4. **集成矩阵测试补充**：`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` 两个触发矩阵用例为技能 5 目标补满层 FateMark（与 unit 矩阵口径一致），修复门控收紧后的 4 处断言失败。

**验证证据**：`build.bat` EXIT=0（RelWithDebInfo）；`ctest --test-dir build -R "nmd.tests.(skill|combat.unit|combat.integration|ci.nonperf)"` 全部通过（含 ci.nonperf 聚合 106,792 断言）；Functional Skill 5 14 用例 / 145 断言全过。

**仍开放（后续任务）**：N10（生成器技能1 契约副作用影响面确认、Baker flag 双源收敛）、N12（535 全体击晕/552 圆盘/554 硬编码等小项）、N13（基底天降交付形态）。531 护甲与 551 闪避的最终数值生效依赖第 2 条接线的属性管线行为，建议实机运行观察一次（测试已覆盖 modifiers 内容断言）。
