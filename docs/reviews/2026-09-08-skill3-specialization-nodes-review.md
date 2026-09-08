# 技能 3「灵剑决」专精树实现完全度审查报告

- **审查日期**: 2026-09-08
- **审查轮次**: 第 1 轮（准确性复核与深度交叉取证扩充）
- **审查人**: 代码审查（codebase-memory 与运行时调用链深度取证）
- **格式基准**: `docs/reviews/2026-09-07-skill1-specialization-nodes-review.md`

---

## 1. 审查目标

对照 `设计文档/职业设计草案_剑修.md` §3.3「灵剑决 (Blade Formation)」，对初审报告进行逐项准确性严格复核，并深度交叉排查技能 3（skill_id = 3）全部 28 个专精节点（300~375）在**数据、契约、烘焙、行为、ECS 实体装配、消费系统、生命周期**各层的实现完全度与隐患。
范围包括：设计文档 ↔ `skills.json` ↔ `skill_contracts_compact.json`（含内嵌契约）↔ `skill_3_tree.json` ↔ 底层代码（`SkillSpecializationBaker.cpp` / `BladeFormation.cpp` / `SummonSystem` 全族 / `SkillSystem.cpp` / `CombatSystem.cpp` / `DamagePipeline.cpp`）。

## 2. 结论

**修改**

复核确认原报告主体论断真实准确，同时通过全局交叉调用链审计，发现 **3 项新增 Blocker 级缺陷、2 项新增 High 级缺陷、1 项新增 Medium 级缺陷**，并校准了 2 处事实陈述。整改评估：

- **严重缺陷总计**: Blocker 级问题 9 项（C0~C8），High 级 6 项（H1~H6），Medium 级 9 项（M1~M9）。
- **完全度判定**: 28 个节点中，**完整正确实现 0 个（0%）**；20 个完全零实现（71%），4 个部分实现但带严重缺陷（300/301/311/370），4 个实现错误（310/330/350/351），1 个存在遗留残片但严重偏离设计（353）。
- **运行时全面瘫痪（新增 C7、C8）**: 灵剑实体被 `SummonLifecycleSystem` 在**首帧当做超时实体直接销毁**（C8）；且实体**完全未装配** `SummonAIProfile` 与 `SummonRuntimeState`，致使 `SummonAISystem` 视图永空（C7），灵剑在运行时根本不公转、不索敌、不攻击。**技能 3 在基底无天赋状态下实战功能即 100% 瘫痪**。
- **不可达支线蔓延（新增扩充 C1）**: 除 375 外，节点 311 因 300 `max_points: 3`（需 4 点）同样**永久不可达**，并连带导致其子节点 312 永久不可达，剑雨流续航支线被完全锁死。
- **数值与机制灾难**: 330 伤害双重乘算达设计 3.6 倍（9× vs 2.5×）；Transmuter 互斥失效（370+372 可双持）；351 剑影环身被替换为命中回蓝，环绕近战沦为死代码；350 被残留在 `SkillSystem.cpp` 的 320 僵尸代码屏蔽并引发热路径堆分配。
- **契约系统性错位 5 组**: Trigger(373 vs 335)、Synergy(330 vs 355)、剑意(352 vs 315)、TypeB(372 vs 374)、Transmuter([370,371] vs [370,372])。

## 3. 输入

| 源 | 文件 |
| --- | --- |
| 设计 | `设计文档/职业设计草案_剑修.md` §3.3（L227-279） |
| 节点数据 | `assets/data/skills.json`（技能 3 基底 + 28 节点） |
| 树布局 | `assets/data/skill_3_tree.json` |
| 契约 | `assets/data/skill_contracts_compact.json`（skill 3）+ `skills.json` 内嵌 `skill_contract` |
| 烘焙层 | `src/game/systems/skill/SkillSpecializationBaker.cpp:68-71, 112-133, 375-398` |
| 行为层 | `src/game/systems/skill/behaviors/BladeFormation.cpp`（全 100 行） |
| 召唤与生命周期 | `src/game/systems/skill/SummonSystem.cpp`、`SummonAISystem.cpp`、`SummonLifecycleSystem.cpp`、`SummonCombatBridge.cpp` |
| 战斗与技能管线 | `src/game/systems/skill/SkillSystem.cpp:1048-1088, 1230-1330`、`CombatSystem.cpp:630-655`、`DamagePipeline.cpp:830-845` |
| 资源与通用支持 | `SkillBehaviorBase.hpp:22-77`、`SkillDefs.hpp:950-1035`、`BladeResourceService.cpp:355-385`、`StatsSystem.cpp:460-490` |
| 测试 | `tests/unit/SkillSpecializationBakerTests.cpp:602-604`、`tests/integration/SkillSystemTests.cpp:970-985` |

## 4. 变更文件边界

`git status --short`：仅 `M settings.json`（编辑器环境生成，与技能 3 无关）。**本次审查为纯只读分析与报告扩充，零代码篡改。**

## 5. 范围对齐

| 维度 | 设计 | 实现 | 判定 |
| --- | --- | --- | --- |
| 基底形态 | Toggle 维持型：每秒耗 5 法力维持，法力不足灵剑消散 | 施放型：mana_cost 25 + CD 5s，灵剑无持续耗蓝，但由于 C8 首帧直接暴毙 | ❌ C0, C8 |
| 基底灵剑数 | 最大基础灵剑数 3 | `projectile_count = 3`（Baker:70） | ✅ |
| 实体生命周期 | 维持激活期间常驻存在 | `sm.lifetime = -1.0f`，在第 1 帧被 `SummonLifecycleSystem` 判定超时直接销毁 | ❌ C8 |
| 运行时 AI 索敌 | 环绕公转、索敌、自动向目标飞剑攻击 | 实体缺少 `SummonAIProfile` / `SummonRuntimeState`，AI 视图永空完全不运转 | ❌ C7 |
| 节点数 | 28（300~375） | 28，名称/前置/坐标齐全 | ✅ |
| 节点可达性 | 28 节点均可通过合法加点解锁 | 311、312、375 三个节点因前置 max_points 截断**永久不可达** | ❌ C1 |
| Delivery 选型 | 环绕灵剑 | `OrbitingSentinel` archetype（angular 180°/s, radius 60） | ✅ |

## 6. 质量与风险评估

- **实战全面瘫痪 (Blocker)**: C7 与 C8 导致召唤出的灵剑首帧即被销毁，且即便未销毁也因组件残缺导致 AI 永不运行。实战中玩家施放技能 3 无任何攻击收益，伴随高频死亡事件。
- **数值严重失控 (Blocker)**: C3 双重乘算使 330 巨剑单发达设计的 3.6 倍（9× vs 2.5×）；H2 的 t31 错位使每点天赋错误提供 +20 全局投射物数量。
- **多条支线锁死 (Blocker)**: C1 导致整条“无尽剑匣+灵力网络”（311+312）以及“充能引爆”（375）三节点永久不可达。
- **机制与语义颠倒 (Blocker)**: C6/H3/H4 导致火/雷转质脱节，351 环绕近战变成命中回蓝，373 抢占 335 触发点。
- **确定性与性能违规 (High)**: H5 烘焙层无序遍历 `unordered_map` 直接覆盖赋值破坏确定性；H6 在 `SkillSystem::Update` 热路径按字符串和动态 map 查找 320 遗留节点，违背 AOT 与无堆分配规范。
- **工程惯例违规 (Medium)**: 数值未接入 `skill_mechanics.json`（M4）；测试保护网为零（M5）。

## 7. 发现项

