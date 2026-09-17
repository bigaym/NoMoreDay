# UMR 技能专精改造第二批（技能 4/5/6）审查报告 · 第 3 轮

- 系统代号：`UMR-SKILL-BATCH-2`
- 审查日期：2026-09-17
- 审查标准：`docs/workflows/review.md`
- 代码硬规则来源：`conductor/code_standard.md` (V2.1)
- 触发条件：外部 OCR 审查文件 `review.md`（工作区根目录）提出 12 条发现，随后实施修复；本文件为针对该 12 条修复的独立审查（新建文件，不覆盖第 1/2 轮小节）。

---

## 1. 审查目标

对「针对外部 `review.md` 12 条发现的修复」做独立、逐条、可证伪的审查，覆盖：

- 技能 4「剑气护体」：节点 471 fail-closed 定案的论证是否成立、470 回退读取的钳制与守卫、ward 持续时间单源化；
- 技能 5「万剑归宗」：锁定半径与下落弹速由「数值 sentinel」改为「`feature_flags & 16` 语义门控」的等价性与语义正确性；
- Baker：删除重复/无效写入后烘焙结果是否改变；
- 测试：470 双路径区分度、471 回归断言、超 `max_points` 穷举值修正、603 未点场景与负值钳制用例的有效性；
- 设计/计划新增条目与代码/测试的一致性。

判定对象为「12 条发现是否被真实修复，且未引入新缺陷」，不针对未涉及的其他技能节点。

## 2. 结论

**提交**

无 Blocker / High / Medium 级问题；无范围泄漏；无缺失的必备行为；12 条发现全部真实修复并具备可证伪的回归守护。新增 2 条 Low（注释准确性）与 2 条可维护性观察项，均为非阻塞。

## 3. 审查轮次

第 3 轮（新建独立文件）。第 1 轮（结论 `提交`）与第 2 轮（跟进，结论 `提交`）见 `docs/reviews/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-review.md`，其 F1–F4 已全部关闭。

## 4. 输入

| 类型 | 路径 |
|---|---|
| 外部审查 | `review.md`（工作区根目录，既有输入，本轮未改动） |
| 设计 | `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md` |
| 计划 | `docs/plans/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-plan.md` |
| 前序报告 | `docs/reviews/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-review.md`（第 1/2 轮） |
| 标准 | `docs/workflows/review.md`、`conductor/code_standard.md` (V2.1)、`AGENTS.md` |
| 代码 | `src/game/systems/skill/behaviors/{BladeWard.cpp,BladeWardRuntime.hpp}`、`src/game/systems/skill/{BeamChannelDeliverySystem.cpp,SkillProfileResolve.cpp,SkillSpecializationBaker.cpp,ShadowDuplicationHook.cpp,SkillSystem.cpp}`、`src/game/systems/modifier/{ModifierEvaluator.cpp,ModifierRuntimeTypes.hpp,ModifierContext.hpp}`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/combat/damage/DamageInterceptors.hpp` |
| 数据 | `assets/data/skills.json`、`assets/data/skill_mechanics.json` |
| 测试 | `tests/functional/{Skill4FollowupTests.cpp,SwordArrayNodes.cpp}`、`tests/unit/SkillSpecializationBakerTests.cpp`、`tests/python/SkillSpecBatch2GateTest.py`、`tests/TestCommon.hpp` |
| 脚本 | `scripts/validate_skill_spec_modifiers.py` |

审查过程只读：未修改任何实现代码、数据、脚本或测试；唯一写入为本报告文件。

## 5. 变更文件边界（实测 `git status --short` / `git diff --stat`）

- **已跟踪修改（22 项）**：`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`、`assets/data/modifier_v2/skill_spec_modifiers.json`、`assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json`、`scripts/gen_modifier_runtime_v2.py`、`scripts/migrate_skill_spec_modifier_slice.py`、`scripts/validate_skill_spec_modifiers.py`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/combat/damage/DamageInterceptors.hpp`、`src/game/systems/modifier/ModifierContext.hpp`、`src/game/systems/modifier/ModifierEvaluator.cpp`、`src/game/systems/modifier/ModifierEvaluator.hpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/BladeWard.cpp`、`src/game/systems/skill/behaviors/BladeWardRuntime.hpp`、`tests/functional/InfiniteBladesNodes.cpp`、`tests/functional/Skill4FollowupTests.cpp`、`tests/functional/Skill5FollowupTests.cpp`、`tests/functional/SwordArrayNodes.cpp`、`tests/unit/SkillManaCostSettlementTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`。
- **未跟踪新增（5 项）**：`docs/designs/…-design.md`、`docs/plans/…-plan.md`、`docs/reviews/…-review.md`、`tests/python/SkillSpecBatch2GateTest.py`、`tests/unit/SkillBatch2DeliveryOpTests.cpp`。
- `git diff --stat`：`22 files changed, +2192 / -270`（相对第 2 轮 `+2032 / -267`，本轮净增 `+160 / -3`，与「小范围 12 条修复」规模相符）。
- 文件集合与第 1/2 轮完全一致，**无越界新增文件**；工作区同时包含上一阶段 Batch2 主体改动，本轮审查聚焦其上叠加的 12 条修复。
- 环境警告：多文件 LF→CRLF（`git` 提示），无功能影响。

