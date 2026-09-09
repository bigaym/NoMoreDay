# 技能 4「剑气护体」专精树实现完全度审查报告

- 审查日期：2026-09-08
- 审查对象：技能 4 BladeWard（剑气护体）专精树全链路（设计 ↔ 数据 ↔ 契约 ↔ 烘焙 ↔ 行为 ↔ 运行时 ↔ 测试）
- 对照基准：`docs/reviews/2026-09-08-skill3-specialization-nodes-review.md`（同格式、同方法）
- 审查方式：静态审查（只读），无任何文件修改

---

## 1. 审查目标

按技能 3 审查同一口径，核对技能 4（`设计文档/职业设计草案_剑修.md` §3.4，L283-336）的 29 个专精节点是否在数据、契约、烘焙、行为、运行时五层被完整且正确地实现，并给出整改判定。

## 2. 结论

**判定：修改（Blocker）。**

| 级别 | 数量 |
| --- | --- |
| Blocker | 1（不可达链：5/29 节点玩家永久无法点亮，含唯一 Trigger） |
| Critical | 5 |
| High | 5 |
| Medium | 7 |

技能 3 审查发现的结构性错误模式（Trigger 参数挂错节点、transmuter_node_ids 指向子节点、max_points 批量偏差造成不可达链、stat_modifiers 枚举错位、Keystone 默认生成器污染、skill_mechanics 未覆盖）在技能 4 **全部复现**，且新增两类特有错误：行为层节点 ID 语义错位（451/452 互换）与反击链元素标签错挂。

## 3. 输入清单

| 输入 | 路径 | 说明 |
| --- | --- | --- |
| 设计基准 | `设计文档/职业设计草案_剑修.md` §3.4 L283-336 | 29 节点、4 分支、关键节点清单 |
| 技能数据 | `assets/data/skills.json`（skill id=4） | 29 节点 + 内嵌 skill_contract |
| 树布局 | `assets/data/skill_4_tree.json` | 与 skills.json 完全一致（0 差异） |
| 紧凑契约 | `assets/data/skill_contracts_compact.json`（skill_id=4） | 含 synergy/sword_intent/resist_models/trigger_nodes |
| 技能机制 | `assets/data/skill_mechanics.json` | **无技能 4 条目**（顶层仅 1/2/3） |
| 烘焙层 | `src/game/systems/skill/SkillSpecializationBaker.cpp` L72-75、L486-510 | case 4 交付原型 + feature_flags |
| 行为层 | `src/game/systems/skill/behaviors/BladeWard.cpp`（88 行） | DoCast L34-83 |
| 运行时 | `src/game/systems/combat/DamagePipeline.cpp` L1196-1212、L1621-1637；`src/game/systems/skill/ProjectileSystem.cpp` L596-612、L738-776；`src/game/systems/skill/SkillSystem.cpp` L1152-1187、L1900-1974 | 拦截/反击/过期/分配校验 |
| 组件定义 | `src/game/foundation/components/SkillDefs.hpp` L891-902；`DeliveryArchetypes.hpp` L219-229；`Stats.hpp` L224-284 | BladeWardComponent / ReactiveWardComponent / StatType 枚举 |
| 注册表 | `src/game/foundation/data/SkillRegistry.cpp` L227-254（默认契约生成）、L370-373/L471-528（exclusion group） | |
| 测试 | `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` L123-126；`tests/integration/SkillSystemTests.cpp` L1056-1093；`tests/SkillKeyNodeMatrixTestHelpers.hpp` L110 | |
| 元素转换 | `src/game/systems/skill/behaviors/SkillBehaviorBase.hpp` L22-77 | 472/474 映射已预留 |

## 4. 变更文件边界

本审查为只读操作，未修改任何文件。`git status` 变更边界与本报告无关（无 C++ 变更被引入）。

## 5. 范围对齐表

| 层 | 设计要求 | 实现状态 | 判定 |
| --- | --- | --- | --- |
| 节点数量 | 29 | 29（400-476 连续无缺号） | ✅ |
| 树布局一致性 | — | skill_4_tree.json ≡ skills.json（prereq/坐标/名称 0 差异） | ✅ |
| 交付原型 | 环绕剑影 + 响应护盾 | OrbitingSentinel(primary) + ReactiveWard(secondary)（Baker L72-75） | ✅ |
| 可达性 | 全 29 节点可点亮 | 433/434/452/453/455 永久不可达 | ❌ |
| 契约角色 | Keystone×4 / Trigger 452 / Transmuter 472,474 / 曝光 476 | Trigger→451、Transmuter→470,471、曝光→472、Synergy→430 | ❌ |
| 互斥 | 472↔474 互斥 | 无 keystone_exclusion_group 数据、max_transmuters=2 | ❌ |
| 点数成长 | 20 个多点节点有成长数值 | 全部布尔开关化，无 per-point 数值 | ❌ |
| 剑意交互 | 435 格挡回剑意 / 455 闪避回剑意 | 均未实现；GainSwordIntent 错挂 452 位 | ❌ |
| 基底口径 | Toggle 每秒耗蓝维持 | 10s 定时 Buff + CD 1s 施放型；tags 缺 [Defense] | ⚠️ |

## 6. 发现项

### Blocker

**B1｜不可达链：5 个节点永久无法点亮（数据层 max_points 矛盾）**

`assets/data/skills.json` 技能 4 中三个前置依赖节点的 max_points 被压成 1，但下游前置要求 2-4 点：

| 节点 | 前置要求 | 依赖节点实际 max_points | 后果 |
| --- | --- | --- | --- |
| 433 鲜血壁垒 (Keystone) | 432×2 | 432 max=1 | 永久不可达 |
| 434 无瑕之御 (Keystone) | 431×4 | 431 max=1 | 永久不可达 |
| 452 瞬身反打 (**唯一 Trigger**) | 451×3 | 451 max=1 | 永久不可达 |
| 453 流风余韵 | 452×1 | （连带 452） | 永久不可达 |
| 455 以攻代守 (剑意交互点) | 451×2 | 451 max=1 | 永久不可达 |

- 分支 B（完美格挡）的两个 Keystone（433、434）与分支 C（灵动反击）的 Trigger（452）及其后续全部灭失；玩家在游戏内走任何加点路线都无法点亮这 5 个节点。
- `SkillSystem::AllocateSkillNode`（`src/game/systems/skill/SkillSystem.cpp` L1919-1940）只校验「玩家已点数 ≥ required_points」，不校验「依赖节点 max_points 是否允许达到 required_points」，不可达是**静默死路**：无告警、无 UI 提示（M8）。
- 加载期校验（`SkillRegistry`）同样不检测该矛盾（与技能 3 审查结论一致，属系统性缺失）。
- 设计对照：431 势不可挡 0/4、432 剑盾屏障 0/3、451 借力打力 0/3 —— max_points 数据本身即与设计冲突（见 M5 清单）。

### Critical

**C1｜契约-数据五重错位（技能 3 同构错误完整复现）**

`skills.json` 内嵌 `skill_contract` 与 `skill_contracts_compact.json`（skill_id=4）：

| 项 | 数据值 | 设计值 | 性质 |
| --- | --- | --- | --- |
| Trigger 节点 | **451**（trigger_skill_id=1 流云刺，eff=1.0，ICD=2.0，无耗蓝） | **452 瞬身反打**（触发流云刺 CD 2s） | 触发参数语义正确但挂在 451 借力打力；451 是「每次闪避移速/攻速提升」的被动 |
| transmuter_node_ids | **[470, 471]** | **[472 雷霆法环, 474 霜铠]**（互斥） | 470 是 Keystone 剑气反震、471 是被动以眼还眼 |
| Synergy | 430（剑意格挡） | 设计明示「Synergy 缺，待补」 | 唯一被标 Synergy 的是分支 B 一层被动 |
| 472 雷霆法环 | role=Passive + resist_model=TypeC_Exposure | Transmuter（转闪电） | 转质节点被标被动；Type C 曝光本应属 476 元素曝光 |
| 470/471 | role=Transmuter | 470=Keystone、471=Passive 0/4 | |

- 后果：`max_transmuters=2` 配合错位 IDs，运行时「双 Transmuter 上限」实际约束的是 470+471（一个 Keystone + 一个被动）双持，而真正的 472/474 互斥对完全不受任何契约约束。
- `resist_models: {"472": "TypeC_Exposure"}` 把元素曝光抗性模型挂在 472（雷霆法环）——设计要求曝光节点是 476（元素曝光，受对应元素伤害 +4...16%，不可叠加）。
- 与技能 3 的 C1（114 挂 134 参数）、C2（transmuter 含子节点）同源同构。