### CRITICAL

**C0 (Blocker) 基底形态偏差：Toggle 维持机制完全缺失**
设计要求灵剑决为 Toggle 型（每秒耗 5 法力维持，法力不足消散）。实现为常规施放（`skills.json`: mana_cost 25, cooldown 5.0），灵剑 `SummonComponent.lifetime = -1`，无任何每秒维持耗蓝、无法力不足回收逻辑。基底形态与设计严重脱节；更因负数生命周期被 C8 在首帧直接销毁。

**C1 (Blocker) 节点不可达链延伸：311、312、375 三节点永久不可达**
数据源多处 `max_points` 严重断档，导致整条专精支线锁死：
1. **375 灵剑充能**: `skills.json` 配置 `prerequisites = [{node_id: 374, required_points: 2}]`，但 374 灵剑蚀甲 `max_points = 1`（设计为 0/4）。前置永远无法满足 → 375 及其设计意图（双倍元素伤害+引爆异常）不可达。`skill_3_tree.json` 同源同错。
2. **311 无尽剑匣（初审遗漏，复核新增）**: `skills.json` 与 `skill_3_tree.json` 配置 `prerequisites = [{node_id: 300, required_points: 4}]`，但 300 剑池充盈的 `max_points = 3`（设计为 0/4）。投入点数永远到不了 4 → 311 永久不可达。
3. **312 灵力网络（初审遗漏，复核新增）**: 前置依赖 311（需 1 点），因 311 不可达连带被永久锁死。
整条剑雨流（311+312）与充能引爆（375）支线彻底无法体验。

**C2 (Blocker) Transmuter 互斥机制失效**
契约 `transmuter_node_ids = [370, 371]`——371 灼魂剑舞是 370 的子节点（Passive），真正的互斥对 370 地火明夷 / 372 紫电紫雷 不在同一组。后果：① 372 被标为 Passive，根本不受 Transmuter 约束，导致 370（火）+ 372（雷）可同时点亮叠加，设计明令互斥完全失效；② `skill_contracts_compact.json` 中 skill 3 的 `max_transmuters: 2`，`SkillCastConstraintService.cpp:43` 只在 `transmuterCount > contract->max_transmuters` 时拦截，即便 370 和 371 同时点亮也直接放行；③ 运行时互斥（`StatsSystem.cpp:469-479` 以契约为准）会把 371 当互斥成员处理。技能 1 C2 同款。

**C3 (Blocker) 330 巨剑降临伤害双重乘算，实际 9 倍 vs 设计 2.5 倍**
乘算链：`SkillSpecializationBaker.cpp:387` `more_damage_mult *= 3.0f`（设计为「伤害提升 150%」即 ×2.5，本身已超标 20%）→ `BladeFormation.cpp:43` `formation.damage_penalty = profile->more_damage_mult` → `BladeFormation.cpp:60/78` `damage_scale = (has_giant_sword ? 1.5f : 0.5f) * damage_penalty`。点 330 后 `damage_scale = 1.5 × 3.0 = 4.5`，相对基底灵剑 0.5 为 **9 倍**；设计为 2.5 倍，超标 3.6 倍。`SummonCombatBridge.cpp:86` `ApplyDamageScale(resolved, profile->damage_scale)` 确认该值进入实战结算。结构性根源：`SkillDefs.hpp:1017` `damage_penalty` 注释「Talent 311」但实际承载 Baker 的 more 乘区，330 的 ×3.0 混入其中（另见 M7）。在随后的 `ShadowCast` 中又额外叠加了 0.3f 乘算，造成多重混乱。

**C4 (Blocker) 契约角色系统性错位（compact 与内嵌契约同构，5 组全错）**

| 字段 | 实际 | 设计/正确值 | 后果 |
| --- | --- | --- | --- |
| `trigger_nodes` | node **373**（雷弧连锁） | **335** 巨剑裂空 | 点雷系节点获得裂空斩触发；点 335 无触发（H4） |
| `synergy_node_ids` | **330**（巨剑降临 Keystone） | **355** 剑阵共鸣 | Synergy 语义错挂 |
| `sword_intent_node_ids` | **352**（反击剑网） | **315** 御剑共振 | 剑意交互点错挂 |
| `resist_models` | **372** TypeB_Shred | **374** 灵剑蚀甲 | 降抗模型挂到 Transmuter 上 |
| `transmuter_node_ids` | **[370,371]** | **[370,372]** | 互斥失效（C2） |

内嵌契约 `skills.json.skill_contract.nodes` 同构错位：372 标 `Passive + resist TypeB_Shred`（应 Transmuter）、371 标 Transmuter（应 Passive）、330 标 Synergy（应 Keystone）、373 标 `Trigger{skill 2, eff 0.3, ICD 4.0}`（触发参数本身正确，节点错——与技能 1「114 挂 134 参数」C6 同构）。**触发参数（裂空斩 30% 效力、CD 4s、不耗蓝）全部正确，仅挂错节点。**

**C5 (Blocker) 20/28 节点零实现，实现完全度约 4%**
以下 20 个节点在 Baker 无分支、行为层无处理、无通用机制承载（字面量 + 常量双路检索 `rg "== 3xx"` 全 src 零命中）：
`302 锋灵, 303 五行归元, 312 灵力网络, 313 神速, 314 集中号令, 315 御剑共振, 331 弱点锁定, 332 致命锋芒, 333 剑压, 334 碎岩, 335 巨剑裂空, 352 反击剑网, 354 法术共鸣, 355 剑阵共鸣, 371 灼魂剑舞, 372 紫电紫雷, 373 雷弧连锁, 374 灵剑蚀甲, 375 灵剑充能`（19 个）+ `310`（仅错误 stat_modifiers 数据，见 H2）。
*注：350 存在废弃 320 僵尸代码（见 H6），353 存在残余 322 遗留代码（见 H1），但均无法按设计运行。详见 §8 矩阵。*

**C6 (Blocker) 351 剑影环身语义整体错位，环绕近战为死代码**
设计：351 = 灵剑不再飞出攻击，环绕自身持续近战伤害。实现：
- `SkillSpecializationBaker.cpp:389-390` flag4 绑定为 `mana_on_hit`（命中回蓝 2 点，`BladeFormation.cpp` DoHit）——**来路不明的错误效果顶替了节点核心语义**；
- 环绕近战字段 `SkillDefs.hpp:1029` `bool melee_orbit = false; // Talent 352 (Melee Orbit)` ——注释同时错标 352，且**全局无任何置 true 代码**；
- 近战接触系统 `SummonCombatBridge.cpp:177` `ApplyMeleeOrbitContact`（含 `melee_orbit_hit_radius=30` / `melee_orbit_base_damage=25`，SkillDefs.hpp:966-967）因 flag 永 false 成为死代码；
- 组件注释层 351/352 语义互换（`SkillDefs.hpp:1027-1029`：351→mana_on_hit，352→melee_orbit）。

**C7 (Blocker) 【新增】运行时 AI 架构断裂：`SummonAISystem` 依赖未装配组件，灵剑永久静止不攻击**
`src/game/systems/skill/SummonAISystem.cpp:16-17` 的查询视图要求实体同时持有 6 个组件：
```cpp
auto view = registry.view<SpiritSwordTag, SummonComponent, SummonAIProfile,
                          SummonRuntimeState, SpiritSwordAI, Position>();
```
然而，`BladeFormation.cpp:73-80` 在实例化灵剑实体时，**仅装配了** `LocalLevelTag`、`SpiritSwordTag`、`Position`、`Velocity`、`SummonComponent`、`SpiritSwordAI`、`SummonCombatProfile`。
`SummonAIProfile` 在全局无任何一处装配（全仓仅定义于 SkillDefs.hpp 与 SummonAISystem 读取）；`SummonRuntimeState` 亦从未挂载到灵剑。
后果：**`SummonAISystem::Update` 的 query 视图恒为空！** 灵剑无法进入 AI 循环，不会索敌、不会移动公转、更无法调用 `CastSpiritSwordShadow` 进行攻击。基底技能在实战中功能完全冻结。

