# 技能7「心剑·无影」专精树实现审查报告

- 日期：2026-09-10
- 审查对象：技能 7 (Mind Blade / 心剑·无影) 专精树的完全性、拓扑正确性与多层一致性
- 结论：**修改**
- 审查轮次：第 1 轮复核与深化（验证原评审准确性，纠偏并增补新发现项）

## 1. 审查目标

逐节点核对技能 7 专精树（28 节点）在设计文档 `设计文档/职业设计草案_剑修.md` §3.7 定义下的实现完全性，覆盖八层：
1. **数据层**：`skills.json` / `skill_7_tree.json` / `skill_contracts_compact.json` / `skill_mechanics.json`
2. **烘焙层**：`SkillSpecializationBaker.cpp`
3. **行为与交付层**：`MindBlade.cpp` / `MindBlade.hpp` / `BeamChannelDeliverySystem.cpp`
4. **管线与结算层**：`DamageMitigationService.cpp` (抗性上限压制 Type E) / `SkillSystem.cpp` (蓝耗与冷却)
5. **跨技能联动层**：万剑归宗 (Skill 5) / 灵剑决 (Skill 3) / 影分身 (ShadowDuplication)
6. **输入与移动层**：`InputSystem.cpp` (微步引导 Keystone 732) / `ChannelingComponent`
7. **视觉与表现层**：`GPUSkillEffectSystem.cpp` / `GameplayRenderAdapter.cpp` / `VisualFXSystem.cpp`
8. **测试防线层**：矩阵测试 / 契约测试 / 守护测试 / 行为测试

## 2. 结论

| 轮次 | 结论 | 摘要 |
|---|---|---|
| 1 | **修改** | 初审判定：树的 name_key/prereq 基本符合 §3.7，但契约与烘焙/行为层是旧残留，28 个节点中 0 个按设计实现。 |
| 1 (复核深化) | **修改** | **全面验证初审并深化**：发现初审未暴露的致命架构冲突（C3：现代 `BeamChannelComponent` 与旧版 `ChannelingComponent` 双轨并存冲突，导致技能 7 实际运行于半吊子激光逻辑且 0.25 秒强制猝死，写在旧循环中的全部 VFX 与投射物沦为不可达死代码）；挖掘出抗性管线缺失 Type E 计算（H8）、持续引导零扣蓝（H9）、输入系统锁定阻断微步移动（H10）、缺失引导结束派发（M9）等重大新缺陷；同时纠偏了初审报告中关于“引擎不支持多前置 OR（实际上 `SkillSystem:2104` 已原生支持）”以及“`max_transmuters=2` 正确（实际上互斥转质必须为 1 且配互斥组）”的 2 处误判。测试防线中多达 4 个文件固化了错位 ID。 |

## 3. 输入

- `设计文档/职业设计草案_剑修.md:451-505`（§3.7 心剑·无影，28 节点 + 关键节点清单）
- `assets/data/skills.json:4238-4890`（技能 7 主体 + talent_tree + 内嵌 skill_contract）
- `assets/data/skill_7_tree.json`（布局副本，349 行）
- `assets/data/skill_contracts_compact.json:285-317`
- `assets/data/skill_mechanics.json:1-608`（确认无 `"7"` 键）
- `scripts/gen_skill_contracts.py:204-470`（契约生成规则与默认 Keystone 推断）
- `src/game/systems/skill/behaviors/MindBlade.cpp:1-68`、`MindBlade.hpp:1-23`
- `src/game/systems/skill/SkillSpecializationBaker.cpp:91-95,730-746,825-852`
- `src/game/systems/skill/BeamChannelDeliverySystem.cpp:46-420,422-860`
- `src/game/systems/combat/DamageMitigationService.cpp:60-145`（抗性结算与上限 clamp）
- `src/game/application/input/InputSystem.cpp:88-117,125-156`（按键阻断移动）
- `src/game/foundation/components/SkillDefs.hpp:918-930,1250-1272`
- `src/game/foundation/components/DeliveryArchetypes.hpp:127-153`
- `src/game/foundation/data/SkillRegistry.cpp:64-65,198-199,588-616`
- `src/game/systems/skill/SkillSystem.cpp:652,1003,1703,2049,2104-2120`
- `src/game/application/states/GameplayState.cpp:864-874`
- `src/game/application/render/GameplayRenderAdapter.cpp:452-456`
- `tests/fixtures/skill_specialization_keynodes.json:36`
- `tests/SkillKeyNodeMatrixTestHelpers.hpp:70,113,178`
- `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp:135-138`
- `tests/integration/SkillContractRegistryTests.cpp:244,258-260`
- `tests/unit/SkillBehaviorGuardTests.cpp:928`

## 4. 变更文件边界

`git status --short`：
- ` M settings.json`（运行时动态文件，非技能相关）
- `?? docs/reviews/2026-09-10-skill7-specialization-nodes-review.md`

技能 2-6 的整改已全部闭环提交（`0e1eba33`…`c331791a`）。技能 7 处于历史初版状态，本轮为纯审查与核验，无源代码修改。

## 5. 范围对齐