## 6. 范围对齐：12 条发现的逐条独立核验

> 以下「修复前」行为均取自 `review.md` 原文，不采信修复方自述；「证据」列全部为本次实读/实跑所得。

| # | review.md 发现（摘要） | 独立核验结论 | 证据（文件:行号） |
|---|---|---|---|
| 1 | 471 未烘焙退化 0 与 470 不对称，缺论证 | **已修复，且论证成立**（详见 §6.1） | `BladeWard.cpp:144-153`、`SkillProfileResolve.cpp:9-39`、`ShadowDuplicationHook.cpp:21-26`、`SkillSystem.cpp:408/1577/1720` |
| 2 | `SkillSpecBatch2GateTest.py` 循环未用变量 `node` | **已修复** | `tests/python/SkillSpecBatch2GateTest.py:112`（`for record_id, skill, _node, ...`） |
| 3 | 索敌半径 `>0.0f` sentinel 应改 `feature_flags&16` | **已修复，三情形数值等价** | `BeamChannelDeliverySystem.cpp:1135-1146`、`SkillSpecializationBaker.cpp:92-99/205-209/610-612` |
| 4 | 下落弹速同语义门控 | **已修复，等价；Baker 确置位 16** | `BeamChannelDeliverySystem.cpp:1197-1203`、`SkillSpecializationBaker.cpp:610-612` |
| 5 | 470 float→uint8_t 窄化缺钳制、守卫语义 | **已修复** | `BladeWard.cpp:137-143`（`std::clamp(...,0.0f,255.0f)`）、`BladeWard.cpp:508-530`（`count == 0` 守卫 + 注释）、`BladeWardRuntime.hpp:23-24` |
| 6 | 470 parity 两路径同断言 5，无区分度 | **已修复，具备证伪力** | `tests/functional/Skill4FollowupTests.cpp:661-748`（临时机制表 7；cached 断言 5 `:693`、fallback 断言 7 `:710`） |
| 7 | 超 `max_points` 断言不可达 | **已修复，逐节点核对 `max_points` 与算术均正确** | `tests/unit/SkillSpecializationBakerTests.cpp`（510/511/554/610/611 五条 subcase）、`assets/data/skills.json` |
| 8 | Baker case5 重复写 `effective_mana_cost=20` | **已删除且等价** | `SkillSpecializationBaker.cpp:37-40`（步骤 1 覆写）、`:92-99`（case5 无再次写入） |
| 9 | 技能 4 duration 三份副本（行为层硬编码） | **已单源化，行为等价** | `BladeWard.cpp:66-68`（`wardDuration`）、`:78-79`、`:120`；`SkillSpecializationBaker.cpp:87` |
| 10 | 603 未点场景传 `nullptr` 使 Bake 跳过专精+UMR | **已修复** | `tests/functional/SwordArrayNodes.cpp:293-298`（`noNodes.skill_id = kSkillId`，断言 mana 30 / range 400） |
| 11 | case4 no-op `more_damage_mult = 1.0f` | **已删除且等价** | `SkillSpecializationBaker.cpp:59`（步骤 1 初始化）、`:84-91`（case4 无写入） |
| 12 | Baker `del.speed/duration` 的 `max(0,...)` 无负输入触发 | **已修复，用例可真红** | `tests/unit/SkillSpecializationBakerTests.cpp:97-139`（`BuildDeliveryRuntimeBlob`）、`:2004-2033`（用例） |

### 6.1 疑点专核（针对任务点名的高风险点）