**C8 (Blocker) 【新增】实体生命周期缺陷：`sm.lifetime = -1.0f` 在首帧被判定为超时直接销毁**
`BladeFormation.cpp:76` 企图通过 `sm.lifetime = sm.max_lifetime = -1.0f` 实现永久存在。
但在生命周期调度器 `src/game/systems/skill/SummonLifecycleSystem.cpp:16-24` 中：
```cpp
summon.lifetime -= dt;
if (summon.lifetime <= 0.0f || !registry.valid(summon.owner)) {
  CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateMinionDeath(summon.owner, entity));
  toDestroy.push_back(entity);
  continue;
}
```
系统并未将负数识别为永久存在，首帧更新即判定 `-1.0f - dt <= 0.0f` 成立，立即派发 `CreateMinionDeath` 并将灵剑实体彻底 destroy 销毁！
后果：灵剑在生成后的第 1 帧（约 16ms）即集体暴毙，玩家施法后看到实体瞬间闪现后消失，伴随大量死亡事件与音效轰炸。

### HIGH

**H1 353 不灭剑魂存在脱离设计的遗留消费残片（且无冷却防线）**
初审判定为全局无消费，经深度代码排查修正如下：
`src/game/systems/combat/CombatSystem.cpp:631-641` 实际上存在消费代码：
`if (auto *formation = registry.try_get<BladeFormationComponent>(target)) { if (formation->immortality_ready) { formation->immortality_ready = false; hp.current = hp.max * 0.3f; ... } }`
但该消费逻辑存在严重缺陷：
1. 标注为 `(Node 322)` 遗留残存逻辑；
2. 未校验灵剑数量门槛（设计要求：拥有至少 3 柄灵剑）；
3. 未消耗/销毁任何灵剑（设计要求：消耗所有灵剑抵消伤害）；
4. 回血数值固定为 30%（设计要求：4% 最大生命/柄）；
5. 缺失 90s 内置冷却（由于 `BladeFormation::DoCast:42` 每次重新施法均直接由 flag 恢复 `immortality_ready = true`，玩家只要再次施放即可瞬间规避死亡惩罚，形成事实上的无敌漏洞）。

**H2 310 stat_modifiers 危险错位（技能 1 C3 同款）**
`skills.json` 310: `{type: 31, mode: 1, value: 20.0}`。StatType 枚举（`Stats.hpp:224-269`）t31 = **ProjectileCount**，即每点 +20 全局投射物；desc 为「追踪半径 +20%...60%」，应为 **t32 AreaScale**。消费链存在（`StatsSystem.cpp:484`、UI 预览 `UISkillTalentTree.cpp:421` / `SkillDisplayPreviewService.cpp:76`）。

**H3 灵剑元素来源错误，370/372 转质与实际攻击脱钩**
`BladeFormation.cpp:64/79`：灵剑 `SkillModifierComponent` 的 Convert 元素来自 `BladeResourceService::GetHeavenlyAttunementElementTag`（`BladeResourceService.cpp:359-372`，读取**天剑降临**的 BladeAttunement 状态），而非 370/372 专精节点。后果：① 370 的 Baker 层 `effective_tags` 转换（Baker:393-397）对灵剑实体攻击无效（灵剑走 ShadowCast 伤害链，不读该 profile tag）；② 372 紫电紫雷零实现且 Baker case 3 **无 372 分支**（`ResolveElementalConversion` 函数层已就绪 case 372，但无调用点）；③ 371 点燃 DoT 无从谈起（无转火就无点燃源）。常量 `BladeFormationNodes::ElementEnchant = 370`（BladeFormation.cpp:24）定义后**从未使用**。

**H4 335/373 Trigger 错挂的运行时后果**
Baker:123-133 按契约 `role == Trigger` 通用写入 `TriggerRuleComponent`。当前契约 Trigger 挂 373 → 玩家点 373（雷弧连锁，需 372 前置）将意外获得「暴击触发裂空斩」；点 335（设计触发点）无任何触发。触发行为与设计**完全颠倒**，且随 C2 修复前 370+372 双持会进一步放大混乱。

**H5 【新增】烘焙层迭代 `unordered_map` 进行硬赋值覆盖，引入非确定性风险**
`SkillSpecializationBaker.cpp:112` 遍历 `spec->allocated_points`（类型为 `std::unordered_map<uint32_t, int>`），其迭代顺序是不确定的。
在 case 3 分支内：
- 节点 300 执行累加：`out_profile.projectile_count += points;`
- 节点 311 执行硬覆盖：`out_profile.projectile_count = 8;`
- 节点 330 执行硬覆盖：`out_profile.projectile_count = 1;`
若 300 先遍历、311/330 后遍历，则 300 点数被强行重置为 8 或 1；若 311/330 先遍历、300 后遍历，则最终值为 8 + 3 = 11 或 1 + 3 = 4。相同的天赋加点在不同运行期产生不同的烘焙结果，直接违反 `conductor/code_standard.md` 确定性规范。

**H6 【新增】`SkillSystem.cpp:1063-1087` 遗留废弃 320 逻辑，热路径逐帧查询 map 与分配字符串**
`SkillSystem::Update` 中遗留了对技能 3 的硬编码分支：
```cpp
if (spec.skill_id == 3 && spec.allocated_points.contains(320) && spec.allocated_points.at(320) > 0) { ... }
```
1. 使用废弃旧节点 ID 320（实际为 350 灵剑护体），导致实际投入 350 点数的玩家无法触发；
2. 在每帧调用的系统 Update 中遍历技能槽并执行 `unordered_map::contains/at` 查找，违背现代化重构“热路径零查表、AOT 预烘焙”的核心设计原则；
3. 每 0.2 秒调用 `std::string(BuffIdToString(BuffId::LingJianHuTi))` 产生临时堆分配，违反 `code_standard.md` §2.1 热路径严禁临时堆分配的硬规则。

### MEDIUM

**M1 已实现节点的数值偏差**
- 301 疾风意：`sub_interval = 0.1f * points`（Baker:379）→ 点满 +50% 频率，desc 为 8%...40%（超标 25%）；
- 300 剑池充盈：`max_points = 3`，desc/设计为 0/4（+1/2/3/4）→ 上限灵剑 6 vs 设计 7（且诱发 C1 不可达）；
- 311 无尽剑匣：Baker:381 `projectile_count = 8` 写死（应基于当前值翻倍；若 300 修复为 +4 则应为 14）；且 profile 路径（写死 8）与 fallback 路径（`BladeFormation.cpp:55-57` 当前值 ×2）**行为不一致**。

**M2 max_points 批量偏差（13 处）**
`300(3vs4), 312(5vs3), 315(5vs3), 332(5vs4), 335(5vs1), 352(1vs3), 354(5vs1), 355(5vs1), 370(5vs1), 371(1vs3), 372(5vs1), 373(1vs3), 374(1vs4)`。其中 300 与 374 的偏差直接诱发 C1 致命不可达；335/354/355/370/372 应为 1 点 Keystone/Trigger/Synergy/Transmuter（当前 5 点会稀释关键抉择感，污染契约生成器）。