**C2｜行为层节点 ID 语义错位（技能 4 特有，最严重的新错误）**

`src/game/systems/skill/behaviors/BladeWard.cpp` L18-29 常量表：

```cpp
constexpr uint32_t BlinkCounter = 451;  // 注释“瞬身反击” —— 451 实际是借力打力
constexpr uint32_t AgileCounter = 452;  // 注释“灵动反击” —— 452 实际是瞬身反打(Trigger)
constexpr uint32_t RainbowQi   = 471;   // 注释“剑气如虹” —— 471 实际是以眼还眼；“剑气如虹”不存在于设计
constexpr uint32_t BladeStorm  = 473;   // 注释“剑刃风暴” —— 473 实际是雷贯长虹；“剑刃风暴”是技能 7 的 362 节点
```

- 行为层把 451（借力打力）当瞬身反打用（`has_blink_counter`，反击伤害 55）、把 452（瞬身反打 Trigger）当「+1 环绕剑」用（`has_agile_counter`）——**451/452 语义整体互换**。
- `SkillDefs.hpp` L896-901 的 `BladeWardComponent` 字段注释（`// Talent 451 / 452 / 471 / 473`）已把错误节点号**固化进组件定义**，与 Baker L497-508（`451→flag32 瞬身反击`、`452→flag64 灵动反击`、`471→flag256 剑气如虹`、`473→flag512 剑刃风暴`）、契约 trigger_nodes（451）三层一致地错。
- 结果：设计上「瞬身反打=闪避成功自动释放流云刺」的 Trigger 行为根本不存在；451 点 1 点就白送反击增强，且真正的 Trigger 通道（Baker L123-135 的 TriggerRule 烘焙）因契约 452 被标 Keystone 而永远不会为 452 生成触发规则。

**C3｜Transmuter 全链失效（472 雷霆法环 / 474 霜铠）**

四道断点叠加，任何一道都足以让双转质失效：
1. Baker case 4（`SkillSpecializationBaker.cpp` L486-510）**没有**调用 `ResolveElementalConversion(472/474)`（对照 case 3 L465-467、case 5 L530-534 均有调用）→ `effective_tags` 永不变更，`effective_tags` 仍是纯 Physical。
2. 契约 `transmuter_node_ids=[470,471]` 指向错误节点，转质名额计数也错。
3. `keystone_exclusion_group` 数据在技能 4 的契约与节点中**零条目**（skills.json 仅技能 2/3 部分节点有该字段；技能 4 contract keys 无 keystone_exclusion_groups）→ 472/474 互斥无数据通道，`max_transmuters=2` 反而放任双持。
4. `SkillBehaviorBase.hpp` L39/L47 已为 474(Cold)/472(Lightning) 预留映射（✅ 预留正确），只差 Baker 接线——即「转质基建就绪、技能 4 未接入」。

**C4｜反击链元素标签错挂（运行时层）**

`src/game/systems/skill/ProjectileSystem.cpp` L754：拦截反击伤害池
```cpp
counterPool.Add(ward->has_rainbow_qi ? Tag::Lightning : Tag::Physical, ...)
```
- 闪电标签挂在 `has_rainbow_qi`（471 以眼还眼——伤害 More 增伤节点）上；设计上把剑气护体转为闪电的是 **472 雷霆法环**（Transmuter）。
- 即使未来修好 C3，若 471 语义不纠正，会出现「点被动增伤节点反而让反击变闪电」的隐性错误耦合。

**C5｜19/29 节点零运行时效果**

Baker case 4 仅处理 10 个节点（400/401/411/412/430/451/452/470/471/473），行为层同集合。以下 19 个节点在烘焙与行为层**无任何处理**（分配后无任何游戏效果）：

> 402 持久、403 剑压外放、410 厚积薄发、413 破釜沉舟、414 御剑防壁、415 坚韧回生、431 势不可挡、432 剑盾屏障、433 鲜血壁垒、434 无瑕之御、435 剑意格御、450 幻影步、453 流风余韵、454 御剑闪步、455 以攻代守、472 雷霆法环、474 霜铠、475 永冻领域、476 元素曝光

其中 403 剑压外放是 Base Tier 四节点之一（反击触发几率/范围），是分支 D 剑气反震（470）的父节点——父节点自身却无效果（470 的触发实现也不依赖 403 点数）。

### High

**H1｜点数成长全面缺失（布尔开关化）**

行为层所有节点效果均为 `feature_flags` 位测试的布尔开关，无任何 per-point 数值缩放：

| 节点 | 设计（点数成长） | 实现 | 偏差 |
| --- | --- | --- | --- |
| 400 金钟罩 0/4 | 护甲 +15%...60% | `phys_dr = 10 + 5` 固定 ResistPhysical | 基础 10%（设计 12%），加点无成长 |
| 401 拨云见日 0/4 | 偏转 +4%...16% | `interception_chance += 0.25` 一次性 | 一次性 +25%，幅度与成长双错 |
| 411 五行御守 0/4 | 全元素抗性 +6%...24% | `ResistAll Flat 15` 固定 | 固定值、幅度错 |
| 430 剑意格挡 0/5 | 格挡率 +4%...20% | `BlockChance Flat 10` 固定 | 固定值、幅度错 |
| 452→has_agile_counter | （瞬身反打 Trigger） | `sword_count += 1` | 语义整体错位 |

`skills.json` 中技能 4 仅 411/430/450 三节点带 `stat_modifiers`，其余 26 节点为空——即使 Baker 通用路径（`ApplyNodeModifiersToProfile` L138 逐节点调用）想按点数交付也无数据可依（见 H2、M5）。

**H2｜stat_modifiers 枚举错位 ×3（技能 3 C1 同构）**

`StatType` 枚举索引（`src/game/foundation/components/Stats.hpp` L224-284 实测）：

| 节点 | 数据 | 实际枚举 | 设计语义 | 应为 |
| --- | --- | --- | --- | --- |
| 411 五行御守 | t27 v5/点 | **27=ResistShadow** | 全元素抗性 +6%...24% | 28=ResistAll（或 23-27 五系分加） |
| 430 剑意格挡 | t35 v4/点 | **35=DodgeChance** | 格挡率 +4%...20% | 36=BlockChance 或 48=BlockRating |
| 450 幻影步 | t34 v3/点 | **34=DurationScale** | 闪避等级 +15%...60% | 47=DodgeRating（或 35=DodgeChance） |

三处全部错位：防御树节点挂出「暗抗、闪避率、持续时间」三种不相关属性。t27 错位与技能 3 的 311 节点完全同款。

**H3｜剑意交互点未实现（435/455），GainSwordIntent 错挂**

- 设计关键节点清单明示剑意交互点 = 435 剑意格御（格挡 15...45% 几率回 1 层剑意）+ 455 以攻代守（闪避后 2s 内 More+20% 且必得 1 层剑意）。两者分别属于不可达链与零实现集合（B1、C5）。
- 现存唯一回剑意点：`ProjectileSystem.cpp` L764-766 在**拦截投射物**时 `GainSwordIntent(target, 1, 4)`，条件是 `has_agile_counter`（bit64=452 瞬身反打）——触发条件（拦截≠格挡/闪避）、挂载节点（452≠435/455）双双错位。
- 455 以攻代守的「闪避后 More+20%」攻击增益亦无任何实现路径。

**H4｜ReactiveWard 死组件 + 反击触发条件残缺（470 设计三条件只实现一条）**

- 设计 470 剑气反震：「偏转投射物、**格挡**、或**被近战击中**」时发射 5 道反击剑气。
- 实现：反击仅存在于 `ProjectileSystem.cpp` L749-772 的**投射物拦截**一条路径（发 1 发 35/55 点池，非 5 道，见 M3）。
- `BladeWard::DoCast` L64-65 放置的 `ReactiveWardComponent`（counter_window=0.8、counter_skill_id=4）**全项目零消费**：`ProcEngine`、`DamagePipeline` 中的 counter_window 逻辑全部针对 `PhantomFlashComponent`（技能 9）；ReactiveWard 仅有 `SkillSystem.cpp` L1152-1162 的过期移除。组件字段 `triggered/damage_absorb_pool/counter_skill_id` 均为死数据。
- 近战受击与格挡触发反击的分支在 DamagePipeline L1604-1619 位置完全缺席（该处只处理技能 9 的反击）。