| 设计项（§3.7） | 数据层 | 烘焙/行为层 | 管线/系统层 | 结论 |
|---|---|---|---|---|
| 引导持续撕裂，0.3s 爆发范围伤害，15 法力/秒 | `mana_cost=15` (一次性)<br>`cooldown=4.0` (偏离) | 烘焙 `sub_interval=0.12`<br>行为层 `tick_interval=0.3` | `ContinuousLaser` 硬编码半径 60<br>每秒扣蓝零实现 (H9) | **严重失配** |
| Keystone 碎空爆 (711) | `max=5` (应 1)，契约缺位 | 零实现 | 缺引导结束引爆机制 (M9) | **缺失** |
| Keystone 步影随行 (732) | `max=5` (应 1)，契约缺位 | 零实现 | `InputSystem` 按键强制停步 (H10) | **缺失且架构受阻** |
| Trigger 寂灭 (714) | 错绑 713 精神透支，714 错配为 Keystone | 零实现 | Baker 仅认 `OnSkillHit` (H6) | **系统性错位** |
| Synergy 剑意化无 (754) | 错绑 730 神识多开，754 错配为 Keystone | 零实现 | 缺灵剑决飞剑瞬移穿刺 (M1) | **系统性错位** |
| 剑意交互：意念风暴 (753) / 心念反哺 (755) | 契约错绑 752 深渊侵蚀，753 错配为 Keystone | 752 臆造“施放吞满 10 层剑意” | 缺引导逐秒扣剑意增伤 | **严重失配** |
| Transmuter 天外冰晶 (770, 冰) / 神雷天罡 (772, 雷) 互斥 | 错绑 [770, 750]，772 错配为 Keystone<br>`max_transmuters=2` (应 1)<br>缺互斥组 | 770 转 Lightning (反向)<br>750 臆造转 Void<br>772 零实现 | 缺冰刺碎裂投射物<br>缺 0.5s 周期雷柱与感电区 | **系统性错位** |
| Type E 抗性上限压制 (心念灭抗 775) | 契约错绑 771 极寒碎骨 | 零实现 | `DamageMitigationService` 上限写死，无 Type E 结算 (H8) | **系统性错位且管线缺失** |

---

## 6. 初审结论核验与纠偏说明

在深化审查中，对第 1 轮审查报告进行了逐项验证，确认其绝大多数发现项（C1、C2、H1、H3、H4、H5、H6、H7、M1、M2、M3、M5、M6、M7、M8 及各 Low 项）完全属实，但修正并澄清了原报告的 **2 处认知误区**：

1. **纠偏原报告 §11 关于 `max_transmuters=2 数值正确` 的陈述**：
   - *原报告认为*：`max_transmuters=2` 是正确的。
   - *事实核验*：设计文档 §3.7 明确定义“天外冰晶与神雷天罡互斥，只能选其一”。对照技能 3、4、5、6，凡是两转质互斥的技能，其紧凑契约中的 `max_transmuters` 必须为 `1`，且必须配置 `keystone_exclusion_groups: {"770": 1, "772": 1}`。当前配置为 2 且缺少互斥组，会导致玩家可以同时点出冰晶与神雷。这是数据层的重大缺陷，必须纠偏并归入 C1 Blocker。
2. **纠偏原报告 M4 关于“引擎不支持多前置 OR 语义”的陈述**：
   - *原报告认为*：`SkillRegistry.cpp:64-65, 198-199` 导致 774（依赖 770 或 772）的第二前置被忽略，仅点雷会被拒。
   - *事实核验*：`SkillRegistry.cpp` 那些行仅用于 UI 拓扑图生成相对坐标（以首个前置为锚点）；而负责玩家实际加点前置校验的核心代码在 `SkillSystem.cpp:2104-2120`，其对 `node.prerequisites` 采用 `for` 循环迭代，只要任一前置满足 `pre_pts >= required_points` 即判定满足并 `break`。**引擎实际上原生支持 OR 语义**！774 配置 `[770@1, 772@1]` 在运行时完全能正常解锁，不存在仅点雷被拒的问题。

---

## 7. 发现项（按严重度）

### Blocker

- **C1 契约系统性错位与互斥约束失效** — `assets/data/skill_contracts_compact.json:285-317` 与 `skills.json` 内嵌契约一致性错位：
  - `trigger_nodes` 绑定 `713`（精神透支，应为 `714` 寂灭）；
  - `synergy_node_ids` 绑定 `[730]`（神识多开，应为 `754` 剑意化无）；
  - `sword_intent_node_ids` 绑定 `[752]`（深渊侵蚀，应为 `[753, 755]`）；
  - `transmuter_node_ids` 绑定 `[770, 750]`（750 引力坍缩非转质，应为 `[770, 772]`）；
  - `resist_models` 绑定 `771: TypeE_CapSuppression`（771 极寒碎骨，应为 `775` 心念灭抗）；
  - 缺少 `keystone_node_ids: [711, 732]`；因缺少显式定义，`scripts/gen_skill_contracts.py:432` 触发降级机制，将所有 `max_points==1` 的节点（714/734/753/754/772）全部误标为 `Keystone`，而真 Keystone（711/732）因点数被配为 5 沦为 `Passive`；
  - `max_transmuters` 错设为 `2`（应为 `1`），且完全缺失 `keystone_exclusion_groups: {"770": 1, "772": 1}`。
  - **为何成问题**：契约是运行时互斥校验、触发规则自动注册、抗性模型注入的权威来源。当前错位会导致玩家点满 713 后每次普通命中以 35% 效力触发万剑归宗；真 Trigger 寂灭与真 Synergy 剑意化无永远不生效；玩家可同时点亮冰霜与闪电转质；心念灭抗无 Type E 标签。
  - **修复建议**：全面按 §3.7 规范重写紧凑契约与内嵌契约，配置 `keystone_node_ids: [711, 732]`、`max_transmuters: 1`、`keystone_exclusion_groups`，重跑 `scripts/gen_skill_contracts.py`。

