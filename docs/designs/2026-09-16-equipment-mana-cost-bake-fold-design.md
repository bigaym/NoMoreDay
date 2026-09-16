# 装备降耗统一结算（烘焙折叠）设计

- **文档状态**：设计完成 / 待审阅
- **文档路径**：`docs/designs/2026-09-16-equipment-mana-cost-bake-fold-design.md`
- **设计日期**：2026-09-16
- **系统代号**：`UMR-MC`（Unified Mana Cost Settlement）
- **输入来源**：
  - `docs/reviews/2026-09-16-umr-framework-completion-and-hardening-review.md`（外部评审，结论 `修改`，意见 #4/#5/#6/#12/#14）
  - `docs/designs/2026-09-16-umr-framework-completion-and-hardening-design.md`、`docs/plans/2026-09-16-umr-framework-completion-and-hardening-plan.md`（Phase 4 原始范围）
  - `docs/designs/2026-09-15-umr-foundation-and-pipeline-consolidation-design.md` §2.1（`MANA_COST_MULT` 算子闭环目标）
  - 用户 2026-09-16 决策：**蓝耗按统一结算，触发节点也享受降低**

---

## 1. 背景与问题陈述

### 1.1 任务来源

Phase 4 已于 `EquipmentModifierAdapter::GetEquippedManaCostMultiplier` 中打通 `MANA_COST_MULT` 算子的求值，并在**施法路径**（`SkillSystem.cpp:2052-2055`）接入。外部评审指出该接线存在三项结构性缺陷：

1. **结算点不统一（#14）**：触发节点施法路径（`SkillSystem.cpp:920-931`）使用裸 `trigger_skill->mana_cost`，绕过装备乘算、专精修正与冷却缩减，与计划中「单一法力结算点」的前提直接矛盾。
2. **引导抽蓝漏算（#5）**：`BeamChannelDeliverySystem.cpp:772`、`:1085` 读取烘焙的 `effective_mana_cost` 作为每秒抽蓝速率，该值不含装备乘算，导致引导类技能的持续消耗与施法消耗脱节。
3. **显示层无消费者（#4）**：`SkillDisplayPreviewService` 计算出的 `preview.display_mana_cost` 在生产 UI 路径上**没有任何消费者**。`UIRenderer::GetSkillTooltipLines`（`UIRenderer.cpp:1052-1114`）虽读取该字段却无调用点；实际生产路径 `TooltipController.cpp:244` → `UIRenderer::DrawSkillTooltipFromSnapshot`（`UIRenderer.cpp:1857`）读取裸 `skill->mana_cost`，热键栏 `GameUiSnapshotBuilder.cpp:843` 同样读取裸值。

### 1.2 根因

法力消耗的「装备乘算」被当作**一次性补丁**挂在单一调用点，而非作为**技能最终形态的一部分**。但本工程早已存在专门承担「技能最终形态」的烘焙设施：

- `SkillSpecializationBaker.hpp:13-15` 文档明确：*「在调整天赋或更换装备时，一次性计算出技能的最终形态常数与触发规则」*。
- `SkillSystem.hpp:141` 明确：*「Rebake pure POD skill profiles for an entity based on talents **and equipped item modifiers**」*。
- `SkillSpecializationBaker::Bake` 末尾（`SkillSpecializationBaker.cpp:175-212`）已有独立的「装备修饰器烘焙」段，逐装备累加 `mod.mana_cost_delta`。

即：**烘焙档案的契约本就包含装备修正，装备降耗乘算却挂在烘焙之外**。这是契约与实现的分裂，也是上述四项缺陷的共同根因。

### 1.3 决策（用户已定）

> **蓝耗按统一结算，触发节点也享受降低。**

据此确定：将装备降耗乘算**折叠进烘焙档案 `BakedSkillProfile::effective_mana_cost`**，使所有消费者（施法、引导抽蓝、触发、UI 显示）共享同一结算结果，而非在各消费点重复求值。

---

## 2. 目标、范围与非目标

### 2.1 目标（Goals）