**H5｜skill_mechanics.json 技能 4 零条目**

顶层键仅 `["version","comment","1","2","3"]`。技能 3 同款问题（M4）：运行时 `GetMechanics().GetPts/GetFloat(4, ...)` 无数据可读，行为层被迫硬编码数值（35/55 反击、0.3 拦截等无出处的魔法数），也堵死后续数据驱动整改路径。

### Medium

**M1｜基底口径偏差：Toggle→定时 Buff**

设计 Base：「召唤 3 柄旋转剑影……每秒消耗法力」（维持型）。实现：`mana_cost=30.0`、`cooldown=1.0` 一次性施放，10s 定时 Buff（`ward.duration=remaining=10.0`），「持久 402」（维持耗蓝 −15...45%）因此无立足点。对照技能 3 第三轮裁决（C0=数据保持施放型、口径待设计统一），本技能延续同款口径但**未做任何维持语义的补偿实现**。另：`tags=[Physical,Spell,Buff]` 缺设计要求的 `[Defense]`。

**M2｜拦截概率双口径（三处两套公式）**

| 位置 | 公式 | 效果（3 剑/0.3） |
| --- | --- | --- |
| `ProjectileSystem.cpp` L601、L739 | `chance = sword_count × interception_chance` | 0.9 |
| `DamagePipeline.cpp` L1200、L1625 | `chance = interception_chance`（仅 count>0 门控） | 0.3 |

同一「剑影拦截」机制在两条命中路径上概率差 3 倍。另有默认值不一致：`BladeWardComponent.interception_chance=0.15`（SkillDefs L895）vs DoCast 写入 0.3 vs 设计基础偏转 10%。

**M3｜反击数值与形态无出处**

设计 470：**5 道**反击剑气（物理）。实现：单发 `DamagePool(Physical, 35 或 55)`（L753-755），35/55 均无数据出处（skill_mechanics 无条目）；Baker L503 `sub_count=8`（470 置 8）行为层从未读取（`sentinel.count=3/6` 硬编码）——sub_count 是死数据，且 8≠5≠1。

**M4｜Keystone 默认生成器污染（413/431/432/452/475）**

`SkillRegistry::BuildDefaultContract`（L246-252）：`role = (max_points==1) ? Keystone : Passive`。技能 4 中 413 破釜沉舟、431 势不可挡、432 剑盾屏障、452 瞬身反打、475 永冻领域 因 max_points=1 全被推为 Keystone（设计全部为普通被动/Trigger；后者还连带 `affects_sword_intent` 误报）。契约错误=内嵌显式错标（C1 的 430/451/470/471/472）+ 生成器推导污染（本项）两层叠加。

**M5｜max_points 偏差清单（20 节点与设计不符）**

> 400 5v4、401 3v4、410 5v3、411 5v4、415 5v3、431 1v4、432 1v3、433 3v1、434 5v1、435 5v3、450 5v4、451 1v3、455 5v1、470 5v1、471 1v4、472 5v1、473 5v3、474 5v1、475 1v3、476 5v4（格式：实现v设计）

其中 431/432/451 的 1 直接造成 B1 不可达链；433/434/452/470/471/472/474/475 的偏离同时扭曲了 Keystone/Trigger 生成器推导（M4）。仅 402/403/412/413/414/430/450之外的 402(3)、403(4)、412(1)、413(1)、414(3)、452(1)、453(3)、454(3) 等与设计一致。

**M6｜fixture/测试掩盖不可达链（测试有效性缺陷）**

- `tests/SkillKeyNodeMatrixTestHelpers.hpp` L110：技能 4 冒烟节点 `{430, 451, 452, 470, 471}` —— **包含不可达的 452**；矩阵测试用 `AsAllocatedPoints(..., 1)` 直接构造 allocated_points，绕过 `AllocateSkillNode` 校验 → 测试全绿，游戏内该路线无法达成。
- `SkillSystemTests.cpp` L1056-1093「BladeWard interception counter loop」直接 `emplace` 组件并手置 `has_blink_counter/has_agile_counter/has_rainbow_qi` 全 true——同样绕过节点语义，掩盖 451/452 错位。
- 测试断言（SkillKeyNodeMatrix L123-126）仅验「组件存在 + trigger_counter 为真」，未覆盖任何设计语义。

**M7｜476 双前置的布局锚点截断（视觉层）**

476 元素曝光 prerequisites=[473×1, 474×1]。分配校验为 OR 语义（`SkillSystem.cpp` L1921-1936，任一满足即通过 ✅ 与设计「任意 Transmuter」一致）；但布局锚点计算取 `prerequisites.front()`（`SkillRegistry.cpp` L65、L199）→ 476 的相对坐标锚定在 473 上，走 474 线时父子连线视觉锚错。功能无损，视觉小偏差。

**M8｜分配器无不可达诊断**

`AllocateSkillNode`（L1919-1940）对「前置永远无法满足」无静态校验与告警，B1 的死路对开发者与玩家均静默。建议加载期或分配期增加 `required_points > dep.max_points` 检测（技能 3 报告同款建议，未落地）。

**M9｜skill_4_tree.json 无 max_points 字段**

与 skills.json 的 29 节点 prereq/坐标/名称 0 差异（✅），但该文件不含 max_points/stat_modifiers，作为独立布局源存在双源风险（同技能 3）。

## 7. 逐节点核对矩阵（29 节点）

图例：✅ 符合 ｜ ⚠️ 部分符合/偏差 ｜ ❌ 错误/错位 ｜ ✖️ 缺失（无任何实现）｜ ☠️ 不可达

| ID | 设计（名称/点数/效果摘要） | 实现 max/前置 | 契约角色 | Baker | 行为层 | 判定 |
| --- | --- | --- | --- | --- | --- | --- |
| 400 | 金钟罩 0/4 护甲+15...60% | 5/0×1 | — | flag1 | 固定+5 减伤 | ⚠️ 点数/幅度偏差 |
| 401 | 拨云见日 0/4 偏转+4...16% | **3**/0×1 | — | flag2 | +25% 一次性 | ⚠️ |
| 402 | 持久 0/3 维持耗蓝−15...45% | 3/0×1 | — | 无 | 无 | ✖️ |
| 403 | 剑压外放 0/4 反击几率/范围+10...40% | 4/0×1 | — | 无 | 无 | ✖️ |
| 410 | 厚积薄发 0/3 减伤效果+10...30%/每1000甲+1% | **5**/400×3 | — | 无 | 无 | ✖️ |
| 411 | 五行御守 0/4 全抗+6...24% | **5**/400×2 | — | flag4 | 固定 ResistAll 15 | ⚠️ +H2(t27) |
| 412 | 不动如山 0/1 Keystone 免疫击退/硬直/冰冻，移速−15% | 1/410×2 | Keystone | flag8 | is_solidified（剑不消耗，语义≠免疫/减速） | ❌ 语义不符 |
| 413 | 破釜沉舟 0/1 HP<35% 护甲×2、减伤24% | 1/412×1 | Keystone(污染) | 无 | 无 | ✖️ |
| 414 | 御剑防壁 0/3 御剑步减伤+10...30%、甲×2 | 3/400×2 | — | 无 | 无 | ✖️ |
| 415 | 坚韧回生 0/3 每秒回0.5...1.5%缺失生命 | **5**/410×2 | — | 无 | 无 | ✖️ |
| 430 | 剑意格挡 0/5 格挡率+4...20% | 5/401×3 | **Synergy(误)** | flag16 | 固定 BlockChance 10 | ❌ 角色+H2(t35) |
| 431 | 势不可挡 0/4 格挡效率+15...60% | **1**/430×2 | Keystone(污染) | 无 | 无 | ☠️+✖️ |
| 432 | 剑盾屏障 0/3 每次格挡获10...30 Ward | **1**/430×4 | Keystone(污染) | 无 | 无 | ☠️+✖️ |
| 433 | 鲜血壁垒 0/1 Keystone 血量替代法力 | **3**/432×2 | — | 无 | 无 | ☠️+✖️ |
| 434 | 无瑕之御 0/1 Keystone 每5s完全抵消一次击中 | **5**/431×4 | — | 无 | 无 | ☠️+✖️ |
| 435 | 剑意格御 0/3 格挡回剑意（交互点） | **5**/430×3 | — | 无 | 无 | ✖️ |
| 450 | 幻影步 0/4 闪避等级+15...60% | **5**/401×2 | — | 无 | 无 | ✖️ +H2(t34) |
| 451 | 借力打力 0/3 闪避后移速/攻速+5...15%(2s) | **1**/450×2 | **Trigger(误)** | flag32“瞬身反击”+sub_count=1 | has_blink_counter→反击伤害55 | ❌ 语义互换 |
| 452 | 瞬身反打 0/1 Trigger 闪避自动放流云刺 | 1/**451×3** | Keystone(污染)+sword_intent(误) | flag64“灵动反击” | has_agile_counter→+1剑/回剑意 | ☠️+❌ |
| 453 | 流风余韵 0/3 反打回血5...15% | 3/452×1 | — | 无 | 无 | ☠️+✖️ |
| 454 | 御剑闪步 0/3 御剑步闪避+50...150、刷新持续 | 3/450×2 | — | 无 | 无 | ✖️ |
| 455 | 以攻代守 0/1 闪避后More+20%+必得剑意（交互点） | **5**/**451×2** | — | 无 | 无 | ☠️+✖️ |
| 470 | 剑气反震 0/1 Keystone 偏转/格挡/近战→5道反击剑气 | **5**/403×2 | **Transmuter(误)** | flag128+sub_count=8(死) | trigger_counter→单发35/55 | ❌+M3 |
| 471 | 以眼还眼 0/4 反击More+20...80% | **1**/470×1 | **Transmuter(误)** | flag256“剑气如虹” | 反击转 Tag::Lightning | ❌+C4 |
| 472 | 雷霆法环 0/1 Transmuter 转闪电光环，与474互斥 | **5**/403×2 | Passive+TypeC(误) | 无 | 无 | ✖️ |
| 473 | 雷贯长虹 0/3 电频+30...90%等 | **5**/472×1 | — | flag512“剑刃风暴” | counter_spin→粒子 | ❌ 语义错挂 |
| 474 | 霜铠 0/1 Transmuter 转冰霜护盾，与472互斥 | **5**/470×1 | — | 无 | 无 | ✖️ |
| 475 | 永冻领域 0/3 冰龙卷范围/频率、冰碎 | **1**/474×1 | Keystone(污染) | 无 | 无 | ✖️ |
| 476 | 元素曝光 0/4 需任意Transmuter 曝光+4...16% | **5**/473×1+474×1 | — | 无 | 无 | ✖️（曝光被误挂472） |

