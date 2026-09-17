# SKILL-SYSTEM-FINAL-CLOSURE 实施审查报告

## 审查目标

技能系统最终收尾与全链路闭环包（`SKILL-SYSTEM-FINAL-CLOSURE`），包含四项内容：

1. **F-06**：属性修饰符运行时来源通路静默失效修复（`apply_if_tags_match` 的 `source_prebaked` 显式化）。
2. **F-02**：只读烘焙档案安全访问守卫统一（`SkillSystem::GetValidBakedSkillProfile` 取代 `GetBakedSkillProfile` 直连点）。
3. **F-01**：技能 10 节点 1015 的 `desc_key` 文案与 UMR 数据/既有测试基线三方对齐。
4. 10 项自动化回归用例（`tests/unit/SkillSpecializationStatModifierTests.cpp`）与技能 1-9 遗留总账（backlog）结项归档。

## 结论

**修改**

判定依据（`docs/workflows/review.md` 判定规则 1）：包目标的四项中，backlog 结项归档本身即为「必要验证证据」产物；本轮新增的归档小节 `docs/plans/2026-09-12-skill1-9-followup-backlog.md:379` 作出了「该新增测试文件尚未在 `tests/unit/` 下检出」的**已被证伪的断言**，且与同一文件 :109 / :116 的销项陈述直接自相矛盾。作为闭环证据链的账本内部不一致，须先修正证据陈述后再行签核。**代码实现本身未发现 Blocker / High 缺陷，无需改动任何实现文件**；本轮返回修改仅针对总账文档的证据陈述（见发现项）。

## 审查轮次

首次审查（2026-09-17）。

## 输入

- 设计：`docs/designs/2026-09-17-skill-system-final-closure-design.md`（v1.2）
- 实施计划：`docs/plans/2026-09-17-skill-system-final-closure-plan.md`（v1.2）
- 遗留总账：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`
- 审查标准：`docs/workflows/review.md`；代码规范：`conductor/code_standard.md`（硬否决条款涉及的 §2.1 / §2.2 / §5.2 / §5.3 / §6.1 / §7.2 / §8.1）
- 验证证据（**本审查独立复跑**，非采信实现方上报）：
  - `git status --short`（见「变更文件边界」）
  - `bin/NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*"` → `test cases: 10 | 10 passed`、`assertions: 56 | 56 passed`，SUCCESS
  - `bin/NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*,*SkillProfileResolveSentinel*,*SkillWrapupHardening*,*SkillSpecializationBaker*"` → `test cases: 35 | 35 passed`、`assertions: 476 | 476 passed`，SUCCESS（覆盖 F-01 的 0.59s 基线）
  - 四个离线门禁独立复跑全部 `EXIT=0`：`gen_skill_contracts.py --check --check-idempotency --check-determinism`（`[OK] skill_contract blocks are up to date.`）；`sync_skill_node_icon_ids.py --check`（`Nodes updated: 0 / unchanged: 76 / missing icon files: 0`）；`validate_skill_spec_modifiers.py --check`（`6/6 passed`）；`gen_skill_mechanics_schema.py --check`（`OK: skill_mechanics_schema.json up to date (437 entries, 62 unreferenced)`）
  - 实现方上报但本审查**未复跑**（受「禁止运行 `build.bat`」约束）：`build.bat RelWithDebInfo` EXIT 0 / 0 warnings；`ctest -L unit|integration|skill` 与 `-R nmd.tests.ci.nonperf`

## 变更文件边界（`git status --short` 摘要）

`19 modified + 3 untracked`（实现方描述为「~18 modified」，实测入库变更文件为 19 个 tracked 修改）。`git diff --stat`：`19 files changed, 118 insertions(+), 79 deletions(-)`（无 CRLF 警告以外的异常；LF/CRLF 提示为 git autocrlf 常态，非本包缺陷）。

```
 M assets/data/mastery_skill_trees.json
 M docs/plans/2026-09-12-skill1-9-followup-backlog.md
 M src/game/application/states/GameplayState.cpp
 M src/game/application/ui/GameUiSnapshotBuilder.cpp
 M src/game/contracts/impl/StatsSystem.cpp
 M src/game/systems/combat/DamageMitigationService.cpp
 M src/game/systems/combat/DamagePipeline.cpp
 M src/game/systems/skill/AreaFieldDeliverySystem.cpp
 M src/game/systems/skill/BeamChannelDeliverySystem.cpp
 M src/game/systems/skill/ElementPathSystem.cpp
 M src/game/systems/skill/SkillDisplayPreviewService.cpp
 M src/game/systems/skill/SkillSystem.cpp
 M src/game/systems/skill/SkillSystem.hpp
 M src/game/systems/skill/behaviors/BladeBoomerang.cpp
 M src/game/systems/skill/behaviors/FlowingThrust.cpp
 M src/game/systems/skill/behaviors/InfiniteBlades.cpp
 M src/game/systems/skill/behaviors/PhantomTrance.cpp
 M src/game/systems/skill/behaviors/SevenStarSlashShared.hpp
 M 设计文档/职业设计草案_剑修.md
?? docs/designs/2026-09-17-skill-system-final-closure-design.md
?? docs/plans/2026-09-17-skill-system-final-closure-plan.md
?? tests/unit/SkillSpecializationStatModifierTests.cpp
```

已直接检视的变更文件：上列全部 19 个 tracked 文件 + 3 个 untracked 文件（设计、计划、新测试），以及为实现判定所需而读取的对照文件 `src/game/foundation/stats/AttributePipeline.cpp`、`src/game/systems/skill/SkillProfileResolve.{hpp,cpp}`、`src/game/application/ui/PlayerHUD.cpp`、`src/game/foundation/components/{Stats.hpp,SkillDefs.hpp}`、`src/game/foundation/data/TagRegistry.hpp`、`assets/data/modifier_v2/skill_spec_modifiers.json`、`tests/CMakeLists.txt`。

## 范围对齐

设计与计划的批准范围、非目标、权限边界与验证策略均被遵守，逐项核对如下：