- **C2 烘焙与行为层残留旧节点逻辑且元素转质反向** — `SkillSpecializationBaker.cpp:730-746` 与 `MindBlade.cpp:16-56`：
  - 713（精神透支）→ 错写为增加固定暴击率 20（设计为蓄力加速 + 满层自伤）；
  - 730（神识多开）→ 错写为射线锁定最近敌人（设计为额外生成次级空间撕裂场）；
  - 750（引力坍缩）→ 错写为 Physical→Void 转质与 1.25 倍增伤（设计为牵引拉扯轻中型敌人，§3.7 无虚空转质）；
  - 752（深渊侵蚀）→ 错写为施法瞬间吞满 10 层剑意与 1.5 倍增伤（设计为击中附加流血与流血加速，强吞 10 层剑意系臆造且破坏剑意循环）；
  - 770（天外冰晶）→ 错将物理转为 **闪电 (Lightning)**（设计为转为 **冰霜 (Cold)**，生成冰刺与飞散冰片）；
  - 代码中残留 `HeavenMan/MindLock/MindUnity/OneLaw/RayFocus` 等旧命名常量。
  - **为何成问题**：28 节点中无一处符合策划设计案，核心元素转质方向颠倒，剑意循环机制被臆造逻辑彻底破坏。
  - **修复建议**：彻底清除旧常量，按 §3.7 逐一重写 Baker `case 7` 与 MindBlade 行为。

- **C3 (新增) 现代 DeliveryArchetype 与旧 Channeling 双轨冲突导致技能 7 运行死锁与不可达死代码** — `BeamChannelDeliverySystem.cpp:425` 与 `MindBlade.cpp:24-56`：
  - 在 `MindBlade::OnCast` 中，同时向 caster 挂载了 `ChannelingComponent` 与 `BeamChannelComponent`；
  - 但在 `BeamChannelDeliverySystem::Update` 中，进入旧版兼容循环（Part 2，422-860 行）的门禁为：
    ```cpp
    if (registry.any_of<BeamChannelComponent>(entity)) { continue; }
    ```
  - **后果 1（巨额死代码）**：写在 Part 2 中的所有技能 7 逻辑（491-605 行连续通道粒子特效、720-813 行空间撕裂环与切割斩击特效、815-858 行通过 stationary projectile 派发的伤害 Hitbox）**被 100% 跳过，全部沦为不可达死代码**！
  - **后果 2（0.25 秒强制猝死）**：在被实际执行的 Part 1 现代循环（50-420 行 `BeamChannelComponent`）中，`MindBlade::OnCast` 将 `beam.max_channel_time` 错赋为 `chan.channel_timer`（即 0.25 秒），且该字段在按键持续按下时从未被 `SkillSystem::HandleSkillInput` 刷新（`SkillSystem:2051` 仅刷新 `chan->channel_timer`）。因此无论玩家如何按住按键，`beam.current_channel_time >= 0.25f` 在 1~2 帧后必然超时，在 415 行强制同时拔除 `BeamChannelComponent` 与 `ChannelingComponent`，技能引导瞬间猝死！
  - **后果 3（Part 1 极度粗糙简陋）**：被实际执行的 Part 1 `ContinuousLaser` 分支（391-409 行）仅有一个硬编码 60 半径的简单 `grid.query`，无任何 GPU 视觉效果，无元素标签转换，不调用 `DoHit`，甚至不计算暴击。玩家在游戏内表现为“按 Q 后无特效、瞬间中断、伤害缺失”。
  - **修复建议**：统一技能 7 交付管道到 `BeamChannelComponent`。将 Part 2 中优秀的撕裂 VFX 与 Hitbox 逻辑迁移重构至 Part 1 或行为层，彻底清理双轨组件挂载，并在 `SkillSystem::HandleSkillInput` 中同步保活 `BeamChannelComponent`。

---

### High

- **H1 28 节点中 23 个零实现** — 基础层 700/701/702/703（引导减耗/基础伤害/撕裂半径/追踪范围）、分支 A（710/711/712/714/715）、分支 B（731/732/733/734/735）、分支 C（751/753/754/755）、分支 D（771/772/773/774/775）全库检索确认无任何读取或处理路径。
  - **修复建议**：参照技能 5、6 工业化整改标准，依托机制数值表逐一落地 28 个节点。

- **H2 引力坍缩前置错挂** — `skills.json` 与 `skill_7_tree.json` 中 750 的前置被配为 `700@3`（神识凝聚），设计案明确要求为“无影无形 3/5”（即 `701@3`）。
  - **修复建议**：两份 JSON 同步将 750 前置修回为 `701@3`。