1. **单一结算点**：装备降耗乘算在 `SkillSpecializationBaker::Bake` 内完成一次，结果写入 `BakedSkillProfile::effective_mana_cost`。
2. **消除双重折扣风险**：移除施法路径与预览服务中的重复乘算。
3. **触发节点降耗（D4）**：触发施法改为使用统一结算后的消耗。
4. **引导抽蓝降耗（D3）**：引导类技能每秒抽蓝随烘焙结果自动降耗。
5. **显示一致性（#4）**：热键栏与技能提示显示的消耗与实际扣费一致，且**不引入渲染路径的注册表访问**（保持 R8 约束）与**逐帧求值开销**（消解 #12）。
6. **失效自动化**：复用既有 `StatsDirty → AttributePipeline::Calculate → RebakeSkillProfiles` 链路，装备变更即自动重烘焙，无需新增缓存与失效逻辑。

### 2.2 非目标（Non-Goals）

1. **不新建 per-skill 降耗缓存表**：原 D2 方案（新增缓存 + 独立失效）被本设计取代——烘焙档案本身就是该缓存。
2. **不改造 `SkillDisplayPreviewService` 的伤害/持续预览**：仅涉及其法力消耗字段的乘算归属。
3. **不统一「法力返还」路径**：`SevenStarSlashShared.hpp:133-136`（返还裸 `skill->mana_cost`）、`RendingWave.cpp:88`（返还烘焙值）属返还语义，本轮不纳入统一结算。
4. **不修改 `skill_modifiers` 的上限约束**：并入 Schema V2 约束仲裁（计划既有非目标）。
5. **不接入 `skill_id_whitelist` 之外的过滤条件重排**：乘算的过滤语义保持与现状逐位一致。
6. **不重构 `Bake` 的 1100 行节点表**。

---

## 3. 现状取证

### 3.1 烘焙基础设施

| 设施 | 位置 |
| --- | --- |
| `SkillSpecializationBaker::Bake(registry, caster, skill_id, spec, out_profile, out_triggers)` | `src/game/systems/skill/SkillSpecializationBaker.hpp:28-33` |
| 装备修饰器烘焙段（`item.skill_modifiers`，含 `+ mod.mana_cost_delta`） | `SkillSpecializationBaker.cpp:175-212` |
| 基础法耗初始化 | `SkillSpecializationBaker.cpp:33`；skill 5 → `20.0f`（`:34-36`）；skill 7 ← `mana_cost_per_sec`（`:37-41`） |
| 专精节点对法耗的**最后一次**写入 | `SkillSpecializationBaker.cpp:812-813`（skill 8 / node 800，`-1.0f × points`） |
| `BakedSkillProfile::effective_mana_cost` 字段 | `src/game/foundation/components/SkillDefs.hpp:676` |
| `ActiveSkillsComponent::baked_profiles` 存储 | `SkillDefs.hpp:714` |
| 重烘焙入口 `SkillSystem::RebakeSkillProfiles`（遍历槽位、幂等写回） | `src/game/systems/skill/SkillSystem.cpp:2692`、调用 `Bake` 于 `:2740`、比较后写回于 `:2743-2746` |
| 烘焙档案读取 `SkillSystem::GetBakedSkillProfile`（按 `skill_id` 线性查找，未命中返回 `nullptr`） | `SkillSystem.cpp:2750` |

### 3.2 失效链路（既有，无需新增）

```
装备/卸下 (src/game/systems/item/InventorySystem.cpp:452 / :514 / :810)
  → registry.get_or_emplace<StatsDirty>(character)
  → GameplayState.cpp:360  StatsSystem::update(registry)
  → StatsSystem.cpp:533-542  registry.group<StatsDirty, CombatStats>() → Recalculate → clear<StatsDirty>()
  → StatsSystem.cpp:100-103  Recalculate = ClearCache + AttributePipeline::Calculate
  → AttributePipeline.cpp:835-837  if any_of<ActiveSkillsComponent> → SkillSystem::RebakeSkillProfiles
```

`AttributePipeline.cpp:835-837` 是 `RebakeSkillProfiles` 的**唯一生产调用点**，本设计不改动该链路。

### 3.3 法力结算路径全清单