| 设计要求 | 落点 | 判定 |
| --- | --- | --- |
| F-06：`apply_if_tags_match` 新增**无默认值**的 `source_prebaked` 形参 | `src/game/contracts/impl/StatsSystem.cpp:303-304` | ✅ 无默认值，漏传即编译失败 |
| F-06：5 处调用位显式传参 | `:345`(ModifierList=true)、`:353-354`(Astrolabe=true)、`:364`(SkillModifier=false)、`:370`(GlobalModifier=false)、`:499-500`(specialized_slots=false) | ✅ 与设计 §3.1.2 表格逐一对应 |
| F-02：新增只读守卫 API | `SkillSystem.hpp:155-156`、`SkillSystem.cpp:2782-2786` | ✅ `[[nodiscard]]`、`const registry&`、纯读 |
| F-02：28 处直连点迁移 | 见下表 | ✅ 无遗漏 |
| F-02：BeamChannel 手工归一化移除 | `BeamChannelDeliverySystem.cpp:159-161` 区域 | ✅ 已移除，改为直接调用新 API |
| F-02：RebakeSkillProfiles / SkillProfileResolve / 定义 / 测试保持原样 | `SkillSystem.cpp:2708-2765`、`SkillProfileResolve.cpp:14-21`、`SkillSystem.cpp:2766-2779` | ✅ 未迁移，符合设计 §3.2.2 保留清单 |
| F-01：仅改文案/数据，不改数值 | `mastery_skill_trees.json` 单行、`职业设计草案_剑修.md` 2 行 | ✅ 见发现项核验 |
| 非目标：不得修改 AttributePipeline 基础折叠 | `git status` 无 `AttributePipeline.cpp` | ✅ |
| 非目标：不得改 `GetBakedSkillProfile` 缓存语义 | `SkillSystem.cpp:2766-2779` 未改 | ✅ |
| 验证：`tests/CMakeLists.txt` GLOB 自动收集 | 新文件已编译入套件（本审查复跑 10/10） | ✅ |

**F-02 调用点全量分类（`rg "GetBakedSkillProfile(" src tests`）**

- 已迁移（生产 28 处）：`GameUiSnapshotBuilder.cpp:846`；`GameplayState.cpp:481,885`；`DamagePipeline.cpp:337,613`；`DamageMitigationService.cpp:131,163,318`；`ElementPathSystem.cpp:69`；`BeamChannelDeliverySystem.cpp:75,161,945,1014,1093,1113,1149,1176`；`AreaFieldDeliverySystem.cpp:447,468`；`BladeBoomerang.cpp:252`；`FlowingThrust.cpp:301`；`InfiniteBlades.cpp:57`；`PhantomTrance.cpp:87`；`SevenStarSlashShared.hpp:142`；`SkillDisplayPreviewService.cpp:25`；`SkillSystem.cpp:925,1805,2004`。
- 合法豁免：`SkillSystem.hpp:148`（声明）、`SkillSystem.cpp:2766`（定义）、`SkillSystem.cpp:2784`（新 API 内部）、`SkillProfileResolve.cpp:16-21`（内部已判 `is_baked`，设计 §3.2.2 明示保留）。
- 仅测试：`PhantomTranceNodes.cpp:767`、`InfiniteBladesNodes.cpp:236,409`、`SkillKeyNodeMatrixIntegrationTests.cpp:421`、`ItemSkillModifierTests.cpp:82,114,138,165,192`、`ItemSkillModifierE2ETests.cpp:157`、`SkillProfileResolveSentinelTests.cpp:64,86`、`SkillManaCostSettlementTests.cpp:118,126`、`SkillSpecializationStatModifierTests.cpp:312`、`SkillSpecializationBakerTests.cpp:549,555,605,612`。
- **MISSED：无。** 未发现遗漏的生产直连点。

## 质量与风险评估

### 1. F-06 语义（设计 §3.1.2）— 逐条核验，全部正确

- **`source_prebaked==false` 下 `required_tags==Tag::None` 现为「无条件生效」**：✅ 新代码 `is_baked` 仅在 `source_prebaked` 为真时求值，false 分支恒为 `false`，随后 `tags_match = (mod.required_tags == Tag::None) || HasTag(combined_query_tags, mod.required_tags)` 对 `None` 恒真，修饰符被累加。这正是 32 个休眠节点恢复交付的机制。
- **`required_tags` 与 `combined_query_tags`（而非 `player_tags`）比较**：✅ `StatsSystem.cpp:337` 使用 `combined_query_tags`；`combined_query_tags = tags | player_tags`（`:168-169`），其中 `player_tags` 仅由 `MovementStance::SwordRiding` 置位。旧代码在 `is_baked` 判定后亦用 `combined_query_tags` 做最终匹配，故本项无回归。
- **预烘焙路径逐字节保全**：✅ 从 `git diff` 原始文本可见，旧代码为 `bool tags_match = HasTag(combined_query_tags, mod.required_tags);`，而 `TagRegistry.hpp:115-117` 定义 `HasTag(mask, tag) { return (mask & tag) == tag; }`，`Tag::None == 0` ⇒ `HasTag(x, Tag::None)` 恒为 `true`。因此新增的显式 `(required_tags == Tag::None) ||` 前缀在语义上恒等，预烘焙路径行为不变（设计 §3.1.2 注释「HasTag(x, Tag::None) 恒为 true」准确）。
- **`source_prebaked` 布尔值未传错**：✅ 交叉核对 `AttributePipeline::Calculate`（`AttributePipeline.cpp:324-643`）：基础折叠仅发生于 `ModifierList`（`:579`，过滤条件 `required_tags==None || HasTag(etags,...)`）、`ActiveEffectsComponent`（`:588`）、`AstrolabeComponent`（`:615`，`IsActive(player_tags)`）；`SkillModifierComponent` 与 `specialized_slots` 从未被折叠。对 `GlobalModifierComponent`：管道在 `:349-350` 先 `clear()` 再于 `procAff` 内 `:441-448` 将 `required_tags != Tag::None` 的**条件词缀推入 `global_mods.stat_modifiers` 而不计入 `calcs`**（`else` 分支才走 `AffixDispatcher` 折叠）——即条件词缀确实未被折叠，`false` 正确。

### 2. F-02 空指针回归风险 — 逐站点核验，全部有守卫