- **H3 max_points 大面积偏离设计（17/28 错误）** — 28 节点中有 17 处错误配为 5 点：
  - 基础：700 (4→5)、703 (4→5)；
  - 分支 A：711 Keystone (1→5)、712 (3→5)、713 (3→5)、715 (3→5)；
  - 分支 B：730 (1→5)、731 (3→5)、732 Keystone (1→5)、733 (3→5)；
  - 分支 C：750 (4→5)、751 (3→5 puzzles: 5)、752 (3→5)；
  - 分支 D：770 Transmuter (1→5)、771 (3→5)、774 (3→5)、775 (4→5)；
  - 正确仅 11 个（701/702/710/714/734/735/753/754/755/772/773）。破坏点数经济与 Keystone/Transmuter 机制约束。
  - **修复建议**：对照 §3.7 规格全量订正 `skills.json`。

- **H4 机制数值零外置** — `skill_mechanics.json` 包含技能 1~6，但完全缺失 `"7"` 顶级键；Baker `case 7` 无一处 `mech.GetFloat(7, ...)`。当前数值全为不可调硬编码。
  - **修复建议**：在 `skill_mechanics.json` 中建立完整的 `"7"` 机制表，Baker 与行为层统一通过 `SkillMechanicsRegistry` 读数。

- **H5 tags 缺 Channeled/Area，误带 Projectile，导致引导系统联动全面失效** — `skills.json:4243-4249` 技能 7 tags 为 `["Physical", "Spell", "Projectile", "Hit", "sword_skill"]`，设计要求 `[Spell], [Channeling], [Physical], [Area], [Void]`。
  - 缺失 `Tag::Channeled` 导致：`SkillSystem:652` 命中回资源连续型状态判定落空、`SkillDisplayPreviewService:144` UI 引导窗口显示错误、`ShadowDuplicationHook:23` 影分身过滤失灵；
  - 误带 `Projectile` 会被视作飞行投射物技能，而其本质是光标处的局部空间撕裂（Area）；
  - `sword_skill` 命名与技能 1、5 的 `SwordSkill` 大小写不一（已由别名层兜底，但应统一）。
  - **修复建议**：将 tags 纠正为 `["Spell", "Channeled", "Physical", "Area", "SwordSkill"]`（元素标签由转质动态附加）。

- **H6 寂灭/碎空爆链路整体缺失且 TriggerRule 监听不匹配** — 碎空爆为停止引导或引导结束时的坍缩爆发，寂灭要求被碎空爆直接击杀时触发小型万剑归宗。
  - 目前全库无 `SpatialCollapse` 行为；
  - `SkillSpecializationBaker.cpp:841-843` 将除 452 以外的所有 Trigger 统一固化为 `CombatEventType::OnSkillHit` 和 `Victim`。即使契约绑对 714，也会在每次撕裂击中时疯狂触发万剑归宗，违背“直接击杀 (OnKill)”语义。
  - **修复建议**：扩展 Baker 支持 `rule.listen_event = CombatEventType::OnKill`，并在行为层 `DoHit` / 结算层打通碎空爆击杀与小型万剑归宗的派发链路。

- **H7 行为层 Update 空实现与死组件定义** — `MindBlade.cpp:58-63` 中的 Update 忽略全部入参并直接返回 true；`SkillDefs.hpp:918-930` 定义的 `MindBladeAI` 与 `MindBladeComponent` 在游戏中从不挂载，沦为死数据。心流层数累加、逐秒扣除剑意等帧驱动逻辑缺少载体。
  - **修复建议**：参考 `BladeFormation` 或 `SwordArray`，在 `SkillDefs.hpp` 中定义紧凑的 `MindBladeRuntimeState`，由行为层接管状态更新，移除虚构的 AI 召唤死组件。

- **H8 (新增) 伤害管线完全缺失 Type E (抗性上限压制) 结算逻辑** — `DamageMitigationService.cpp:135`：
  - 核心减免代码直接写死：`res = std::clamp(res, RESISTANCE_MIN, RESISTANCE_MAX);`；
  - 源码中仅对技能 2 (Type A 穿透)、技能 5 (Type D 属性转穿透)、技能 6 (Type B 减抗) 以及 Type C (暴露) 有结算逻辑；
  - `ResistModel::TypeE_CapSuppression` 仅在 `SkillRegistry.cpp` 的合法性校验与 `GPUSkillEffectSystem.cpp` 的渲染事件中有映射，在核心战斗伤害计算管线中**无任何代码消费**！
  - **为何成问题**：心念灭抗 (775) 的核心机制“处于撕裂中心范围内的敌人对应元素抗性上限被压制 3%...12%，且可影响 Boss”，在当前底层机制下处于完全空转状态，抗性上限始终锁死在固定的 `RESISTANCE_MAX`。
  - **修复建议**：在 `DamageMitigationService` 中引入 `effective_max_res = RESISTANCE_MAX - cap_suppression` 计算，允许技能 7 的 Type E debuff 在合法过滤下动态削减防守方的抗性上限。

