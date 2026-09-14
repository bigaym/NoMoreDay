# 技能1 Slow/Chill legacy buff 收编 ailment 契约设计 (B2-18 Draft for Review)

- 日期：2026-09-13
- 状态：**已落地（2026-09-13，D1=B / D2-D7 保守默认）**——落地记录见 §11
- 范围：backlog `B2-18`——技能1 `ApplyFrostSlowDebuff` / node 173 `FrostChill` 的 Slow/Chill legacy buff 是否/如何收编 `ailment_contracts.json` + `AilmentEngine`，以及「与 HazardSystem 同型」的统一处置
- 权威活清单：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:148`
- 前置文档：`docs/workflows/design.md`；`docs/plans/2026-09-13-skill-followup-plan.md`（§2.2 / C5.2 PARTIAL 记录）；`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（T5.1 `[~]`）
- 上游评审：`docs/reviews/2026-09-07-skill1-specialization-nodes-review.md` §6.5-3；`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`（F1/F7）；`docs/reviews/2026-09-13-final-review-skill-abstraction-b2-cleanup.md`
- 草案阶段约束：调研阶段不改 backlog / plan / GDD / 数据 / 源码 / 测试；不改动记录见 §11（草案完成后按用户拍板执行了方案 B）

---

## 1. 问题陈述与事实核验 (Problem Statement & Fact Check)

### 1.1 条目原文

> `B2-18` [P2] 技能1：Slow/Chill legacy buff 收编 ailment 契约（契约补注册时统一处理，与 HazardSystem 同型）— 源：skill1 §6.5-3（PARTIAL：`ApplyFrostSlowDebuff` 未走 ailment 契约，见计划 §2.2）

计划 §2.2 记录的三条约束：

1. `ApplyFrostSlowDebuff` 手工 `AddOrRefresh`，未走 ailment 契约；
2. `AilmentType::Slow` 尚未注册；
3. 3 处既有用例锁死 buff `id="FrostSlow"` 语义。

### 1.2 逐条事实核验（取证方法：`codebase-memory-mcp` 图工具 + 同步 `rg` + Read）

| # | 核验项 | 结论 | 证据（`文件:行`） |
|---|--------|------|-------------------|
| F1 | 条目/计划引用的 `FlowingThrustNodes.cpp:471` 是否存在 | **反例：文件路径错误** | `src/game/systems/skill/behaviors/FlowingThrustNodes.cpp` **不存在**；真实站点为 `tests/functional/FlowingThrustNodes.cpp:470-494`（`TEST_CASE` 在 `:472`，`:470-471` 为注释）。`FlowingThrustNodes` 是 node 常量命名空间（行为层 include），不是独立 cpp |
| F2 | `AilmentType::Slow` 是否真的「未注册」 | **枚举存在、契约未注册；二者不同** | 枚举：`src/game/contracts/CombatEvents.hpp:64-76`（`Slow` 为第 8 个值）；适配：`AilmentEngine.cpp:80`（ToString）、`:107`（FromString）、`:226`（→`BuffKind::Slow`）、`:504-506`（→`BuffType::SpeedDown`）**均已就绪** |
| F3 | 「未注册」的真正阻塞点 | **`AilmentRegistry` 的 builtin 表 + JSON 表都缺 `Slow`** | `AilmentEngine.cpp:407-444 LoadBuiltins()` 只对 Poison/Ignite/Bleed/Chill/Freeze/Shock 调 `setContract`；`assets/data/ailment_contracts.json:3-76` 只有 6 条，无 Slow。故 `Find(AilmentType::Slow)`→`nullptr`（`:311-317`），`AilmentApplier::Apply` 在 `:606-609` 直接 `return false` → **静默无效** |
| F4 | ailment 契约能否表达「30% 移速减速」 | **不能——这是比「未注册」更深的阻塞点（核心发现）** | `AilmentEngine.hpp` `struct AilmentContract` 无任何移速/慢速字段；`BuildAilmentEffect()`（`AilmentEngine.cpp:233-260`）从不写 `.modifiers`；`:251-252` `effect.tick_damage = max(0.0f, magnitude)` → 契约的 `magnitude` 语义 = **每 tick 伤害**，不是移速比例 |
| F5 | 若把 172 的 0.30 直接塞进 `AilmentApplier` 会发生什么 | **反直觉：会产生一个 0.3 点/秒的 Cold DoT，而不是减速** | `AilmentEngine.cpp:252` + `:786`（`tick_damage<=0` 才跳过）+ `:808-819`（按 `tick_damage`/`damage_tag` 造 `DamageRequest`）；Chill 契约已证明该路径是伤害路（`ailment_contracts.json:52-63`，`damage_tag=Cold`） |
| F6 | `AilmentAdapter::TryMapLegacyBuff` 对 legacy `SpeedDown` 的归类 | **反直觉：映射到 `Chill` 而非 `Slow`** | `AilmentEngine.cpp:476-477`（`case BuffType::SpeedDown: return AilmentType::Chill;`）。区分 `Slow`/`Chill` 的只有 `kind`（`BuffKind::Slow` vs `BuffKind::Chill`）。注意这是 legacy fallback 分支，`managed_ailment==true` 时走结构化字段（`:449-453`） |
| F7 | legacy `FrostSlow` 是否会被 ailment tick 系统误处理 | **不会（靠 `tick_damage<=0` 兜住）** | `FrostSlow` 不设 `tick_damage`（`FlowingThrust.cpp:72-88`），`Tick()` 在 `AilmentEngine.cpp:786` 因 `tick_damage<=0.0f` 跳过 |
| F8 | node 173 是否已部分收编契约 | **是（半收编先例）** | `FlowingThrust.cpp:582-587` 读 `AilmentRegistry::Get().Find(AilmentType::Chill)->max_stacks`，缺失时回退 `skill_mechanics` 的 `chill_stacks_to_freeze`；数据 `ailment_contracts.json:54`（Chill `max_stacks=3`）与 `skill_mechanics.json:38`（`3.0`）当前等值 |
| F9 | §2.2 三条约束的证据 | **三条均成立** | 手工载体 `FlowingThrust.cpp:72-88`（`AddOrRefresh` at `:87`）；Slow 未注册见 F3；id 锁见 F11/F12/F13 |
| F10 | 3 处测试的真实断言 | 见 §1.3 | `tests/functional/FlowingThrustNodes.cpp:472-494`；`tests/unit/SkillSpecializationBakerTests.cpp:885-905`；`tests/unit/SkillSpecializationBakerTests.cpp:1257-1316` |
| F11 | HazardSystem 是否真是「同型」 | **是，但 id/kind 不同** | `HazardSystem.cpp:373-392 ApplyChillDebuff`：手工 `AddOrRefresh`，`id="frozen_chill"`、`type=SpeedDown`、`kind=BuffKind::Chill`、`modifiers=[-slowAmount*100 MoveSpeed PercentAdd]`；唯一调用点 `HazardSystem.cpp:176`（`ApplyChillDebuff(registry, target, 3.0f, 0.5f)` → -50%）。**无任何测试引用 `frozen_chill`/`ApplyChillDebuff`**（`rg` 仅命中 src 与评审文档） |
| F12 | `id="FrostSlow"` 的下游消费者 | 有，且不限于测试 | `DamageConditions.cpp:12-16`（`SpeedDown` 或 `kind==Slow` → `Controlled`）；`RendingWave.cpp:710`（node 275 扩散：`type==SpeedDown\|\|Freeze` → 施加 `AilmentType::Chill`）；`BoomerangDeliverySystem.cpp`（`type==SpeedDown\|\|Freeze`）；`FlowingThrust.cpp:410-412`（`victimHasCold`：`kind ∈ {Chill,Freeze,Slow}`）；`BuffRegistry.cpp`（`SpeedDown` → UI 图标「速 ↓」/RED/「减少移动速度。」） |
| F13 | 是否存在把 id 枚举化的既有设施 | **有，且有慢速 buff 先例** | `src/game/foundation/data/BuffIds.hpp:21-99`（`BuffId` + `kBuffIdNames`，注释明示「逐字符保留以兼容存档」）；先例：`SwordArraySlow`(`"array_slow"`)、`SwordArrayConnectionSlow`(`"SwordArrayConnectionSlow"`)——**它们同样未收编 ailment 契约**，说明「移速减速 ≠ ailment 契约」是仓库既有共识 |