- 全部 28 处迁移点均为「取指针 → 同作用域内立即判空/三目/早返回」模式：`GameplayState.cpp:481` if-init、`:878` 先 `valid(owner)` 再 `:885` if-init；`DamagePipeline.cpp:336-338` 位于 `valid(attacker)` 内；`ElementPathSystem.cpp:69-72` `if (profile == nullptr) return 0.0f;`；`BladeBoomerang.cpp:252` 三目；`AreaFieldDeliverySystem.cpp:447,468` `(p && p->…)`；`BeamChannelDeliverySystem.cpp` 8 处全为 `profile &&` / 三目；`SkillDisplayPreviewService.cpp:25-44`、`GameUiSnapshotBuilder.cpp:845-849`、`PhantomTrance.cpp:87-90` 均为 if/else 双分支。**未发现未判空的解引用。**
- `DamageMitigationService.cpp:131-141` / `:163-169`：哨兵返回 `nullptr` 后进入局部重烘焙分支，`localProfile` 为**函数栈对象**且在同一作用域内使用（`profile = &localProfile` 后即刻消费），无悬垂；`Bake` 即使产出哨兵，其 `feature_flags==0` 使后续 `if (profile && flags & …)` 为假，无可观测副作用。此行为由设计 §3.2.5 与计划 §2.3 显式授权。
- `PhantomTrance.cpp`：历史 High 级空指针问题点的默认回退**仍然保留**——`ResolveBakedProfile` 失败后返回 `static const PhantomTranceParams kFallbackParams{};`（`:80-109`）。
- `GameUiSnapshotBuilder.cpp:846` / `SkillDisplayPreviewService.cpp:25`：哨兵→`nullptr` 使显示数值由「0 消耗/0 冷却」回退为静态技能表数值（并在预览侧叠加装备降耗，`SkillDisplayPreviewService.cpp:33-43` 有注释说明）。设计 §3.2.2 明确列出这两处，属**预期内的设计行为变更**，非缺陷。
- `BeamChannelDeliverySystem.cpp:159-161`：原手工 `!is_baked` 归一化确为冗余（新 API 已内建同判），移除后逐帧路径调用的是纯读 API（`SkillSystem.cpp:2782-2786` 只读 `GetBakedSkillProfile`，不触发 `Bake`），**不会引发逐帧重烘焙**。

### 3. EnTT 安全 / 热路径 / 硬否决项 — 全部未触发

- **EnTT（§5.3）**：本包为访问器替换，指针获取点、生命周期与作用域均未改变；唯一新增语义是「原非空哨兵现可为空」，只会减少解引用。`DamageMitigationService` 的栈对象回退不跨组件增删/实体创建销毁。未发现跨变更持有 EnTT 组件指针。
- **热路径堆分配/字符串（§2.1 / §7.2）**：`StatsSystem.cpp` 改动仅涉及位掩码 `Tag` 比较，无字符串、无分配；新 API 为纯读、无分配。未触发硬否决 7 / 12。
- **UB / 内存泄漏（§2.2）**：无。
- **裸 `new`/`delete`（§5.2）**：无。
- **`dynamic_cast` / C 式转换（§6.1）**：无。
- **裸 `std::thread`（§8.1）**：无。

### 4. 冗余轮子 — 复用决策成立

`GetValidBakedSkillProfile` 与既有 `ResolveBakedProfile`（`SkillProfileResolve.hpp:29-32`）**职责不同、无行为分叉**：前者纯读、未命中即 `nullptr`；后者未命中会**回退烘焙到调用方栈对象**。BeamChannel 逐帧路径必须避免重烘焙，故不能复用 `ResolveBakedProfile`；`SkillProfileResolve.cpp` 未被迁移亦为设计 §3.2.2 明示保留。判定：非重复轮子；`SkillProfileResolve.cpp:14-21` 的手写 `is_baked` 门与新增 API 存在轻微重复，但设计显式豁免且无行为分叉，按 `Low` 归类（见最佳实践）。

### 5. 测试可证伪性 — 10 项用例均为真断言，无琐碎/假测试

- 本包**未修改任何既有测试文件**（`git status` 可证），不存在硬否决 2 的「弱化既有用例」情形。
- 用例 1/2 走 `GetStatWithTags` 真实通路：节点 1000（`stat_modifiers:[{type:21(ArmorPenetration),mode:1,value:5}]`，无 `required_tags`）在**修复前**因 `is_baked=true` 被跳过 → 100，修复后 105（1 点）/120（4 点）；跨技能隔离断言 105/100/100。修复前必红。
- 用例 7（`SkillModifierComponent`，`required_tags==None`）修复前跳过 → 100，期望 150；用例 8（`GlobalModifierComponent`，`required_tags==SwordRiding` 且处于剑修架势）修复前 `is_baked=HasTag(player_tags,SwordRiding)=true` 跳过 → 100，期望 125。两者修复前必红。
- 用例 9（哨兵）修复前 `TryCast` 取得非空哨兵，`effective_mana_cost==0` → 法力保持 100，期望 95；`UpdateCooldowns`（`SkillSystem.cpp:1805` 邻域）`raw_cooldown = bakedProfile ? effective_cooldown : data->cooldown` = 0，期望 4.0。修复前必红。管线通过说明目标技能 `max_charges >= 2`，`UpdateCooldowns` 的冷却与充能断言自洽。
- 用例 5/6（Keystone 排除、Transmuter 互斥）走生产 `specialized_slots` 分支与 `StatsSystem.cpp:481` 的 `can_apply_scope`，有效覆盖 F-06 的 `source_prebaked=false` 作用域门。
- **用例 (a) 裁定**：用例 3/4 断言的是 `SkillSystem::CanApplyScopePolicy`，而非 `GetStatWithTags`。经对照，该函数（`SkillSystem.cpp:2556-2583`）与 `StatsSystem.cpp:396-419` 的局部 lambda **逻辑逐行等价**（仅参数名 `context_skill_id` vs `skill_id` 不同）。因此用例 3/4 验证的是「同一语义的第二份实现」，不能直接证明生产 lambda；但该生产 lambda 本包未改动、设计 §3.1.3 已声明为非目标，且资产侧确无可观测的 `GlobalAlways` 节点（全仓 `rg` 仅 `skills.json:6319` 的节点 991 为 `GlobalWhileBuffActive`，其角色为 `Passive/resist_model`，`mastery_skill_trees.json` 仅含技能 10/11/12 且不包含 991，UMR 亦无 991 记录）。裁定：作为既有作用域门的回归护栏**可接受**，但 F-06 全局域的端到端取值路径**未被断言**，计入剩余风险。
- **用例 (b) 裁定**：用例 10 仅扫 `mastery_skill_trees.json` + `modifier_v2/skill_spec_modifiers.json`，与设计 §4 用例 10 及计划 §3 的字面范围**完全一致**。(2,202)/(6,653)/(8,800) 的重叠位于 `skills.json`，本不在该用例声明的范围内，故「不纳入」符合计划。裁定：**符合计划**。
- **用例 (d) 裁定**：`tests/CMakeLists.txt:4-6` 使用 `file(GLOB_RECURSE … CONFIGURE_DEPENDS "*.cpp")`，新文件已被收集；本审查独立复跑 `--test-case="*SkillSpecializationStatModifier*"` 得 10/10、56 断言，**确认已编入套件**。