**M3 内嵌契约仅 12/28 节点有条目**
300-303/310/312/315/331-335/350/354/355/375 共 16 节点无 `skill_contract.nodes` 条目 → `getModifier`（`SkillBehaviorBase.hpp:112-127`）、触发写入、互斥运行时均拿不到这些节点的契约。契约完整性与技能 1 第 6 轮后的状态不对齐。

**M4 机制数据外置缺失**
`skill_mechanics.json` 仅有技能 1、2 条目，技能 3 零记录。技能 1 第 6 轮确立的 `SkillMechanicsRegistry::GetFloat(skill, node, key, default)` 数据载体模式未覆盖技能 3；现有数值全部硬编码（0.1f / 3.0f / 8 / 80.0f / 1.5f / 0.5f / 180.0f / 60.0f）。

**M5 测试覆盖为零**
`tests/unit/SkillSpecializationBakerTests.cpp`（1140 行）中技能 3 仅有 :602-604 附带断言（`ResolveElementalConversion(370,1)→Fire`，属技能 1 测试组）。`SkillSystemTests.cpp:970-985` 仅检查了 `has_giant_sword` 布尔标志，无 Baker 烘焙矩阵、灵剑攻击与伤害、生命周期、AI 索敌、触发契约测试。回归保护网为零。

**M6 轨道半径双源不一致**
`BladeFormation.cpp:53` `sentinel.orbit_radius = 60.0f`，而 `SummonAISystem.cpp:122` 非近战分支使用常量 `orbitRadius = isGiant ? 55.0f : 35.0f`——组件字段被无视，视觉与逻辑半径脱节。

**M7 damage_penalty 字段语义混杂**
`SkillDefs.hpp:1017` `damage_penalty` 注释「Talent 311: 无尽剑匣」，实际承载 Baker `more_damage_mult`（含 330 的 ×3.0）。C3 双乘的结构性根源，整改时应拆分为独立乘区。

**M8 灵剑攻击跨技能模板耦合**
`SummonCombatBridge.cpp:145` `SkillSystem::ShadowCast(registry, summon, 2, ...)`——灵剑攻击投射物复用**技能 2** 模板，受技能 2 标签、伤害及 Shadow 0.3f 乘区交叉污染。需显式文档化或为灵剑建立独立模板。

**M9 【新增】335 触发事件类型硬编码为 OnSkillHit，与设计要求 OnCrit 错配**
设计文档明确要求 335 巨剑裂空为“巨剑**暴击时**触发一道裂空斩”。但 `SkillSpecializationBaker.cpp:127` 对所有契约 Trigger 硬编码 `rule.listen_event = CombatEventType::OnSkillHit;`，未根据节点特性派发为 `CombatEventType::OnCrit`。


## 8. 逐节点核对矩阵

判定：✅ 完全正确 ｜ ⚠️ 部分/数值偏差 ｜ ❌ 实现错误/不可达 ｜ ✖️ 零实现

| 节点 | 设计（§3.3） | skills.json | 契约 | 代码 | 判定 |
| --- | --- | --- | --- | --- | --- |
| 300 剑池充盈 | 0/4，灵剑 +1/2/3/4 | max **3** | 无条目 | Baker:376 `+= points` | ⚠️ 语义✓ 上限错（**诱发 311/312 永久不可达**） |
| 301 疾风意 | 0/5，频率 +8...40% | max 5 | 无条目 | Baker:379 `0.1f*p` | ⚠️ 点满 50%（超标 25%） |
| 302 锋灵 | 0/5，灵剑物伤 +10...50% | max 5 | 无条目 | 无 | ✖️ |
| 303 五行归元 | 0/4，转换效率 +10...40% | max 4 | 无条目 | 无 | ✖️ |
| 310 索敌范围 | 0/3，追踪半径 +20...60% | max 3，**t31/v20** | 无条目 | 无 | ❌ H2（t31 错配为投射物数） |
| 311 无尽剑匣 | 0/1，上限翻倍，单发 −40% | max 1，prereq 300×**4** | Keystone（应 Passive） | Baker:380-383 写死 8 | ❌ **永久不可达 (C1)** + ⚠️ M1, H5 |
| 312 灵力网络 | 0/3，回蓝 3...9/s + 耗蓝 −5...15% | max **5**，prereq 311×1 | 无条目 | 无 | ✖️ **连带不可达 (C1)** |
| 313 神速 | 0/1 Keystone，攻速 75% | max 1 | Keystone ✓ | 无 | ✖️ |
| 314 集中号令 | 0/1，优先最近技能命中目标 | max 1 | Keystone（应 Passive） | 无 | ✖️ |
| 315 御剑共振 | 0/3，频率 +20...60% + 回剑意 10...30% | max **5** | 无条目（剑意点错挂 352） | 无 | ✖️ |
| 330 巨剑降临 | 0/1 Keystone，仅 1 柄、体积/伤害/范围 +150%、频率 −50% | max 1 | **Synergy**（应 Keystone） | Baker:384-388 ×3.0 + flag2 | ❌ C3（双乘 9 倍）+ H5 |
| 331 弱点锁定 | 0/5，暴击率 +5...25% | max 5 | 无条目 | 无 | ✖️ |
| 332 致命锋芒 | 0/4，暴伤 +25...100% | max **5** | 无条目 | 无 | ✖️ |
| 333 剑压 | 0/3，物理 taken +5...15% | max 3 | 无条目 | 无 | ✖️ |
| 334 碎岩 | 0/3，33...100% 击晕 0.5s + 1 层护甲击碎 | max 3 | 无条目 | 无 | ✖️ |
| 335 巨剑裂空 | 0/1 **Trigger**，暴击触发裂空斩 30% CD4s | max **5** | 无条目（trigger 错挂 373） | 无 | ✖️ + H4, M9 |
| 350 灵剑护体 | 0/5，每柄全局减伤 1...5% | max 5 | 无条目 | SkillSystem:1063 废弃 320 查表 | ❌ 错挂 320 屏蔽 + 违背 AOT (H6) |
| 351 剑影环身 | 0/1 **Keystone**，环绕持续近战 | max 1 | Keystone ✓ | Baker:389 flag4→**mana_on_hit** | ❌ C6（语义篡改，环绕近战死代码） |
| 352 反击剑网 | 0/3，每柄环绕灵剑格挡 +2...6% | max **1** | **Keystone+sw_intent**（应 Passive） | 无 | ✖️ |
| 353 不灭剑魂 | 0/1，致命伤抵消 + 回血 CD90s | max 1 | Keystone（应 Passive） | CombatSystem:631 遗留 322 残片 | ❌ H1（残片脱离设计，无 CD 门槛） |
| 354 法术共鸣 | 0/1，施法时灵剑齐射 20% 效力 | max **5** | 无条目 | 无 | ✖️ |
| 355 剑阵共鸣 | 0/1 **Synergy**，范围内 +50% 攻速 + 附加元素 | max **5** | 无条目（synergy 错挂 330） | 无 | ✖️ |
| 370 地火明夷 | 0/1 **Transmuter**，转火必点燃 | max **5** | Transmuter ✓ | Baker:393 tag✓ 灵剑脱钩 | ⚠️ H3（灵剑元素脱钩） |
| 371 灼魂剑舞 | 0/3，点燃 DoT + 持续 +20...60% | max **1** | **Transmuter**（应 Passive） | 无 | ✖️ + C2（误标 Transmuter） |
| 372 紫电紫雷 | 0/1 **Transmuter**，转雷 + 瞬移 + 静电场 | max **5** | **Passive+TypeB**（应 Transmuter） | 无 | ✖️ + H3, C2 |
| 373 雷弧连锁 | 0/3，闪电弧 1...3 连锁 | max **1** | **Trigger**（应 Passive） | 无 | ✖️ + H4（误标 Trigger） |
| 374 灵剑蚀甲 | 0/4，TypeB 降抗 2...8/次 ≤8 层 | max **1** | Keystone（应 Passive+TypeB） | 无 | ✖️（**诱发 375 永久不可达**） |
| 375 灵剑充能 | 0/3，双倍元素伤害 + 引爆异常 | max 3，prereq 374×**2** | 无条目 | 无 | ✖️ **永久不可达 (C1)** |