### 1.3 三处测试的具体断言（迁移影响面基准）

**测试 A — `tests/functional/FlowingThrustNodes.cpp:472-494`**（`TEST_CASE("[Unit] Skill - Flowing Thrust 172 Freezing Wind applies Slow-class 30% slow")`）

```
hitFunc(registry, player, victim, Tag::Cold, false);
const auto *slow = fx->GetByKind(BuffKind::Slow);   // :487
CHECK(slow->id == "FrostSlow");                     // :489  字符串 id 锁
CHECK(slow->type == BuffType::SpeedDown);           // :490  legacy 类型锁
CHECK(slow->modifiers[0].type == StatType::MoveSpeed);   // :492
CHECK(slow->modifiers[0].value == doctest::Approx(-30.0f)); // :493
```

**测试 B — `tests/unit/SkillSpecializationBakerTests.cpp:885-905`**（父 `TEST_CASE` 在 `:777`；SUBCASE `"Node 172 FreezingWind explosion hit carries Cold tag and applies 30% slow"`）

```
CHECK((capturedTags & Tag::Cold) != Tag::None);     // :893
const auto *slow = fx->Get("FrostSlow");            // :899  字符串 id 锁
REQUIRE(slow->type == BuffType::SpeedDown);         // :901
REQUIRE_FALSE(slow->modifiers.empty());             // :902
CHECK(slow->modifiers[0].value == doctest::Approx(-30.0f)); // :903
CHECK(slow->modifiers[0].type == StatType::MoveSpeed);      // :904
```

> 注释 `:895-896` 明写「Slow 异常未注册 ailment 契约，与 `HazardSystem::ApplyChillDebuff` 的既有减速机制保持一致」——即该测试把 legacy 行为当**契约**在锁。

**测试 C — `tests/unit/SkillSpecializationBakerTests.cpp:1257-1316`**（`TEST_CASE("[Unit] FlowingThrust - 175 spread slow keeps SpeedDown type on refresh")`）

```
vfx.AddOrRefresh({.id="FrostSlow", .type=SpeedDown, .kind=Slow, ...});   // 主目标预置 :1282-1288
afx.AddOrRefresh({.id="FrostSlow", .type=SpeedDown, .kind=Slow, ...});   // 传染目标预置 :1297-1303
hitFunc(registry, player, victim, Tag::Cold, false);                     // :1307
const BuffEffect *spread = fxA->GetByKind(BuffKind::Slow);               // :1311
CHECK(spread->id == "FrostSlow");                                        // :1313
CHECK(spread->type == BuffType::SpeedDown);                              // :1314
CHECK(spread->kind == BuffKind::Slow);                                   // :1315
```