### 6. F-01 数据/文案核验

- `assets/data/mastery_skill_trees.json` 仅 1 行变更（节点 1015 `desc_key`），改为「延长七星斩起手与收招阶段的无敌帧持续时间，每级延长 0.03 秒容错窗口。」；该节点 `stat_modifiers:[]`、`max_points:3`。
- 权威数据源一致：`assets/data/modifier_v2/skill_spec_modifiers.json:1774` 记录 `id=2010150`，`skill_id_whitelist:[10]`、`node_id_whitelist:[1015]`，`ops[0].opcode="SKILL_DURATION_FLAT"`、`param_f32=0.03`。
- 既有基线一致且未改动：`SkillSpecializationBakerTests.cpp:2354-2361`（`0.5 + 0.09 = 0.59`）与 `SkillWrapupHardeningTests.cpp:334-336`（`0.5s + 0.03s*3 = 0.59s`）；两者不在变更清单内。本审查复跑含 `SkillSpecializationBaker` 的 35 项用例全绿。
- `设计文档/职业设计草案_剑修.md` 仅 2 行（`:930` 条目、`:971` 表格行）同步为同一语义。**无无关数据改动。**

### 7. backlog 结项核验

- 授权来源逐条对齐设计 §3.4：A-01（§3.4-1）、A-05（§3.4-2）、F-06/F-02/F-01（§3.4-3）、B1-21~23（§3.4-4，设计点名 `InfiniteBladesNodes.cpp`/`AreaFieldDeliveryTests.cpp`/`MindBladeNodes.cpp`）、B1-24/26 与 O-01~O-07（§3.4-5 转出性能/渲染 Track）、F-05（§3.4-6 移入未来玩法特性清单）。**未发现越权核销。**
- F-03、F-04 维持 `- [ ]` 并附「设计 §3.4 未授权核销，维持登记」；F-07、F-08 未作改动。✅
- **append-only 纪律**：原始条目文本与既有证据子条目全部保留，销项以 `〔销项 … 依据 …〕` 追加形式书写；未删除既有证据引用。✅
- **问题**：本轮新增的 §10 小节 `:379` 断言新增测试文件「尚未在 `tests/unit/` 下检出」，与实际（文件已交付且 10/10 通过）及同文件 `:109`/`:116` 的销项陈述矛盾。详见发现项 F-1。

## 发现项

### F-1（Medium）闭环账本新增小节作出已被证伪的验证陈述

- 位置：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:379`（本轮新增 §10）
- 问题：该行断言 `tests/unit/SkillSpecializationStatModifierTests.cpp`「截至本次归档尚未在 `tests/unit/` 下检出（计划 §3 F-06 要求新增）」。实测该文件存在于工作区（`?? tests/unit/SkillSpecializationStatModifierTests.cpp`），且独立复跑 10/10 用例、56 断言通过；同一文件 `:109`（F-02 销项）与 `:116`（F-06 销项）均已把该用例集作为销项证据引用。账本内部自相矛盾。
- 为何成问题：本包目标之一即 backlog 结项归档，该账本是闭环证据链的载体。`docs/workflows/review.md` 判定规则 1 要求「必要验证证据」不得失效；`报告必填字段` 第 4 项要求给出验证证据引用。在交付产物中新写入一条与实际状态相反、且被同文件否证的陈述，使结项证据不可信，命中「缺失的必备行为/未验证的关键路径」（判定规则 2）的证据面。
- 修复建议：改写 `:379` 为已完成状态——登记文件已落地（路径 + `git status` 的 `??` 状态）与 `ctest -R SkillSpecializationStatModifier` / doctest 复跑结果；若担心文件尚未 `git add`，仅说明「工作区已存在、尚未纳入版本控制」，不要表述为「尚未检出」。

### F-2（Low）账本收尾盖戳仍留未回填的证据占位

- 位置：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:8`
- 问题：盖戳段以「（**证据占位**：待填 review 报告路径 / ctest 结果）」结束，交付时仍未回填。
- 为何成问题：该占位为工作流显式约定的交接点（本报告即为回填对象），本身诚实、不构成伪造；但交付态账本尚不自洽，`review.md` 判定规则 5「必要输入缺失」与剩余风险须显式记录之间存在张力。按 `Low` 归类。
- 修复建议：在本报告落盘后，将 `:8` 占位替换为本报告路径 `docs/reviews/2026-09-17-skill-system-final-closure-review.md` 与本审查复跑的 35/35、476 断言及 4 门禁 `EXIT=0` 结果摘要。

### F-3（Low）BeamChannel 清理残留失效的 `is_baked` 判定

- 位置：`src/game/systems/skill/BeamChannelDeliverySystem.cpp:75-77`
- 问题：迁移为 `GetValidBakedSkillProfile` 后，条件仍写作 `profile != nullptr && profile->is_baked && profile->delivery.range > 0.0f`。因新 API 已保证 `is_baked == true`，`&& profile->is_baked` 为恒真死条件。
- 为何成问题：设计 §3.2.1/§3.2.4 的目标是统一「哨兵即不存在」的语义并移除各站点的手工归一化；保留陈旧 `is_baked` 判定会让后续读者误以为该 API 仍可能返回哨兵，弱化新契约（设计 §3.2.1「未烘焙档案一律 null」）。非正确性缺陷。
- 修复建议：简化为 `profile != nullptr && profile->delivery.range > 0.0f`（与同文件其他 7 处保持一致），或在注释中注明该判定为防御性冗余的历史残留。