1. **471 fail-closed 论证的独立验证（核心）**：结论 **成立**。
   - 调用点 `BladeWard.cpp:47-48`：`ResolveBakedProfile(registry, owner, kSkillId, localProfile)` —— 第 5 参 `fallbackSpec` 取默认 `nullptr`。
   - `SkillProfileResolve.cpp:9-38`：步骤 1 缓存命中即返回；步骤 2 仅当 `fallbackSpec != nullptr` 才烘焙（本调用无）；步骤 3 扫描 `active->specialized_slots` 中 `skill_id == skillId` 者即时烘焙；否则 `:38 return nullptr`。**因此 `profile == nullptr` 当且仅当「无缓存档案 且 无技能 4 专精槽」。**
   - `exec.active_nodes` 的全部写入来源（全仓库检索）：`SkillSystem.cpp:408`（`PopulateActiveNodesFromSpecialized`，由专精槽填充，`:397-411`）、`:1577`（`exec.active_nodes = sc->snapshot.active_nodes`，影子路径）、`:1720`（`TriggerCast`，经 `FindSpecializedSkillContext` `:1709-1724`）。`TryCast`（`:2124-2136` → `FindSpecializedSkill` `:343-359`）与触发链（`:967-972` → `FindSpecializedSkill(*active, trigger_skill_id, -1)`）的 active_nodes 均来源于与 `exec.skill_id` 匹配的专精槽，与档案缓存同源，**不存在「有 active_nodes 而无专精槽」的非影子入口**。
   - 影子入口：`snapshot` 仅由 `SkillSystem.cpp:1638`（`SpawnShadowEcho`，全仓库唯一调用者 `FlowingThrust.cpp:113/121/123/126`，技能 1）与 `ShadowDuplicationHook.cpp:48-70` 写入；`ShadowSystem.cpp:22` 仅以 `shadow.snapshot.skill_id` 施放。`ShadowDuplicationHook.cpp:21-26` 对 `Tag::Movement/Buff/Aura/Channeled` 一律 `return {}` 排除，而 `assets/data/skills.json` 技能 4 的 `tags` 含 `Buff`（技能 5 含 `Channeled`），**故技能 4 无法经影子入口带着 active_nodes 进入 DoCast**。
   - 结论：未烘焙分支的 `0.0f` 相对修复前 `nodePoints(471, specState.vengeancePoints)` 无行为回退；470 保留机制表回退的结构性理由亦成立（`DamageInterceptors.hpp:73-74` 仅持有组件、只能读 `ward.counter_sword_count`）。属「有意且已注释」的不对称，**非缺陷**。
2. **470 parity 测试是否真的能捕获回归**：**能**。
   - 临时机制表将 `4/470.counter_swords` 置 `7`（`Skill4FollowupTests.cpp:678-680`），并 `REQUIRE` 加载成功（`:682-683`）。
   - 若删除 DoCast 中档案分支：cached 子用例将读到 7 而断言 5 → **红**（`:693`）；若整体删除赋值：`get_or_emplace<BladeWardComponent>` 的默认 `5`（`SkillDefs.hpp:974`）使 cached 子用例仍绿，但 fallback 子用例期望 7 而实得 5 → **红**（`:710`）。两条子用例合并构成完整守护。
   - 临时表是否会被 `TestSetupScope`/加载顺序破坏：**不会**。`TestSetupScope` 构造（`TestCommon.hpp:63-72`）后测试体才加载临时表，子用例期间无其他代码写入机制表；析构（`:73-80`）再 `ResetSkillRegistries()` 清空，故不会污染后续用例（`EnsureSkillMechanics` 每次显式重载真实表，`Skill4FollowupTests.cpp:37-45`）。
