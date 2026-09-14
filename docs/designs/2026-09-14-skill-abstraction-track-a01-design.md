# A-01 技能 1~12 专精实现抽象化/模块化 — 设计（Wave E）

- 状态：**设计 / 待裁定**
- 日期：2026-09-14
- 归属：`docs/plans/2026-09-13-skill-followup-plan.md:188-194` §7 Wave E（A-01 独立 Track，E1 = 本设计）
- 关联：backlog `docs/plans/2026-09-12-skill1-9-followup-backlog.md:162`（A-01）
- 前置设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（Rev.2，已裁决方案 B）
- 前置计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（A1/A2 已基本落地）
- 评审来源：`docs/reviews/2026-09-07-modular-skill-archetypes-implementation-review.md`、`docs/reviews/2026-09-13-skill-followup-review.md`

> 本文只做设计，不含实现代码；接口为签名雏形，用于评审范围与边界。

---

## 1. 背景与问题陈述

### 1.1 任务来源

backlog A-01 的原始陈述（`:162`）要求「技能 1~9 专精实现抽象化/模块化」，并注明**必须先重定 DoD**，且给出两个候选方向：

- 方向一：「恢复薄装配 ~400 行/文件」（09-07 评审基线为 **≤100 行/文件**；Wave E `skill-followup-plan.md:190` 已明确**不设 ≤100 行硬指标**，故该方向的表述本身已发生漂移）。
- 方向二：「承认节点逻辑内聚、只抽取『触发/效果/交付』三层参数化模式」。

### 1.2 前置设计已经作出的裁决（必须承接，不得推翻重来）