### F-4（Low）`SkillProfileResolve` 保留第二份 `is_baked` 门

- 位置：`src/game/systems/skill/SkillProfileResolve.cpp:14-21`
- 问题：缓存命中分支手写 `GetBakedSkillProfile(...) + profile->is_baked` 判定，与新增 `GetValidBakedSkillProfile` 完全同义。
- 为何成问题：设计 §3.2.2 显式保留该处（原因：需保留未命中后的回退烘焙路径），故非违规；纯重复、无行为分叉，按 `review.md` 判定规则 6 归 `Low`。双份维护存在未来语义漂移的隐患。
- 修复建议：缓存命中分支可直接以 `GetValidBakedSkillProfile` 取代手写门，回退烘焙逻辑保持不变；若因 const/接口签名原因不便复用，建议在 `:14` 注释中显式标注「与 GetValidBakedSkillProfile 同义，刻意保留」。

### F-5（Low）用例 3/4 的全局域证据仅锁定作用域门的第二份实现

- 位置：`tests/unit/SkillSpecializationStatModifierTests.cpp:152-158`、`:172-194`；对照 `src/game/contracts/impl/StatsSystem.cpp:396-419` 与 `src/game/systems/skill/SkillSystem.cpp:2556-2583`
- 问题：用例 3/4 经 `SkillSystem::CanApplyScopePolicy` 断言 `GlobalAlways` / `GlobalWhileBuffActive`，而非经生产 `GetStatWithTags` 路径。`CanApplyScopePolicy` 与 `StatsSystem` 的局部 `can_apply_scope` 是两份等价实现，故用例未直接证明生产 lambda；F-06 全局域的端到端取值路径无断言。
- 为何成问题：设计 §3.1.3 已声明该作用域门为「已健全、非本包目标」，且资产侧确无 `GlobalAlways` 节点可产生可观测数值，故不构成设计偏离；但作为「F-06 全局域修复通过」的证明存在证据面缺口，须显式登记为剩余风险（`review.md` 判定规则 4 / 出口门禁 5）。
- 修复建议：将 `StatsSystem.cpp` 的 `can_apply_scope` 与 `SkillSystem::CanApplyScopePolicy` 归并为单一定义并复用，使用例 3/4 直接锁定唯一实现；或在资产侧补入一个 `GlobalAlways` 且带 `stat_modifiers` 的测试节点后，追加一条经 `GetStatWithTags` 的端到端断言。

### F-6（Low）UI 直接遍历 `baked_profiles` 未纳入 F-02 统一守卫

- 位置：`src/game/application/ui/GameUiSnapshotBuilder.cpp:406-411`、`src/game/application/ui/PlayerHUD.cpp:144-148`
- 问题：两处按值遍历 `active->baked_profiles`，仅按 `skill_id == 12u` 过滤后读取 `effective_tags`，未过滤哨兵档案。哨兵档案 `skill_id` 非零而 `effective_tags` 为空，故当前不产生错误结果，也不会空指针解引用。
- 为何成问题：设计 §3.2 的标题为「只读档案安全访问守卫统一」，但收敛清单只覆盖 `GetBakedSkillProfile` 直连点，未覆盖该直接遍历路径；属守卫统一的不完整面。当前无可观测缺陷，按 `Low` 登记为契约清晰度/一致性缺口。
- 修复建议：两处遍历追加 `prof.is_baked &&` 谓词，或在 `ActiveSkillsComponent` 上提供 `ForEachBakedProfile` 只读视图统一门禁。

## 最佳实践建议

1. **（对应 F-1/F-2）** 结项归档文档应遵循「先落地、后盖戳、再回填证据」顺序：销项陈述中的每一项可验证断言（文件是否存在、测试是否通过）都应以一次性复跑结果为准，避免在实现落地前后两次表述不一致。建议在盖戳前执行 `git status --short` 与目标用例复跑并留存输出。
2. **（对应 F-3/F-4/F-5）** 消除 `is_baked` 判定的多份实现：以 `GetValidBakedSkillProfile` 为唯一契约入口，`SkillProfileResolve` 的缓存命中分支与 `BeamChannelDeliverySystem.cpp:75` 均改为复用，避免三处语义各自维护。
3. **（对应 F-5）** 将 `StatsSystem.cpp:396-419` 的 `can_apply_scope` 与 `SkillSystem::CanApplyScopePolicy` 合并为单一实现（二者逐行等价），消除作用域策略的双份维护。
4. **（剩余风险 R-2）** 资产不变量用例建议扩展至 `skills.json`，对 (2,202)/(6,653)/(8,800) 等已知重叠采用显式白名单 + 理由注释，使 F-06 激活的全部 10 条 `SkillModifierComponent` 修饰符都处于自动护栏内。
5. `SkillDisplayPreviewService` 的哨兵回退行为（0 消耗/冷却 → 静态值）建议在 UI 变更日志或该函数注释中明确标注为「F-02 后的预期显示变化」，便于后续 UI 验收比对。

## 剩余风险

- **R-1（数值增长）**：F-06 修复后，此前静默失效的 32 个专精节点 `stat_modifiers`、10 条 `SkillModifierComponent` 修饰符与命中 `SwordRiding` 的 `GlobalModifierComponent` 条件词缀将立即生效，技能伤害/攻速数值上升。设计 §5 已将其定性为「GDD 预期的休眠修正」而非数值膨胀，本审查接受该定性，但**未做数值平衡实测**，需玩法侧验收。
- **R-2（skills.json 重叠未纳入不变量）**：用例 10 依计划仅覆盖 `mastery_skill_trees.json` + UMR 表；`skills.json` 中 (2,202)/(6,653)/(8,800) 的「`stat_modifiers` 与 UMR 重叠属合法」这一断言由测试作者口头给出，**本审查未能独立验证其不构成双重计入**。F-06 已使这些 `SkillModifierComponent` 生效，故该重叠路径当前无自动护栏。
- **R-3（全局域端到端未断言）**：见 F-5；F-06 的 `GlobalAlways` 分支无生产取值断言，仅由像素级等价的作用域门用例间接覆盖。
- **R-4（UI 显示变化）**：`GameUiSnapshotBuilder.cpp:846` / `SkillDisplayPreviewService.cpp:25` 处哨兵→`nullptr` 使显示消耗/冷却由 0 回退为静态技能表数值（预览侧叠加装备降耗）。设计 §3.2.2 已授权，但属玩家可见的表层行为变化。
- **R-5（构建门禁未独立复现）**：实现方上报 `build.bat RelWithDebInfo` EXIT 0 / 0 warnings、`ctest -L unit|integration|skill` 全绿。受本次审查约束（禁止运行 `build.bat`），本审查仅独立复现了 doctest 定向套件（35/35、476 断言）与 4 个离线门禁（均 `EXIT=0`）。构建零警告与 ctest 标签结果采信实现方上报。