- **H9 (新增) 持续引导每秒 15 点扣蓝链路完全缺失** — `skills.json:4241` 与 `BeamChannelDeliverySystem.cpp`：
  - 技能 7 配置了 `mana_cost: 15.0`，但引导技能必须在引导期间按秒持续扣费（15/s）；
  - 对比技能 5 在 `BeamChannelDeliverySystem.cpp:206` 中每 tick 结算扣蓝：
    ```cpp
    float mana_rate = profile ? profile->effective_mana_cost : 20.0f;
    float mana_cost = mana_rate * beam.tick_interval;
    ```
  - 技能 7 在 `BeamChannelDeliverySystem.cpp` 中**完全没有任何扣蓝代码**！
  - **为何成问题**：玩家引导心剑·无影时，除了起手扣除一次 15 点蓝外，后续引导全程零耗蓝，且当玩家空蓝时技能不会自动中断，形成无限白嫖引导漏洞。
  - **修复建议**：在 `BeamChannelDeliverySystem` 中增加技能 7 的持续扣蓝分支，结合节点 700（神识凝聚 10%...40% 减耗）与 Keystone 732（步影随行 +50% 基础扣蓝）动态结算，并在蓝量耗尽时正确打断引导并触发收尾。

- **H10 (新增) 输入系统无条件清除移动目标阻断 Keystone 732 (步影随行)** — `InputSystem.cpp:88-112`：
  - 当检测到 Q/W/E/R 等技能键按下时，输入系统无条件执行 `s_hasMovementTarget = false;`；
  - 在随后的移动计算中（125-146 行），因 `s_hasMovementTarget` 为 false，`input.moveX` 与 `input.moveY` 被强行置 0，玩家在引导期间绝对无法移动；
  - Keystone 732 核心设计为“引导时你可以进行微步移动（移速保留且固定为正常移速的 30%），基础蓝耗增加 50%”。
  - **为何成问题**：若不在输入层为带有 732 特性的引导技能放开微步位移权限并注入移速惩罚，该 Keystone 在引擎最外层就会被硬生生掐死。
  - **修复建议**：在 `InputSystem.cpp` 中检查玩家是否处于携带 732 专精的引导状态，若处于该状态则允许鼠标移动更新，并将位移向量按 30% 比例缩放。

---

### Medium

- **M1 剑意化无（Synergy 754）跨技能协同未实现** — 需联动灵剑决飞剑（`BladeFormationComponent`），在撕裂中心生成高频穿刺（30% 效力）。参考技能 2 灵剑追击（`RendingWave.cpp:426`）的挂接方式。

- **M2 撕裂范围未接入 Area 机制与裂空 (702) 增益** — 当前 Part 1 硬编码 `radius = 60.0f`，Baker 的 `out_profile.area_radius` 对技能 7 无消费者，裂空 702 的 10%...40% 范围增益无法生效。应建立基础撕裂半径并由机制表驱动放大。

- **M3 双轨 tick 采样间隔矛盾** — Baker `del.sub_interval = 0.12f`（8.3 次/秒），而 `MindBlade.cpp:26` 写入 `chan.tick_interval = 0.3f`（3.3 次/秒，符合设计 0.3s）。且转质神雷天罡 (772) 设计为 0.5s 周期。应统一收拢到机制表与 Baker 组合计算。

- **M4 (更正) 拓扑与加点前置判定核实** — 初审报告指出的“引擎不支持多前置 OR 语义”经核实不成立（`SkillSystem.cpp:2104-2120` 已原生支持 OR）。774（异常切割）数据配置 `[770@1, 772@1]` 在运行时校验正常。仅 `SkillRegistry.cpp:198` 在计算 UI 节点渲染锚点时会选取 `front()`，需注意其在界面上的拓扑连线视觉呈现。

- **M5 矩阵与契约测试全链条固化错位 ID** — 下列 4 处测试与夹具文件均硬编码了错误的旧节点集（如 `{713, 730, 750, 752, 770}` 或 `713` 为 trigger），形成虚假全绿防线：
  1. `tests/SkillKeyNodeMatrixTestHelpers.hpp:113`：`{7, {713, 730, 750, 752, 770}}`；
  2. `tests/fixtures/skill_specialization_keynodes.json:36`：`"key_nodes": [713, 730, 750, 752, 770]`；
  3. `tests/integration/SkillContractRegistryTests.cpp:244`：`expected_trigger_nodes` 包含 `713`；同文件 260 行断言 `max_transmuters == 2`；
  4. `tests/unit/SkillBehaviorGuardTests.cpp:928`：`trigger_matrix` 包含 `{7u, 713u}`。
  - **修复建议**：在修复数据层后，必须同步清洗这 4 处测试防线，将关键集改为 `{711, 714, 732, 753, 754, 755, 770, 772, 775}`，Trigger 改为 `714`，`max_transmuters` 改为 `1`。

- **M6 技能 7 测试断言过浅且无独立专项用例** — `SkillKeyNodeMatrixIntegrationTests.cpp:136-138` 仅测试了 `skill_id == 7` 是否存在 `ChannelingComponent`，未做任何节点效果校验；`tests/functional/` 下缺 `MindBladeNodes.cpp` 专项测试套件（对照技能 5、6 的独立功能测试）。

- **M7 技能描述遗留旧文案** — `skills.json:4240` 为“高频引导发射极细剑气。”，设计案为“在光标位置不断撕裂空间，每 0.3 秒爆发一次无形的剑气切割”。