| # | 位置 | 现状取值 | 折叠后 |
| --- | --- | --- | --- |
| 1 | `SkillSystem.cpp:2036` 施法基准 | `baked ? effective_mana_cost : data->mana_cost` | 自动含降耗 |
| 2 | `SkillSystem.cpp:2052-2055` 施法乘算 | **`*= GetEquippedManaCostMultiplier(...)`** | **须删除**（否则双乘） |
| 3 | `SkillSystem.cpp:920-931` 触发施法 | 裸 `trigger_skill->mana_cost` | **改为烘焙值 + 回退乘算（D4）** |
| 4 | `BeamChannelDeliverySystem.cpp:772`（skill 7） | `profile ? effective_mana_cost : 15.0f` | 自动含降耗（D3） |
| 5 | `BeamChannelDeliverySystem.cpp:1085-1095`（skill 5） | 同上，再 `× beam.tick_interval` | 自动含降耗（D3） |
| 6 | `SkillDisplayPreviewService.cpp:25-37` | 烘焙分支 / 裸值分支 | 烘焙分支自动含降耗 |
| 7 | `SkillDisplayPreviewService.cpp:39-43` | **额外 `*=`** | **须删除并改造回退分支** |
| 8 | `GameUiSnapshotBuilder.cpp:843` 热键栏 | 裸 `skill->mana_cost` | **改走烘焙值（#4）** |
| 9 | `UIRenderer.cpp:1886-1889` 快照提示 | 裸 `skill->mana_cost` | **改走快照内已结算值（#4）** |
| 10 | `UIRenderer.cpp:1298-1305` 注册表提示 | 已改为 `preview.display_mana_cost` | 不变 |
| 11 | `SevenStarSlashShared.hpp:133-136` 返还 | 裸 `skill->mana_cost` | 不变（非目标） |
| 12 | `RendingWave.cpp:88` 返还 | 烘焙值 | 自动含降耗（无害） |

### 3.4 乘算的过滤语义（必须保持逐位一致）

`assets/data/modifier_v2/equipment_modifiers.json` 记录 `1001001`：

```
skill_id_whitelist: [1]
required_skill_tags_all: 4294967296   (= 1 << 32 = Tag::Hit)
equip_slot_mask: 2
ops: ADD_SKILL_LEVEL(skill,1,2.0) ; MANA_COST_MULT(skill,1,0.9)
```

技能 1（`assets/data/skills.json`）`tags = ["Physical","Melee","Attack","Movement","Hit","SwordSkill"]`，含 `Hit`；`mana_cost = 5.0` → 结算后 **4.5**。

现状三处调用（施法路径、预览服务）均传入 **`skillData->tags`（静态标签）**。为保持行为等价，折叠时**同样使用 `skillData->tags`**，不使用 `out_profile.effective_tags`（后者含装备/专精标签转化，会改变过滤命中集合）。

---

## 4. 设计决策

### 4.1 方案对比

| 方案 | 说明 | 结论 |
| --- | --- | --- |
| A. 在原位继续补点 | 在引导抽蓝、触发、热键栏、快照提示各补一次 `GetEquippedManaCostMultiplier` | ✗ 违反单一结算点；UI 逐帧求值（#12 恶化）；渲染路径被迫访问注册表（破 R8）；漏一处即回归 |
| B. 新建 per-skill 降耗缓存 + 独立失效 | 新增缓存表与装备变更监听 | ✗ 与既有烘焙档案职责重叠；需自建失效链路，易漏 |
| **C. 折叠进烘焙档案（选定）** | 在 `Bake` 末尾把乘算写入 `effective_mana_cost` | ✓ 复用既有契约与失效链；零新增缓存；所有消费者自动一致；零逐帧开销 |

### 4.2 选定方案：烘焙折叠

**插入点**：`SkillSpecializationBaker::Bake` 的「装备修饰器烘焙」段之后（`SkillSpecializationBaker.cpp:212` 之后、`)` 之前的函数体内），与既有 `item.skill_modifiers` 遍历同段，语义连贯：

- 该位置在**全部**专精节点写入（最后写入点 `:812-813`）之后 → 乘算作用于节点修正后的最终值。
- 该位置与既有装备修正（`:188` 的 `+ mod.mana_cost_delta`）同段 → 「装备对法耗的加性与乘性修正」集中在同一处。
- 使用 `skillData->tags`（`skillData` 在 `:22` 已就绪）→ 过滤语义与现状逐位一致。
- 无条件执行（不置于 `if (equipment)` 内）→ 无装备时 `GetEquippedManaCostMultiplier` 自身返回 `1.0f`，语义清晰。