统计：✅ 0 ｜ ⚠️ 3 ｜ ❌ 8 ｜ ✖️(含☠️) 18。

## 8. JSON 数据矛盾清单

1. `skills.json` skill 4 `talent_tree[].max_points` vs 设计点数：20 处偏差（M5），其中 3 处（431/432/451）引发 B1 不可达链。
2. `skill_contract.trigger_nodes=[451]` vs 设计 Trigger=452（C1）。
3. `skill_contract.transmuter_node_ids=[470,471]` vs 设计 [472,474]（C1）；`max_transmuters=2` 无互斥分组数据支撑（C3）。
4. `skill_contract.nodes[430].role=Synergy` vs 设计「Synergy 缺待补」（C1）。
5. `resist_models={"472":"TypeC_Exposure"}` vs 设计曝光节点=476（C1）。
6. `nodes[452].affects_sword_intent=true` vs 设计剑意交互点=435/455（H3 链）。
7. `stat_modifiers`：411 t27/430 t35/450 t34 枚举错位（H2）；26/29 节点 stat_modifiers 为空导致点数成长无数据（H1）。
8. `skill_mechanics.json` 无技能 4 条目（H5）。
9. `skill_4_tree.json` 与 skills.json 一致但缺 max_points（M9）。

## 9. 架构与数据驱动偏差

- **三层一致性反向成立**：契约（451=Trigger）、Baker（451=瞬身反击）、行为层（451=BlinkCounter）、组件注释（// Talent 451）四层**一致地错**——说明错误源头在最上游（数据表编制时把 451/452 的语义对调），下游是忠实执行。修复必须从 skills.json 起头逐层改，而非只改某一层。
- 「剑气如虹」「剑刃风暴」两个节点名不存在于技能 4 设计（后者属技能 7 万剑归宗 362 节点），提示行为层编制时参考了错误的节点清单。
- `ResolveElementalConversion` 的 472/474 映射与分配校验的 OR 语义两处**基建正确**，说明通用层已具备支撑条件，缺口集中在技能 4 的数据与接线。

## 10. 符合项

1. 29/29 节点存在，编号连续无缺号；`skill_4_tree.json` 与 `skills.json` 零差异。
2. 交付原型对齐：OrbitingSentinel + ReactiveWard 双原型烘焙（Baker L72-75），环绕剑影/拦截的基底链路可用（投射物路径有集成测试）。
3. 分配校验多项前置为 OR 语义（L1921-1936），与 476「任意 Transmuter」设计一致。
4. `SkillRegistry` 加载期对 exclusion group 有完整性校验与告警（L471-528），基建就绪（技能 4 无数据可用）。
5. `BladeWardComponent` 过期 tick（SkillSystem L1167-1187）与 `StatsDirty` 通知链路完整。
6. 六节点（400/401/411/412/430/470）的 feature_flags 位打包与行为层解读一一对应（数值语义另计）。

## 11. 整改路线

**P0（随本轮修改必改——数据修正 + 解锁可达性）**
1. 修 `skills.json` 技能 4：431 max 1→4、432 max 1→3、451 max 1→3（解 B1）；按 M5 清单校其余 17 处 max_points 对齐设计（433→1、434→1、452→1、470→1、471→4、472→1、473→3、474→1、475→3、476→4 等）。
2. 契约修正：trigger_nodes 451→452；transmuter_node_ids [470,471]→[472,474]；430 Synergy 移除（或按设计补真正 Synergy）；472 role=Transmuter、曝光模型移至 476；470=Keystone、471=Passive；清 452 的 Keystone/sword_intent 误标；为 472/474 写入同一 keystone_exclusion_group。
3. 行为层/组件注释修正：`BladeWardNodes` 451/452 语义对调归位；删除或改名「剑气如虹/剑刃风暴」；`SkillDefs.hpp` L896-901 注释同步（451 借力打力、452 瞬身反打、471 以眼还眼、473 雷贯长虹）。
4. Baker case 4 接线：472/474 调用 `ResolveElementalConversion`；452 走 TriggerRule 通道（触发流云刺 skill 1，ICD 2s——契约参数已备，仅换节点号）；删除 470 的 sub_count=8 或改为 5 并由行为层消费。

**P1（本轮或下轮——补齐 High）**
5. 点数成长：为 20 个多点节点补 stat_modifiers/机制数据（优先 400/401/403/410/411/430/450 七个基干），行为层按 points 缩放；修正 411 t27→ResistAll、430 t35→BlockChance/BlockRating、450 t34→DodgeRating。
6. 剑意交互：435 格挡回剑意、455 闪避后 More+20%+回剑意（挂对触发事件），移除 L765 的错挂条件。
7. ReactiveWard 复活或删除：实现格挡/近战受击反击分支（470 设计三条件），或明确移除死组件并收敛到单一反击通道；反击形态对齐 5 道剑气。
8. `skill_mechanics.json` 增补技能 4 条目（对齐技能 3 的数据驱动改造）。

**P2（跟进轮次）**
9. 统一拦截概率口径（M2 三处两套公式 + 默认值 0.15/0.3/设计 10%）；基底 Toggle 口径随技能 3 C0 裁决统一（M1，含补 [Defense] 标签）；Keystone 默认生成器改为显式契约驱动（M4）；分配器加不可达诊断（M8）；测试 fixture 移除 452 并补「真实分配路径可达」断言（M6）；476 布局锚点支持多前置（M7）。

## 12. 剩余风险

1. 行为层 451/452 语义互换若只修数据不动代码，反击行为会静默反转（451 将承接瞬身反打语义）——P0 三步必须同轮完成并跑全量测试。
2. `BladeWardComponent` 字段（has_blink_counter 等）名称已固化错误语义，重命名波及 ProjectileSystem/DamagePipeline/测试三处，建议随 P0 一起改。
3. 技能 5-12 未审，预计存在同源问题（Keystone 生成器污染、skill_mechanics 覆盖缺口为全局性）。
4. 本报告基于静态审查；拦截概率双口径在实机中的实际表现需一轮运行时验证。

## 13. 证据命令摘要