该用例是 F7 回归护栏：证明同一 id 的多个创建点在 `AddOrRefresh` 刷新时不会把 `type` 清成 `None`（`Buff.hpp:205-206` 刷新会覆盖 `type`/`kind`）。

### 1.4 反直觉点汇总

1. **「未注册」不等于「枚举缺失」**：`AilmentType::Slow` 及全部适配函数早已就位，缺的只是两张注册表（F2/F3）。
2. **真正的阻塞点是契约模型不表达移速**：`magnitude` = 每 tick 伤害；慢性收缩进契约会产生一个 0.3 点/秒的 Cold DoT（F4/F5）。
3. **`SpeedDown` 的 legacy 归类是 `Chill` 而不是 `Slow`**（F6）。
4. **条目/计划引用的源码路径不存在**（F1），实际是功能测试文件。
5. **仓库既有慢速 buff（`array_slow` 等）都没进 ailment 契约**，说明这并非历史疏漏，而是分层边界（F13）。
6. **`ApplyFrostSlowDebuff` 的价值恰恰在契约之外**：它产出的是 `StatModifier`（移速），归 `AttributePipeline` / `Modifier` 体系，不属于 ailment（DoT/状态身份）体系。

---

## 2. 权威源判定 (Source of Truth)

| 关注点 | 权威源 | 说明 |
|--------|--------|------|
| ailment 身份 / legacy 映射 | `src/game/contracts/CombatEvents.hpp`（`AilmentType`）+ `AilmentEngine.cpp::AilmentAdapter` | 代码为准；`AilmentTypeToString/FromString/ToLegacyBuffType/BuffKindFromAilment` 是唯一映射口 |
| ailment 语义（层数上限/刷新/覆盖/免疫/tick） | `assets/data/ailment_contracts.json`（数据）+ `AilmentEngine.cpp:407-444 LoadBuiltins()`（兜底） | `LoadFromFile` 先 builtin 再 JSON 覆盖（`:327-400`）；两处必须同值，否则「文件缺失回退」会漂移 |
| 技能数值（减速幅度/时长） | `assets/data/skill_mechanics.json`（`172`/`173`/`175`） | 见 §6.1；当前 `slow_magnitude=0.3` → modifier `-30.0` |
| 移速减速的运行时真值 | `BuffEffect.modifiers[]`（`Buff.hpp:105`）+ `ActiveEffectsComponent` | 由 `AttributePipeline` / Stats 消费；**ailment 契约当前不是权威源，也不应在本条变更它** |
| legacy 身份 `id` 字符串 | 既有存档 + 3 处测试 + `BuffIds.hpp` 兼容约定 | `id="FrostSlow"` 已入存档（`BuffEffect::to_json` 序列化 `id`，`Buff.hpp:137-154`）；`id="frozen_chill"` 同 |
| UI/受控判定等消费 | `BuffType::SpeedDown` | `BuffRegistry.cpp` 图标、`DamageConditions.cpp:12-16` |

**结论**：ailment 契约的权威域是「异常身份 + DoT 语义」；`Slow` 这一词在仓库里横跨两个域——**作为异常身份（`AilmentType::Slow`）** 与 **作为移速 debuff（`BuffType::SpeedDown` + `modifiers`）**。B2-18 的争议本质是「是否让契约域吞并移速域」。收编设计必须先裁定这条边界。

---

## 3. 目标与非目标

### 3.1 目标

1. 消除 `AilmentType::Slow`「枚举存在但无契约」造成的**静默失效/向前兼容死分支**（计划 §2.2 与 B1 复审都点名）。
2. 若采纳统一，给出 **FlowingThrust（172/175）与 HazardSystem（冰冻球减速）单源构建**方案，杜绝 F7 型「同 id 多创建点 `type/kind` 漂移」。
3. 保持玩家可见行为不变（`-30%` / `-50%` 数值、时长、刷新语义、UI 类型），或对任何有意变更给出显式裁决点。
4. 保持 3 处回归用例的护栏价值（可迁移断言，不可删护栏）。

### 3.2 非目标

- 不改造 ailment 契约为泛用 buff 框架（不新增通用 `StatModifier` 载体）。
- 不收编 `array_slow` / `SwordArrayConnectionSlow` / `BladeWard` shock-slow / `HeavenlySwordDescent` 等其他移速减速（超出本条范围，除非用户另有指令）。
- 不做热路径性能优化（无性能诉求）。
- 不动 `DamagePipeline` 的 `FrostSlow` 元素位（`DamagePipeline.cpp:1608`）与 172 冻结增伤（`frozen_more_mult`）。

---

## 4. 方案选项 (Options)

### 方案 A — 完整收编：扩展 ailment 契约为可携带移速修正

把「移速减速」纳入 `AilmentContract` + `AilmentApplyRequest`，两条链路全部改走 `AilmentApplier`，`id` 改为 `ailment:Slow`。

**改动面**