- **M8 转质覆盖与优先级不清晰** — `MindBlade.cpp:43-48` 中 Void 与 Lightning 相互覆盖，且默认 fallback 到御天诀共鸣。修正互斥组后（每次仅允许点一个转质），需明确转质元素与御天诀共鸣的覆盖优先级。

- **M9 (新增) 缺少引导结束/中断统一事件派发 (CombatEventType::OnChannelEnd)** — 碎空爆 (711) 依赖“停止引导时在光标处引爆一次极其恐怖的坍缩爆炸”，神游脱战 (734) 依赖“按下位移键立即打断引导并瞬移”。
  - 目前 `CombatEvents.hpp:452` 中的 `CreateChannelEnd` 全仓零调用，缺少标准化的 `OnChannelEnd` / `OnChannelInterrupt` 派发机制；
  - **修复建议**：在 `BeamChannelDeliverySystem` 移除组件或超时打断处统一调用 `CreateChannelEnd` 派发事件，使得行为层与触发引擎能稳定捕获引导收尾。

---

### Low

- **L1 双份数据来源漂移风险** — `skill_7_tree.json` 与 `skills.json` 的 talent_tree 经比对完全一致，但因属于并行维护文件，修改 750 前置时需两边同步。
- **L2 cooldown=4.0 与 mana_cost=15.0 初始设计冲突** — 引导技能配置 4s CD 会导致玩家松开鼠标后必须等待 4 秒才能再次引导（技能 5 引导技能 CD 仅 1.0s），建议将 CD 调整为 1.0s，并将 `mana_cost` 设为 0.0，统一改为引导持续扣蓝。
- **L3 注释固化错位 ID** — `SkillDefs.hpp:1268` 中的 `// Skill 7 node 730`。
- **L4 OnCast 未显式重置 `bonus_armor_pen`** — `MindBlade.cpp:28` 遗漏重置，依靠默认值兜底。
- **L5 命中视觉复用技能 2 参数** — `VisualFXSystem.cpp:72-77` 金色墨水飞溅，缺少空间撕裂的碎裂视觉冲击力。

---

## 8. 逐节点核对矩阵（28 节点）

| 节点 ID | 设计名称 | 设计 max | 实际 max | 设计 prereq | 实际 prereq | 设计角色 | 契约角色 | 实现状态 | 缺陷与偏差摘要 |
|---|---|---|---|---|---|---|---|---|---|
| 700 | 神识凝聚 | 4 | **5** | 根 | 根 | 基础 | Passive | ✗ 零实现 | max 偏离；引导减耗未实现 |
| 701 | 无影无形 | 5 | 5 | 根 | 根 | 基础 | Passive | ✗ 零实现 | 基础增伤未接入 |
| 702 | 裂空 | 4 | 4 | 根 | 根 | 基础 | Passive | ✗ 零实现 | 撕裂范围增益未接入 |
| 703 | 心念映射 | 4 | **5** | 根 | 根 | 基础 | Passive | ✗ 零实现 | max 偏离；追踪/施法范围未接入 |
| 710 | 心流叠加 | 5 | 5 | 701@2 | 701@2 | 功能 | Passive | ✗ 零实现 | 引导时间叠加 More 增伤未接入 |
| 711 | 碎空爆 | 1 | **5** | 710@3 | 710@3 | **Keystone** | Passive | ✗ 零实现 | max 偏离；契约缺 Keystone；蓄力核爆零实现 |
| 712 | 虚无牵引 | 3 | **5** | 711 | 711 | 功能 | Passive | ✗ 零实现 | max 偏离；蓄力引力场拖拽零实现 |
| 713 | 精神透支 | 3 | **5** | 711 | 711 | 功能 | **Trigger** | ⚠️ 严重错位 | 错绑为 Trigger；max 偏离；错写为暴击+20 |
| 714 | 寂灭 | 1 | 1 | 711 | 711 | **Trigger** | **Keystone** | ✗ 零实现 | 契约错标 Keystone；碎空爆击杀触发小型万剑零实现 |
| 715 | 破绽洞察 | 3 | **5** | 702@3 | 702@3 | 功能 | Passive | ✗ 零实现 | max 偏离；撕裂中心暴伤增加未实现 |
| 730 | 神识多开 | 1 | **5** | 702@2 | 702@2 | 功能 | **Synergy** | ⚠️ 严重错位 | 错绑为 Synergy；max 偏离；错写为射线锁定目标 |
| 731 | 千面阵 | 3 | **5** | 730 | 730 | 功能 | Passive | ✗ 零实现 | max 偏离；额外次级撕裂未实现 |
| 732 | 步影随行 | 1 | **5** | 730 | 730 | **Keystone** | Passive | ✗ 零实现 | max 偏离；契约缺 Keystone；微步移动零实现 |
| 733 | 御剑神游 | 3 | **5** | 732 | 732 | 功能 | Passive | ✗ 零实现 | max 偏离；御剑步下次级撕裂强化未实现 |
| 734 | 神游脱战 | 1 | 1 | 732 | 732 | 功能 | **Keystone** | ✗ 零实现 | 契约错标 Keystone；按位移打断并瞬移未实现 |
| 735 | 精准切割 | 4 | 4 | 702@1 | 702@1 | 功能 | Passive | ✗ 零实现 | 孤立目标 More 增伤未实现 |
| 750 | 引力坍缩 | 4 | **5** | 701@3 | **700@3** | 功能 | **Transmuter** | ⚠️ 严重错位 | 前置错挂 700；错绑 Transmuter；错写为转 Void |
| 751 | 空间粉碎 | 3 | **5** | 750@2 | 750@2 | 功能 | Passive | ✗ 零实现 | max 偏离；击碎护甲未实现 |
| 752 | 深渊侵蚀 | 3 | **5** | 751 | 751 | 功能 | **Passive(SwordIntent)** | ⚠️ 严重错位 | 错标剑意节点；max 偏离；错写为强吞10层剑意 |
| 753 | 意念风暴 | 1 | 1 | 751 | 751 | **剑意交互** | **Keystone** | ✗ 零实现 | 契约错标 Keystone；引导每秒扣剑意换增伤零实现 |
| 754 | 剑意化无 | 1 | 1 | 753 | 753 | **Synergy** | **Keystone** | ✗ 零实现 | 契约错标 Keystone；灵剑决飞剑协同瞬移穿刺零实现 |
| 755 | 心念反哺 | 3 | 3 | 753 | 753 | **剑意交互** | Passive | ✗ 零实现 | 契约缺剑意标签；撕裂击杀回蓝回剑意零实现 |
| 770 | 天外冰晶 | 1 | **5** | 703@2 | 703@2 | **Transmuter** | Transmuter | ⚠️ 严重错位 | max 偏离；转质方向颠倒（转了雷，应转冰） |
| 771 | 极寒碎骨 | 3 | **5** | 770 | 770 | 功能 | **Resist(TypeE)** | ✗ 零实现 | 错挂 Type E 标签；max 偏离；冰刺碎裂穿透零实现 |
| 772 | 神雷天罡 | 1 | 1 | 703@2 | 703@2 | **Transmuter** | **Keystone** | ✗ 零实现 | 契约错标 Keystone；转闪电、0.5s周期雷柱零实现 |
| 773 | 天劫落雷 | 3 | 3 | 772 | 772 | 功能 | Passive | ✗ 零实现 | 落雷单体增伤与感电暴击未实现 |
| 774 | 异常切割 | 3 | **5** | 770∨772 | 770,772 | 功能 | Passive | ✗ 零实现 | max 偏离；对异常目标 More 增伤未实现 |
| 775 | 心念灭抗 | 4 | **5** | 774 | 774 | **Type E** | Passive | ✗ 零实现 | 契约缺 Type E 标签；抗性上限压制零实现 |