```
rg -n "剑气|BladeWard" 设计文档/职业设计草案_剑修.md        # §3.4 定位 L283-336
python dump skills.json(skill 4) / skill_contracts_compact.json(skill_id=4) / skill_mechanics.json 顶层键
python diff skill_4_tree.json vs skills.json               # 29 节点，0 差异
python 校验 prereq 可达性                                   # 433/434/452/455 直接不可达
rg -n "case 4|BladeWard" src/game/systems/skill/SkillSpecializationBaker.cpp   # L72-75, L486-510
Read src/game/systems/skill/behaviors/BladeWard.cpp            # 全文 88 行
Read src/game/foundation/components/Stats.hpp L224-284         # StatType 索引
Read src/game/foundation/data/SkillRegistry.cpp L227-254       # BuildDefaultContract
Read src/game/systems/skill/SkillSystem.cpp L1900-1974         # 分配校验
Read src/game/systems/combat/DamagePipeline.cpp L1168-1212, L1596-1645
Read src/game/systems/skill/ProjectileSystem.cpp L592-616, L738-777
Read tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp L1-159
Read tests/integration/SkillSystemTests.cpp L1050-1093
rg -n "keystone_exclusion_group" assets/data/skills.json       # 技能 4 无数据
rg -n "ReactiveWardComponent|counter_skill_id" src tests       # 死组件确认
rg -n "\b403\b" src/game                                       # 零引用确认
```

## 14. 审查记录

- 轮次 1（2026-09-08）：首次审查，判定**修改**。Blocker 1 / Critical 5 / High 5 / Medium 7。
- 整改后请按 P0→P1 顺序回归，并复跑 `SkillKeyNodeMatrixIntegrationTests`、`SkillSystemTests`（BladeWard 用例）与数据校验脚本。

---

## 15. 深度交叉复核与增补审查（2026-09-09 第二轮）

- **复核日期**：2026-09-09
- **复核轮次**：第 2 轮（初审准确性全量核实验证 + ECS 运行时调用链深度交叉取证）
- **复核结论**：**维持修改（Blocker）判定**。初审 23 项发现（B1, C1~C5, H1~H5, M1~M9）经代码与数据比对**100% 准确属实**；深度交叉排查进一步发现 **1 项新增 Blocker、2 项新增 Critical、3 项新增 High、3 项新增 Medium、1 项 Low**。

### 15.1 初审论断准确性复核确认

| 初审项 | 结论 | 复核证据与代码对照 |
|---|---|---|
| **B1 不可达链** | ✅ 确证真实 | `skills.json` 中 431 max=1 (434 需 4 点)、432 max=1 (433 需 2 点)、451 max=1 (452 需 3 点、455 需 2 点)，导致 433/434/452/453/455 共 5 节点在数据层绝对锁死。 |
| **C1 契约五重错位** | ✅ 确证真实 | `skill_contracts_compact.json` 与内嵌契约中：Trigger 错挂 451、transmuter 错挂 [470,471]、synergy 错标 430、472 错标 Passive+TypeC、476 曝光模型遗失。 |
| **C2 行为层节点颠倒** | ✅ 确证真实 | `BladeWard.cpp` L18-29 常量表把 451 命名为 BlinkCounter 并赋予反击增强；452 命名为 AgileCounter 并赋予 +1 剑；471 命名为“剑气如虹”；473 命名为“剑刃风暴”。 |
| **C3 转质全链断开** | ✅ 确证真实 | Baker L486-510 未调用 `ResolveElementalConversion(472/474)`；`skills.json` 缺少 `keystone_exclusion_group`；`max_transmuters=2` 放任双持。 |
| **C4 元素标签错挂** | ✅ 确证真实 | `ProjectileSystem.cpp` L754 反击元素转换门控为 `ward->has_rainbow_qi`（471 以眼还眼，物理增伤点），而非 472 雷霆法环。 |
| **C5 19 节点零实现** | ✅ 确证真实 | Baker case 4 仅匹配 10 节点，其余 19 节点全 src 零命中、零分支、零处理。 |
| **H1~H5 机制与数据缺陷** | ✅ 确证真实 | 点数全布尔化（H1）；411 t27 (暗抗)、430 t35 (闪避)、450 t34 (持续时间) 枚举三连错（H2）；435/455 剑意交互未实现且 L765 拦截错回剑意（H3）；ReactiveWard 全项目零消费（H4）；`skill_mechanics.json` 顶层缺 4（H5）。 |
| **M1~M9 口径与工程缺陷** | ✅ 确证真实 | Toggle 口径（M1）；拦截双公式（M2）；反击形态与数值无出处（M3）；Keystone 默认生成器污染（M4）；20 节点 max_points 偏差（M5）；集成测试绕过加点假绿（M6）；476 双前置连线视觉锚错（M7）；分配器无死路检测（M8）；树文件无 max_points（M9）。 |

---

### 15.2 第二轮新增发现项（深度调用链审计）

#### BLOCKER

**B2｜`OrbitingSentinelComponent` 生命周期泄漏（永不注销）致使 30% 偏转永久常驻**
- **位置**：`src/game/systems/skill/behaviors/BladeWard.cpp` L59；`src/game/systems/skill/SkillSystem.cpp` L1183-1188
- **现象**：
  `BladeWard::DoCast` 施法时为玩家挂载 `OrbitingSentinelComponent`（设置基础偏转 `interception_chance = 0.3f`）。
  但在 `SkillSystem.cpp` 护盾 10 秒倒计时结束（`ward.remaining <= 0.0f`）的清理代码中：
  ```cpp
  // SkillSystem.cpp L1187
  registry.remove<BladeWardComponent>(entity);
  // 致命遗漏：未移除 OrbitingSentinelComponent！
  ```
- **后果**：`OrbitingSentinelComponent` 一旦挂上后永久驻留玩家实体。`OrbitingSentinelDeliverySystem::Update` 每帧持续运行该组件的拦截逻辑，玩家在护盾失效后，整场战斗永久享受 30% 投射物被动拦截。

#### CRITICAL

**C6｜四重拦截判定竞争混乱，OrbitingSentinelDeliverySystem 吞噬投射物阻断反击链**
- **位置**：`src/game/systems/skill/OrbitingSentinelDeliverySystem.cpp` L52-82；`ProjectileSystem.cpp` L599-612, L738-747；`DamagePipeline.cpp` L1196-1215, L1621-1637
- **现象**：全系统存在 4 套彼此割裂的拦截与偏转调度：
  - `OrbitingSentinelDeliverySystem.cpp` L52-82：以玩家为中心 32px 范围内对敌方投射物独立掷骰 `sentinel.interception_chance`。命中直接打上 `KilledTag` 并在帧末销毁。不扣减剑数，绝不触发反击！
  - `ProjectileSystem.cpp` L599-612：飞行碰撞判定，以 `sword_count * interception_chance` 掷骰拦截，成功则压入 `DeferredAction::Damage`。
  - `ProjectileSystem.cpp` L738-747：执行延迟动作时，第 2 次执行完全相同的独立掷骰。成功则扣减剑数并触发 L751 反击。
  - `DamagePipeline.cpp` L1196-1215：最终伤害结算层，以 `interception_chance` 独立掷骰。成功则扣减剑数并置伤为 0，但不触发反击。
- **后果**：近身投射物优先进入 32px 判定，被 `OrbitingSentinelDeliverySystem` 直接静默销毁，根本无法进入 `ProjectileSystem` 的延迟反击动作，470 剑气反震在近身拦截时被自身的底层原型完全吞噬。

**C7｜412 不动如山 is_solidified 在 ProjectileSystem.cpp 中使拦截完全失效**
- **位置**：`src/game/systems/skill/ProjectileSystem.cpp` L740-747
- **代码取证**：
  ```cpp
  // ProjectileSystem.cpp L740
  if (!ward->is_solidified && ward->sword_count > 0 &&
      (float)GetRandomValue(0, 1000) / 1000.0f < chance) {
    intercepted = true;
    ward->sword_count--;
    ...
  }
  ```
- **后果**：注意判定门控中的 `!ward->is_solidified`。一旦玩家点亮 412 不动如山（`ward->is_solidified = true`），该 if 条件恒为假，`intercepted` 永远无法置为 true！原本设计意图为「剑影不被消耗」（如 `DamagePipeline.cpp` L1202 正确落地的逻辑），但在 `ProjectileSystem.cpp` 中却将 `is_solidified` 误写为拦截启动门槛，导致玩家只要点亮 412，`ProjectileSystem` 彻底罢工、完全不再拦截任何投射物！