**幂等性**：`Bake` 每次都从 `skillData->mana_cost` 重新初始化（`:33`），`effective_mana_cost` 不会跨次累积，折叠天然幂等；`RebakeSkillProfiles` 的比较后写回（`:2743-2746`）亦不会被破坏。

### 4.3 行为规则（合同中不可破坏的不变量）

- **IC-1 单一结算**：装备 `MANA_COST_MULT` 对某技能的乘算结果，在系统中**只求值一次**（`Bake` 内），其余路径只读 `effective_mana_cost`。
- **IC-2 统一生效域**：烘焙档案的所有消费者（施法基准、引导每秒抽蓝、触发施法、UI 显示、返还）取到的是**同一数值**。
- **IC-3 过滤等价**：乘算的过滤条件与记录 `1001001` 的既有语义逐位一致（`skill_id_whitelist` + `required_skill_tags_all` 基于静态 `skillData->tags` + `equip_slot_mask` + `profession_mask`）。
- **IC-4 免费施法优先**：`SkillSystem.cpp:2040-2047` 的 FreeCast 置零发生在乘法链之前且结果为 0，折叠后仍为 0。
- **IC-5 分身复制值**：`CheckPreCastShadowDuplication`（`SkillSystem.cpp:2066`）接收的 `base_cost` 必须已是折扣后值——现状注释已声明此意图，折叠后基准值自带降耗，为该不变量提供更稳固保证。
- **IC-6 不改渲染约束**：UI 获取降耗后数值的途径仅为**读取快照**，不得调用 `SkillDisplayPreviewService` 或访问 `ModifierRuntimeRegistry`（R8）。
- **IC-7 幂等**：重复 `Bake` 同一（角色，技能，专精，装备）组合产出相同 `effective_mana_cost`。

### 4.4 触发路径（D4）规则

触发施法（`SkillSystem.cpp:920-931`）的判定与扣费必须使用统一结算值：

- 优先取 `SkillSystem::GetBakedSkillProfile(registry, caster, trigger_skill_id)->effective_mana_cost`（覆盖已入槽/已烘焙的触发技能）；
- 未命中烘焙档案时（触发技能不在槽位）回退为 `trigger_skill->mana_cost` **再乘** `GetEquippedManaCostMultiplier(registry, caster, trigger_skill_id, trigger_skill->tags)`，保证未入槽触发技能同样享受降耗；
- 判定（`stats->mana < cost` → 跳过、不发 `StatsDirty`）与扣费（`stats->mana -= cost`）**必须使用同一个局部变量**，避免判定/扣费口径分裂。

### 4.5 UI 显示（#4）规则

- **热键栏**：`GameUiSnapshotBuilder.cpp:843` 改为优先取烘焙值（`GetBakedSkillProfile(registry, player, slots[i].id)->effective_mana_cost`），未命中回退 `skill->mana_cost`。该值同时驱动 `SkillHotbarController.cpp:133` 的 `hasEnoughMana` 门槛——门槛与实际扣费一致，属**修正**而非回归。
- **快照技能提示**：`UIRenderer::DrawSkillTooltipFromSnapshot`（`UIRenderer.cpp:1857`）已有 `const GameUiSnapshot&` 参数。改为在 `snapshot.skillBar.slots` 中按 `skillId` 匹配取 `manaCost`，未匹配则回退 `skill->mana_cost`。**不新增快照字段**，**不访问注册表**，**不逐帧求值**（`slotView.manaCost` 已是构建快照时一次写入的 POD）。

---

## 5. 影响面分析

### 5.1 接口与合同