3. **Fix7 点数上限与算术逐项核对**（`assets/data/skills.json` 实测）：技能 5 节点 510 `max_points=1`、511 `=3`、554 `=1`；技能 6 节点 610 `=1`、611 `=1`。用例取 510:1→`20*1.30=26.0`、511:3→`450*1.45=652.5`/`1000*1.75=1750`、554:1→`1.0`、610:1→`0.85`、611:1→`30*1.30=39.0`，**全部可达且算术正确**。另核 402 `max_points=3`（用例取 1/2/3）、470 `=1`、471 `=4`（471 用例取 3，可达）。
4. **Fix12 是否真能捕获「删除 max 包装」的回归**：**能**。
   - 合成 blob（`BuildDeliveryRuntimeBlob`，`SkillSpecializationBakerTests.cpp:97-139`）仅含 oper 41 `SKILL_DURATION_FLAT`（`param_u32=6`）与 42 `SKILL_SPEED_MULT`（`param_u32=6`），参数分别为 `-100.0f` / `-2.0f`；注入后 duration `=5.0+(-100*1)=-95`、speed `=300*(1+(-2*1))=-300`，二者均需 `std::max(0.0f, ...)`（`SkillSpecializationBaker.cpp:208-209`）才为 0，删除包装两断言必红（`:2028-2029`）。
   - 结构对照：`ModifierRuntimeTypes.hpp` 的 header/record/filter/op 尺寸与偏移、`opcode/reserved/param_u32/param_f32` 字段名，与该文件既有 `BuildManaRuntimeBlob`（`:48-91`）完全同构，偏移 64/88/136/160 与计数、`crc32=0` 之约定一致，**写法正确**。
   - 该用例非「空跑」：若注入未生效，duration/速度分别为 5.0/300，断言会失败；实跑通过即证明注入生效（见 §10）。
   - 速度偏移取 `-2.0f` 的修正是**正确**的：技能 6 基准 speed 并非 1000，而是 `BakedDeliveryParams` 默认 `300`（`SkillDefs.hpp:630-634`），Baker case6（`SkillSpecializationBaker.cpp:100-105`）不写 speed；若取 `-1.0f` 则乘性系数为 0、乘积为 0，无法区分「钳制生效」与「未施加下限」，取 `-2` 才能真正触发负值路径。用例注释（`:2012-2013`）已说明该动机。
5. **Fix8/Fix9/Fix11 的删除与单源化是否改变烘焙结果**：**不改变**。
   - 步骤 1（`SkillSpecializationBaker.cpp:37`）先写 `effective_mana_cost = skillData->mana_cost`，`:38-40` 对技能 5 覆写为 `20.0f`；case5（`:92-99`）删除后无其他写入点，等价。
   - `more_damage_mult` 在 `:59` 初始化为 `1.0f`，case4（`:84-91`）现无写入，步骤 3（`:200`）前无其他写入，删除为 no-op。
   - `del.duration` 对技能 4 仅在 `:87` 写 `10.0f`，且无任何 `SKILL_DURATION_FLAT` 的 canonical 记录作用于技能 4（`del.duration` 的其余写入点：`:69/102/119/125/209/857` 均属其它技能或步骤 3），故 `BladeWard.cpp:66-68` 改读 `profile->delivery.duration` 后行为恒为 `10.0f`，与替换前等价。
6. **Fix3/Fix4 三情形数值等价**（`GetSkill5Point` 与 Baker 同源：`GetSkillPoint` `BeamChannelDeliverySystem.cpp:38-49` 读 `specialized_slots` 的 `ReadPoints`，Baker 亦由该专精烘焙）：
   - 未点 511：旧 `speedMult=1 → 1000`、`lock_range=450`；新 `flags&16==0 → 1000/450`。**等价**。
   - 已点 511 N 点：旧 `1000*(1+0.25N)` / `450*(1+0.15N)`；新读 `profile->delivery.speed` / `.range`，其值正是步骤 1 基准乘步骤 3 系数后的同一表达式。**等价**。
   - 未烘焙（profile `nullptr`）：旧 profile 为空 → `1000/450`；新 `profile==nullptr` → `1000/450`。**等价**。
   - 且新门控以语义位取代 `>0.0f`，不再把合法 0 值静默替换为基准，消除了 review.md 指出的 sentinel 退化，与设计 `:384` / 计划 `:201` 一致。

## 7. 质量与风险评估