#### HIGH

**H6｜Baker 通用触发通道无法支持防御型 Trigger（452 瞬身反打）**
- **位置**：`src/game/systems/skill/SkillSpecializationBaker.cpp` L123-135
- **代码取证**：
  ```cpp
  if (node_contract && node_contract->role == SpecNodeRole::Trigger) {
    if (out_triggers && node_contract->trigger.trigger_skill_id != 0) {
      TriggerRule rule;
      rule.rule_id = node_id;
      rule.listen_event = CombatEventType::OnSkillHit; // 硬编码攻击命中！
      rule.requires_crit = node_contract->trigger.requires_crit;
      rule.cast_skill_id = node_contract->trigger.trigger_skill_id;
      rule.effectiveness = node_contract->trigger.effectiveness;
      rule.internal_cooldown = node_contract->trigger.internal_cooldown;
      rule.target_mode = TriggerTargetPolicy::Victim; // 硬编码目标为受害者！
      out_triggers->AddRule(rule);
    }
  }
  ```
- **后果**：Baker 的契约通用触发通道将所有 Trigger 硬编码为「攻击命中敌人时向受害者施放」。但 452 瞬身反打是防御型触发器（闪避成功时向攻击者施放流云刺），契约 Schema 缺失 `listen_event` 与 `target_mode` 字段，且 Baker 无针对防御触发的特化分派。即使修复了数据层错位，452 也会被错误烘焙为命中触发。

**H7｜闪避判定在 DamagePipeline.cpp 首部提前 return，451/452/455 闪避链零挂接**
- **位置**：`src/game/systems/combat/DamagePipeline.cpp` L641-650
- **代码取证**：
  ```cpp
  if (defense_resolution.dodged) {
    DamageResult dodged_result;
    dodged_result.was_dodged = true;
    dodged_result.block_multiplier = defense_resolution.block_multiplier;
    return dodged_result; // 提前退出伤害管线
  }
  ```
- **后果**：闪避成功后直接返回。尽管在 L121 派发了 `CreateOnDodge` 事件，但全代码库无任何系统或被动监听该事件来赋予 451 借力打力（移速攻速 +5%...15%）或 455 以攻代守（下次攻击 More+20% 且必得剑意）。分支 C「灵动反击」整条闪避增益支线在底层完全脱节。

**H8｜401 拨云见日导致拦截几率几何级溢出（165%~960%）**
- **位置**：`src/game/systems/skill/behaviors/BladeWard.cpp` L76；`src/game/systems/skill/ProjectileSystem.cpp` L601, L739
- **数值失控分析**：
  - `BladeWard.cpp` L76：点亮 401 带来 `ward.interception_chance += 0.25f`（基底 0.3f，累加达 0.55f）。
  - `ProjectileSystem.cpp`：实际计算公式为 `chance = sword_count × interception_chance`：
    - 3 柄剑时：`3 × 0.55 = 1.65`（165% 必中）；
    - 452 加成 4 柄剑时：`4 × 0.55 = 2.20`（220%）；
    - 处于 `is_empowered` 状态时：`6 × (0.55 × 2.0) = 6.60`（660%）！
  - 设计文档要求 401 增加 4%...16% 偏转（与基础 10% 合计 14%~26%），实现中投入 1 点即产生绝对拦截，数值体系彻底失控。

#### MEDIUM

**M10｜投射物双重掷骰致使未点 401 时拦截几率暴跌**
- **位置**：`src/game/systems/skill/ProjectileSystem.cpp` L602 与 L741
- **现象**：投射物在飞行中调用 `GetRandomValue` 掷骰通过后生成 `DeferredAction`，但进入延迟命中结算时再次进行相同的随机掷骰。在低几率场景下（如 1 剑 0.3 几率），实际拦截率跌落为 `0.3 × 0.3 = 0.09`（9%）。第一次通过但第二次失败时，投射物直接穿透并造成伤害。

**M11｜470 反击形态为单体目标瞬发近战，脱离「四周发射 5 道剑气」设计**
- **位置**：`src/game/systems/skill/ProjectileSystem.cpp` L753-763
- **现象**：代码直接调用 `DamagePipeline::ResolveDamage`，附加 `Tag::Hit | Tag::Melee` 对 `act.instigator` 单体目标进行瞬发伤害结算，既无剑气实体生成，亦无向四周发射的过程。若攻击者处于远距离或掩体后，仍会被瞬发近战结算。

**M12｜行为层随意堆砌无出处机制**
- **位置**：`src/game/systems/skill/behaviors/BladeWard.cpp` L76-80
- **现象**：451 额外加 0.1f 拦截率、452 额外加 1 柄剑影、471 额外加 0.15f 拦截率、empowered 几率翻倍等，均为开发者随意添加的无出处逻辑，与 GDD 规范完全相悖。

#### LOW

**L1｜BladeWard::DoCast 施法热路径存在 std::string 临时堆分配**
- **位置**：`src/game/systems/skill/behaviors/BladeWard.cpp` L45
- **现象**：构造 `BuffEffect` 时显式调用 `std::string(BuffIdToString(BuffId::BladeWard))`，产生临时堆分配，违反 `conductor/code_standard.md` §2.1 热路径严禁临时堆分配规范。

### 15.3 增补问题与架构关联矩阵

| 问题 ID | 级别 | 涉及模块 / 文件 | 破坏机制 | 修复层级 |
| --- | --- | --- | --- | --- |
| B2 | Blocker | `SkillSystem.cpp:1187` | 护盾结束后 OrbitingSentinel 永不销毁，全图白嫖 30% 偏转 | P0 必改 |
| C6 | Critical | `OrbitingSentinelDeliverySystem.cpp:52` / `ProjectileSystem.cpp` | 原型独立判定销毁投射物，导致 470 反击剑气链断裂 | P0 必改 |
| C7 | Critical | `ProjectileSystem.cpp:740` | `!ward->is_solidified` 条件错写，点亮 412 直接阻断拦截 | P0 必改 |
| H6 | High | `SkillSpecializationBaker.cpp:127` | TriggerRule 硬编码 OnSkillHit/Victim，452 防御触发架构性失效 | P0 必改 |
| H7 | High | `DamagePipeline.cpp:641` / `CombatEvents.hpp` | 闪避提前退出，451/455 无事件监听接入，闪避增益支线全瘫痪 | P1 补齐 |
| H8 | High | `BladeWard.cpp:76` / `ProjectileSystem.cpp:601` | 401 错误加 0.25 且乘剑数，几率达 165%~660% 严重失真 | P1 补齐 |
| M10 | Medium | `ProjectileSystem.cpp:602, 741` | 双重随机掷骰，低几率平方级暴跌至 9% | P1 补齐 |
| M11 | Medium | `ProjectileSystem.cpp:753-763` | 470 瞬发单体近战，缺失向四周发射 5 道剑气投射物形态 | P1 补齐 |
| M12 | Medium | `BladeWard.cpp:76-80` | 451/452/471 行为层硬编码加剑/加偏转等野指针数值 | P0 必改 |
| L1 | Low | `BladeWard.cpp:45` | std::string 临时堆分配违规 | P2 优化 |

### 15.4 关键证据代码索引

```text
# B2: 生命周期泄漏
src/game/systems/skill/SkillSystem.cpp:1183-1188 (仅 remove BladeWardComponent，遗漏 OrbitingSentinelComponent)
# C6: 原型静默吞噬与竞争
src/game/systems/skill/OrbitingSentinelDeliverySystem.cpp:52-82 (32px 独立掷骰打 KilledTag 并不触发反击)
src/game/systems/skill/ProjectileSystem.cpp:601-610 与 739-747 (双重随机掷骰)
# C7: 412 is_solidified 逻辑反向错误
src/game/systems/skill/ProjectileSystem.cpp:740 (if (!ward->is_solidified && ...) 错误阻断拦截)
# H6: Baker 硬编码攻击命中
src/game/systems/skill/SkillSpecializationBaker.cpp:127-132 (rule.listen_event = OnSkillHit / rule.target_mode = Victim)
# H7: 闪避断路
src/game/systems/combat/DamagePipeline.cpp:641-650 (defense_resolution.dodged 成立后立即 return)
# H8: 偏转数值几何级失控
src/game/systems/skill/behaviors/BladeWard.cpp:76-81 (interception_chance += 0.25f, 乘 3/4/6 剑)
```

### 15.5 增补整改指导（更新 P0/P1 执行动作）