## 下一步动作

1. **修正 `docs/plans/2026-09-12-skill1-9-followup-backlog.md:379`** 的不实证陈述（F-1），并在 `:8` 回填本报告路径与实测证据（F-2）。这是本包返回 `修改` 的唯一原因，不涉及任何实现文件。
2. 可选（非阻塞）：按 F-3 / F-4 / F-5 / F-6 清理 `is_baked` 冗余判定与 UI 遍历守卫缺口；按最佳实践 3 归并作用域策略双份实现；按最佳实践 4 扩展资产不变量至 `skills.json`。
3. 修正后可发起跟进审查；预期结论为 `提交`（无代码缺陷，仅需核对证据陈述与占位回填）。
4. 补充玩法侧数值验收（R-1），并在 backlog 中显式登记 R-2/R-3 的未验证状态（不新建 Track，仅作为已接受剩余风险的记录）。

---

# 第 2 轮审查（2026-09-17 跟进，仅审查增量）

## 审查目标（本轮）

仅复核第 1 轮发现项 F-1~F-6 的修复增量及其引入的新风险，不重审第 1 轮已判定为「干净」的其余范围。第 1 轮结论 `修改` 的唯一阻塞项为 F-1（Medium）。

## 结论

**提交**

依据：F-1（第 1 轮唯一 `修改` 依据）已修正为与事实一致的陈述，且与本文件 `:109`/`:116` 的 F-06/F-02 销项陈述自洽；F-2~F-5 全部落地；F-6 作为已登记残余被显式接受。第 1 轮已判定「无 Blocker / High、实现文件无需改动」的结论在本轮增量核对后仍然成立。本轮无新增 Blocker / High / Medium。仅余 1 项 Low（N-1，账本计数陈旧，属保守少报而非虚假成功声明），不构成 `review.md` 判定规则 1/2 的阻塞条件。

## 审查轮次

第 2 轮（跟进）。第 1 轮内容保留于本文件上半部分，未删除、未改写。

## 输入（本轮增量）

- 第 1 轮本报告 `docs/reviews/2026-09-17-skill-system-final-closure-review.md`
- 实现方上报的修复清单（F-1~F-6）与本轮新证据
- **本审查独立复跑的证据**（非采信上报）：
  - `git status --short` / `git diff --stat`：边界现为 **22 modified + 4 untracked**（较第 1 轮新增 `src/game/systems/skill/SkillProfileResolve.cpp`、`src/game/systems/skill/SkillProfileResolve.hpp`、`tests/unit/SkillWrapupHardeningTests.cpp` 三个 tracked 修改；untracked 新增第 1 轮报告本身）
  - `bin\NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*"` → **11 用例 / 68 断言，0 失败，SUCCESS**（第 1 轮为 10/56）
  - `bin\NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*,*SkillProfileResolveSentinel*,*SkillWrapupHardening*"` → **17 用例 / 97 断言，0 失败，SUCCESS**
  - 随机序防污染复跑：`--order-by=rand` 在 seed `1,3,7,11,42` 下 SMM 套件均 11/11；扩展集 `*SkillSpecialization*,*SkillWrapup*,*SkillProfileResolve*,*SkillSpecializationBaker*` 在 seed `2,9,23` 下均 **54 用例 / 784 断言全绿**；并对 seed `0..40` 扫描，**seed 25 / 28 使用例 11 排在首位**，仍 11/11 通过（见「用例 11 污染链核验」）
  - 四道离线门禁独立复跑全部 `EXIT=0`（与第 1 轮同）：`gen_skill_contracts.py --check --check-idempotency --check-determinism`（`[OK] skill_contract blocks are up to date.`）；`sync_skill_node_icon_ids.py --check`（`0 updated / 76 unchanged / 0 missing`）；`validate_skill_spec_modifiers.py --check`（`6/6 passed`）；`gen_skill_mechanics_schema.py --check`（`437 entries, 62 unreferenced`）
  - **未复跑**（禁止运行 `build.bat`）：`build.bat RelWithDebInfo` 与 `ctest -L unit|integration|skill`、`-R nmd.tests.ci.nonperf`，采信上报

## 变更文件边界（本轮增量）

```
新纳入 tracked 修改：
 M src/game/systems/skill/SkillProfileResolve.cpp
 M src/game/systems/skill/SkillProfileResolve.hpp
 M tests/unit/SkillWrapupHardeningTests.cpp
untracked 新增：
 ?? docs/reviews/2026-09-17-skill-system-final-closure-review.md   （第 1 轮本报告）
```
第 1 轮其余全部变更文件未回退、未追加无关改动；`AttributePipeline.cpp` 仍未被触碰（设计非目标保持）。本轮直接检视的增量文件：上列 3 个 tracked 修改 + `tests/unit/SkillSpecializationStatModifierTests.cpp`（重读全文）+ `tests/TestCommon.hpp`、`src/game/foundation/data/SkillRegistry.cpp`、`src/game/contracts/impl/StatsSystem.cpp`（读证链路）。

## 第 1 轮发现项处置