- **内存安全（code_standard §2.2 / §5.2）**：本批新增/改动无裸 `new/delete`、无 `malloc`、无裸拥有指针；`SpawnBladeWardCounterSwords` 保持 `registry.valid(owner)`、`all_of<Position>`、`count == 0` 三重前置守卫（`BladeWard.cpp:508-530`）。**无风险**。
- **未定义行为（§6.1 转换）**：`BladeWard.cpp:140-143` 以 `std::clamp` 钳制后再 `static_cast<uint8_t>`，当前数据域（`5.0`）安全；NaN 情形见 F1（Low，注释层面）。**无实质风险**。
- **热路径（§2.1 / §7.2）**：两处改动均由「运行帧读取专精组件 + 查机制表二次乘算」改为「读取已烘焙档案字段」，指令数与查表次数下降；无热路径字符串比较、无热路径堆分配。**无风险**。
- **ECS / 并发（§5.3 / §8.1）**：`wardDuration` 与 `counter_sword_count` 均为值拷贝；无跨增删组件持有 EnTT 指针；无新增 `std::thread`。**无风险**。
- **测试卫生**：470 parity 用例在测试体末尾恢复真实机制表（`Skill4FollowupTests.cpp:744-746`），并且 `TestSetupScope` 析构（`TestCommon.hpp:73-80`）会二次清空机制表，双重保护（见 F2）。**无泄漏风险**。
- **无重复轮子（出口门禁）**：`wardDuration` 复用既有 `BakedDeliveryParams.duration` 字段（`SkillDefs.hpp:630-634`），未新建字段或平行容器；`feature_flags & 16` 复用既有 511 语义位（`SkillSpecializationBaker.cpp:610-612`）；`BuildDeliveryRuntimeBlob` 复用既有 `BuildManaRuntimeBlob` 结构。**通过**。
- **范围泄漏**：无新增文件、无越界改动，改动集中在 6 个源文件/测试文件。**通过**。

## 8. 发现项

### F1 — Low（注释准确性）：`BladeWard.cpp:136` 关于 NaN 的行为描述与 `std::clamp` 实际语义不符