1. `AilmentEngine.hpp`：`AilmentContract` 增 `float move_speed_pct = 0.0f;`（或 `BuffKind`→modifier 描述）；`AilmentApplyRequest` 增 `float slow_pct = 0.0f;`（因为慢速幅度是**每次施放的数值**，不是契约常量）。
2. `AilmentEngine.cpp`：`BuildAilmentEffect` 在 `slow_pct>0` 时 push `MoveSpeed/PercentAdd` modifier；`Apply` 的刷新分支（`:698-721`）目前**完全不触碰 `modifiers`**，必须新增 modifier 重建/合并逻辑；`SyncAilmentSnapshot`+`Tick` 必须对 Slow 关闭伤害（`damage_tag=None` 或显式 `tick_damage=0`）；`LoadBuiltins`+`AilmentTypeFromString` 路径补 Slow。
3. `assets/data/ailment_contracts.json`：新增 Slow 条目（`damage_tag` 需为「无」语义，而现有 parser 用 `TagFromString`/`DefaultDamageTag`，需确认能否表达 `None`）。
4. `FlowingThrust.cpp:72-88` 删除 helper，改 `AilmentApplier::Apply`；`:572`/`:782` 调用点改造；node 173 `FrostChill`（`:589-632`）与 `Frozen`（`:623-632`）需一并决定是否收编（否则遗留手工 Cold 载体）。
5. `HazardSystem.cpp:373-392` 改走 `AilmentApplier`（`id="frozen_chill"` → `ailment:Slow`）。
6. 3 处测试断言改写（`FrostSlow` → 契约 id；`Get("FrostSlow")` → 结构化查询）。
7. 存档兼容：既有存档里的 `id="FrostSlow"` / `frozen_chill` 将不再被新代码生成，读档后是否需迁移？

**风险**：高。触及战斗核心 `AilmentEngine`（Poison/Ignite/Bleed/Chill/Freeze/Shock 全线回归面）、存档格式、`DamageConditions` 受控判定、275 扩散的 `SpeedDown` 归因。且引入「契约同时管 DoT 与统计修正」的双语义，与仓库既有慢速 buff 分层（F13）冲突。

**可逆性**：中。代码可整体 revert；但若已生成 `ailment:Slow` 存档/测试基线，回退需迁移。

**DoD 满足度**：如果 DoD =「`ApplyFrostSlowDebuff` 不再手工 `AddOrRefresh`」→ 满足；如果 DoD =「行为不变」→ 需要额外大量护栏。

### 方案 B — 保留 legacy 载体，统一「构建点 + 契约注册」（推荐）

承认「移速减速」不属 ailment 契约权威域（§2/F13），但消除本条真正的债：**同一 `FrostSlow` 身份散落多个创建点** + **Slow 契约缺失导致的静默失效**。

**改动面**

1. `assets/data/ailment_contracts.json` + `AilmentEngine.cpp:407-444`：补注册 `Slow`（与 Chill 同型：`max_stacks`、`refresh_policy=Refresh`、`overwrite_policy=Strongest`、`damage_tag` 取无伤害语义、`legacy_buff_type=SpeedDown`），使 `Find(Slow)!=nullptr`、B1 复审点名的「不可达向前兼容分支」变为可达且可校验。
2. 新增单一构建口（建议放 `AilmentAdapter`，它已拥有 legacy 映射；或放 `BuffFactory` 头）：`BuildMoveSpeedDebuff(AilmentType, id, name, slow_pct, duration, source_skill_id)`，内部统一写 `type=ToLegacyBuffType(type)`、`kind=`、`modifiers=[-slow_pct*100 MoveSpeed PercentAdd]`、`is_debuff`、`source_skill_id`。
3. `FlowingThrust.cpp:72-88` 的 `ApplyFrostSlowDebuff` 改为薄封装调用该构建口（`id` / `kind` / `type` 不变 → 3 处测试零改动）；`:572`/`:782` 调用点不变。
4. `HazardSystem.cpp:373-392` 改为调用同一构建口（`id="frozen_chill"` 保留，避免跨技能刷新语义变更；`kind` 从 `Chill` 是否改为 `Slow` 见决策点 D4）。
5. node 173 `FrostChill`（`:589-632`）可选纳入同一构建口（当前它已自建设 `type/kind/modifiers`，与 helper 重复）。

**风险**：低。不触碰 `AilmentApplier` 主路径、不改伤害、不改存档、不改消费方；唯一行为差异是注册 `Slow` 后若未来有代码走 `AilmentApplier(Slow)` 会被接受（当前无调用方）。

**可逆性**：高。改动局部、无格式迁移。

**DoD 满足度**：如果 DoD =「契约补注册」→ 满足；如果 DoD =「`ApplyFrostSlowDebuff` 不再手工 `AddOrRefresh`」→ **不满足**（载体仍手工）。这是本方案与 A 的分歧核心，必须由用户裁定 DoD 口径。

### 方案 C — 最小改动：仅补注册，不动代码

只做 `ailment_contracts.json` + `LoadBuiltins` 的 Slow 条目，`ApplyFrostSlowDebuff`/`HazardSystem` 原样保留。

**改动面**：2 个文件、约 10 行数据 + 1 次 `setContract` 调用。
**风险**：极低。
**可逆性**：极高（回退即删条目）。
**DoD 满足度**：条目标题「契约补注册时统一处理」中的「补注册」满足，「统一处理（与 HazardSystem 同型）」不满足——两个创建点仍各自手写。

### 4.4 对比矩阵

| 维度 | A 完整收编 | B 统一构建口 | C 仅补注册 |
|------|-----------|-------------|-----------|
| 消除「Slow 未注册」 | 是 | 是 | 是 |
| 消除「多创建点漂移」 | 是 | 是 | 否 |
| 触碰 `AilmentApplier` 主路径 | 是（刷新/modifier/tick 多处） | 否 | 否 |
| 存档 / id 变更 | 是（`ailment:Slow`） | 否 | 否 |
| 3 处测试改动 | 需改写 | 零改动 | 零改动 |
| 玩家可见行为风险 | 中高 | 无 | 无 |
| 与仓库慢速分层一致性 | 冲突（新增双语义） | 一致 | 一致（但债未清） |
| 工作量（估） | 2–4 人日 + 全回归 | 0.5–1 人日 | 0.2 人日 |
| 可逆性 | 中 | 高 | 极高 |