| 发现项 | 严重度 | 处置 | 证据 |
| --- | --- | --- | --- |
| F-1 账本 §10 已证伪陈述 | Medium | **已解决** | `:379` 改写为「已落地、尚未纳入版本控制」，并显式声明「不得据本条表述为『尚未检出/未实现』」；与 `:109`/`:116` 销项自洽 |
| F-2 盖戳证据占位 | Low | **已解决** | `:8` 占位替换为审查报告路径 + build/门禁/ctest/doctest 实测结果；正确标注第 1 轮结论 `修改` 与「第 2 轮待追加」 |
| F-3 BeamChannel 死条件 | Low | **已解决** | `BeamChannelDeliverySystem.cpp:75-76` 移除 `&& profile->is_baked`，仅保留 `profile != nullptr`；解引用仍在守卫内，无新空指针风险 |
| F-4 SkillProfileResolve 重复判定 | Low | **已解决** | `SkillProfileResolve.cpp:16-19` 改用 `GetValidBakedSkillProfile`；`SkillProfileResolve.hpp:19` 注释同步。等价性已独立复验（见下） |
| F-5 用例 3/4 证据面缺口 | Low | **已解决** | 测试头 `:17-31` 显式分级可证伪证据；用例 4 新增 stat 层缓存刷新断言；新增用例 11 覆盖 `required_tags != Tag::None` 两侧 |
| F-6 UI 直连遍历哨兵 | Low | **接受（不修改）** | 与实现方裁决一致：属 backlog §1.6 已登记的既有残余，哨兵 `effective_tags` 为空故无可观测缺陷；本审查同意并转入剩余风险 R-1 |

### F-4 等价性独立复验（含槽位遮蔽场景）

- 旧实现：`GetBakedSkillProfile(...)` 后嵌套 `if (profile->is_baked) return profile;`（见 `git diff` 原行）。
- 新实现：`GetValidBakedSkillProfile(...)` 后 `return profile;`；而新 API 定义（`SkillSystem.cpp:2782-2786`）为 `GetBakedSkillProfile(...)` 后附 `profile->is_baked` 判定。
- 两者均为「**首次匹配**同 `skill_id` 的缓存项 → 再做 `is_baked` 门」。**槽位遮蔽场景**（slot0 为哨兵、slot1 为合法档案、同 `skill_id`）：二者都由首次匹配拿到 slot0 哨兵、`is_baked==false`，故都**不返回**，随后走同一段 `fallbackSpec` → `specialized_slots` 回退（`:21-37`），行为逐字一致。指针身份亦一致（都返回缓存内同一 `BakedSkillProfile*`）。判定：**等价证明成立**，无行为分叉。

### 用例 4 缓存刷新断言的真正可证伪性（硬核验）

断言链：读取并缓存 buffed 值（`:252-255`）→ 关闭窗口 → `emplace<StatsDirty>` → `StatsSystem::update`（`:259-262`）→ 复读要求等于 100 且严格小于 buffed（`:264-267`）。

对生产代码逐环核对（非推断）：
- `GetStatWithTags` 确有缓存，键为 `hash(type, tags, skill_id, source_entity)`，外层按 `entity_id` 存于 `s_tagStatCache`（`StatsSystem.cpp:104-131`）。缓存键**不含** `PhantomTranceComponent.remaining`，故窗口值的唯一失效机制就是清缓存。
- `Recalculate` 先 `ClearCache` 再 `AttributePipeline::Calculate`（`StatsSystem.cpp:100-103`）。
- `update` 经 `registry.group<StatsDirty, CombatStats>()` 逐实体调用 `Recalculate`，随后 `registry.clear<StatsDirty>()`（`StatsSystem.cpp:545-554`）。
- `ClearCache` 执行 `s_tagStatCache.erase(entity_id)`（`StatsSystem.cpp:620-624`）。

因此若移除 `ClearCache`（或 `update` 不触发重算），窗口关闭后复读将命中旧缓存返回 130，`CHECK(reverted == Approx(baseline=100))` 与 `CHECK(reverted < buffed=130)` **双双失败**。判定：断言**真实且对缓存刷新可证伪**。✅

### 用例 11 污染链核验（硬核验，逐环）

用例 11（`:516-556`）经 `SkillRegistry::Get().GetMutableSkillTree(kMasterySkillId)`（`:527-528`）**原地改写进程级全局技能树**节点 1000 的 `stat_modifiers`（`:532-533`）。链核验：

1. `TestSetupScope` 构造调用 `ResetSkillRegistries()`（`tests/TestCommon.hpp:82`），**析构也调用**（`:89`）——构造与析构各一次，包围每个用例。
2. `ResetSkillRegistries()` → `SkillRegistry::Get().LoadFromJson("assets/data/skills.json")`（`tests/TestCommon.hpp:183-190`）。
3. `LoadFromJson` 在重载前执行 `skills_.clear(); skill_trees_.clear(); skill_contracts_.clear();`（`SkillRegistry.cpp:677-679`）——**整个技能树容器被清空**，故节点 1000 的夹具被丢弃而非合并。
4. 随后 `LoadMasterySkillTrees(*mastery_path, ...)`（`SkillRegistry.cpp:715`）重读 `mastery_skill_trees.json` 并按 `RegisterSkillTreeAndContract` 重建 `skill_trees_`。
5. `GetMutableSkillTree` 返回 `skill_trees_` 内元素指针（`SkillRegistry.cpp:771-777`），其生命周期与容器一致；用例 11 仅在用例内使用该指针，作用域析构时容器清空——**无跨重载持有指针**，不构成 use-after-free。

**经验证据**：随机序 seed 25 / 28 将用例 11 置于**首位**，其后用例 1/2/5/6（均依赖节点 1000 的原始 `+5/点` 数据）仍全通过 → 夹具确未泄漏。判定：**污染链证明成立**，无跨用例污染。✅ 同一机制亦覆盖用例 4 对技能 9 节点 991 的夹具注入（用例 4 之后的用例 5-10 均通过）。

### 用例 11 的 F-06 可证伪性

夹具 `required_tags = Tag::SwordRiding`（`:532-533`）。修复前旧逻辑：`is_baked` 初值 `false`，继而 `if (!is_baked && player_tags != Tag::None) is_baked = HasTag(player_tags, required_tags)`；御剑架势下 `player_tags` 含 `SwordRiding` ⇒ `is_baked = true` ⇒ 跳过 ⇒ 期望的 140 落空（得 100）⇒ **修复前必红**。不匹配侧（步行）期望 100，修复前后一致，用例内注释已如实标注为「契约锁定而非 F-06 证据」。✅