在落实技能 4 前，必须在原报告第 11 节整改路线基础上追加以下硬性技术改动：

- **[P0 必做]** 闭环生命周期回收（修 B2）：在 `SkillSystem.cpp:1187` 中同步执行 `registry.remove<OrbitingSentinelComponent>(entity)`，确保 Buff 退出时彻底清除环绕与拦截状态。
- **[P0 必做]** 修复 412 拦截瘫痪反转错误（修 C7）：在 `ProjectileSystem.cpp:740` 将 `!ward->is_solidified` 从外部 if 门控中剥离，仅作为扣减剑数的保护条件：
  ```cpp
  if (ward->sword_count > 0 && roll < chance) {
    intercepted = true;
    if (!ward->is_solidified) {
      ward->sword_count--;
    }
    ...
  }
  ```
- **[P0 必做]** 统一投射物拦截管线（修 C6, M10）：
  - 彻底废除 `OrbitingSentinelDeliverySystem.cpp:52-82` 的重复独立拦截逻辑，将拦截职责唯一收敛到 `ProjectileSystem`，确保投射物拦截与 470 反击发射闭环协同；
  - 消除 `ProjectileSystem.cpp` L602 与 L741 的双重掷骰，仅保留 L602 一次性判定结果。
- **[P0 必做]** Baker 扩展防御触发通道（修 H6）：在 `SkillSpecializationBaker.cpp` 中识别节点 452（瞬身反打），将 TriggerRule 特化配置为：`rule.listen_event = CombatEventType::OnDodge` 且 `rule.target_mode = TriggerTargetPolicy::Attacker`。
- **[P1 必做]** 打通闪避事件监听（修 H7）：在 ProcEngine 或 CombatEventDispatcher 中建立针对 `CombatEventType::OnDodge` 的专精回调，为玩家装配 451 移速攻速 Buff，并在下次攻击伤害乘区中注入 455 的 20% More 伤害与必得剑意。
- **[P1 必做]** 拦截率公式归一化（修 H8）：移除 `BladeWard.cpp` 中 401 粗暴加 0.25 与多处无出处的偏转加成；严格按设计基础 10% + 401 (4%...16%) 换算为单剑拦截率，避免出现超过 100% 的荒谬溢出。

---

## 16. 第 3 轮复审（2026-09-09 整改变更验证）

### 16.1 复审范围与基线

- 变更边界：22 个文件，+1309/-557（`git diff HEAD`；含 `AGENTS.md` +3 行流程指令，见 R3-10）。
- 验证手段：全量 diff 逐层取证（行为层/运行时层/数据层/契约层/测试层）、数据脚本直读 JSON、codebase-memory-mcp 图谱交叉核对、`build.bat` RelWithDebInfo 全量构建（exit 0）、`ctest -R nmd.tests.skill`（unit + integration 2/2 通过）。
- 设计对照：`设计文档/职业设计草案_剑修.md` §3.4 L283-336。

### 16.2 判定：提交

P0 清单（B1/B2/C1/C2/C3/C4/C6/C7/H6/M12）**全部关闭**，P1 主体（H2/H3/H5/H7/H8/M2/M5/M6/M10）关闭，无新增 Blocker/High 问题，构建与测试全绿。残留项集中在 P2 范围与既知未实现节点（C5 尾巴），见 16.4，不构成高风险缺陷。

### 16.3 已解决项证据矩阵

| 编号 | 状态 | 关键证据 |
|---|---|---|
| B1/M5 | ✅ | skills.json 技能 4 talent_tree 29 节点 max_points 与设计全一致（431=4、432=3、451=3 等 20 处）；434[431x4]/433[432x2]/452[451x3] 链全解锁 |
| B2 | ✅ | SkillSystem.cpp:1285-1305 过期同步 remove OrbitingSentinelComponent + ReactiveWardComponent；测试 "B2 expiration cleanup" 覆盖 |
| C1 | ✅ | 契约显式化：452=Trigger{trigger_skill_id=1,icd=2.0,consumes_mana=false}、472/474=Transmuter+exclusion_group=1、476=TypeC_Exposure、470/433/434/412=Keystone、430 Synergy 移除；compact 副本与 skills.json 一致 |
| C2 | ✅ | BladeWard.cpp 节点常量归位（CounterSpeed=451/BlinkCounter=452/StaticField=472/FrostArmor=474 等），SkillDefs.hpp:892-910 注释同步，feature_flags 位分配与 Baker 一致 |
| C3 | ✅ | Baker case 4 接线 ResolveElementalConversion(472→Lightning/474→Cold)，SkillBehaviorBase.hpp:22-77 主 switch 含 472/474 映射 |
| C4 | ✅ | ProjectileSystem 反击元素改由 is_lightning_ward/is_cold_ward 决定，旧 has_rainbow_qi→Lightning 错挂移除 |
| C6 | ✅（收敛式） | BladeWard.cpp:110 `sentinel.interception_chance = 0.0f` 关停独立拦截；OrbitingSentinelDeliverySystem.cpp:53 概率门控使其永不掷骰；拦截唯一收敛 ProjectileSystem（系统本体保留，见 R3-5） |
| C7 | ✅ | `is_solidified` 剥离为仅扣剑门控（ProjectileSystem.cpp:746-748、DamagePipeline.cpp:1208-1210 同构）；测试两分支覆盖 |
| M10 | ✅ | ProjectileSystem.cpp:601 单次掷骰存 DeferredAction.flag，执行段 `intercepted = act.flag` 不再二次掷骰 |
| H6 | ✅ | Baker 两处（Bake/SyncTriggerRules）：452 → `listen_event=OnDodge` + `target_mode=Attacker`；ProcEngine.cpp:89-97 防御事件 attacker=event.target 解析正确；测试 "452 defensive trigger" 用真实 LoadFromJson 数据全链路断言 skill 1 SkillExecution 生成 |
| H7/H3 | ✅ | SkillSystem.cpp:972-1076 OnDodge/OnBlock 钩子：451 移速/攻速 5%×pts、455 伤害 More20%+必得剑意、432 Ward 10×pts、435 掷骰回剑意；OnDodge 从 DamagePipeline 派发（L122） |
| H4 | ✅（主体） | 470 反击三条件补齐：DamagePipeline Calculate/Batch 各增 Melee hit/Block 反击分支，反击带 Tag::SecondaryHit 防自放大 |
| H2 | ✅ | 411 t28(ResistAll)/430 t36(BlockChance)/450 t47(DodgeRating)，与 Stats.hpp 枚举精确核对 |
| H1 | ✅（主体） | 400 t8(Armor PercentAdd 15/pt)/411/430/450 数据齐 + StatsSystem.cpp:484 通用消费路径；401 行为层按点计算 |
| H5 | ✅（数据） | skill_mechanics.json 新增 key "4"，29 节点全量参数（消费不全归入 C5 残留） |
| H8/M2/M12 | ✅ | 拦截率 = clamp(sword_count × (0.10+0.04×401pts), 0, 1)，empowered ×1.5 封顶 1.0；双管线公式统一；无出处加成清除 |
| M6 | ✅（主体） | fixture 冒烟节点 452 可达化、互斥矩阵与触发矩阵同步；新增 6 个行为测试（412/470/451/455/432/435/452/B2） |
| L1 | ✅ | BladeWard.cpp static const buff 字符串（仅限该文件，见 R3-6） |
| M1 | ✅（部分） | tags 补 Defense（Tag bit24 + TagRegistry + skills.json）；Toggle 口径未改（P2） |

### 16.4 残留与新增发现（第 3 轮）

**Medium：**