| 对象 | 影响 |
| --- | --- |
| `SkillSpecializationBaker::Bake` | 签名**不变**；仅新增一次乘算写入。新增对 `EquipmentModifierAdapter` 的头文件依赖（同目录模块，无新外部依赖） |
| `BakedSkillProfile::effective_mana_cost` | 语义**收紧**：由「基础+专精+词条加性」扩展为「含装备乘算的最终值」。字段名与类型不变，存档为 POD 快照，无格式变更 |
| `EquipmentModifierAdapter::GetEquippedManaCostMultiplier` | 签名不变；调用点由 2 处（施法、预览）变为 2 处（烘焙、触发回退）+ 1 处（预览回退分支） |
| `SkillDisplayPreviewService::Build` | 返回的 `display_mana_cost` 语义不变（仍是最终显示值）；改为烘焙分支直接可用、回退分支内联乘算 |
| `GameUiSnapshot` | **无字段变更**（复用已存在的 `skillBar.slots[].manaCost`） |

### 5.2 存档

无影响。`BakedSkillProfile` 属运行时烘焙产物（随 `StatsDirty` 重算），不写入存档；存档中装备词缀记录 ID 与本次改动无关。

### 5.3 资产与配置

无影响。不新增/修改任何 JSON 与生成器；两条漂移门禁（`gen_map_monster_modifier_v2.py --check`、`gen_modifier_runtime_v2.py --check`）应保持 RC=0。

### 5.4 性能预算

- `Bake` 新增一次 `GetEquippedManaCostMultiplier` 求值：仅在 `RebakeSkillProfiles`（属性重算）时发生，**不在逐帧热路径**。
- **净收益**：移除 `SkillDisplayPreviewService` 的乘算后，若未来接线预览服务，开销下降；热键栏与快照提示改读 POD，**零额外求值**。
- 直接消解评审 #12：不再需要「逐帧 UI 求值 + 缓存失效」设计。

### 5.5 回退路径

单点回退：将 `Bake` 末尾的乘算行注释即可恢复「施法点单独乘算」的旧行为——但需同时恢复 `SkillSystem.cpp:2052-2055` 与 `SkillDisplayPreviewService.cpp:39-43`。由于三处改动由同一不变量（IC-1）约束，**不建议部分回退**；如需回退应整体回退。

---

## 6. 验收标准与验证方式

### 6.1 可观察验收标准（DoD）

| # | 标准 | 验证方式 |
| --- | --- | --- |
| A1 | 装备记录 `1001001`、技能 1 时 `BakedSkillProfile::effective_mana_cost == 4.5f`（5.0 × 0.9），且不含降耗装备时为 `5.0f` | 单元测试（烘焙层） |
| A2 | 重复 `Bake` 结果稳定（幂等），不随调用次数累积 | 单元测试（连续两次 `Bake` 同值） |
| A3 | 施法路径不再二次乘算：`SkillSystem` 接入点的 `base_cost` 等于烘焙值经 rcr/专精修正后的结果，装备乘算**不被应用两次** | 单元测试（`TryCast` 集成，D5） |
| A4 | 触发节点施法扣费等于统一结算值（含降耗），且判定与扣费同源 | 单元测试（触发扣费断言） |
| A5 | 引导每秒抽蓝（skill 5/7）与施法口径一致（同一 `effective_mana_cost`） | 单元测试或既有引导用例回归 |
| A6 | 热键栏 `slotView.manaCost` 与快照提示显示值等于烘焙值；`hasEnoughMana` 门槛与扣费一致 | 单元测试（快照构建 + 快照提示取值路径） |
| A7 | FreeCast / 剑意 / 绝影等既有修正仍生效，结果与折叠前逐位一致 | 既有技能测试回归 |
| A8 | 无装备角色：`effective_mana_cost == skillData->mana_cost`（乘算为 1.0） | 单元测试 |
| A9 | `build.bat RelWithDebInfo` RC=0；`ctest -L unit` 100% 通过；两条生成器 `--check` RC=0 | 命令验证 |
| A10 | 渲染路径未新增注册表访问（R8 不变量保持） | 代码审查（`UIRenderer.cpp` diff 仅读 `snapshot`） |

### 6.2 回归范围（必须重跑）

- `*Modifier*`、`*MonsterAffix*`、`*ItemPersistence*`、`*ItemStore*`、`*AttributePipeline*`、`*EquipmentModifier*`
- 技能相关：`*Skill*`、`*Beam*`、`*Blade*`、`*Rending*`
- 全量 `ctest --test-dir build -C RelWithDebInfo -L unit`