---

## 5. 需要用户拍板的决策点 (Decision Points)

> 编号 D1–D7。每项给出选项、建议与依据；请逐项裁定后再进入 planning。

**D1｜收编深度（决定 B2-18 的 DoD 口径）**
- 选项：A 完整收编 / **B 统一构建口 + 补注册** / C 仅补注册
- 建议：**B**
- 依据：`AilmentContract` 的 `magnitude` 是每 tick 伤害（F4/F5），移速修正属 `StatModifier` 域；仓库既有慢速 buff（`array_slow` 等）均未入契约（F13），强行收编会制造「契约既管 DoT 又管移速」的双语义。B 清掉条目真正指认的债（多创建点 + 未注册），且零玩家可见行为风险。

**D2｜`id` 是否迁移**
- 选项：a) 保留 `"FrostSlow"` / `"frozen_chill"`；b) 统一为 `"ailment:Slow"`（A 方案）；c) 枚举化为 `BuffId::FrostSlow`
- 建议：**a**（B/C 方案默认）；若希望按 B2-15 同型推进，可另立 c 作为后续独立条目
- 依据：`id` 已入存档（`Buff.hpp` 序列化）；3 处测试 + 275 扩散 + UI 均依赖字符串/`SpeedDown`（F12）；改名属高影响、低收益。

**D3｜HazardSystem 是否同批收编，且是否与 `FrostSlow` 合并 id**
- 选项：a) 同批、仍各用各的 id；b) 同批、合并为同一 id；c) 不同批
- 建议：**a**
- 依据：合并 id 会让「冰冻球减速」与「172 减速」在 `AddOrRefresh` 下互相刷新（改变时长/幅度语义，参见 B1 复审 F1 的同类事故）；同批共享构建口即可获得单源收益，且 `frozen_chill` 无测试锁（F11），改动成本低。

**D4｜`HazardSystem` 的 `kind` 是否从 `Chill` 改为 `Slow`**
- 选项：a) 保持 `BuffKind::Chill`；b) 改为 `BuffKind::Slow`
- 建议：**保持 a**（仅共享 builder，不改字段）
- 依据：`FrostChill`(kind=Chill) / `frozen_chill`(kind=Chill) / `FrostSlow`(kind=Slow) 目前三者语义不同；改 `kind` 会改变 node 173 的 `GetByKind(Chill)` 叠层判定与 275 归因。若统一种类，须单列裁决与护栏。

**D5｜Slow 契约的伤害语义（防 DoT）**
- 选项：a) `damage_tag=None` + `tick_interval` 极大 + 文档注明「身份用途」；b) `tick_interval` 极大；c) 不注册（回到 C 的缺口）
- 建议：**a**，并在 `AilmentEngine`/JSON 注释中显式标注「Slow 为身份型契约，不产生 DoT」
- 依据：契约 parser 用 `TagFromString`/`DefaultDamageTag`（`AilmentEngine.cpp:378-384`、`:512-517`）；`Tick()` 依赖 `damage_tag != None`（`:558-559`）。必须验证 `None` 在 `DefaultDamageTag(Slow)`/`damage_tag` 处能被表达，否则任何未来 `AilmentApplier(Slow)` 都会打伤害（F5）。

**D6｜测试护栏迁移**
- 选项：a) 3 处测试原样保留（B/C 方案）；b) 改写断言为契约查询（A 方案）；c) 保留 A 断言并新增契约等价断言
- 建议：**a**（B 方案）；若最终选 A，则 **c**——不要删护栏，改为「契约 id + `SpeedDown` legacy 对齐」双断言。

**D7｜是否顺带做「Slow 契约消费点」接线**
- 选项：a) 只注册，不接线；b) 把 `FlowingThrust.cpp:410-412` 的 `victimHasCold` 改为「`AilmentType::Slow` 托管 or legacy」双路径
- 建议：**a**（保持最小面）
- 依据：`:410-412` 已按 `BuffKind` 判定（含 `Slow`），legacy 载体现行可用；提前接线无收益且扩大回归面。

---

## 6. 逐行 before/after 草案（以推荐方案 B 为准）

> 以下为**草案**，供评审；落地须另出 planning。实际行号以实施时文件为准。

### 6.1 数据：`assets/data/ailment_contracts.json`

**before**（`:64-76`）
```json
    {
      "type": "Shock", ...
    }
  ]
}
```

**after**（在 Shock 之后追加，保持「Chill 同型」；具体 `damage_tag` 取值由 D5 裁定）
```json
    {
      "type": "Shock", ...
    },
    {
      // B2-18：Slow 为「身份型」异常——用于异常身份/受控归类，不产生 DoT。
      // 移速修正仍由 BuffEffect.modifiers 承载（见 BuildMoveSpeedDebuff）。
      "type": "Slow",
      "max_stacks": 1,
      "refresh_policy": "Refresh",
      "overwrite_policy": "Strongest",
      "immunity_and_resistance": 1.0,
      "tick_interval": 1.0,
      "damage_pool_policy": "PerStack",
      "base_duration": 2.5,
      "damage_tag": "<由 D5 裁定：无伤害语义>",
      "legacy_buff_type": "SpeedDown"
    }
```

### 6.2 代码：`src/game/systems/combat/AilmentEngine.cpp` `LoadBuiltins()`（`:407-444`）