- **R3-1 技能 3 契约校验失败刷屏（既有遗留，被新校验显性化）**：`SkillRegistry.cpp:517-524` 新增豁免逻辑（transmuter 节点有 exclusion_group 才豁免 max_transmuters），但技能 3 的 370/372 无 `keystone_exclusion_group` 数据 → 每次加载报 `Skill 3 contract validation failed: transmuter overflow, got=2 max=1`（测试进程启动即刷 6+ 次，`--list-tests` 亦触发）。已核实为既有问题（HEAD 版本 L503 同条件也失败），失败仅 LOG_ERROR 不阻断（SkillRegistry.cpp:716-722）。**建议**：技能 3 的 370/372 补 exclusion_group 或修正 max_transmuters 语义。
- **R3-2 拦截路径不触发 470 反击（设计语义丢失，双路径不对称）**：DamagePipeline.cpp:1248/1735 `if (!intercepted)` 使"偏转"这一设计明确的三条件之一在 DamagePipeline 拦截路径不触发反击；ProjectileSystem 拦截路径有反击 → 同一攻击在两条管线行为不同。影响面小（仅 Projectile 标签但实体无 Projectile 组件的伤害进入此路径）。
- **R3-3 反击逻辑 5 处复制**：ProjectileSystem 1 处 + DamagePipeline Calculate 2 处 + CalculateBatch 2 处，各约 20 行（元素选择 + 35.0f 魔数 + DamageRequest 构造）。`mechanics 470.base_damage=35.0` 数据存在但零消费者。另 Batch 拦截反击缺 `skill_id != 4` 守卫（L1705-1708），仅靠 SecondaryHit 单层防递归（Calculate 路径是双守卫）——当前安全，属一致性缺陷。
- **R3-4 TagRegistry 三份事实源**：GetTagName/TagFromString 改为手写 switch/if-链（TagRegistry.hpp:98-178），kTagInfoTable（31 条目）仍被 SkillDefs.hpp:368/377 消费 → 新增 Tag 需同步 3 处，"single source of truth" 注释被删除。`sword_skill` 小写别名删除由 SkillRegistry::StringToTag kLegacyTags（L638-641）兜底，无回归。
- **R3-5 OrbitingSentinelDeliverySystem 空转保留**：C6 采用概率 0 关停而非删除，系统仍每帧 Update 且拦截分支成为死代码；B2 过期清理使组件出域，但常驻期间每帧白算距离。低开销，建议后续批次真删或注释标注。
- **R3-6 SkillSystem InitHooks 数值双源 + 堆分配**：451/455/432/435 点值硬编码（5.0f/20.0f/10.0f/0.15f）与 skill_mechanics.json 重复，max() 合并使数据改动不生效；每事件构造 BuffEffect 的 std::string 临时（code_standard §2.1），与 BladeWard.cpp 已修的 static const 做法不一致。
- **R3-7 ReactiveWardComponent 半死**：仅 timer/ward_duration 有消费（SkillSystem.cpp:1251-1265 到期清理）；counter_window/triggered/damage_absorb_pool/counter_skill_id 在技能 4 路径零消费。H4 的"复活或删除"未完成，B2 顺带做了过期清理。
- **R3-8 C5 残留（约 13 节点零运行时效果）**：已实现 435/455/432/452；472/474 半实现（转质+标签 ✓，472 静电光环 tick、474 冰龙卷未做）；402/403/410/413/415/431/433/434/453/454/473/475/476 未实现（476 曝光完全无运行时，mechanics exposure_* 零消费者）。属原报告既知分批范围，非本轮回归。

**Low：**

- **R3-9** M4 残留 1/5：BuildDefaultContract.cpp:249-250 未改，413（max=1，设计 Passive）无显式契约覆盖 → 仍误标 Keystone（原 5 处已清 4 处）。测试无 keystone_node_ids 精确断言故测不出。
- **R3-10** AGENTS.md +3 行（子代理并行/重试/rg 同步规则）与技能 4 整改无直接关联，属流程文档变更——请作者确认是否有意纳入本次提交。
- **R3-11** deprecated 字段 has_blink_counter/has_agile_counter/has_rainbow_qi（SkillDefs.hpp:908-910，字段名与 451/452 语义错位有误导性）仅写入零消费，建议删除。
- **R3-12** 455 设计"下一次攻击 More+20%"实现为 2s 全程伤害 buff（保真度近似）；435 的 mechanics crit_bonus=0.05 无实现；435 行为测试无断言（仅执行不验证）。
- **R3-13** 452 触发的流云刺走 skill 1 通用施放，未确认使用"已专精版本"加成（P2）。
- **R3-14** M7/M8/M9 维持未修（476 锚点、不可达诊断、skill_4_tree.json 双源），P2 范围可接受。

### 16.5 后续批次建议

1. P1 尾巴：403/410 stat_modifiers 数据、472 静电光环/474 冰龙卷、反击代码合并为单一函数（含补 Batch 守卫）、DamagePipeline 拦截路径补 470 反击（R3-2）、ReactiveWard 复活或删除（R3-7）、InitHooks 改读 mechanics（R3-6）。
2. P2：C5 剩余节点（476 曝光优先，数据已就绪）、413 显式契约、TagRegistry 收敛单源、OrbitingSentinelDeliverySystem 死代码清理、455"下一次攻击"语义、技能 3 转质互斥数据（R3-1）。
3. 测试补强：435 剑意回复断言、keystone_node_ids 精确断言、Batch/Calculate 反击一致性用例。

### 16.6 第 3 轮收尾执行记录（2026-09-09，子代理实施 + 复核）

16.4 中以下 R3-x 项已在本轮收尾修复（由实现子代理按本节清单执行，主代理逐项复核 diff 并独立复跑测试）：

- **R3-1 ✅**：skills.json 技能 3 节点 370/372 各加 `keystone_exclusion_group: 1`；skill_contracts_compact.json 同步为列式 `keystone_exclusion_groups: {"370":1,"372":1}`（沿用技能 4 的 472/474 格式）。加载刷屏消除（全量测试输出 `contract validation failed` 计数 = 0）。
- **R3-9 ✅**：skills.json 技能 4 契约补 node 413 = Passive（SkillOnly），413 不再被 BuildDefaultContract 误标 Keystone。compact 副本**有意不加 413**：compact 为列式"关键节点桶"而非全量列表，加入会改变 `ExpectedTotalKeyNodeCount=97` 语义导致矩阵测试失败——以 skills.json 显式契约为准已完整达成目的。
- **R3-11 ✅**：删除 BladeWardComponent 的 has_blink_counter/has_agile_counter/has_rainbow_qi 字段（SkillDefs.hpp）及 BladeWard.cpp 写入行；rg 确认全库零消费。
- **R3-3（部分）✅**：CalculateBatch 两处反击条件补 `skill_id != 4` 守卫，与 Calculate 单体路径双守卫对齐（防御递归从单层 SecondaryHit 升为双层）。
- **R3-2 ✅**：Calculate/CalculateBatch 反击段改为无条件进入，触发条件扩为 `intercepted || isMelee || isBlocked`——设计 §3.4 的"偏转"反击条件在 DamagePipeline 路径补全，两条管线语义一致。反击请求 tags/skill_id/基伤不变。
- **测试补强 ✅**：SkillContractRegistryTests 新增技能 3 转质断言（370/372 role+group）与 SUBCASE"Skill 4 keystone/passive node contract is exact"（keystones={412,433,434,470}、413=Passive、472/474 group=1）。

收尾验证：build.bat（RelWithDebInfo）成功；`nmd.tests.skill` 2/2、`nmd.tests.combat` 4/4 通过；NoMoreDayTests SkillContract 5 cases / 268 assertions 通过；DamagePipeline.cpp:1222/1684 中文注释 UTF-8 编码正确（此前 rg 终端显示乱码为查看端编码问题，非文件缺陷）。

### 16.7 后续收尾清单（复杂度较高，转后续批次）

| 项 | 内容 | 批次 |
|---|---|---|
| R3-3 剩余 | 反击逻辑 5 处合并为单一 helper；35.0f 基伤改读 mechanics（470.base_damage）；Melee 反击 5 道剑气实体（M3/M11 形态） | P1 |
| R3-4 | TagRegistry 收敛单源（switch 链与 kTagInfoTable 二选一，恢复 single source of truth） | P1 |
| R3-5 | OrbitingSentinelDeliverySystem 拦截死分支真删除（当前以概率 0 关停） | P2 |
| R3-6 | SkillSystem InitHooks 改读 skill_mechanics.json（451/455/432/435 数值）+ BuffEffect static const 字符串 | P1 |
| R3-7 | ReactiveWardComponent 复活（470 反击承接）或删除 | P1 |
| R3-8 | C5 剩余约 13 节点：472 静电光环、474 冰龙卷、476 曝光（数据就绪）、402/403/410/413/415/431/433/434/453/454/473/475 | P1/P2 |
| R3-12 | 455"下一次攻击"语义（一次性消费 buff）、435 crit_bonus=0.05 实现、435 可控 RNG 断言 | P2 |
| R3-13 | 452 触发的流云刺使用专精版本加成 | P2 |
| R3-14 | M7 476 锚点选择、M8 分配器不可达诊断、M9 skill_4_tree.json 双源治理 | P2 |
| R3-10 | AGENTS.md +3 行流程指令归属确认 | 待作者 |