统计：✅ 0 ｜ ⚠️ 2（301/370）｜ ❌ 6（310/311/330/350/351/353）｜ ✖️ 20 ｜ 不可达 3（311/312/375）

## 9. JSON 数据矛盾清单

| # | 矛盾 | 位置 | 严重度 |
| --- | --- | --- | --- |
| 1 | `transmuter_node_ids: [370,371]` vs 设计 `[370,372]` | compact + 内嵌 | Blocker (C2) |
| 2 | `synergy_node_ids: [330]` vs 355 | compact + 内嵌(330 role) | Blocker (C4) |
| 3 | `sword_intent_node_ids: [352]` vs 315 | compact + 内嵌(352 affects_sword_intent) | Blocker (C4) |
| 4 | `resist_models: {372: TypeB_Shred}` vs `{374}` | compact + 内嵌(372 resist) | Blocker (C4) |
| 5 | `trigger_nodes: [{373, skill2, 0.3, ICD4}]` vs `[{335, ...}]`（参数正确节点错） | compact + 内嵌(373 role Trigger) | Blocker (C4/H4) |
| 6 | 374 `max_points: 1` vs 设计 0/4，致 375 prereq 374×2 不可达 | skills.json + skill_3_tree.json | Blocker (C1) |
| 7 | 300 `max_points: 3` vs 设计 0/4，致 311 prereq 300×4 及 312 连带不可达 | skills.json + skill_3_tree.json | Blocker (C1) |
| 8 | 310 `stat_modifiers t31/v20`（投射物+20）vs 应 t32（范围） | skills.json | High (H2) |
| 9 | 13 处 max_points 批量偏差（含 300 与 374 两大致命阻断点） | skills.json | Medium (M2) |
| 10 | 16 节点契约条目缺失 | skills.json 内嵌 + compact key_nodes | Medium (M3) |
| 11 | 基底 mana_cost 25 / CD 5s vs 设计 Toggle 每秒 5 法力 | skills.json | Blocker (C0) |

## 10. 架构与数据驱动偏差

对照 `conductor/code_standard.md` 与现代化重构架构规范：

1. **实体生命周期与 ECS 系统契约脱节 (C8)**: `BladeFormation.cpp` 设置 `sm.lifetime = -1.0f` 意图代表常驻，但 `SummonLifecycleSystem.cpp:16-24` 直接对负数判定超时并立即销毁，灵剑生成即暴毙。
2. **实体装配与 AI 视图脱节 (C7)**: `SummonAISystem::Update` 要求持有 `SummonAIProfile` 与 `SummonRuntimeState`，而 `BladeFormation.cpp` 未装配上述组件，导致系统 query 视图恒为空，灵剑实战彻底冻结。
3. **烘焙无序遍历破坏确定性 (H5)**: `SkillSpecializationBaker.cpp` 遍历 `std::unordered_map` 对 `projectile_count` 采用硬覆盖赋值（8、1），使得烘焙结果依赖底层哈希桶分布，违背 ECS/DOD 确定性规范。
4. **热路径残留废弃逻辑与堆分配 (H6)**: `SkillSystem.cpp:1063-1087` 遗留 320 旧逻辑，逐帧在 Update 中查询 map 并动态构造 `std::string`，违反 §2.1 热路径严禁临时堆分配与 AOT 预烘焙规范。
5. **数值外置未达成 (M4)**: 技能 3 全部数值硬编码于 `SkillSpecializationBaker.cpp` / `BladeFormation.cpp`，`skill_mechanics.json` 零条目。
6. **契约生成器残留缺陷**: `gen_skill_contracts.py:418-427` 对 `max_points==1` 默认推导 Keystone 的规则再次污染数据（311/314/353/374 被误标），且 `max_transmuters: 2` 放任了转质双持。
7. **跨技能模板与影分身衰减耦合 (M8)**: 灵剑伤害借道技能 2，且叠加了 `ShadowCast` 的 0.3f 乘区。
8. **组件注释层节点语义错位**: `SkillDefs.hpp:1027-1029` 351/352 注释互换，与 `BladeFormation.cpp` 错挂 mana_on_hit 互为表里。

## 11. 符合项

1. 基底灵剑数 3（Baker:70）与设计一致。
2. `OrbitingSentinel` archetype 选型合理（环绕 + 攻击间隔 + 拦截骰，`DeliveryArchetypes.hpp:187-189`）。
3. 300 灵剑数量累加语义正确（`+= points`，但需配合确定性烘焙改造）。
4. 330 的「仅 1 柄」（Baker:385）与「攻击频率 −50%」（`BladeFormation.cpp` attack_interval ×2）形态正确。
5. 311 的「单发伤害 −40%」（`more ×0.6`）减伤语义符合设计。
6. 28 节点树拓扑结构整体完整（坐标分布与前置拓扑链条基本成型）；前置判定 OR 语义（`SkillSystem.cpp:1890-1908`）正确落地了 374「任意 Transmuter」的需求。
7. `skill_3_tree.json` 28 节点名称/坐标完整。
8. `ResolveElementalConversion`（`SkillBehaviorBase.hpp:29-54`）函数层已就绪 370→Fire、372→Lightning 显式 case。
9. 契约触发参数本体正确（trigger_skill_id=2 裂空斩、effectiveness 0.3、ICD 4.0、不耗蓝）。

## 12. 整改路线

- **阶段 1（数据修复先行，解除不可达与契约错位）**
  1. `skills.json` & `skill_3_tree.json`：300 max_points 3→4（解 311/312 不可达）；374 max_points 1→4（解 375 不可达）；13 处 max_points 全量校正（M2）；310 t31→t32（H2）。
  2. 校正契约：修正五组错位（C4/C2），将 `max_transmuters` 校正为 1，补齐 16 个缺失节点的契约条目（M3）。
  3. 基底形态对齐设计意图（Toggle 每秒持续耗蓝 vs 施放型，需确定简化口径）。
- **阶段 2（ECS 实体装配与生命周期闭环，修复实战瘫痪）**
  1. `SummonLifecycleSystem.cpp:16-24`：引入对永久召唤物（`lifetime < 0.0f`）的豁免逻辑，彻底消除首帧秒杀（C8）。
  2. `BladeFormation.cpp:73-80`：在实体生成时完整装配 `SummonAIProfile` 与 `SummonRuntimeState`，打通 `SummonAISystem` 视图，激活灵剑索敌、公转与攻击（C7）。
  3. `OrbitingSentinelDeliverySystem` 与 `SummonAISystem` 统一轨道半径消费（M6）。