**before**
```cpp
  setContract(AilmentType::Shock, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 3.0f,
              Tag::Lightning, BuffType::Shock);
}
```

**after**
```cpp
  setContract(AilmentType::Shock, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 3.0f,
              Tag::Lightning, BuffType::Shock);
  // B2-18：Slow 身份型契约（身份/受控归类），不产生 DoT；
  // 移速修正由 BuffEffect.modifiers 承载。
  setContract(AilmentType::Slow, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 2.5f,
              /*tag=*/<D5 裁定>, BuffType::SpeedDown);
}
```

### 6.3 单一构建口（建议置于 `AilmentEngine.hpp`/`AilmentAdapter`）

**新增（after）**
```cpp
// B2-18：移速减速载体的唯一构建口。ailment 契约不承载 StatModifier（其
// magnitude 语义为每 tick 伤害），故移速修正统由此处构造，避免同 id 多创建点
// 出现 type/kind 漂移（见 2026-09-12 评审 F1/F7）。
[[nodiscard]] BuffEffect AilmentAdapter::BuildMoveSpeedDebuff(
    AilmentType identity, std::string_view id, std::string_view name,
    BuffKind kind, float slowPct, float duration, int sourceSkillId);
```

**实现要点**：`.type = ToLegacyBuffType(identity)`、`.kind = kind`、`.modifiers = {{.value=-slowPct*100, .type=StatType::MoveSpeed, .mode=ModifierMode::PercentAdd}}`、`.is_debuff=true`、`.source_skill_id=sourceSkillId`。

### 6.4 `src/game/systems/skill/behaviors/FlowingThrust.cpp`（`:72-88`）

**before**
```cpp
  BuffEffect slow{
      .id = "FrostSlow",
      .name = "Frost Slow",
      .type = systems::AilmentAdapter::ToLegacyBuffType(AilmentType::Slow),
      .kind = BuffKind::Slow,
      ...
  };
  slow.modifiers.push_back({...});
  registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(slow);
  registry.get_or_emplace<StatsDirty>(target);
```

**after**（行为与字段完全等价；`id/type/kind/modifier` 逐字不变）
```cpp
  auto slow = systems::AilmentAdapter::BuildMoveSpeedDebuff(
      AilmentType::Slow, "FrostSlow", "Frost Slow", BuffKind::Slow,
      slowMagnitude, duration, /*sourceSkillId=*/0);
  registry.get_or_emplace<ActiveEffectsComponent>(target).AddOrRefresh(slow);
  registry.get_or_emplace<StatsDirty>(target);
```

> 注：`slowMagnitude` 语义为 0–1 比例（数据 `slow_magnitude=0.3` → `-30.0`），与现状一致。

### 6.5 `src/game/systems/combat/HazardSystem.cpp`（`:373-392`）

**before**
```cpp
  BuffEffect chill{};
  chill.id = "frozen_chill";
  chill.name = "冰冻减速";
  chill.type = BuffType::SpeedDown;
  ...
  chill.modifiers.push_back({ ..., StatType::MoveSpeed, PercentAdd });
  effects.AddOrRefresh(chill);
```

**after**（`id`/`name`/`kind` 由 D3/D4 裁定，默认不变）
```cpp
  auto chill = AilmentAdapter::BuildMoveSpeedDebuff(
      AilmentType::Chill, "frozen_chill", "冰冻减速", BuffKind::Chill,
      slowAmount, duration, /*sourceSkillId=*/0);
  effects.AddOrRefresh(chill);
```

### 6.6 可选：node 173 `FrostChill`（`:589-632`）

当前 `FrostChill` 自建设 `type=SpeedDown`/`kind=Chill`/`modifier=-20%`，与 helper 重复。推荐**同批纳入**构建口（`id="FrostChill"`、`name="Frost Chill"`、`kind=BuffKind::Chill`、`slowPct=chillSlowPct`），但 `max_stacks` 仍需保留（构建口需支持传 `maxStacks`），或保持现状以免扩大面——见 D7 的延伸。**此项列入 planning 时再定。**

### 6.7 测试（仅 A 方案需要；B/C 零改动）

- `tests/functional/FlowingThrustNodes.cpp:489`：`CHECK(slow->id == "FrostSlow")` 改为按契约 id（A）或保留（B）。
- `tests/unit/SkillSpecializationBakerTests.cpp:899`：`fx->Get("FrostSlow")` 改为 `GetByKind(BuffKind::Slow)` + 契约断言。
- `tests/unit/SkillSpecializationBakerTests.cpp:1257-1316`：预置载体改走新构建口；`CHECK(spread->id == ...)` 随 D2 调整。

---

## 7. 验收标准与验证方法 (Acceptance & Verification)

### 7.1 验收标准（推荐方案 B）

1. `AilmentRegistry::Get().Find(AilmentType::Slow) != nullptr`，且 `builtins` 与 JSON 条目**逐字段等值**（max_stacks / refresh_policy / overwrite_policy / tick_interval / damage_tag / legacy_buff_type）。
2. `AilmentType::Slow` 契约**不产生 DoT**：施加后 `Tick()` 不结算伤害（T4 断言）。
3. 场景等价：node 172 命中 Cold → 目标 `GetByKind(BuffKind::Slow)` 的 `type==SpeedDown`、`modifiers[0]=={-30.0, MoveSpeed, PercentAdd}`、`id=="FrostSlow"`、时长 2.5s；node 175 传染后刷新不丢 `type/kind`（沿用测试 C）。
4. `HazardSystem` 冰冻球爆炸对玩家仍为 -50% 移速、3.0s，且与 172 的减速**互不刷新**（D3=a）。
5. 无新增 `id.find(...)` / 字符串比较热路径。
6. 三处既有测试**不改断言即通过**（B 方案）。