- `src/game/systems/skill/behaviors/BladeWard.cpp:136`：注释「机制表浮点值在窄化前钳制到 [0,255]，负值/**NaN** 收敛为 0（不生成反击剑气）」。
- `src/game/systems/skill/behaviors/BladeWard.cpp:140-143`：`static_cast<uint8_t>(std::clamp(mechanics.GetFloat(...), 0.0f, 255.0f))`。

**为何成为问题**：`std::clamp(v, 0.0f, 255.0f)` 对 `v = NaN` 的两次比较（`v < lo`、`hi < v`）均为 false，返回 NaN 本身；随后 `static_cast<uint8_t>(NaN)` 属未定义行为。负值确实收敛为 0，**但 NaN 不会**。当前机制表为 JSON 数值、无 NaN 来源，故无实际触发路径；属注释过度承诺。若后续改为从二进制/运行时路径灌入浮点，注释会误导维护者认为已有防护。

**修复建议**：把注释改为只声明负值收敛，并显式指出 NaN 未被防护；或改为 `const float raw = mechanics.GetFloat(...); const float safe = std::isfinite(raw) ? std::clamp(raw, 0.0f, 255.0f) : 0.0f;` 后收窄。

### F2 — Low（可维护性）：470 parity 用例的机制表恢复为「冗余但无害」，且失败时临时文件不清理

- `tests/functional/Skill4FollowupTests.cpp:744-747`（测试体末尾 `ResetForTests()` + `LoadFromFile(真实表)` + `remove`）。
- `tests/TestCommon.hpp:73-80`（`TestSetupScope` 析构再次 `ResetSkillRegistries()`，机制表本就会被清空）。
- `tests/functional/Skill4FollowupTests.cpp:37-45`（`EnsureSkillMechanics()` 每次用例显式重载真实表）。

**为何成为问题**：由于 `TestSetupScope` 析构必然清空机制表、且每个依赖机制表的用例都以 `EnsureSkillMechanics()` 重载，`:744-746` 的恢复对「防跨用例污染」无净效果；它唯一的作用是让测试体在自身范围内回到干净状态。真正值得注意的是：若某个子用例的 `REQUIRE` 失败，异常会跳过 `:747` 的 `remove`，在 `%TEMP%\nmd_skill4_470_parity\counter_swords_7.json` 留下残留文件（每次运行覆盖写，偶发交叉、不影响正确性）。当前写法已由注释说明意图，属可接受；建议把清理改为 RAII 作用域守卫。

### F3 — Best Practice（可维护性，非缺陷）：未分配 511 时的弹速基准 `1000.0f` 仍是硬编码副本

- `src/game/systems/skill/BeamChannelDeliverySystem.cpp:1199`：`float finalSpeed = 1000.0f;`
- `src/game/systems/skill/SkillSpecializationBaker.cpp:93`：Baker 侧技能 5 基准 `del.speed = 1000.0f`。

**说明**：索敌半径一侧已改读机制表 `5/510 lock_range`（`BeamChannelDeliverySystem.cpp:1135-1137`，默认 450），无副本漂移；弹速一侧的「未分配 511」分支仍是字面量 `1000.0f`，与 Baker 基准构成两份副本。此为修复前既有形态（修复前同样硬编码 1000），本轮未引入回归；设计 `:384` 亦显式声明「未分配 511 时沿用基准 450/1000」。若后续调整 Baker 基准，需同步此处，建议提取为具名常量或复用机制表基准。

### F4 — Low（注释准确性）：Fix12 用例注释中的乘法笔误

- `tests/unit/SkillSpecializationBakerTests.cpp:2010`：`//   duration 增量 = -100 * 1 = -95（基准 5.0 + (-95)），`。

**为何成为问题**：`-100 * 1` 应为 `-100`（净结果 `5.0 + (-100) = -95` 是正确的，但式子写错），易误导读数者按注释推断增量语义。功能与断言均无误。

**修复建议**：改为「duration 增量 = -100 * 1 = -100（基准 5.0 → -95）」。

> 未发现 Blocker / High / Medium 级问题；未发现假测试/恒真断言、既有用例被删除或旁路、越权、隐藏失败、伪造证据、UB/UAF/泄漏、热路径字符串分支、跨增删组件持有 EnTT 指针、裸 `new/delete`、无注释 C 式转换、裸 `std::thread`、热路径堆分配等硬否决项。12 条修复均未削弱既有断言（改动均为新增或修正不可达断言，方向为「更强」）。

## 9. 最佳实践建议（可执行）

本包结论为 `提交`，无强制整改项。以下为本轮 Low/观察项的可执行改法：

1. **F1**：按上文改写 `BladeWard.cpp:136` 注释（推荐仅声明负值收敛），或补 `std::isfinite` 防护；同步检查 `BeamChannelDeliverySystem.cpp:1199-1203` 是否有同类「NaN 已防护」的隐含假设（该处无窄化，无 UB，仅语义提示）。
2. **F2**：在 `Skill4FollowupTests.cpp:671-681` 引入一个小型 RAII 守卫（析构中 `ResetForTests()` + 重载真实表 + `remove`），使失败路径也能清理。
3. **F3**：将技能 5 弹速基准 `1000.0f` 与索敌基准 `450` 一样改由机制表/具名常量单源读取，或在设计 §5.2 显式登记该硬编码为「与 Baker 基准同源的常量副本」。
4. **F4**：修正 `SkillSpecializationBakerTests.cpp:2010` 的乘法笔误。

## 10. 独立复算与验证证据

全部命令为只读执行；未触发 `build.bat` / `ctest` 全量编译（依赖既有构建产物，并已核对其时效性）。

| 命令 | 结果 |
|---|---|
| `Get-Item bin\NoMoreDayTests.exe` | `LastWriteTime = 2026/9/17 11:28:39`，晚于全部改动源码/测试（最新 `11:24:26`）→ 二进制覆盖本轮修复 |
| `git status --short` / `git diff --stat` | 22 个已跟踪修改 + 5 个未跟踪新增，无越界；`22 files changed, +2192 / -270` |
| `python scripts/validate_skill_spec_modifiers.py --check` | `id_decode / projectile_integer / skill_delivery_domain / migration_equivalence / retired_keys_absent / registry_reverse` 全 `[OK]`，`6/6 passed`，EXIT=0 |
| `python -m unittest tests.python.SkillSpecBatch2GateTest -v` | `Ran 14 tests ... OK`，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*470 Counter Sword Count Parity*,*Skill 6 Delivery Floors At Zero*,*Blade Ward UMR Baking*,*Infinite Blades UMR Baking*,*Sword Array UMR Baking*"` | `5 cases / 125 assertions`，0 failed，SUCCESS |
| `bin\NoMoreDayTests.exe --test-case="*Skill 6 - Shape & Synergy Nodes*"` | `1 case / 41 assertions`，0 failed，SUCCESS（覆盖 Fix10 的 603 parity 子用例） |
| `rg` 全仓库核 `active_nodes` 写入点 / `ShadowComponent` 产生点 / `del.duration` 写入点 | 与 §6.1 结论一致 |
| `assets/data/skills.json` 逐节点读 `max_points` | 510=1、511=3、554=1、610=1、611=1、402=3、470=1、471=4，与 Fix7 用例取值一致 |

已提供的验证基线（`cmd /c build.bat RelWithDebInfo` 退出码 0 无告警；`ctest -L ci` 1/1 Passed；全量 doctest 1734 cases / 141628 assertions 0 failed；`gen_modifier_runtime_v2.py --check` / `gen_skill_spec_modifier_contract.py --check` OK）作为支持性背景；本轮关键项（含 471 回归、470 双路径、负值钳制、603 parity）已由上表独立复跑覆盖。

## 11. 剩余风险

1. **471 fail-closed 依赖「技能 4 无影子施放入口」的不变量**（`BladeWard.cpp:147-152`、设计 §5.1 `:378`）：当前由 `ShadowDuplicationHook.cpp:21-26` 的 `Tag::Buff` 排除保证。若未来新增可携带 `active_nodes` 施放技能 4 的影子/复制入口，该分支会静默输出 `0`。已有注释与设计登记，属可接受的低风险，建议在新增影子机制时回归本轮用例。
2. **未分配 511 的弹速基准 `1000.0f` 为字面量副本**（F3）：与 Baker 基准存在潜在漂移，当前值一致。
3. **`static_cast<uint8_t>` 收窄的 NaN 防护缺口**（F1）：当前数据域无 NaN 来源，影响可忽略。
4. **技能 6 `delivery.range`（603 施法范围）仍无消费端**（自第 1 轮延续）：本轮 Fix10 仅强化了数值 parity 守护（`SwordArrayNodes.cpp:275-278` 已注明），后续接入消费端时需补行为层用例。
5. **报告未覆盖项**：未对非本批节点的既有行为做回归审查（超出范围）；未执行 GPU/渲染相关验证（本批无渲染路径改动）。

## 12. 下一步动作

1. 合入本批（结论 `提交`）。
2. 建议在后续批次顺手处理 F1–F4（均为 Low / Best Practice，非阻塞），其中 F1 的注释修正成本最低、收益明确（避免「NaN 已防护」的误导）。
3. 若采纳 F3 的单源化，需同步更新设计 §5.2 的基准表述，避免文档漂移。
4. 维持本轮新增回归用例（470 双路径、471 未烘焙/烘焙、负值钳制、603 parity）作为后续 UMR 改造的守护基线。

---

# 13. 复审（F1–F4 闭环）

- 复审日期：2026-09-17
- 触发条件：针对本报告 §8 的 F1–F4 实施修改后请求复核（`docs/workflows/review.md`「报告生命周期」：同包后续轮次追加小节，不删除早期轮次）。
- 本节结论：**提交**。F1–F4 全部闭合，未引入新问题。

## 13.1 复审范围与方法

只读复核四处改动：读取改动后的源码/测试原文，独立判断「问题是否真正消除、是否引入新缺陷（类型/语义/悬垂/告警）」；重跑受影响用例；核对设计/计划是否因改动产生文档漂移。未修改任何源码/测试/数据，唯一写入为本报告追加内容。

## 13.2 逐条复核结论

| 编号 | 上轮 severity | 状态 | 独立核验证据与判断 |
|---|---|---|---|
| F1（`BladeWard.cpp:136` NaN 注释失真 + 窄化 UB） | Low | **已闭合（实现已真正 NaN-safe，非仅改注释）** | `BladeWard.cpp:137-144`：`const float rawCounterSwords = mechanics.GetFloat(...)`，回退式改为 `rawCounterSwords > 0.0f ? std::min(rawCounterSwords, 255.0f) : 0.0f`。逐值核对（IEEE-754 大小比较，NaN 比较恒 false）：NaN → `NaN > 0.0f == false` → `0.0f`（不再有 `static_cast<uint8_t>(NaN)` 的 UB）；负值 → `false` → `0.0f`；`0.0f` → `0.0f`；`(0,255]` → 原值；`>255` / `+inf` → `255.0f`。与旧 `std::clamp` 版本在全部非 NaN 取值上**逐值一致**（无行为回归），NaN 由 UB 变为确定性的 0。注释 `:136` 已改为「NaN 使大小比较为假而归 0，负值归 0，上限钳到 255」，与实际语义一致，不再过度承诺。类型/悬垂检查：`std::min(float, float)` 返回 `const float&` 绑定到具名局部 `rawCounterSwords`（`<algorithm>` 已由本文件既有 `std::min`/`std::max` 使用而包含，`:128/:468`），三目表达式取公共类型 `float` 右值，无悬垂、无窄化告警；`std::min` 的「先判 `>0` 再取上限」避免了 `<cmath>` 依赖（规避了所报 MSVC C2061），写法成立 |
| F2（470 parity 用例失败路径临时文件残留） | Low | **已闭合** | `tests/functional/Skill4FollowupTests.cpp:684-685`：`LoadFromFile` 成功后立即 `std::filesystem::remove(mechanics_path)`；用例尾部（`:745-748`）仅保留真实机制表复位，原重复 `remove` 已删除（文件由 750 行变为 751 行，尾部无 `remove` 残留）。文件内容在 `LoadFromFile` 返回前已读入内存，删除不影响后续子用例读取机制表值 7。故「子用例断言失败 → 尾部清理被跳过」的残留路径已消除。残留观察（非发现）：若 `:682` 的 `REQUIRE(LoadFromFile)` 本身失败，`:685` 不会执行，文件留在 `%TEMP%`——此为不可完全消除的固有次序（删文件则无法加载），且内容确定、每轮覆盖写，无实际影响 |
| F3（未分配 511 弹速基准 `1000.0f` 硬编码副本） | Best Practice | **已按「不改行为 + 显式登记」闭合** | `src/game/systems/skill/BeamChannelDeliverySystem.cpp:1197-1203`：逻辑与取值完全未变（`finalSpeed = 1000.0f`，仅 `profile != nullptr && (feature_flags & 16) != 0` 时改读 `.speed`），注释已明确「基准 1000 与 Baker 步骤 1 写入的引导基准一致，该字面量仅为无档案回退」，即把该副本登记为「与 Baker 基准同源的常量回退」而非未声明魔数。与设计 `:384` 的表述一致；定性为可维护性建议而非缺陷，采纳登记式闭合理由充分 |
| F4（Fix12 用例注释乘法笔误） | Low | **已闭合** | `tests/unit/SkillSpecializationBakerTests.cpp:2010` 已改为 `//   duration 结果 = 5.0 + (-100 * 1) = -95，`，算式与净结果一致；同段 `:2011-2013` 关于「取 -2 而非 -1 才能真正触发钳制」的说明保持不变且正确 |

**新增问题：无。** 未发现 `std::min` 语义/类型问题、悬垂引用、窄化告警、行为回归或文档漂移（`rg clamp|NaN|255` 于设计/计划均无命中，故 F1 的实现调整不产生文档漂移）。

## 13.3 独立复算与验证证据（复审）

| 命令 | 结果 |
|---|---|
| `Get-Item bin\NoMoreDayTests.exe` + 源文件时间戳 | 二进制 `2026/9/17 11:42:33` 晚于四处改动源（`11:40:54 ~ 11:41:59`）→ 证据覆盖本轮复审代码 |
| `git diff --stat -- <四文件>` | `465 insertions(+), 46 deletions(-)`，改动范围仅限 F1–F4 所述区域 |
| `bin\NoMoreDayTests.exe --test-case="*470 Counter Sword Count Parity*,*Skill 4 -*,*Blade Ward UMR Baking*"` | `14 cases / 214 assertions`，0 failed，SUCCESS（覆盖 F1 的 470 回退路径与 471 断言） |
| `bin\NoMoreDayTests.exe --test-case="*SkillSpecializationBaker*"` | `17 cases / 335 assertions`，0 failed，SUCCESS（覆盖 F4 用例与 F1 相关烘焙路径） |
| `rg -n "clamp\|NaN\|255"`（设计/计划） | 无命中 → 无文档漂移 |

已提供的验证基线（`cmd /c build.bat RelWithDebInfo` 退出码 0；`ctest -C RelWithDebInfo -L ci` 1/1 Passed 13.65s；全量 doctest 1734 cases / 141628 assertions 0 failed）作为支持性背景；上述受影响用例已独立复跑覆盖。

## 13.4 复审后剩余风险

1. 471 fail-closed 仍依赖「技能 4 无影子施放入口」的不变量（第 3 轮 §11 风险 1 未变，已有注释与设计登记）。
2. 技能 6 `delivery.range`（603 施法范围）仍无消费端（第 3 轮 §11 风险 4 未变）。
3. F3 采纳「登记式」方案，弹速基准 `1000.0f` 仍为字面量副本；若后续调整 Baker 基准，需同步该处（已在注释与设计中登记，风险可接受）。
4. 第 1 轮遗留项（非本批节点回归、GPU/渲染验证未覆盖）依然适用。

## 13.5 复审结论

**提交。** F1–F4 四项发现全部真实闭合，其中 F1 由「注释纠偏」升级为「实现层 NaN-safe 且与旧 clamp 逐值等价」，F2 消除了失败路径残留，F3 经显式登记、F4 修正笔误；未引入新问题、无既有断言弱化、无范围泄漏。总体结论维持第 3 轮 §2 的 `提交`。