- **阶段 3（烘焙层与架构合规，消除非确定性与热路径违规）**
  1. `SkillSpecializationBaker.cpp`：消除在无序遍历中对 `projectile_count` 的硬赋值，改用基础值累加与倍率独立乘算，确保任何遍历顺序下的确定性结果（H5）；补齐 372 分派与 21 个节点烘焙分支。
  2. `SkillSystem.cpp:1063-1087`：彻底拆除 320 遗留热路径查表与字符串构造代码，将 350 灵剑护体统一接入 AOT 预烘焙减伤管线（H6）。
- **阶段 4（机制数据外置与行为层重构）**
  1. 在 `skill_mechanics.json` 中建立技能 3 全量机制参数（M4）。
  2. 351 `melee_orbit` 重新置位并接入 `ApplyMeleeOrbitContact`（C6）。
  3. 353 免死重构：在 `CombatSystem` 中建立严密防线（检查 >=3 柄灵剑、销毁灵剑、按每柄 4% 回血、进入 90s CD，杜绝 DoCast 重置漏洞）（H1）。
  4. 335 触发机制：接线 OnCrit 暴击事件，以灵剑位置触发（H4, M9）。
  5. 370/372 灵剑攻击元素绑定契约，解除天剑降临的错误耦合（H3）；拆除 330 的 9 倍双乘与 Shadow 0.3f 杂质污染（C3, M8）。
- **阶段 5（测试套件与回归验证）**
  1. 新建 `tests/functional/BladeFormationNodes.cpp` 专项测试套件（对标 FlowingThrustNodes 与 RendingWaveNodes）。
  2. 覆盖 Baker 确定性烘焙矩阵、灵剑生成/常驻生命周期、AI 索敌攻击、Transmuter 互斥、330 数值 2.5× 基准、353 免死消耗与 90s CD。

## 13. 剩余风险与后续

- C0 的 Toggle 简化口径需设计确认（维持每秒耗蓝 vs 保留施放型），整改前应先对齐设计意图。
- 314「优先攻击最近技能命中目标」、315「御剑步联动回剑意」依赖跨系统事件（SkillHit / 御剑步状态），零实现前提下无法评估接线成本——阶段 4 前建议补充技术设计。
- M8 灵剑攻击投射物模板需独立化，避免与技能 2 相互污染。

## 14. 证据命令摘要

```
python dump: assets/data/skills.json (skill 3) / skill_contracts_compact.json (skill 3) / skill_3_tree.json
Read: SkillSpecializationBaker.cpp:68-71,112-133,375-398 / behaviors/BladeFormation.cpp:1-100
      SkillBehaviorBase.hpp:22-77 / SkillDefs.hpp:950-1035 / Stats.hpp:224-269 / TagRegistry.hpp:12-55
      SummonAISystem.cpp:16-24,40-62,120-141 / SummonLifecycleSystem.cpp:16-24 / SummonSystem.cpp:9-20
      SummonCombatBridge.cpp:53-90,125-175,180-241 / BladeResourceService.cpp:355-385
      SkillSystem.cpp:1048-1088 (320 遗留热路径), 1230-1330 (ShadowCast 0.3f 衰减), 1695-1715, 1805-1815
      CombatSystem.cpp:630-655 (322 遗留免死逻辑) / DamagePipeline.cpp:830-845
rg "== 3(0[123]|1[02345]|3[12345]|5[0245]|7[1-5])" src/   → 20 节点完全零命中
tests: SkillSpecializationBakerTests.cpp:602-604 / SkillSystemTests.cpp:970-985
git status --short → 仅 settings.json
```

## 15. 整改复审（2026-09-08 第二轮）

**复审范围**: 工作区未提交变更（19 文件修改 + 新增 `tests/functional/BladeFormationNodes.cpp`，约 +1003/-200 行），对照本报告第 9-12 节整改路线逐项复核。

### 15.1 复审结论

**修改**。原 23 项发现中 15 项完整修复、5 项部分修复、1 项未修（M9）、1 项按路线声明遗留（C0）；新发现 1 项 Critical、4 项 High、6 项 Medium。构建与测试全绿，但 Critical 数值缺陷与热路径堆分配违反硬否决条款，不满足提交标准。

### 15.2 修复确认

| 原项 | 状态 | 证据 |
|---|---|---|
| C1 不可达 | ✅ 修复 | skills.json 300 max4 / 374 max4；`skill_3_tree.json` 无 max_points 同源字段，prereq required_points（311←300×4、375←374×2）与修复后一致，无需改动；374 前置 OR 语义符合"任意 Transmuter" |
| C2 契约错位 | ⚠️ 部分 | compact/内嵌 5 组错位（trigger 335、synergy 355、sword_intent 315、resist_models 374、transmuter [370,372]）全部修复；**max_transmuters=2 未改（见 HI-2）** |
| C3 双乘 9× | ✅ 修复 | Baker 330 分支删除 `more_damage_mult*=3.0f`；`BladeFormation.cpp` 统一 `baseDamageScale=1.25f/0.5f × damage_penalty × moreMult`；测试断言 1.25/1.875/0.3 验证 |
| C4 契约 5 组 | ✅ 修复 | 内嵌契约 330 Keystone、315 Passive+sword_intent、335 Trigger、351/354 Keystone、355 Synergy、370/372 Transmuter、374 Passive+TypeB_Shred |
| C5 零实现 | ⚠️ 大部分 | Baker 24 节点分支 + DoHit 行为落地；仍有 4 处行为子项缺口（MI-2/3/4/5） |
| C6 351 错绑 | ✅ 修复 | flags|=4 恢复 melee_orbit 语义；AI MeleeOrbit 分支活跃；`ApplyMeleeOrbitContact` 死代码复活（SummonAISystem.cpp:61）；SkillDefs.hpp 注释修正；mana_on_hit 标 Legacy |
| C7 AI 装配缺失 | ✅ 修复 | BladeFormation 双路径（existing 更新 + 新建）均装配 `SummonAIProfile`+`SummonRuntimeState`；测试断言组件齐全 |
| C8 首帧秒杀 | ✅ 修复 | SummonLifecycleSystem 仅 `max_lifetime>0` 倒计时；全局回归排查：SummonComponent 赋值仅 SkillSystem.cpp:1401-1402（同时设置）与灵剑（-1/-1）；MobilityDelivery 的 clone 为 `PhantasmCloneComponent` 不经此系统；UI 侧已有 `max_lifetime>0` 防御 |
| H1 免死残片 | ✅ 修复 | CombatSystem.cpp: ≥3 柄门槛、销毁全部灵剑（收集后 destroy，EnTT 安全）、4% max HP/柄、90s CD、DoCast 重置漏洞封堵；测试验证 120HP/CD/销毁/<3 不触发 |
| H2 t31→t32 | ⚠️ 部分 | t32 已改；但新增 Baker `del.range=500×(1+0.2p)` 与 t32 形成双重供给（见 HI-3） |
| H3 元素错误耦合 | ✅ 修复 | 灵剑元素来自 370/372 → `effective_tags`（has_fire/has_lightning），fallback 天剑降临；测试断言 Convert Fire/Lightning 1.0 |
| H4 trigger 373 | ✅ 修复 | 契约 trigger 335；SkillKeyNodeMatrixTestHelpers/fixture/SkillBehaviorGuardTests 同步 |
| H5 烘焙不确定性 | ✅ 修复 | 循环后统一 post-process（`flags&2→1`，`flags&1→×2`）；测试断言 300+311+330 任意顺序 projectile_count=1 |
| H6 320 热路径 | ⚠️ 部分 | 旧 320 逻辑彻底拆除，350 改走 DamageMitigationService（clamp 0.75）、353 CD 每帧递减、312 回蓝；**但新代码引入同型堆分配（见 HI-1）** |
| M1 数值偏差 | ✅ 修复 | 301 mech 8%/点=点满 40%；311 翻倍 profile/fallback 一致；300 max4 |
| M2 max_points 13 处 | ✅ 修复 | 13 处全部按第 9 节表校正 |
| M3 契约条目缺失 | ⚠️ 部分 | 新增 315/335/354/355；但原 352 条目在重排中丢失，现 13/28 条目（见 MI-1） |
| M4 数值外置 | ⚠️ 部分 | skill_mechanics.json 技能 3 24 节点条目 + GetFloat 接入；351/333/334 等处仍硬编码且部分数据无消费（MI-6、HI-4） |
| M5 测试零覆盖 | ✅ 修复 | 新建 tests/functional/BladeFormationNodes.cpp：24 用例 209 断言，覆盖 C8/C6/C7/C3/H5/C2/H3/H1 及 300-375 各节点 |
| M6 轨道半径双源 | ✅ 修复 | SummonAISystem.cpp:122 改读 `sentinel->orbit_radius`；melee 分支仍硬编码（MI-6） |
| M7 damage_penalty 语义 | ⚠️ 部分 | 计算已纯化（仅承载 311 的 0.6），字段名/注释未更新 |
| M8 模板耦合 | ⚠️ 部分 | `ShadowCast` 新增 `override_damage_scale`，灵剑 owner 有 SpiritSwordTag → 1.0f，0.3f 杂质消除；技能 2 模板耦合仍在（每次灵剑攻击仍触发技能 2 的 hit handler） |
| M9 OnSkillHit | ❌ 未修 | SkillSpecializationBaker.cpp:127 仍硬编码 `OnSkillHit`；335 设计为"巨剑暴击时"，且通用触发从 caster 而非巨剑位置 |
| C0 Toggle 口径 | ➖ 遗留 | skills.json 基底 mana25/CD5s 未动；符合整改路线"需设计确认"声明，待设计裁决 |