### F-1 自洽性复核结论

`:379` 现述「已随本轮交付存在于工作区（`git status --short` 显示 `?? …`）、经 GLOB 重新配置编入套件、定向复跑通过、尚未 `git add`」，与 F-06 销项（`:116` 段）「新增回归用例…由 `tests/CMakeLists.txt` 自动收集」在**存在性**上完全自洽，且与实际状态（文件存在、已编入、通过）一致。第 1 轮的「事实相反」缺陷已消除。

## 本轮新增发现项

### N-1（Low）账本用例/断言计数相对交付态陈旧（保守少报）

- 位置：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:8`、`:379`、`:116` 段
- 问题：三处计数与当前交付态不一致——
  - `:8` 记「定向 doctest … → **16 用例 / 85 断言**」（第 1 轮快照；交付态定向三件套为 **17 用例 / 97 断言**）；
  - `:379`（§10）记「定向复跑 … 为 **10/10 用例通过**」（交付态 SMM 套件为 **11/11**）；
  - F-06 销项段记「新增 **10 项**回归用例」（交付文件实为 **11** 个 `TEST_CASE`）。
- 为何成问题：与第 1 轮 F-1 同类（证据陈述与交付态脱节），但性质 materially 更轻——第 1 轮是「断言不存在」的**虚假否定**，本条是**保守少报**（把已交付的 11 说成 10、把 97 说成 85），既未虚报成功也未掩盖失败。按 `review.md` 判定规则 1/2 均不构成阻塞，归 `Low`。
- 修复建议：在回填第 2 轮证据时同步为 `11/11`、`17 用例 / 97 断言`、`新增 11 项回归用例`，并在 §10 注明「第 2 轮新增用例 11」。

## 最佳实践建议（本轮增量）

1. **（对应 F-5 复核）** 测试头 `:25-27` 把用例 4 整体归入「邻接契约锁定（不是 F-06 回归证据）」，但用例 4 末尾 `:250-266` 的 stat 层断言经 `GetStatWithTags` 真实取值，且**修复前必红**（夹具 `required_tags==None` 会被旧启发式跳过，`buffed` 得 100 而非 130）。建议把用例 4 的 stat 层半边重新归类为「F-06 可证伪证据（`GlobalWhileBuffActive` + 缓存刷新）」，使证据分级与实际强度一致（当前为保守少报）。
2. **（对应用例 11）** 用例 11 现以注释声明「TestSetupScope 会在下个用例前重载技能表，不跨用例泄漏」。建议在用例起始处追加一条对**原始**数据的自校验（如 `REQUIRE` 节点 1000 原始 `stat_modifiers` 尺寸/取值符合资产），使「重载契约」由断言而非注释保证，未来若有人删除 `TestCommon.hpp:89` 的析构复位可被本用例捕获。
3. **（承接第 1 轮）** 归并 `StatsSystem.cpp:396-419` 与 `SkillSystem.cpp:2556-2583` 的两份等价作用域谓词。
4. **（承接第 1 轮 R-3）** 将资产不变量用例扩展至 `skills.json`，对 (2,202)/(6,653)/(8,800) 采用显式白名单 + 理由注释。

## 剩余风险（本轮更新）

- **R-1（UI 直连遍历哨兵，已接受）**：`GameUiSnapshotBuilder.cpp:406-411`、`PlayerHUD.cpp:144-148` 维持既有行为（backlog §1.6 已登记残余）。本审查同意不修改的理由：哨兵 `effective_tags` 为空，读取结果与「无元素」等价，无可观测缺陷；且收紧过滤可能掩盖合法的未烘焙槽位。风险等级：可接受。
- **R-2（`GlobalAlways` 端到端仍未断言）**：见第 1 轮 F-5/剩余风险；资产侧无 `GlobalAlways` 节点，属设计 §3.1.3 非目标。**本轮已部分收窄**：`GlobalWhileBuffActive` 的 stat 层取值已由用例 4 经 `GetStatWithTags` 真实断言（`:250-266`），不再仅由谓词覆盖。
- **R-3（skills.json 重叠未纳入不变量）**：用例 10 依计划仅覆盖 `mastery_skill_trees.json` + UMR 表；(2,202)/(6,653)/(8,800) 的「合法重叠」仍**未独立验证**，F-06 已使这些 `SkillModifierComponent` 生效，故该路径无自动护栏。
- **R-4（数值增长）**：F-06 激活的 32 节点 + 10 条技能修饰符 + `SwordRiding` 条件词缀带来的数值上升，设计 §5 定性为 GDD 预期修正；**未做玩法侧平衡实测**。
- **R-5（UI 显示变化）**：哨兵→`nullptr` 使显示消耗/冷却由 0 回退为静态值，设计 §3.2.2 已授权，属玩家可见表层变化。
- **R-6（构建/ctest 门禁未由本审查独立复现）**：受「禁止运行 `build.bat`」约束，`build.bat RelWithDebInfo`（0 错 0 警）与 `ctest -L unit|integration|skill`、`-R nmd.tests.ci.nonperf` 采信上报；本审查独立复现了 doctest 计数（11/11、68 断言；三件套 17/97）、随机序防污染复跑与 4 道离线门禁（均 `EXIT=0`）。
- **R-7（账本计数陈旧）**：见 N-1；若以当前文本归档，账本记录的覆盖面比交付态少 1 个用例（保守少报），建议随第 2 轮证据回填一并修正。

## 下一步动作

1. **非阻塞（文档收尾）**：将 `docs/plans/2026-09-12-skill1-9-followup-backlog.md:8`、`:379`、§1.3 F-06 销项段的计数修正为 `11/11`、`17 用例 / 97 断言`、`新增 11 项回归用例`（N-1 / R-7），并把第 2 轮结论 `提交` 与随机序防污染复跑记入 `:8` 盖戳。
2. 可选（非阻塞）：按最佳实践 1 调整用例 4 的证据分级；按最佳实践 2 为用例 11 增加原始数据自校验；按最佳实践 3/4 归并作用域谓词、扩展资产不变量。
3. 玩法侧验收 R-4 数值增长。
4. 本包**可提交**：无 Blocker / High / Medium 剩余，N-1 为文档计数陈旧（Low、保守少报），不阻塞签核。