`2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（Rev.2）已完成一轮完整设计并**裁决采用方案 B**（§4.1），其配套计划 A1/A2 基本落地（提交 `0ef68d2d`：`refactor(skill): unify spec-state access, migrate skills 10-12, and clean dead code`）。已落地物：

| 已落地 | 证据 |
|---|---|
| 读点/点亮/机制数值 helper 三件套 | `src/game/foundation/components/SkillPointAccess.hpp:14-43`（`ReadPoints` / `HasNode` / `GetMech`） |
| 技能 10 的 POD SpecState + 表驱动绑定 + `ResolveSpecState` | `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:288-405` |
| `BakedDeliveryParams` 显式语义字段、`BakedSkillProfile` POD | `src/game/foundation/components/SkillDefs.hpp:625-699` |
| 交付原型组件 + 交付系统拆分 | `src/game/systems/skill/{ProjectileSystem,BoomerangDeliverySystem,AreaFieldDeliverySystem,MobilityDeliverySystem,OrbitingSentinelDeliverySystem,BeamChannelDeliverySystem}.cpp` |

因此本设计的定位不是「从头选型」，而是**承接方案 B、重定可验证 DoD、补齐残余缺口（技能 1~9 信封化）并裁定分层边界**。

### 1.3 残余缺口（本 Wave E 的真实工作面）

1. **DoD#4 未闭环**：技能 7/8/9（10/11/12）已有 POD SpecState 信封；**技能 1~9 仍采用「DoCast/DoHit 内临时局部变量 + 内联 loop + lambda 读点」的散装写法**。最典型：`src/game/systems/skill/behaviors/FlowingThrust.cpp:342-371`（9 个局部点数变量 + 内联 `specialized_slots` 循环 + `getPts` lambda）。前置设计 §11 #10 已记录该残余。
2. **profile 解析样板重复 ~9 处**：`GetBakedSkillProfile` + 回退 `SkillSpecializationBaker::Bake(..., &spec, localProfile, nullptr)` 的整段形状在行为层重复：`FlowingThrust.cpp:97-107`、`RendingWave.cpp:82-90`、`BladeFormation.cpp:95-103`、`BladeWard.cpp:66-76`、`BladeBoomerang.cpp:97-105`（另 `:293`）、`InfiniteBlades.cpp:142-150`（另 `:90`）、`SwordArray.cpp:67-76`、`MindBlade.cpp:39-45`、`PhantomTrance.cpp:69-82`。
3. **flag 判定与 HasNode 双轨并存**：行为层仍有约 43 处 `feature_flags &`/`flags &`（`SwordArray.cpp` 14、`RendingWave.cpp` 11、`FlowingThrust.cpp` 5、`BladeWard.cpp` 5、`InfiniteBlades.cpp` 4、`BladeFormation.cpp` 2、`BladeBoomerang.cpp` 2）。按前置设计 §4.4.1「flag 二分准则」，其中属于**节点语义**的部分应迁往 `HasNode`，属于**交付参数**的部分可保留在 Baker。
4. **`--gen-specstate` 生成器未实现**：`scripts/` 下 grep `specstate|SpecState|gen_spec` 无结果。当前测试 `tests/unit/SpecStateMappingTests.cpp` 对技能 10 用手写镜像核对，无法防止漂移（前置计划 T6.5 已注记）。
5. **SpecState 绑定表形状重复**：`SevenStarSlashShared.hpp`、`HeavenlySwordDescent.hpp`、`BloodSea.hpp` 各自手写 `PointBinding`/`FlagBinding` 结构，可模板化（前置设计 §11 #11）。

### 1.4 行数现状（用于观测，不作为门禁）

当前工作树（`Get-Content` 行数，UTF-8）：

| 文件 | 行数 | 文件 | 行数 |
|---|---:|---|---:|
| FlowingThrust.cpp | 1034 | SevenStarSlash.cpp | 656 |
| HeavenlySwordDescent.cpp | 829 | BladeWard.cpp | 590 |
| PhantomTrance.cpp | 762 | BloodSea.cpp | 568 |
| BladeFormation.cpp | 726 | InfiniteBlades.cpp | 513 |
| RendingWave.cpp | 702 | SwordArray.cpp | 498 |
| BladeBoomerang.cpp | 378 | MindBlade.cpp | 80 |

- 技能 1~9（不含 10/11/12）当前合计约 **5499 行**（backlog `:162` 记「~4542 行」**已过期**：BladeWard 由 137 行增至 590 行，系技能 4 后续节点补齐所致，属功能增长而非架构退化）。
- `SkillSpecializationBaker.cpp` = 1077 行，两个 `switch` × 9 个 `case` = 18 个 case（默认交付推导 `:62-113` + 节点烘焙分支）。

---

## 2. 范围

### 2.1 范围内（In Scope）

- 技能 1~12 **行为层**的「触发 / 效果 / 交付」三层边界的最终裁定与参数化落地方式。
- 技能 1~9 的效果层信封化（POD SpecState + 绑定表 + 单点解析）。
- 行为层 `feature_flags` 二分流（节点语义 → `HasNode`；交付参数 → 保留 Baker）。
- profile 解析样板的唯一化基元。
- `--gen-specstate` 生成器的设计契约（生成物、幂等/确定性、与手写镜像的关系）。
- 重定的、可验证的 DoD。

### 2.2 范围外（Out of Scope）

- `src/`、`assets/`、`tests/`、backlog、plan、GDD 的**编辑**（本轮仅设计）。
- 交付系统内部算法（弹道、牵引、光环结算）的重写。
- 技能 10~12 的**再**迁移（已信封化，本轮仅纳入回归不变量）。
- Baker 两个 `switch` 的数据化（改由 JSON 驱动）——列为决策点 D8，默认本轮不动。
- 渲染/表现层、UI 层读点方式的重构。
- 数值平衡调整。

### 2.3 非目标

- 不以「单文件 ≤N 行」为验收目标（Wave E `:190` 已取消硬行数指标）。
- 不引入虚函数派发、RTTI 或运行期字符串节点判定（保持零堆、静态派发）。
- 不为「假想的旧行为」引入兼容层（`docs/workflows/design.md:24`）。

---

## 3. 现状证据（分层视图）

### 3.1 已有分层骨架

**触发层**（已存在，成熟）：
- `src/game/systems/skill/behaviors/SkillBehaviorBase.hpp`：CRTP `SkillBehaviorBase<Derived>`，`OnCast`→`Derived::DoCast`，可选 `OnHit`→`Derived::DoHit`（`if constexpr (requires {...})`），`getModifier(node_id, ModifierParam, default)` 读节点契约。
- `src/game/systems/skill/behaviors/SkillBehaviorRegistry.hpp`：静态函数表 `CastFunc`/`HitFunc`，宏 `REGISTER_SKILL_BEHAVIOR(Class)`，无虚派发。
- `src/game/foundation/components/TriggerRuleComponent.hpp`：`TriggerRule{rule_id=0,...}`（`:26`）、`AddRule` 按 `rule_id` 去重（`:63-75`）、`RemoveRule`（`:77`）、`HasRule`（`:90`）。
- 节点契约 → `TriggerRuleComponent` 的写入由 `SkillSpecializationBaker::Bake` 完成（`SkillSpecializationBaker.cpp:14-20` 入口，`:126-` 起处理 `SpecNodeRole::Trigger`）。

**效果层**（技能 10~12 已有信封；技能 1~9 散装）：
- 正例：`SevenStarSlashShared.hpp:288-405`（POD `SevenStarSlashSpecState` + 17 条点位绑定 + 6 条 flag 绑定 + 单点 `ResolveSpecState`，含 `SkillSystem::GetActiveTransmuterNode` 叠加非点读节点）。
- 反例：`FlowingThrust.cpp:342-371`（DoHit 内临时变量散装读点）。
- 共享效果基元已有雏形：`SevenStarSlashShared.hpp:71-170`（`RefundSkillCooldownPercent`/`RestoreSkillCharge`/`RefundManaCost`/`GrantSwordStep`/`GrantReturningStepDefense`）、`:172-188`（`DeterministicRoll`）。

**交付层**（已存在，成熟）：
- 数据：`SkillDefs.hpp:625-699` `BakedDeliveryParams`（含 `feature_flags` 位掩码 + 通用字段 + 技能 8/9 专项字段 + `PhantomTranceParams`）+ `BakedSkillProfile`。
- 唯一写入方：`SkillSpecializationBaker` case 1~9（`SkillSpecializationBaker.cpp:62-113` 及节点烘焙段）。
- 交付系统：`ProjectileSystem.cpp`、`BoomerangDeliverySystem.cpp`、`AreaFieldDeliverySystem.cpp`、`MobilityDeliverySystem.cpp`、`OrbitingSentinelDeliverySystem.cpp`、`BeamChannelDeliverySystem.cpp`。

### 3.2 主要重复面（按可抽取价值排序）

| 排名 | 重复面 | 规模 | 证据 |
|---|---|---:|---|
| R1 | profile 解析 + 回退 Bake 样板 | 9 处（行为层），另有 7 处非行为消费 | 见 §1.3-2 |
| R2 | 效果层读点散装（局部点变量 + 内联 loop） | 技能 1~9 全部 | `FlowingThrust.cpp:342-371` 等 |
| R3 | `feature_flags` 节点语义判定 | ~43 处 | 见 §1.3-3 |
| R4 | SpecState 绑定表结构体形状 | 3 处手写 | `SevenStarSlashShared.hpp:318-378`、`HeavenlySwordDescent.hpp`、`BloodSea.hpp` |
| R5 | 每技能节点常量命名空间 | 11 个 | `FlowingThrustNodes` 等 |

### 3.3 可复用价值最高的既有基元

- `skills::ReadPoints` / `HasNode` / `GetMech`（`SkillPointAccess.hpp:14-43`）——**已是唯一读点入口**：生产行为层已无 `allocated_points.(find|contains|count)` 直接调用（仅 helper 内部与测试保留），DoD#8 基本达成。
- SpecState 信封 + 表驱动（技能 10 已验证可行）。
- `DeterministicRoll` 等确定性骰子基元。

---

## 4. 目标与非目标

### 4.1 目标

1. **重定 DoD**：以「能力不变量」而非行数作为验收，逐项可 grep / 可单测 / 可脚本校验。
2. **补齐技能 1~9 效果层信封化**，与技能 10~12 对齐到同一模式。
3. **消除 R1/R4 两类结构性重复**（样板唯一化、绑定表模板化）。
4. **裁定并收敛 R3**（flag 二分）。
5. **明确 `--gen-specstate` 契约**，使 SpecState 从「手写镜像」升级为「生成 + 校验」。
6. 为后续 Wave（技能机制继续扩展、新增技能）提供可复用的三层模板。

### 4.2 非目标

见 §2.3。

---

## 5. 行为规则、数据所有权与跨系统契约

### 5.1 分层职责（拟冻结）

| 层 | 职责 | 事实源 | 禁止 |
|---|---|---|---|
| 触发层 | 何时触发、命中/暴击/施法事件路由 | `SkillContract`（`SkillRegistry::GetNodeContract`）+ `TriggerRuleComponent` + `ProcEngine` + `SkillBehaviorBase` | 在触发层写具体伤害/数值公式 |
| 效果层 | 单技能的「机制语义」：谁读点、读什么、效果如何组合 | 行为文件 `XSpecState`（POD）+ `skill_mechanics.json`（经 `GetMech`） | 硬编码机制数值；spawn/交付细节 |
| 交付层 | 如何把效果送到目标：弹道/环场/召唤/引导/位移 | `BakedDeliveryParams` + `*DeliverySystem` | 在交付层解释节点语义（只吃 Baker 产出的参数） |

依赖方向：**触发 → 效果 → 交付**，单向；交付层不得反向依赖具体技能节点 id。

### 5.2 数据所有权

- 节点**点数**：唯一来源 `SpecializedSkill::allocated_points`，唯一读入口 `skills::ReadPoints`（`SkillPointAccess.hpp:14-23`）。
- 节点**点亮语义**：唯一定义 `HasNode`（`ReadPoints>0`，`:26-34`）。
- 节点**机制数值**：唯一来源 `skill_mechanics.json`，唯一读入口 `skills::GetMech`（`:38-43`）。
- 交付**参数**：唯一写入方 `SkillSpecializationBaker`，唯一载体 `BakedSkillProfile`（`SkillDefs.hpp:672-699`）。

### 5.3 生命周期

- 施法时：`BakedSkillProfile` 缓存解析（`SkillSystem::GetBakedSkillProfile`，`SkillSystem.cpp:2753`）；缓存 miss 时行为层回退 `Bake` 到栈上临时 profile（当前 9 处样板，见 R1）。
- 效果层 SpecState 生命周期：**DoCast/DoHit 内一次性解析，函数内只读**，不跨帧存储（对齐 `SevenStarSlashShared.hpp:286-287` 的既有约定）。
- 交付层组件（`Projectile`/`BoomerangComponent`/`PersistentFieldTag` 等）生命周期由各自 DeliverySystem 管理。

### 5.4 必须保持的跨系统契约（行为不变量）

1. `HasNode` 语义 = 点数 > 0，全工程唯一（`SkillPointAccess.hpp:26`）。
2. 非点读节点（转质/激活型，如 1021 `PoleStarOrbit`、1022 `Starfall`、973）必须**保留在绑定表循环之外**，经 `GetActiveTransmuterNode` 叠加（`SevenStarSlashShared.hpp:400-404`）。
3. 交付参数单写入方（Baker），行为/交付系统只读。
4. `TriggerRuleComponent` 按 `rule_id` 去重，Rebake 不得累积（`TriggerRuleComponent.hpp:63-75`）。
5. 热路径零堆分配（`BakedDeliveryParams`/`BakedSkillProfile` 的 `static_assert`，`SkillDefs.hpp:668-699`）。
6. 元素异常状态按 `BuffKind`/`BuffType` 整数比较，不得回到字符串子串匹配（`FlowingThrust.cpp:389-401`）。
7. 已裁定的特定门控不得回归：如技能 3 `BladeFormation.cpp:746-750`（331 巨剑门控）、技能 4 反击经 `damage/DamageInterceptors.hpp:55` 且 `GetMech(4,470,"base_damage",35.0f)`。

---

## 6. 方案对比

四个候选方向，其中 (a)(b) 来自 backlog，(c)(d) 为新提出。

### 方案 A：恢复薄装配薄文件（≤100 行/文件，或放宽至 ~400 行）

- **做法**：把 DoCast/DoHit 的节点逻辑重新下沉到「数据 + 通用交付系统」，行为文件只留装配。
- **改动面**：极大。技能 1~9 共约 5499 行，主体是节点语义（流血、元素、协同、门控），不是可下沉的装配。09-07 评审基线（9 文件 ≤100 行）是在**节点语义尚未实现**时达成的（前置设计 §1.1 已记录），当前 5499 行是语义补齐的结果。
- **风险**：高。为压行数会把 per-skill 特化逻辑塞进「万能通用系统」，制造隐式耦合与条件分支，违反「抽象必须自证复杂度」。
- **可逆性**：低（大规模搬迁难回退）。
- **对 18 个 Baker case/测试的影响**：Baker 结构可能被推翻重写；行为测试大面积位移。
- **对 A-02 的价值**：负（技能 10~12 已信封化，若走此路需推翻）。
- **结论**：**否决**。与 Wave E `:190`「不设 ≤100 行硬指标」相悖。

### 方案 B：三层参数化（触发/效果/交付）

- **做法**：承认 per-skill 效果语义内聚；为技能 1~9 补齐 POD `XSpecState` + 绑定表 + 单点解析；将 flag 节点语义迁往 `HasNode`；抽取 profile 解析样板的唯一基元；SpecState 由 `--gen-specstate` 生成。
- **改动面**：中等且**可控**。技能 1~9 各增 1 个 SpecState 段（结构体 + 绑定表 + Resolve），DoCast/DoHit 的散装读点改为 `const auto st = ResolveSpecState(...)`；行为文件行数可能**上升**（新增信封声明），但语义集中度提升。
- **风险**：中。主要风险是「生成器 × 手写」双源漂移、以及漏迁 flag 导致行为变化。
- **可逆性**：高（每技能独立，可按技能回退；信封是纯增量的数据重排）。
- **对 18 个 Baker case/测试的影响**：Baker **不改**（交付层参数仍在 Baker）；`SpecStateMappingTests` 从「技能 10 手写镜像」扩展为「技能 1~12 生成物校验」。
- **对 A-02 的价值**：高（与已落地的技能 10~12 模式完全同构，形成统一模板）。
- **结论**：**推荐（承接前置裁决）**，但需按 §7 的 D2/D3 做局部强化。

### 方案 C：按机制族分组成基元（DoT/投射/环场/召唤）

- **做法**：在效果层之上，抽出「机制族基元」`skills::primitives::*`（如 ApplyBleedBurst、SpawnProjectileFan、OrbitSentinel、SummonEcho），技能调用基元 + 参数。
- **改动面**：中～大。需要先识别族内真正共享的语义；跨技能耦合最强（一个基元变更影响多技能）。
- **风险**：中高。过早抽象风险大：当前各技能的「看起来相似」逻辑在数值口径、剩余跳数计算、元素判定上差异明显（例：`FlowingThrust.cpp:434-462` 的流血爆发与 `AilmentTickDriver` 口径对齐是**特化契约**，不是通用件）。
- **可逆性**：中（基元一旦被多技能依赖，回退成本上升）。
- **对 18 个 Baker case/测试的影响**：间接（交付层仍归 Baker）。
- **对 A-02 的价值**：中（技能 10~12 机制族与 1~9 不同，复用有限）。
- **结论**：**采纳为方案 B 的「局部增益」，不单独成方向**。仅当某机制族在 ≥3 个技能中**逐字重复**时才抽基元（门槛写入 DoD，见 §7 的 D-A6）。

### 方案 D：维持现状，只做去重小步重构

- **做法**：不做信封化，仅消除 R1（profile 样板）等零星重复。
- **改动面**：小。
- **风险**：低。
- **可逆性**：高。
- **对 18 个 Baker case/测试的影响**：几乎无。
- **对 A-02 的价值**：低——技能 1~9 仍与已信封化的 10~12 模式割裂，Wave E 目标（三层参数化）未达成。
- **结论**：**作为回退底线**。若裁定担心风险，可先执行 D（R1 去重）作为 E1 的前哨批次，但不作为最终方向。

### 对比矩阵

| 维度 | A 薄装配 | B 三层参数化 | C 机制族 | D 只去重 |
|---|---|---|---|---|
| 改动面（文件/行） | 极大（全部行为层） | 中（1~9 行为 + 1 基元 + 1 生成器 + 测试） | 中～大（效果层再抽一层） | 小（~9 处样板） |
| 风险 | 高 | 中 | 中高（过早抽象） | 低 |
| 可逆性 | 低 | 高 | 中 | 高 |
| Baker 18 case 影响 | 推翻重写 | 无 | 无 | 无 |
| 测试影响 | 大面积位移 | 扩展 SpecStateMappingTests | 新增基元测试 | 极小 |
| 对 A-02 复用 | 负 | 高 | 中 | 低 |
| Wave E 目标契合 | 相悖 | 契合 | 部分 | 不契合 |

---

## 7. 推荐方向与重定 DoD

### 7.1 推荐

**推荐方案 B（三层参数化），并吸收方案 C 作为「≥3 次逐字重复才抽基元」的局部增益；以方案 D 的 R1 去重作为 E1 前哨批次。**

理由（一句）：**技能 10~12 已完成信封化且通过评审，方案 B 是唯一能与之同构、可逆、且不惊动 Baker/交付系统与既有 5442 行节点语义的收敛路径；方案 A 会为压行数牺牲语义清晰度，方案 C 单独执行则过早抽象。**

### 7.2 重定 DoD（可验证验收标准）

> 说明：**取消行数门禁**（Wave E `:190`）。行数仅在审查报告里作 before/after 观测记录。

| 编号 | 验收标准（能力不变量） | 验证方式 |
|---|---|---|
| **D-A1** | 技能 1~12 每个技能的效果层有**唯一 POD SpecState**，且在 `DoCast`/`DoHit` 内**一次性解析、函数内只读** | 逐技能存在 `XSpecState` 类型 + 恰 1 个 `ResolveSpecState` 调用点；code review |
| **D-A2** | 效果层「节点存在性」判定**唯一经 `HasNode`**；行为文件中 `feature_flags &`/`flags &` 计数为 **0**，或仅剩被注释标记为 `delivery-param` 且由 Baker 写入的位 | `rg -c "feature_flags &" src/game/systems/skill/behaviors/*.cpp`；逐处标注复核 |
| **D-A3** | profile 解析样板唯一化：行为层不再出现 `SkillSpecializationBaker::Bake(` | `rg -n "SkillSpecializationBaker::Bake" src/game/systems/skill/behaviors/` 命中 0 |
| **D-A4** | 效果层无硬编码机制数值（物理常量除外） | **未脚本化门禁（review 级证据）**：`rg -n "GetFloat\|GetMech\|GetInt" src/game/systems/skill/behaviors/*.cpp` 核对机制系数走注册表 + 人工分类数字字面量 |
| **D-A5** | SpecState 绑定表结构**模板化**：**在迁移范围内（技能 1~9 + 全部 12 张生成表）**收敛为 1 个泛型 `SpecStateTable<State>`；技能 10~12 保留各自手写绑定结构，为 **D7 授权的已接受例外**（仅回归、不重做） | 存在单一 `SpecStateTable<State>` 模板；技能 1~9 与全部生成表无重复 `PointBinding`/`FlagBinding` 定义；技能 10~12 手写定义（`SevenStarSlashShared.hpp:318/323`、`HeavenlySwordDescent.hpp:110/115`、`BloodSea.hpp:132/137`）登记为 D7 例外 |
| **D-A6** | 机制族基元抽取仅当 ≥3 技能逐字重复；每个新基元在设计中列出调用点 | 基元清单 + `trace_path` 调用点 ≥3 |
| **D-A7** | `--gen-specstate` 落地：可从技能树数据生成 SpecState 声明与绑定表，且 `--check` 幂等/确定性通过 | `python scripts/gen_skill_contracts.py --gen-specstate --check ...` PASS |
| **D-A8** | 生成物与运行期读点一致（防手写镜像漂移） | `GeneratedSpecStateTests` 覆盖技能 1~12 生成物；`SpecStateMappingTests`/`SpecStateTableTests` 独立校验技能 10 生产解析器；断言「生成表逐节点 == 运行期 `ReadPoints`」 |
| **D-A9** | 非点读/转质节点保留在绑定表循环外 | 单测覆盖 1021/1022/973 等激活型节点 |
| **D-A10** | 行为不变量不回归（§5.4 第 1~7 条） | `ctest -L ci` 全绿；技能专项 `-L skill` / `-L functional` 全绿 |
| **D-A11** | 构建与数据校验 | `build.bat` RelWithDebInfo **0 警告**；`gen_skill_contracts.py --check` / `sync_skill_node_icon_ids.py --check` / `gen_asset_registries.py` PASS |
| **D-A12** | 行数仅观测 | 审查报告记录每技能 before/after 行数，**不设阈值、不作通过条件** |

### 7.3 必须保留的 per-skill 特化（不得被"通用化"抹平）

- 技能 3 331 巨剑门控（`BladeFormation.cpp:746-750`）。
- 技能 4 反击口径（`damage/DamageInterceptors.hpp:55`，`GetMech(4,470,...)`）。
- 技能 1 流血爆发与 `AilmentTickDriver` 的剩余跳数口径对齐（`FlowingThrust.cpp:434-462`）。
- 技能 2 弹速倍率语义（相对基准 300；`SkillSpecializationBaker.cpp:68-73`）。
- 技能 7 心念射程基于 `base_range` 的 `range_pct_per_point` 放大，缺省会导致点满反而更短（`SkillSpecializationBaker.cpp:92-95`）。
- 技能 8 御剑回旋专项参数（`SkillDefs.hpp:642-661`）。
- 技能 9 绝影形态时长语义（`SkillSpecializationBaker.cpp:104-110`）。
- 技能 10~12 既有信封与转质叠加（`SevenStarSlashShared.hpp:400-404`）。

### 7.4 可删除的重复（预期收益）

1. R1 profile 样板 9 处 → 1 个基元（净减约 9 × 10 行，且消除回退不一致风险）。
2. R2 技能 1~9 散装读点 → 信封（每技能若干段内联 loop/lambda 消失）。
3. R4 三处绑定表结构体 → 1 个模板。
4. R3 中属节点语义的 flag 判定 → `HasNode`。

---

## 8. 三层接口雏形（签名草案，非实现）

> 仅用于评审接口边界与依赖方向；实现细节留给 `*-plan.md`。

### 8.1 效果层：SpecState 泛型 descriptor（解决 R4）

```cpp
// foundation 或 skills 层；节点 id -> SpecState 成员指针
template <typename State>
struct SpecStateTable {
  std::span<const typename State::PointBinding> points;
  std::span<const typename State::FlagBinding>  flags;
};

// 单点解析：首个匹配槽位填表；转质/激活节点由各技能 Resolve 在表循环外叠加
template <typename State>
[[nodiscard]] State ResolveSpecState(const entt::registry &registry,
                                     entt::entity owner,
                                     uint32_t skillId,
                                     const SpecStateTable<State> &table);
```

约束：`State` 为 POD；`ResolveSpecState` 不得读 `skill_mechanics`（数值由效果逻辑经 `GetMech` 单独取）。

### 8.2 效果层：profile 解析唯一基元（解决 R1）

```cpp
// 返回缓存 profile；miss 时在调用方栈上回退 Bake
[[nodiscard]] const BakedSkillProfile *
ResolveBakedProfile(entt::registry &registry, entt::entity owner, uint32_t skillId);

// 栈回退版本，供需要非 const 的少数场景（如 DoHit 重算）
[[nodiscard]] BakedSkillProfile
ResolveBakedProfileLocal(entt::registry &registry, entt::entity owner, uint32_t skillId);
```

依赖方向：置于 `src/game/systems/skill/` 或 foundation 同层，禁止 behaviors 反向被 UI/战斗层依赖（沿用 `SkillPointAccess.hpp:2-3` 的分层理由）。

### 8.3 效果层：共享效果基元（方案 C 增益，D-A6 门槛）

```cpp
namespace NoMoreDay::skills::primitives {
// 仅在 >=3 技能逐字重复时引入；每个基元须列出调用点
[[nodiscard]] float SumRemainingDotDamage(const ActiveEffectsComponent&, BuffType);
void ApplyAilment(...);          // 待机制族盘点后裁定
void GrantTimedStatBuff(...);    // 已有雏形见 SevenStarSlashShared.hpp:138-170
}
```

### 8.4 生成器契约（解决 `--gen-specstate`）

- 输入：技能树数据（节点 id/类型/语义）——具体来源由实施计划确定（未决）。
- 输出：每技能 `XSpecState` 结构体声明 + `PointBinding`/`FlagBinding` 表 + `ResolveSpecState` 特化（或表常量）。
- 校验：`--check` 对比生成物与仓库现状，幂等（重复运行无 diff）+ 确定性（顺序稳定）。
- 手写镜像退役：`tests/unit/SpecStateMappingTests.cpp` 的镜像改为引用生成物。
  - **实施裁定（A-01 Phase 5）**：该文件「解析逻辑镜像」已在此前重构中退役（现直接调用生产 `ResolveSpecState`），残留的测试侧期望表不退役——其独立期望值断言与技能 10 转质节点运行期语义不被 `GeneratedSpecStateTests` 等价覆盖，且 D7 要求技能 10~12 仅回归。D-A8 的 1~12 生成一致性以 `GeneratedSpecStateTests` 为准。

---

## 9. 决策点（需用户拍板）

| 编号 | 决策点 | 选项 | 建议 | 影响 |
|---|---|---|---|---|
| **D1** | DoD 锚定方式 | (a) 恢复行数阈值（≤100 或 ~400）；(b) 能力型 DoD（§7.2） | **(b)** | (a) 与 Wave E `:190` 相悖且会诱发过早抽象 |
| **D2** | 技能 1~9 SpecState 落地方式 | (a) 纯手写；(b) `--gen-specstate` 生成 + 校验 | **(b)**（先手写一版作为生成器黄金样本） | (a) 会重演「手写镜像无法防漂移」 |
| **D3** | SpecState 是否泛化 | (a) 每技能独立 struct+表（同技能 10）；(b) 统一 `SpecStateTable<State>` 模板 | **(b)** | (a) 重复 R4 形状；模板化可减少 3 份手写 |
| **D4** | 是否引入机制族基元 | (a) 引入完整族（DoT/投射/环场/召唤）；(b) 仅 ≥3 次逐字重复才抽；(c) 不引入 | **(b)** | (a) 过早抽象风险；(c) 放弃可复用收益 |
| **D5** | profile 解析基元归属层 | (a) foundation；(b) skill 层 | **(b) skill 层**（若 UI/战斗层也需要再上移） | 影响反向依赖边界 |
| **D6** | `feature_flags` 去留 | (a) 全删；(b) 仅保留交付参数位（Baker 写入），删节点语义位 | **(b)** | (a) 会误伤交付层既有参数契约 |
| **D7** | 技能 10~12 是否纳入 E2 | (a) 一并重做；(b) 仅回归验证不重做 | **(b)** | (a) 推翻已评审成果 |
| **D8** | Baker 两个 `switch` × 9 case | (a) 维持；(b) 迁移为 JSON 驱动 | **(a)** 本轮不动 | (b) 扩大改动面、影响 18 case 契约 |
| **D9** | 生成器输入源 | (a) 现有技能树数据文件；(b) 新增中间描述文件 | **待盘点后定**（未决） | 决定生成器耦合面 |

---

## 10. 影响面分析

### 10.1 接口

- 新增：`ResolveBakedProfile`、`SpecStateTable<State>`/`ResolveSpecState<State>`、`primitives::*`（D4 通过时）。
- 不改变：`SkillBehaviorBase`、`SkillBehaviorRegistry`、`SkillSpecializationBaker` 公开签名、`BakedSkillProfile` 布局。

### 10.2 存档

- 无影响。`ActiveSkillsComponent` 的序列化仅含 `slots`/`specialized_slots`/`available_talent_points`（`SkillDefs.hpp:717-729`）；SpecState 是瞬时 POD，不入存档。

### 10.3 资产与配置

- 无新增资产。生成器可能新增/更新**生成物源文件**（D9 未决）。
- `skill_mechanics.json` 不改（本轮无平衡调整）。

### 10.4 性能预算

- 目标：不劣化。SpecState 解析为 O(绑定项数)、零堆；`BakedDeliveryParams`/`BakedSkillProfile` 保持 `standard_layout` + `trivially_destructible`。
- 观测点：DoCast/DoHit 热路径不新增分配；Tracy 抓点（按 `docs/workflows/performance.md`）。

### 10.5 回滚

- 每技能信封化独立可回退（纯增量重排）；生成器与模板化可单独回退；Baker 不改故无回滚面。

---

## 11. 分阶段实施骨架（Wave E，仅骨架）

| 批次 | 内容 | 出口 |
|---|---|---|
| **E0** | 本设计裁定（D1~D9）+ `*-plan.md` | 用户拍板、设计状态转「已裁定」 |
| **E1a** | R1 前哨：抽出 `ResolveBakedProfile`，替换 9 处样板 | `ctest -L ci` 绿；D-A3 达标 |
| **E1b** | R4：`SpecStateTable<State>` 模板 + 技能 10 回填为黄金样本 | D-A5 在**迁移范围内**达标；技能 10 **未回填**（黄金样本实为技能 8，经用户 Phase 2 指令收窄为「只做一个黄金样本」） |
| **E2a** | 技能 1~3 信封化（每批 1~3 技能，独立审查） | 每批 `-L ci` 绿 + D-A1/A2/A10 |
| **E2b** | 技能 4~6 信封化 | 同上 |
| **E2c** | 技能 7~9 信封化 | 同上 |
| **E3** | `--gen-specstate` 生成器 + `--check` 幂等/确定性 | D-A7 达标 |
| **E4** | 生成物接管 `SpecStateMappingTests`（1~12 全覆盖） | D-A8 达标 |
| **E5** | flag 二分收尾（D6）+ 机制族基元盘点（D4 门槛） | D-A2/A6 达标 |
| **E6** | 回归 + 性能采证 + 复审报告 | D-A10/A11/A12；结论 `提交` |

批次约束（沿用 `skill-followup-plan.md:210-214`）：`build.bat` RelWithDebInfo 0 警告；每批 `ctest -L ci` 全绿 + 数据脚本 PASS；每批出复审报告；禁止 `debug` 构建；不提交未经用户确认的 commit。

---

## 12. 风险与未决

### 12.1 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 「生成器 × 手写」双源漂移 | 中 | D-A7/D-A8 强制 `--check` + 单测对齐运行期 `ReadPoints` |
| 漏迁 flag 导致行为悄然变化 | 中 | 每技能信封化后对照 `-L functional` 技能专项测试；逐 flag 标注 |
| 信封化使部分文件**行数上升**引发误判 | 中 | D-A12 明确行数仅观测 |
| 过早抽机制族基元制造跨技能耦合 | 中高 | D4 采用 ≥3 次逐字重复门槛 |
| 泛型 `ResolveSpecState<State>` 增加编译期复杂度 | 低 | `State` 限 POD + `constexpr` 表 |
| 技能 10~12 回归 | 低 | D7 仅纳入回归不变量 |
| 生成器输入源不明确（D9） | 未决 | E3 前先盘点技能树数据格式 |

### 12.2 未决问题

1. `--gen-specstate` 的输入源与产物落点（D9）。
2. 机制族基元的族划分边界（待 E5 盘点，当前仅有 `SevenStarSlashShared` 的零星雏形）。
3. 技能 1~9 是否存在**跨技能**读点（如技能 1 读技能 8 的 814 节点：`FlowingThrust.cpp:406-408`），信封化时是否需统一「跨技能节点读点」基元——**未决**。
4. 行数观测是否纳入 CI 报告模板。

---

## 13. 证据索引

| 事实 | 证据 |
|---|---|
| 读点/点亮/机制数值 helper | `src/game/foundation/components/SkillPointAccess.hpp:14-43` |
| 技能 10 SpecState + 绑定表 + Resolve | `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:288-405` |
| 技能 10 共享效果基元 | `SevenStarSlashShared.hpp:71-188` |
| 技能 10 非点读节点叠加 | `SevenStarSlashShared.hpp:400-404` |
| 技能 1 散装读点反例 | `src/game/systems/skill/behaviors/FlowingThrust.cpp:342-371` |
| 技能 1 流血爆发特化口径 | `FlowingThrust.cpp:434-462` |
| 技能 1 跨技能 814 读点 | `FlowingThrust.cpp:406-408` |
| profile 样板（行为层 9 处） | `FlowingThrust.cpp:97-107`、`RendingWave.cpp:82-90`、`BladeFormation.cpp:95-103`、`BladeWard.cpp:66-76`、`BladeBoomerang.cpp:97-105`/`:293`、`InfiniteBlades.cpp:90`/`:142-150`、`SwordArray.cpp:67-76`、`MindBlade.cpp:39-45`、`PhantomTrance.cpp:69-82` |
| 交付参数结构 | `src/game/foundation/components/SkillDefs.hpp:625-699` |
| 存档字段（无 SpecState） | `SkillDefs.hpp:717-729` |
| 触发规则去重 | `src/game/foundation/components/TriggerRuleComponent.hpp:26,63-90` |
| Baker 入口与 case | `src/game/systems/skill/SkillSpecializationBaker.cpp:14-20`、`:62-113` |
| 技能 3 331 门控 | `src/game/systems/skill/behaviors/BladeFormation.cpp:746-750` |
| 技能 4 反击口径 | `src/game/damage/DamageInterceptors.hpp:55` |
| 技能 2/7/8/9 交付推导 | `SkillSpecializationBaker.cpp:68-73`、`:92-95`、`:97-103`、`:104-110` |
| 前置设计裁决方案 B + DoD + 残余 #10/#11 | `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md` §4.1/§4.7/§11 |
| 前置计划已落地 | `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` |
| Wave E 定义 | `docs/plans/2026-09-13-skill-followup-plan.md:188-194` |
| backlog A-01 | `docs/plans/2026-09-12-skill1-9-followup-backlog.md:162` |
| 09-07 评审（≤100 行基线） | `docs/reviews/2026-09-07-modular-skill-archetypes-implementation-review.md` |
| 09-13 评审 | `docs/reviews/2026-09-13-skill-followup-review.md` |