### 15.3 新发现问题

- **Critical: 331 弱点锁定暴击率单位错误，1 点即恒定暴击**。`SkillSpecializationBaker.cpp`（case 331）`del.bonus_crit += crit_pct * points`（5.0×p，未除 100），fallback 路径 `BladeFormation.cpp` `bonusCrit = 5.0f * pts331` 同错；而 `CombatStats::crit_chance` 为归一化 0..1（`Stats.hpp:115` 默认 0.05f、`CombatConstants.hpp:173` `Cap::CRIT_CHANCE=1.00`、`DamagePipeline.cpp:1091` 注释明文"归一化小数 [0.0,1.0]"）。`SummonCombatBridge.cpp:1074` `resolved.crit_chance += profile->bonus_crit` 后，1 点 331 → crit_chance=5.05 → 两条暴击判定路径（CombatSystem.cpp:301 min(·,1.0) / DamagePipeline.cpp:1103 clamp/100）均为 100% 暴击。对照 332（÷100，0.25×p）实现正确。**测试 `BladeFormationNodes.cpp:497-501` 断言 25.0 属于固化错误行为，期望值应为 0.25**。
- **High: GetFloat 热路径字符串堆分配（H6 同型复发）**。`SkillMechanicsRegistry.hpp:25` 签名 `const std::string &key`、`.cpp:87` unordered_map 按 string 查找；`BladeFormation.cpp` DoHit 每次灵剑命中调用 6-10 次（333/334/370/371/373/374/375 分支），键均为 >15 字符字面量（超出 SSO，每次命中多次堆分配 + 哈希）；`SkillSystem.cpp:1096` 每帧 fallback 亦同。违反本报告 §2.1/§7.2 热路径硬否决。应在 DoCast 一次性读入 `BladeFormationComponent` 缓存字段（部分节点已如此做，模式不一致）。
- **High: 370/372 互斥失效（C2 半项）**。`skills.json:1808` 技能 3 `max_transmuters` 仍为 2，`SkillCastConstraintService.cpp:43` 仅拦 >max → 可同时点 370+372，违反两者 desc 明文互斥。后果：`BladeFormation.cpp` has_fire && has_lightning 同时为真 → DoHit 同时点燃+感电、元素 Convert 修饰同时存在（元素判定 if/else 静默优先 Fire）。
- **High: 310 双重供给 + 基准跳变**。`skills.json:1436` t32（全局 AreaScale +20..60%）与 `SkillSpecializationBaker.cpp` 310 分支 `del.range = 500×(1+0.2p)` 同时生效；`BladeFormation.cpp` `search_radius = profile->delivery.range>0 ? range : 200.0f` → 不点 310 时 leash=200，点 1 点 = 600（500×1.2），三倍跳变且设计仅描述"追踪半径 +20%...60%"。测试 `BladeFormationNodes.cpp:566` 断言 800（500×1.6）固化了 500 基准。
- **High: 373/375 伤害基数无出处 + 373 全表遍历**。设计"每次跳跃造成基础伤害 15%...45%"/"额外造成双倍元素伤害"，实现为 `BladeFormation.cpp` 闪电弧 `pool.Add(Lightning, 15.0f×0.15×p)`（=2.25p 固定值）、375 爆发 `pool.Add(elem, 30.0f×2)`（=60 固定值），基数 15/30 无设计依据且偏离"按灵剑攻击伤害缩放"；373 命中后遍历 `reg.view<EnemyTag, Position>()` O(N) 无空间网格（同管线其余索敌均走 SpatialHashGrid），200px 范围硬编码。
- **Medium: 内嵌契约 15/28 节点仍无条目，352 条目回归性丢失（M3 半项）**。现 13 条（311,313,314,315,330,335,351,353,354,355,370,372,374）；重排中原 352 条目被 335+351 替换后未补回。key-node fixture 新值 [315,335,355,370,372] 恰好全有条目，矩阵测试无法暴露缺口。
- **Medium: 372 紫电紫雷"瞬移攻击 + 静电场"零实现**。`skill_mechanics.json` static_field_radius/duration 无任何代码消费。
- **Medium: 375"引爆范围节点内的该元素异常"未实现**。detonate_radius 80 无消费，爆发仅单体固定伤害。
- **Medium: 331"暴击对周围造成范围伤害"未实现**。splash_radius 60 无消费。
- **Medium: 355"命中时附加剑阵当前元素属性"未实现**。仅落地 50% 攻速。
- **Medium: 351 数值三处硬编码与 mechanics 数据重复**。`SummonAISystem.cpp:56-60`（radius 50/speed 6.0/tick 0.2）与 `skill_mechanics.json` 351 `melee_orbit_*` 全部重复且数据无消费；`BladeFormation.cpp` sentinel `melee_orbit?50:60` 与 AI 侧硬编码 50 构成双源。
- **Medium: ArmorShred buff id 跨技能重名**。`BladeFormation.cpp:362` 与 `RendingWave.cpp:537`、`FlowingThrust.cpp:412` 同 id，同一敌人被多技能命中时 AddOrRefresh 互相覆盖数值/持续时间。
- **Low: DamageMitigationService.cpp 未直接 include SkillDefs.hpp**（依赖传递包含）；`skill_mechanics.json` 文件尾无换行符。

### 15.4 正面确认（本轮验证）