### 7.2 验证方法（需重跑的脚本与测试标签）

> 以下为**建议验证清单**，本次不执行。

| 类别 | 命令 / 标签 | 目的 |
|------|-------------|------|
| 构建 | `build.bat`（`RelWithDebInfo`） | 编译通过、零新增告警 |
| 单元 | `ctest -C RelWithDebInfo -L unit` | `AilmentEngineTests`（`tests/unit/AilmentEngineTests.cpp`）回归 |
| 功能 | `ctest -C RelWithDebInfo -L skill` | 技能行为回归 |
| 功能（专项） | `ctest -C RelWithDebInfo -R "FlowingThrust"` | 测试 A/C + 173 用例 |
| 特殊 | `ctest -C RelWithDebInfo -R "SkillSpecializationBakerTests"` | 测试 B/C |
| CI 套件 | `ctest -C RelWithDebInfo -L ci` | 全量门禁 |
| 数据脚本 | `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` | 契约数据一致性（若触及 `skill_contracts_compact.json`） |
| 数据脚本 | `python scripts/gen_skill_mechanics_schema.py --check` | 机制键 schema（若触及 `skill_mechanics.json`） |
| 数据脚本 | `python scripts/validate_json.py`（或仓库既有 JSON 校验入口） | `ailment_contracts.json` 语法/字段 |

### 7.3 建议新增回归用例（planning 阶段落地）

- T1｜契约注册：`Find(Slow)!=nullptr` 且字段与 JSON 等值。
- T2｜无 DoT：对目标施加 `Slow` 后 tick 3s，`HealthComponent` 不变。
- T3｜172 全等：字段与时长逐项等于现状（可由测试 A 覆盖，补时长断言）。
- T4｜HazardSystem 等价：`frozen_chill` 幅度/时长/`kind` 与改前一致。
- T5｜互不刷新：同时施加 172 减速与冰冻球减速，两者独立存在（D3=a 的护栏）。

---

## 8. 影响面矩阵

| 系统 / 文件 | 是否受影响 | 说明 |
|-------------|-----------|------|
| `src/game/systems/skill/behaviors/FlowingThrust.cpp` | 是（创建点改造） | 172 `:572`、175 `:782`；node 173 `:589-632`（可选项） |
| `src/game/systems/combat/HazardSystem.cpp` | 是（创建点改造） | 冰冻球减速 `:373-392` |
| `src/game/systems/combat/AilmentEngine.{hpp,cpp}` | 是（补注册 / 新增构建口） | `LoadBuiltins`、`AilmentAdapter` |
| `assets/data/ailment_contracts.json` | 是 | 新增 Slow 条目 |
| `src/game/contracts/CombatEvents.hpp` | 否 | `AilmentType::Slow` 已存在 |
| 3 处测试 | B/C 否；A 是 | 见 §1.3 |
| `DamageConditions.cpp:12-16` 受控判定 | 否 | 依赖 `SpeedDown`/`kind==Slow`，保持不变 |
| `RendingWave.cpp:710`（275 扩散） | 否 | `type==SpeedDown` 不变即可继续归因 |
| `BoomerangDeliverySystem`（`SpeedDown`） | 否 | 同上 |
| `BuffRegistry.cpp`（UI 图标） | 否 | `BuffType::SpeedDown` 不变 |
| `DamagePipeline.cpp:1608`（172 元素位）/ 172 冻结增伤 | 否 | 不在范围 |
| 存档格式 | 否（B/C）；是（A） | B/C 不改 `id`/字段 |
| `AttributePipeline` / Stats | 否 | modifier 数据结构不变 |

**红旗**
1. **A 方案会引入契约双语义**（DoT `magnitude` vs 移速 `slow_pct`），且刷新分支当前不处理 `modifiers`（`AilmentEngine.cpp:698-721`），是明确的正确性陷阱。
2. **`TryMapLegacyBuff` 把 `SpeedDown` 归为 `Chill`**（`:476-477`）：若未来有任何非托管 `Slow` 载体进入 tick 循环且 `tick_damage>0`，会被当作 Chill 处理。B/C 方案下 `FrostSlow`/`frozen_chill` 的 `tick_damage=0`，暂不触发，但应在 planning 中加注释固化。
3. **`FrostChill`(kind=Chill) 与 `FrostSlow`(kind=Slow) 语义并存**，且 173 用 `(kind=Chill, source_skill_id)` 定位叠层——若 D4 误改 `kind`，173 叠满冻结会失效。
4. **条目引用路径错误**（F1）：backlog/plan/review 中的 `.../FlowingThrustNodes.cpp:471` 无法 grep 定位，应在本条销项时顺带订正引用（仅文档，不改 backlog 本身除非用户同意）。

---

## 9. 风险、依赖、未决项

### 9.1 风险

| 风险 | 级别 | 触发条件 | 缓解 |
|------|------|----------|------|
| A 方案刷新丢 modifier | 高 | 选 A 且未改 `Apply` 刷新分支 | 选 B；或 A 必须补 modifier 合并逻辑 + T3 |
| Slow 契约意外产生 DoT | 高 | D5 未把 `damage_tag` 设为无伤害 | D5 + T2 护栏 |
| 合并 id 导致跨技能互刷减速 | 中 | D3 误选 b | D3=a + T5 |
| builtins 与 JSON 漂移 | 中 | 只改一处 | T1 断言逐字段等值 |
| 203 引用错误扩散 | 低 | 文档复制粘贴 | 本条销项时订正 |