### 6.3 新增测试

1. **烘焙层单元测试**（`tests/unit/SkillSpecializationBakerTests.cpp` 或既有等价文件）：A1、A2、A8。
2. **`TryCast` 集成测试（D5，评审 #6）**：构造带装备记录 `1001001` 的角色，调用 `TryCast`，断言结算后的 `CombatStats::mana` 扣减量（A3、A4）。该夹具同时补上评审指出的「施法扣费无测试覆盖」空白。
3. **快照层单元测试**：A6。

---

## 7. 未决问题、风险与依赖

### 7.1 未决问题（需用户决策或后续立项）

| # | 问题 | 现状处置 |
| --- | --- | --- |
| Q1 | **法力返还口径未统一**：`SevenStarSlashShared.hpp:133-136` 返还裸 `skill->mana_cost`，若实际扣费已降耗则返还多于支出 | 本轮记为残留风险；返还上限已有 clamp，影响有限。后续可立项「返还按同一结算值」 |
| Q2 | **`skill_modifiers` 数值上限**：编码侧已对三张旁表 fail-closed，解码侧 `skill_modifiers` 上限仍依赖 Schema V2 约束仲裁 | 计划既有非目标，并入 Schema V2 |
| Q3 | **`StatsDirty` 双重消费者**：`GPUEntitySync.cpp:167-168` 亦会 `remove<StatsDirty>`，若先于 `StatsSystem::update` 执行会吞掉重算标记 | 既存隐患且非本次引入（若成立则**所有**装备属性均失效，不可能长期存在）；本轮不改，记录为残留风险 |
| Q4 | **未入槽触发技能** 的降耗依赖回退乘算（每次触发求值一次） | 可接受（触发非逐帧）；后续若高频可考虑预烘焙全技能表 |

### 7.2 风险

| 风险 | 等级 | 缓解 |
| --- | --- | --- |
| 折叠后与施法点旧乘算叠加导致**双重折扣** | 高 | 同一改动集内**必须**移除 `SkillSystem.cpp:2052-2055` 与 `SkillDisplayPreviewService.cpp:39-43`；以 A1/A3 断言锁死 |
| `effective_mana_cost` 语义收紧后，其他未知消费者期望「不含装备乘算」 | 中 | 已在 §3.3 枚举全部 12 处消费点；全量测试 + `rg effective_mana_cost` 复查 |
| 触发路径判定/扣费口径分裂 | 中 | IC + A4 强制同源局部变量 |
| 装备变更后 UI 值滞后（依赖 `StatsDirty` 在本帧被 `StatsSystem::update` 处理） | 低 | 与所有装备属性同机制，非新增滞后；记录 Q3 |

### 7.3 依赖

- `EquipmentModifierAdapter::GetEquippedManaCostMultiplier`（已实现，Phase 4 产物）
- `SkillSystem::GetBakedSkillProfile` / `RebakeSkillProfiles`（已实现）
- `StatsDirty → AttributePipeline → RebakeSkillProfiles` 失效链（已实现）
- 无新增第三方依赖（符合 `conductor/tech-stack.md`）

### 7.4 残留风险登记（本轮完成后写入评审报告）

1. Q1 返还口径不统一。
2. Q2 `skill_modifiers` 上限待 Schema V2。
3. Q3 `StatsDirty` 双重消费者顺序未验证。
4. Q4 未入槽触发技能降耗为即时求值。
5. 快照提示仅覆盖热键栏技能（在槽技能）；未入槽技能提示回退裸值。

---

## 8. 与既有文档的关系

- **上游**：`docs/designs/2026-09-15-umr-foundation-and-pipeline-consolidation-design.md` §2.1「`MANA_COST_MULT` 算子闭环」——本设计完成其**消费侧闭环**（求值侧已由 Phase 4 完成）。
- **修订**：`docs/designs/2026-09-16-umr-framework-completion-and-hardening-design.md` Phase 4.1/4.2 的接线位置——由「施法点乘算」修订为「烘焙折叠 + 同消费点统一」。
- **评审对应**：`docs/reviews/2026-09-16-umr-framework-completion-and-hardening-review.md` 意见 #4、#5、#6、#12、#14。