1. `DamagePipeline.cpp:229-231` `ResolveEventAttackerContext` 在 summon attribution 有效时将 evt.source 归一为 summon_owner → `BladeFormation::DoHit(attacker=玩家)` 全链成立，315/333/334/370-375 行为层可正确触发。
2. `SkillSystem::GetBakedSkillProfile`（SkillSystem.cpp:2406-2419）为定长数组线性扫描，每帧调用无堆分配。
3. `SummonRuntimeState::current_target` 默认 `entt::null`，314 集中号令目标校验（valid+EnemyTag+非 Killed+leash 内）闭环完整。
4. 353 免死在灵剑收集后统一 destroy，无遍历中失效风险；`hp.current = min(max, totalHeal)` 防溢出。
5. C8 修改对既有召唤物无回归（见 15.2 表内排查记录）。

### 15.5 复审验证证据

```
build.bat (RelWithDebInfo) → EXIT=0 全部步骤成功
ctest -R "nmd.tests.skill.(unit|integration)" → 2/2 Passed (2.08s)
NoMoreDayTests.exe -tc="*Skill 3*" → 24 cases / 209 assertions, 0 failed
rg max_lifetime / crit_chance / EnsureSummonAttribution / GetFloat / splash_radius / static_field / ArmorShred 全库交叉验证
skills.json skill3 内嵌契约条目统计（ConvertFrom-Json）→ 13 条
skill_3_tree.json → 无 max_points 字段；311/374/375 prereq required_points 与修复后 skills.json 一致
```

### 15.6 复审处置建议

1. **必须修复后提交**：CR-331（单位 ÷100 并修正测试期望）、HI-GetFloat（DoCast 缓存进 BladeFormationComponent）、HI-互斥（max_transmuters=1）、HI-310（二选一路径并统一基准）、HI-373/375（基数改为灵剑攻击伤害缩放 + 空间网格）。
2. **建议随同修复**：M3 补齐 352 等条目并让 key-node fixture 覆盖全 28 节点；M9 接线 OnCrit。
3. **可另开任务**：MI-2/3/4/5 四处行为子项、351 数据消费统一、ArmorShred id 重命名、C0 设计裁决。

## 16. 整改复审（2026-09-08 第三轮，修复验证）

**复审范围**: 针对 15.6 处置建议派发的修复变更（数据 JSON / 契约三件套 / Baker / BladeFormation / ProcEngine / SkillSystem / 测试助手与用例），逐段 diff 复核 + 亲测复验。

### 16.1 复审结论

**提交**。15.6 第 1 条全部 5 项（CR-331、HI-GetFloat、HI-互斥、HI-310、HI-373/375 基数）与第 2 条 2 项（M3、M9）完整落地；逐段复核未发现新增 Critical/High 问题；构建与测试全绿。15.6 第 3 条遗留项维持"可另开任务"处置（见 16.4）。

### 16.2 修复验收

| 项 | 状态 | 证据 |
|---|---|---|
| CR-331 单位 | ✅ 修复 | `SkillSpecializationBaker.cpp`（case 331）`del.bonus_crit += (crit_pct / 100.0f) * points`；`BladeFormation.cpp` fallback `(0.05f * pts331)`；测试期望 25.0 → 0.25；332 保持 `(25/100)` 未动 |
| HI-GetFloat 热路径 | ✅ 修复 | `SkillDefs.hpp` `BladeFormationComponent` 新增 15 个预烘焙缓存字段（taken_phys_pct / crush_stun_chance / crush_stun_duration / ignite_* 4 项 / chain_damage_pct / chain_radius / shred_* 3 项 / burst_mult / final_damage_scale）；DoCast 一次性读 mechanics 写入；DoHit 全分支零 `GetFloat`、零 `std::string` 构造（仅 374 条件路径 debuffId 保留，见 16.4）；`SkillSystem.cpp` 312 回蓝改仅由缓存驱动并附注释说明兜底一致性 |
| HI-互斥 | ✅ 修复 | `skills.json` 与 compact 技能 3 `max_transmuters` 2→1（其余 8 技能未动）；`SkillContractRegistryTests.cpp` 改为参数化断言 `skill_id == 3 ? 1 : 2` |
| HI-310 双重供给 | ✅ 修复 | `skills.json` 310 删除 `stat_modifiers`（t31/t32 全组）；Baker 与 BladeFormation fallback 统一 `200 × (1 + 0.20×p)` 基准；测试期望 800 → 320 |
| HI-373/375 基数 | ✅ 修复 | DoHit 统一基数 `baseHit = ((min_weapon_damage + max_weapon_damage) × 0.5) × formation->final_damage_scale`；373 = `baseHit × chain_damage_pct`、375 = `baseHit × burst_mult`；`chain_radius` 外置 mechanics（200）；373 遍历加 `chainsLeft <= 0` break 早退并注释声明回调无 grid 上下文（O(N) 网格化另开任务） |
| M3 契约条目 | ✅ 修复 | `skills.json` 内嵌契约补至 28/28（新增 300-303/310/312/331-334/350/352/371/373/375 等 15 条 Passive 零值条目，352 条目回归，373 Trigger→Passive，330 Keystone、315 sword_intent、335 Trigger、355 Synergy 归位） |
| M9 OnCrit | ✅ 修复 | `TriggerRuleComponent.hpp` / `SkillContract.hpp` 新增 `requires_crit` 字段（standard_layout 保持）；`SkillRegistry.cpp` 解析；Baker Bake 与 SyncTriggerRules 两路写入；`ProcEngine.cpp` 分发处 `if (rule.requires_crit && !event.isCrit) continue;`；`skills.json`/compact 335 `requires_crit: true`；`gen_skill_contracts.py` 三处同步（解析、零值默认、is_default_trigger 判定含 requires_crit，防止生成器重跑丢 335 条目） |

### 16.3 复核证据（第三轮）

```
逐段 diff 复核: SkillDefs.hpp + BladeFormation.cpp（663 行全读）/ Baker + SkillSystem +
  ProcEngine + 契约三件套 / skills.json + compact + mechanics + 测试助手与 5 个测试文件
rg 复验: BladeFormation.cpp DoHit 区无 GetFloat 调用（仅 DoCast 区 19 处）；
  SkillSystem.cpp 1088-1092 回蓝仅缓存驱动
build.bat (RelWithDebInfo) → EXIT=0
ctest -L ci → 1/1 Passed；ctest -L integration → 6/6 Passed
NoMoreDayTests.exe -tc="*Skill 3*" → 24 cases / 213 assertions, 0 failed
NoMoreDayTests.exe -tc="*Trigger*" → 23 cases / 334 assertions, 0 failed
```

### 16.4 遗留项与说明

1. **374 debuffId `std::string`**（`BladeFormation.cpp` 条件路径 `"SpiritCorrosion_Fire"/"_Lightning"`）：仅在"目标带对应元素异常且 374 已点"时构造，非每次命中；属可接受取舍，后续可用 `BuffId` 枚举消除。
2. **373 O(N) 敌人遍历**：频率受 335 OnCrit + victim 已感电 + ICD 4s 约束，已 break 早退；空间网格化待 DoHit 回调签名扩展后另开任务。
3. **MI-2/3/4/5**（372 瞬移+静电场、375 引爆、331 溅射、355 剑阵元素附加）、**351 数据消费统一**、**ArmorShred id 重命名**、**C0 Toggle 口径设计裁决**：维持另开任务，数据侧（mechanics 351/372/375/331/355 键）已就绪。
4. **测试适配说明**：3 个冒烟用例因互斥改为单点 370（`CastSmokeNodes` 剔除 372），372 的契约/数据验证仍由矩阵加载用例覆盖；互斥拦截的正向"双点被拒"用例未新增，建议后续在 `SkillCastConstraintService` 测试补齐。
5. `skill_mechanics.json` 文件尾无换行符（Low，沿用原状）。