**统计结果**：
- max_points 正确：**11 / 28**（17 处错误偏离）；
- prerequisites 正确：**27 / 28**（750 错挂 700，774 多前置经核查引擎已支持）；
- 契约角色与标签正确：**2 / 28**（仅 770 为 Transmuter，但转质方向反向；其余 26 处全错）；
- 按设计规范实现：**0 / 28**（5 个错位逻辑 + 23 个零实现）。

---

## 9. 架构与跨系统偏差

1. **交付管道双轨并存冲突 (C3)**：
   - 当前在 `BeamChannelDeliverySystem` 中存在 Part 1 (`BeamChannelComponent`) 与 Part 2 (`ChannelingComponent`) 的双重实现；
   - 技能 7 起手挂载双组件，导致旧循环 Part 2 的丰富 VFX 与 Hitbox 逻辑被跳过；
   - Part 1 现代激光逻辑中缺少保活机制，导致 0.25 秒强制超时夭折。必须彻底重构收拢至单一现代交付原型。
2. **抗性减免管线漏洞 (H8)**：
   - 整个 `DamagePipeline` 与 `DamageMitigationService` 仅支持绝对减抗与穿透，无任何抗性上限压制（Cap Suppression）逻辑，导致 Type E 节点即使挂载也无法产生数学收益。
3. **输入系统冻结位移 (H10)**：
   - `InputSystem.cpp` 缺乏对“可移动引导技能”的白名单与分支支持，按住按键直接打断移动，阻断了 Keystone 732 的落地。
4. **Trigger 派发与击杀事件不匹配 (H6)**：
   - `SkillSpecializationBaker.cpp:841` 将通用 Trigger 锁死在 `CombatEventType::OnSkillHit`，无法支撑 714 寂灭所要求的“被碎空爆直接击杀 (OnKill)”触发链路。
5. **持续引导法耗模型脱节 (H9)**：
   - 技能 7 起手一次性扣除 15 蓝，后续引导不扣蓝，违背“15/秒”的设计案标准，且空蓝不打断。

---

## 10. 整改落地路线图（四阶段原子化实施计划）

### Phase 1: 数据层与契约闭环（先行，解除数据错位）
1. **修改 `assets/data/skills.json`**：
   - 修正 17 处错误的 `max_points`（700, 703, 711, 712, 713, 715, 730, 731, 732, 733, 750, 751, 752, 770, 771, 774, 775）；
   - 修正 750 的前置为 `701@3`；
   - 修正技能 7 标签：`["Spell", "Channeled", "Physical", "Area", "SwordSkill"]`；
   - 调整技能 7 冷却与消耗：`cooldown: 1.0`, `mana_cost: 0.0`（扣蓝由引导系统负责）；
   - 修正技能 7 描述文案（M7）。
2. **同步 `assets/data/skill_7_tree.json`**：
   - 将 750 前置同步修正为 `701@3`。