### 9.2 依赖

- `ailment_contracts.json` 的字段/解析能力（`TagFromString` / `DefaultDamageTag`）需确认能表达 D5 的「无伤害」语义——**这是 B/C 方案的前置校验点**（若不能表达，D5 退化为「tick_interval 极大 + 不调用 Apply」）。
- 无外部依赖、无新框架/工具。

### 9.3 未决项

1. D1–D7 待用户裁定。
2. node 173 `FrostChill` 是否纳入统一构建口（§6.6）。
3. `frozen_chill` 的 `kind` 最终取值（D4）。
4. B2-18 销项时的 DoD 口径（是否接受「保留手工载体」为最终态）——直接决定 B 是否足够。
5. 是否另立条目把 `BuffId::FrostSlow` 枚举化（与 B2-15 同型），本设计仅登记。

---

## 10. 参考

- backlog：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:148`（B2-18）、`:262`
- 计划：`docs/plans/2026-09-13-skill-followup-plan.md`（§2.2）、`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（T5.1 `:345,356-366`）
- 评审：`docs/reviews/2026-09-07-skill1-specialization-nodes-review.md:510` §6.5-3；`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`（F1/F7）；`docs/reviews/2026-09-13-final-review-skill-abstraction-b2-cleanup.md`
- 相关设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（B2-18 现状行）、`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`

---

## 11. 落地记录（2026-09-13，D1=B / D2–D7 保守默认）

用户裁定：**D1=方案 B**；D2 保留 `"FrostSlow"` id；D3 HazardSystem 同批但各保留 id；D4 `frozen_chill` 维持 `kind=Chill`；D5 契约显式「无伤害」并在注释固化；D6 既有护栏零改动；D7 暂不接线 `victimHasCold` 双路径。

### 11.1 改动文件

| 文件 | 改动 |
|------|------|
| `assets/data/ailment_contracts.json:64-77` | 追加 `Slow` 条目（`max_stacks=1` / `Refresh` / `Strongest` / `tick_interval=1.0` / `base_duration=2.5` / `damage_tag="None"` / `legacy_buff_type="SpeedDown"`） |
| `src/game/systems/combat/AilmentEngine.cpp` | `DefaultDamageTag()` 增 `Slow → Tag::None`；`LoadBuiltins()` 补 `setContract(Slow, …, Tag::None, BuffType::SpeedDown)`；新增 `AilmentAdapter::BuildMoveSpeedDebuff()` 定义；`Tick()` 增 `contract->damage_tag == Tag::None` 跳过守卫；补 `<utility>` |
| `src/game/systems/combat/AilmentEngine.hpp` | `AilmentAdapter` 增 `BuildMoveSpeedDebuff()` 声明；补 `<string>` |
| `src/game/foundation/data/TagRegistry.hpp:160-165` | `TagFromString` 新增 `"None" → Tag::None`（D5 parser 前置） |
| `src/game/systems/skill/behaviors/FlowingThrust.cpp:65-78` | `ApplyFrostSlowDebuff` 改为调用单源构建口；`id/type/kind/modifier` 逐字不变 |
| `src/game/systems/combat/HazardSystem.cpp` | 补 `AilmentEngine.hpp` include；`ApplyChillDebuff` 改为调用单源构建口；`id="frozen_chill"`、`kind=Chill`、`-50%` 不变 |
| `tests/unit/AilmentEngineTests.cpp` | 新增 3 条 B2-18 用例（契约注册/无 DoT/单源构建口） |

### 11.2 D5 parser 验证结论

`TagFromString("None")` 原先返回 `std::nullopt`，且 `DefaultDamageTag(Slow)` 落到 `default: Tag::Poison`——即 JSON 中的 `"damage_tag": "None"` 会**静默回退为 Poison 伤害元素**，无法表达「无伤害」。落地同时修正两处：`TagFromString` 显式解析 `"None"`，`DefaultDamageTag` 对 `Slow` 返回 `Tag::None`；并额外在 `AilmentEngine.cpp` 的 `Tick()` 增加 `damage_tag == Tag::None` 跳过守卫，防止未来调用方误把减速幅度当 DoT 底数。三条改动对既有 6 条契约与其他 `TagFromString` 调用方均为等价行为（`Tag::None == 0`，`value_or(Tag::None)` 结果不变）。

### 11.3 验证证据

- 构建：`build.bat`（RelWithDebInfo）EXIT=0，**零新增警告**（全日志无 `warning C*`）。
- `ctest -C RelWithDebInfo -L ci` → 1/1 passed；`-L skill` → 2/2；`-L unit` → 8/8；`-L integration` → 6/6；`-L combat` → 2/2；`-L contract` → 1/1。
- 3 处护栏用例（`tests/functional/FlowingThrustNodes.cpp:472`、`tests/unit/SkillSpecializationBakerTests.cpp:885`/`:1257`）**零 diff**，与 3 条新用例合并运行 6/6 passed、96 assertions passed。
- 新用例单独运行 3/3 passed、55 assertions passed。
- `python scripts/validate_json.py` → EXIT=0，「All JSON files are valid」。

### 11.4 未决（承接 §9.3）

- node 173 `FrostChill` 仍为自建载体（§6.6 未执行，保持最小面）。
- `BuffId::FrostSlow` 枚举化、`frozen_chill` 无独立测试（`ApplyChillDebuff` 为 private，当前由构建口单测等价覆盖）——若后续需要 source 级锁定，另立条目。