3. **重写 `assets/data/skill_contracts_compact.json` 技能 7 块**：
   - `max_transmuters: 1`, `max_triggers: 1`；
   - `keystone_node_ids: [711, 732]`；
   - `transmuter_node_ids: [770, 772]`；
   - `keystone_exclusion_groups: {"770": 1, "772": 1}`；
   - `synergy_node_ids: [754]`；
   - `sword_intent_node_ids: [753, 755]`；
   - `resist_models: {"775": "TypeE_CapSuppression"}`；
   - `trigger_nodes`: 配置 `node_id: 714`, `trigger_skill_id: 5`, `effectiveness: 0.35`, `internal_cooldown: 3.0`。
4. **运行 `python scripts/gen_skill_contracts.py`**：全量重新生成 `skills.json` 内嵌契约，确保数据自洽。

### Phase 2: 机制数值外置与 Baker 烘焙重构
1. **在 `assets/data/skill_mechanics.json` 中建立 `"7"` 表**：
   - 录入全 28 节点的策划配置项（减耗比例、基础伤害加成、半径加成、心流叠层参数、引力牵引范围与力度、寂灭万剑效力、多开撕裂参数、步影移速惩罚与蓝耗倍率、意念风暴每秒消耗、天外冰晶冰刺碎片参数、神雷天罡雷柱周期与感电区域、心念灭抗上限削减等）。
2. **重构 `SkillSpecializationBaker.cpp` case 7**：
   - 彻底删除 713/730/750/752/770 的旧残留逻辑与死常量；
   - 绑定转质节点：770Physical→Cold并附Cold标签；772Physical→Lightning并附Lightning标签；
   - 接入 700 减耗、701 基础增伤、702 撕裂半径、710 心流上限等参数；
   - 针对 714 寂灭配置 `rule.listen_event = CombatEventType::OnKill`。

### Phase 3: 行为层、交付管线与底层系统打通
1. **重构 `MindBlade.cpp` 与 `SkillDefs.hpp`**：
   - 清理死数据结构 `MindBladeAI` 与 `MindBladeComponent`，设计紧凑的 `MindBladeChannelState`（记录引导时长、蓄力层数、心流进度、剑意消耗计时等）；
   - 实现 `MindBlade::OnCast`，仅挂载单一 `BeamChannelComponent`，初始化正确参数；
   - 实现 `MindBlade::DoHit`，挂接 751 护甲击碎、752 流血、755 击杀回蓝回剑意、771 极寒击碎、774 异常切割等命中效果。
2. **打通 `BeamChannelDeliverySystem` 现代管线**：
   - 迁移并复用 Part 2 的空间撕裂粒子与切割斩击 VFX 提交至 Part 1；
   - 实现每秒 15 点持续扣蓝逻辑，受 700 减免与 732 惩罚影响，蓝尽自动打断；
   - 在按键保持时正确维持 `BeamChannelComponent` 存活；在按键释放时派发 `CreateChannelEnd` 事件；
   - 实现 Keystone 711 碎空爆逻辑：蓄力期间不产生 tick 伤害，引爆时在光标处结算范围坍缩核爆并触发 714 寂灭判定；
   - 实现 730/731 多维撕裂追踪次级目标的生成。
3. **打通 `InputSystem.cpp` 微步移动 (Keystone 732)**：
   - 允许带 732 特性的引导状态维持鼠标位移输入，注入 30% 移速限制。
4. **打通 `DamageMitigationService.cpp` 抗性上限压制 (Type E)**：
   - 在抗性计算中动态扣减 `RESISTANCE_MAX`。

### Phase 4: 测试防线清洗与全量用例覆盖
1. **清洗 4 处受污染的既有测试**：
   - `tests/fixtures/skill_specialization_keynodes.json`；
   - `tests/SkillKeyNodeMatrixTestHelpers.hpp`；
   - `tests/integration/SkillContractRegistryTests.cpp`；
   - `tests/unit/SkillBehaviorGuardTests.cpp`。
2. **新建专项行为测试套件 `tests/functional/MindBladeNodes.cpp`**：
   - 覆盖 28 节点的全部原子逻辑（碎空爆蓄力与引爆、寂灭 OnKill 触发、步影微步位移、剑意化无飞剑穿刺、双 Transmuter 互斥防线、Type E 抗性压制实战验证等）。
3. **全套自动化验证**：
   - `build.bat` 确保编译 0 警告 0 错误；
   - 运行 CTest 确保单元、集成与功能测试 100% 绿灯。

---

## 11. 剩余风险

1. **玩家端严重错位行为**：当前只要点满 713 精神透支，每次命中就会以 35% 效力连发万剑归宗，此为严重影响数值平衡的运行时 Bug，需尽快落地 Phase 1 数据层修正。
2. **UI 拓扑连线与多前置视觉表现**：774 虽然在逻辑上支持双前置 OR，但 UI 渲染层（`UISkillTalentTree.cpp`）对双父节点的连线与高亮展示是否美观流畅需在实机下进行视觉确认。
3. **微步位移与移动碰撞系统交互**：732 引入的“引导中微步移动”为本项目首个可边走边引导的技能，需关注与 `MovementStanceSystem`、闪避及碰撞体积的边界行为。
