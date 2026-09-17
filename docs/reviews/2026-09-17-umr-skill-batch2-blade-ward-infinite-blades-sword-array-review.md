# UMR 技能专精改造第二批（技能 4/5/6）审查报告

- 系统代号：`UMR-SKILL-BATCH-2`
- 审查日期：2026-09-17
- 审查标准：`docs/workflows/review.md`
- 代码硬规则来源：`conductor/code_standard.md` (V2.1)

---

## 1. 审查目标

审查 UMR 技能专精改造第二批的完整交付包，覆盖：

- 技能 4「剑气护体」（节点 402/470/471）— 法耗折扣下沉交付档案、反击剑数单源缓存、反击增伤单源读取；
- 技能 5「万剑归宗」（节点 500/502/510/511/533/554/555）— 数值迁移到 canonical 修饰器记录、索敌半径与弹速二次重算消除；
- 技能 6「剑阵·诛仙」（节点 600/601/602/603/610/611/634/653）— 持续/半径/增伤/法耗/施法范围迁移；
- 新增交付算子 41 `SKILL_DURATION_FLAT`、42 `SKILL_SPEED_MULT`；
- 24 个机制表退役键删除（19 迁移 + 5 死键）、4 个 KEPT 键保留；
- 对应门禁脚本、生成器、canonical 数据、单元/功能/Python 测试。

判定对象为「设计—计划—实现—测试—证据」的一致性，不针对未涉及的其他技能节点。

## 2. 结论

**提交**

无 Blocker / High 级问题；无范围泄漏；无缺失的必备行为；关键路径均已被独立复算或独立执行的功能用例覆盖。全部 4 条发现项为 Low（可维护性/门禁覆盖/测试表达力），不阻塞合入，建议在后续批次顺手收敛。

## 3. 审查轮次

第 1 轮（本文件首建，无追加小节）。

## 4. 输入

| 类型 | 路径 |
|---|---|
| 设计 | `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md`（v2.1） |
| 计划 | `docs/plans/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-plan.md`（v2.1） |
| 标准 | `docs/workflows/review.md`、`conductor/code_standard.md` (V2.1) |
| 代码 | `src/game/systems/modifier/{ModifierContext.hpp,ModifierEvaluator.hpp,ModifierEvaluator.cpp}`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/{BladeWard.cpp,BladeWardRuntime.hpp}`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/combat/damage/DamageInterceptors.hpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp` |
| 数据 | `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`、`assets/data/modifier_v2/skill_spec_modifiers.json`、`assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json` |
| 脚本 | `scripts/{gen_modifier_runtime_v2.py,migrate_skill_spec_modifier_slice.py,validate_skill_spec_modifiers.py}` |
| 测试 | `tests/unit/{SkillBatch2DeliveryOpTests.cpp,SkillSpecializationBakerTests.cpp,SkillManaCostSettlementTests.cpp}`、`tests/functional/{Skill4FollowupTests.cpp,InfiniteBladesNodes.cpp,SwordArrayNodes.cpp,Skill5FollowupTests.cpp}`、`tests/python/SkillSpecBatch2GateTest.py` |
| 图谱 | `codebase-memory-mcp`（project `D-PRJ-NoMoreDay`） |

审查过程只读：未修改任何实现代码、数据、脚本或测试。唯一写入为本报告文件。

## 5. 变更文件边界

`git status --short` 记录（实测）：

- **已跟踪修改（22 项）**：`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`、`assets/data/modifier_v2/skill_spec_modifiers.json`、`assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json`、`scripts/gen_modifier_runtime_v2.py`、`scripts/migrate_skill_spec_modifier_slice.py`、`scripts/validate_skill_spec_modifiers.py`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/combat/damage/DamageInterceptors.hpp`、`src/game/systems/modifier/ModifierContext.hpp`、`src/game/systems/modifier/ModifierEvaluator.cpp`、`src/game/systems/modifier/ModifierEvaluator.hpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/BladeWard.cpp`、`src/game/systems/skill/behaviors/BladeWardRuntime.hpp`、`tests/functional/InfiniteBladesNodes.cpp`、`tests/functional/Skill4FollowupTests.cpp`、`tests/functional/Skill5FollowupTests.cpp`、`tests/functional/SwordArrayNodes.cpp`、`tests/unit/SkillManaCostSettlementTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`。
- **未跟踪新增（4 项）**：`docs/designs/…-design.md`、`docs/plans/…-plan.md`、`tests/python/SkillSpecBatch2GateTest.py`、`tests/unit/SkillBatch2DeliveryOpTests.cpp`。
- `git diff --stat`：22 files changed, `+2028 / -258`（其中 canonical `+817`、`skill_spec_modifiers.json +646`、`skill_mechanics_schema.json -120`、`SkillSpecializationBakerTests.cpp +242`）。
- 测试侧变更全为增量：`tests/functional/*` 合计 `+75`，其中 `Skill4FollowupTests.cpp` 有 2 行既有断言替换（见 §6 范围对齐第 3 条，设计第 403 行显式授权）、`tests/unit/SkillSpecializationBakerTests.cpp` 有 2 行既有断言替换（设计第 194/424 行显式授权）。
- 生成物 `assets/generated/modifier_runtime_v2.bin` 未纳入版本控制（`assets/data/modifier_v2` 下无二进制），`gen_modifier_runtime_v2.py --check` 以内存编译结果与 JSON 比对。
- 环境警告：多文件 LF→CRLF 转换，无功能影响。

## 6. 范围对齐

| 设计/计划要求 | 实现证据 | 判定 |
|---|---|---|
| 19 条 canonical 记录：ID 解码、字段模板、`stacks==param_u32==skill_id`、`stat_path==target`、`debug_name` 与设计 §4.2 表逐项一致 | 独立脚本逐字段比对 19 条 → `FAILS: 0`；`validate_skill_spec_modifiers.py --check` 6/6 | ✅ |
| 24 个机制表键删除；4 个 KEPT 键保留 | `skill_mechanics.json` 实测删除 24 键、`5/502 splash_radius`、`5/510 lock_range`、`5/533 giant_radius`、`5/554 intent_cost` 仍在；`src/tests` 无退役键字面量残留 | ✅ |
| 算子 41 加性 / 42 乘性公式 | `ModifierEvaluator.cpp:323-327`（`AddSkillDurationFlat(param*pts)` / `AddSkillSpeedMult(1+param*pts)`）、`480-491`（加性 `+=`、乘性首次 emplace 后 `*=`）、`574-580`（默认 0.0/1.0 走 `ReadOr`，只读不插入） | ✅ |
| `ModifierDelta` 中性值与 `MergeFrom` 合并语义 | `622-623` 使用既有 `mergeAdditive`/`mergeMultiplicative`；`SkillBatch2DeliveryOpTests.cpp` 覆盖空容器中性值、新键插入、跨技能键与合并累加/累乘 | ✅ |
| `CategoryOfOp` 归 `SkillDelivery` | 41/42 均在 `ModifierOpCategory::SkillDelivery` 分支 | ✅ |
| Baker 步骤 1 基准显式化 | `SkillSpecializationBaker.cpp:84-105`：技能 4 `duration=10/sub_count=5/more_damage_mult=1`；技能 5 `sub_interval=0.3/speed=1000/range=lock_range(450)/effective_mana_cost=20`；技能 6 `area_radius=150/duration=5/sub_interval=0.5/range=400` | ✅ |
| 步骤 2 无残留数值写入（仅 `feature_flags`） | 目标节点分支仅置标志位；`502` 写 `pull_radius` 属 KEPT 几何键（`splash_radius`）单源读取，非迁移值残留 | ✅ |
| 步骤 3 下限 0 | `208-209`：`std::max(0.0f, del.speed * GetSkillSpeedMult())`、`std::max(0.0f, del.duration + GetSkillDurationFlat())` | ✅ |
| 步骤 4 533 保底晚于 `area_mult` 合成 | 533 保底块位于步骤 3 之后，`area_radius = std::max(area_radius, giant_radius)`，消除遍历顺序依赖 | ✅ |
| 消费端单源：BeamChannel 无 `GetSkill5Point(511)` 二次重算 | 索敌分支 `1138-1142` 用 `profile->delivery.range`；弹速 `1197-1199` 用 `profile->delivery.speed`；文件内 `GetSkill5Point` 仅剩 501/515/531/532/535 等无关节点 | ✅ |
| BladeWard 471 读 `profile->more_damage_mult-1.0f` | `BladeWard.cpp:137-140`，带不变量注释（技能 4 More 唯一来源为 471） | ✅ |
| BladeWard 470 缓存 `counter_sword_count` | `BladeWard.cpp:130-136`，档案命中取 `delivery.sub_count`（5），否则回退机制表 `counter_swords`；`SkillDefs.hpp:974` 新增 `uint8_t counter_sword_count = 5` | ✅ |
| BladeWard 402 下沉 `effective_mana_cost` | `BladeWardComponent` 删除 `mana_cost_reduction`；`BladeWard::DoCast` 不再组装 `ResourceCostReduction`；`SkillSystem.cpp:2048-2056` 已烘焙档案直接取 `effective_mana_cost`，未烘焙时补装备乘算 | ✅ |
| `SpawnBladeWardCounterSwords` 4 参签名与唯一调用点一致 | 定义 `BladeWard.cpp:497`、声明 `BladeWardRuntime.hpp:23`、唯一调用 `DamageInterceptors.hpp:73-74`，均为 `(registry, owner, cast_id, count)` | ✅ |
| 既有断言未被弱化 | 既有仅 2 处替换：`Skill4FollowupTests.cpp:159-166`（`mana_cost_reduction==0.15f` → `effective_mana_cost==25.5f`）与 `SkillSpecializationBakerTests.cpp:542-548`（技能 5 默认 speed/range 由结构体默认值 300/200 → 显式基准 1000/450），两者均由设计第 194/403/424 行显式授权，且新断言更强（等价数值守护）。其余既有用例零改动 | ✅ |
| 功能测试夹具补齐 `ReloadModifierRuntimeFromAsset()` | `Skill4FollowupTests.cpp`、`Skill5FollowupTests.cpp`、`SwordArrayNodes.cpp`、`InfiniteBladesNodes.cpp` 的 `EnsureSkillMechanics()` 均新增 `REQUIRE(ReloadModifierRuntimeFromAsset())` | ✅ |
| 603 parity 与 470 双路径断言存在 | `SwordArrayNodes.cpp:275-292`（603 点满 =560、未点 =400）；`Skill4FollowupTests.cpp:660-690`（档案命中路径 5、档案缺失回退机制表路径 5） | ✅ |
| 无重复轮子 | 见 §7 检索条目 | ✅ |

### 6.1 疑点专核结论

1. **T3 空 `else-if` 骨架**：`SkillSpecializationBaker.cpp:594-595`（技能 5 节点 500）与 `700-708`（技能 6 节点 600/601/602/603）为空分支、仅注释。经核：两条 `if/else if` 链均以 `break`（`694`、`…`）收尾，空分支与删除分支行为等价（被移除的数值均已在 UMR 记录承载，`feature_flags` 无对应位需求）；不构成死代码缺陷，但属可维护性冗余 → 见发现项 F2（Low，非阻塞）。
2. **T5 移除 `feature_flags & 16` 门控的等价性**：`BeamChannelDeliverySystem.cpp:1141` 改以 `profile->delivery.range > 0.0f` 判定。未点 511 时 Baker 步骤 1 写入 `del.range = lock_range(450)`，步骤 3 乘 1.0 不变，故 `del.range==450` 恒 >0，`lock_radius` 与旧路径 `GetSkill5Point(511)` 门控关闭时的 450 基准完全一致。测试 `SkillSpecializationBakerTests.cpp`「510 mana penalty and lock range baseline」断言 `range==450`，`SwordArrayNodes`/既有技能 5 用例亦通过 → **等价性成立**。
3. **T6 用技能 1 node101 替代技能 4 装备复合场景**：`SkillManaCostSettlementTests.cpp:161` 新增用例注释明确说明「402 专精折扣限定技能 4，而真实资产中唯一带降耗的装备记录 `1001001` 限定技能 1，无技能 4 装备记录，故以技能 1 节点 101（同 `SKILL_MANA_COST_MULT`，每点 0.15）等价验证乘算复合语义」。断言 `2.75 → 2.475`，并以 `CHECK_FALSE(== 2.25f)` 显式排除加性结果 → **属有效覆盖**（覆盖的是复合语义不变量，而非技能 4 专属数值；数值层面由 `Skill4FollowupTests` 的 `25.5f` 与 `SkillSpecializationBakerTests` 的 25.5/21.0/16.5 独立守护）。
4. **设计 §5.1 与计划 §3.3 / 设计 §6.3 的 KEPT 清单不一致**：设计第 378 行（§5.1）称机制表 `counter_swords`「仅保留在 `KEPT_MECHANICS_KEYS` 作为安全回退」，但设计第 411 行（§6.3 注册列表）与计划第 179 行只列 4 个 KEPT 键，均未含 `(4,470,counter_swords)`；实现对齐 §6.3。实测数据层 `assets/data/skill_mechanics.json:332` 仍保留 `"counter_swords": 5.0`，`BladeWard.cpp:135-136` 回退读取正常，**无实际功能/回退缺口**；但 `validate_skill_spec_modifiers.check_registry_reverse` 只遍历 `MIGRATION_EQUIVALENCE`（470 非迁移节点），该键当前无门禁保护 → 见发现项 F1（Low，非阻塞）。
5. **402 由「叠加」改「乘算」的语义变更一致性**：设计第 375 行、计划第 21/56/193 行均显式声明为有意修正；实现 `SkillSystem.cpp:2048-2071`（`raw_mana_cost = bakedProfile->effective_mana_cost`，再 `*(1 - min(0.9, rcr))`）确认为乘算；回归保护由 `Skill4FollowupTests.cpp:159-166`（技能 4 452 折叠值 25.5）与 `SkillManaCostSettlementTests.cpp:161`（专精×装备乘算、显式排除加性）双重覆盖 → **一致且有回归保护**。

## 7. 质量与风险评估

- **内存安全（code_standard §2.2 / §5.2）**：新增代码无裸 `new/delete`、无裸拥有指针、无 `malloc`；`SpawnBladeWardCounterSwords` 通过 `emplace` 创建实体并前置 `registry.valid(owner)`、`all_of<Position>(owner)`、`count == 0` 三重守卫，消除了除零与无效实体解引用。**无风险**。
- **热路径分配（§2.1 / §7.2）**：41/42 取值走 `ReadOr`（`find`，不插入），`SkillBatch2DeliveryOpTests` 以 `skill_duration_flat.empty()` / `skill_speed_mult.empty()` 断言只读查询零分配；`BeamChannelDeliverySystem` 改造后由「读取专精组件 + 二次乘算」改为「读取已烘焙档案字段」，指令数下降；无热路径字符串比较、无热路径堆分配。**无风险**。
- **ECS 组件指针/并发（§5.3 / §8.1）**：本批未新增跨增删组件的 EnTT 指针持有；`ward.counter_sword_count` 为值拷贝缓存；无新增 `std::thread`。**无风险**。
- **RAII / Rule of Five（§5.1）**：`ModifierDelta` 仅新增两个标准容器成员，其特殊成员由标准库生成，`MergeFrom` 自赋值保护（`this == &other`）保持。**无风险**。
- **const 正确性 / 命名 / 注释语言**：新增访问器均为 `const`；注释为中文且基于设计语境；未引入 `using namespace std`。**无风险**。
- **`static_cast<uint8_t>` 收窄（§6.1 转换）**：`BladeWard.cpp:132-136` 将 `sub_count` / 机制表浮点回退值显式转换为 `uint8_t`，与本文件既有写法一致，且注释已说明回退语义；设计未要求边界钳制，当前取值域（5、5.0）安全。**低风险**，列入剩余风险。
- **无重复轮子（出口门禁）**：经图谱与 `rg` 双向检索——`SpawnBladeWardCounterSwords` 全库仅 1 处定义 + 1 处调用；`counter_sword_count` 仅 1 个字段定义 + 1 处消费；`GetSkillDurationFlat/GetSkillSpeedMult` 无既有等价实现（`split_speed_mult` 为投射物分裂系数、`getSpeedMultiplierAtWorld` 为地图地形移速，语义无关）；交付算子复用既有 `BakedDeliveryParams.duration/speed` 字段，未新建组件或平行容器。**通过**。

## 8. 发现项

### F1 — Low（可维护性 / 门禁覆盖）：`4/470 counter_swords` 的 KEPT 登记在设计 §5.1 与 §6.3/计划 §3.3 之间不一致，且当前无门禁保护

- `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md:378`（§5.1 声称保留在 `KEPT_MECHANICS_KEYS`）
- `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md:411`（§6.3 注册列表未列该键）
- `docs/plans/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-plan.md:179`（KEPT 清单未列该键）
- `scripts/validate_skill_spec_modifiers.py:89-103`（`KEPT_MECHANICS_KEYS` 实际未登记）
- `tests/python/SkillSpecBatch2GateTest.py:52-57`（`BATCH2_KEPT_KEYS` 未登记）
- `assets/data/skill_mechanics.json:332`（数据键仍存在）、`src/game/systems/skill/behaviors/BladeWard.cpp:135-136`（回退读取）

**为何成为问题**：设计自身两处对同一键的处置不一致，计划沿用了 §6.3 口径且未声明偏差；`check_registry_reverse` 只遍历 `MIGRATION_EQUIVALENCE`（节点 470 非迁移节点），因此该键的实际存在性当前无任何门禁断言。功能上 `counter_swords` 值 5.0 与硬编码默认 5.0 相同，删除键不会立即改变行为，正是这种「不可观测」使未来清理可能静默移除回退数据源。

**修复建议**：三选一并同步三处文本：(a) 以 §6.3 为准，删除 §5.1 中「保留在 KEPT_MECHANICS_KEYS」的表述，明确该键为「非门禁保护的数据级回退」；或 (b) 在 `KEPT_MECHANICS_KEYS` 与 `BATCH2_KEPT_KEYS` 补入 `(4, 470, "counter_swords")`（数据已存在，门禁即刻通过，且新增真实存在性断言）；或 (c) 若决定彻底删除该键，则把 `BladeWard.cpp:135-136` 的回退改为常量并移除回退分支。推荐 (b)，成本最低且符合 §5.1 原意。

### F2 — Low（可维护性，非阻塞）：`SkillSpecializationBaker` 保留空 `else-if` 骨架分支

- `src/game/systems/skill/SkillSpecializationBaker.cpp:594-595`（技能 5 节点 500，空分支仅注释）
- `src/game/systems/skill/SkillSpecializationBaker.cpp:700-708`（技能 6 节点 600/601/602/603，四条空分支仅注释）

**为何成为问题**：经核两条分支链均以 `break`（`:694` 及对应行）收尾，空分支与直接删除分支行为完全等价——即当前为纯死骨架。它制造了「这些节点在 Baker 中仍有特殊处理」的错觉，增加后续节点新增/重排时的误判成本；同类节点（如 502/510/511/533/610/611/634/653）均以「注释 + `feature_flags |=`」形态保留，500 与 600-603 缺少标志位使风格不统一。

**修复建议**：删除这四个空分支（节点未命中时自然走链尾 `break`，语义不变）；若希望保留「已迁移」的可追溯性，可改为在链前统一注释块列出「以下节点数值已迁入 canonical UMR 记录：500/600/601/602/603」并删除空 `if/else if`。两种改法均不改变行为。

### F3 — Low（测试表达力）：交付门禁存在恒真断言

- `tests/python/SkillSpecBatch2GateTest.py:105`：`self.assertIn((record_id - 2000000) % 10, range(10))`

**为何成为问题**：`(x) % 10` 必然落在 `range(10)`，该断言永远通过，无法证明「ID 解码结果等于设计期望的节点号」。虽然同一用例的其它断言（记录存在性、逐字段比对、重复检测、registry 反向）已实质覆盖 ID 解码，且 `validate_skill_spec_modifiers.py` 的 `id_decode` 门禁独立通过，故此断言不构成「唯一证据是假测试」的硬否决，但它稀释了门禁信号，属于计划中「ID 解码」验收点的薄弱项。

**修复建议**：改为从 `BATCH2_RECORDS` 取期望节点号并对解码值做等式断言，例如 `self.assertEqual((record_id - 2000000) // 10, expected_node_id)`，保留原 `% 10 == 0` 作为「ID 必须整十」的结构校验。

### F4 — Low（注释准确性）：两处「已迁入 UMR」表述与实现不符

- `src/game/systems/skill/SkillSpecializationBaker.cpp:640-641`（技能 5 节点 552 注释称「该几何参数已迁入 UMR」）——实际 `circle_radius` 未被迁移：`assets/data/skill_mechanics.json:430` 仍保留 `"circle_radius": 150.0`，由 `src/game/systems/skill/BeamChannelDeliverySystem.cpp:1126-1129` 单源读取。该节点实际变更是「不再写入 `del.range`，几何参数留在机制表」，与 UMR 无关。
- `src/game/systems/skill/SkillSpecializationBaker.cpp:569-571`（技能 4 节点 470 注释称「数量缩放交由 UMR 承载」）——470 的剑数并无 canonical 记录（19 条记录中无 470），其基准 5 来自步骤 1 的 `del.sub_count = 5`，属交付基准而非 UMR 缩放。

**为何成为问题**：注释把「下沉到交付档案 / 保留在机制表」误述为「迁入 UMR」，会误导后续维护者到错误的单一事实源去排查数值（例如在 canonical JSON 中找不到 552/470 的键）。功能无影响。

**修复建议**：将 552 注释改为「该几何参数保留在机制表 `5/552 circle_radius`，由交付层 `BeamChannelDeliverySystem` 单源读取；此处不再写入 `del.range`，避免污染 510/511 的 450 索敌基准」；将 470 注释改为「反击剑数基准已在步骤 1 显式写入 `del.sub_count = 5`，由 `BladeWard::DoCast` 缓存；此处仅保留 Keystone 标志位，无数值缩放」。

> 未发现 Blocker / High / Medium 级问题；未发现越权、隐藏失败、伪造证据、既有断言被注释/删除/旁路、UB/UAF/泄漏、热路径字符串分支、跨增删组件持有 EnTT 指针、裸 `new/delete`、无注释 C 式转换、裸 `std::thread`、热路径堆分配等硬否决项。

## 9. 最佳实践建议（可执行）

本包结论为 `提交`，无强制整改项。以下为对上述 Low 发现项的可执行改法，建议择机随下批合入：

1. **F1**：编辑 `scripts/validate_skill_spec_modifiers.py:89-103` 的 `KEPT_MECHANICS_KEYS` 追加 `(4, 470, "counter_swords")`；同步 `tests/python/SkillSpecBatch2GateTest.py:52-57` 的 `BATCH2_KEPT_KEYS` 追加同名元组；并修正设计 §5.1/§6.3 与计划 §3.3 的口径一致（推荐统一为「保留并在 KEPT 登记」）。执行 `python scripts/validate_skill_spec_modifiers.py --check` 与 `python -m pytest tests/python/SkillSpecBatch2GateTest.py` 复验。
2. **F2**：删除 `src/game/systems/skill/SkillSpecializationBaker.cpp:594-595` 与 `700-708` 的空分支，链尾 `break` 保持不变；复跑 `bin\NoMoreDayTests.exe --test-case="*Skill 4*,*Skill 5*,*Skill 6*"` 确认行为不变。
3. **F3**：把 `tests/python/SkillSpecBatch2GateTest.py:105` 改为对 `BATCH2_RECORDS` 期望节点号的等式断言。
4. **F4**：按上文逐字替换两条注释，避免「迁入 UMR」的歧义表述。

## 10. 独立复算与验证证据

全部命令为只读执行，未触发 `build.bat` / `ctest` 全量编译。

| 命令 | 结果 |
|---|---|
| 自写 `batch2_canon_check.py`（19 条 canonical 逐字段对照设计 §4.2） | `FAILS: 0` |
| `python scripts/validate_skill_spec_modifiers.py --check` | `skill_spec modifier offline gates passed: 6/6 passed.`，EXIT=0 |
| `python scripts/gen_modifier_runtime_v2.py --check` | `Check passed: binary is in sync with modifier_v2 json.`，EXIT=0 |
| `python scripts/gen_skill_spec_modifier_contract.py --check` | up to date，EXIT=0 |
| `python scripts/gen_skill_mechanics_schema.py --check` | `[OK] 453 entries, 65 unreferenced`，EXIT=0 |
| `python scripts/gen_skill_contracts.py --check` | up to date，EXIT=0 |
| `python -m pytest tests/python/SkillSpecBatch2GateTest.py -q` | `14 passed`，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*SkillBatch2Delivery*,*Blade Ward UMR Baking*,*Infinite Blades UMR Baking*,*Sword Array UMR Baking*,*470 Counter Sword Count Parity*,*603 cast range parity*"` | 10 cases / 111 assertions，0 failed，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*Skill 4*,*Skill 5*,*Skill 6*,*Sword Array*,*Infinite Blades*,*Blade Ward*,*ManaCost*"` | 60 cases / 1160 assertions，0 failed，EXIT=0 |

已提供的验证基线（`build.bat` RelWithDebInfo EXIT=0、0 warning/0 error；`ctest -L ci` 1639 cases / 116198 assertions 全通过）作为支持性背景，关键项已由上表独立复算。

## 11. 剩余风险

1. **`static_cast<uint8_t>` 收窄无边界防护**（`BladeWard.cpp:132-136`）：若未来机制表 `counter_swords` 被改为 >255 或负值，将发生静默截断。当前取值为 5.0，风险可忽略；建议后续在回退读取处加 `std::clamp` 或改用整数键。
2. **F1 所涉回退键无门禁**：`4/470 counter_swords` 依赖人工记忆而非门禁保护，后续机制表清理可能静默移除（影响极小，因默认值同值）。
3. **技能 6 与技能 5 的部分交付字段暂无消费端**：`Skill 6 delivery.range`（603 施法范围）已烘焙但「当前无消费端读取」（见 `SwordArrayNodes.cpp:276-278` 注释），仅做数值 parity 守护；若后续接入消费端，需补充行为层用例。技能 5 `del.range` 已由索敌分支消费，无此风险。
4. **报告未覆盖项**：未对非本批节点的既有行为做回归审查（超出范围）；未执行 GPU/渲染相关验证（本批无渲染路径改动）。

## 12. 下一步动作

1. 合入本批（结论 `提交`）。
2. 建议在下一批 UMR 改造中顺手处理 F1–F4（均为 Low，非阻塞）。
3. 若采纳 F1 的 (b) 方案，需同步更新设计 §5.1/§6.3、计划 §3.3 三处文本，避免文档再次漂移。

---

# 第 2 轮（跟进审查）

- 跟进日期：2026-09-17
- 触发条件：第 1 轮结论 `提交`，随后针对 F1–F4 实施修复（review.md「报告生命周期」第 5/6 条：同包后续轮次追加小节，不删除早期轮次）。
- 本节结论：**提交**

## 13. 本轮变更内容（实测 `git status --short` / `git diff`）

`git status --short` 文件集合与第 1 轮完全一致（22 个已跟踪修改 + 4 个未跟踪新增 + 本审查报告），**无越界新增文件**。本轮在上述边界内的改动为：

| 文件 | 本轮改动 |
|---|---|
| `src/game/systems/skill/SkillSpecializationBaker.cpp` | 删除 case5 节点 500 与 case6 节点 600/601/602/603 的空 `else-if` 分支并重接链首（`case 5` 链首改为 `if (node_id == 501)`，`case 6` 链首改为 `if (node_id == 610)`）；修正 470 注释（`:569-572`）与 552 注释（`:639-643`） |
| `scripts/validate_skill_spec_modifiers.py` | `KEPT_MECHANICS_KEYS` 增补 `(4, 470, "counter_swords")`（`:99`，并加注释说明该键无 canonical 记录、仅作无 Profile 回退） |
| `tests/python/SkillSpecBatch2GateTest.py` | `BATCH2_KEPT_KEYS` 增补 `(4, 470, "counter_swords")`（`:53`）；`:105` 恒真断言改为 `assertEqual((record_id - 2000000) // 10, node)`，原 `% 10` 校验改为 `assertIn(..., (0, 1))` 的 op_index 合法性校验 |
| `docs/designs/…-design.md` | §6.3（`:411`）KEPT 清单补 `4/470 counter_swords` |
| `docs/plans/…-plan.md` | §3.3（`:179`）KEPT 清单补 `4/470 counter_swords`，并注明 6/610、6/611 `max_arrays_bonus` 仍为死键 |

`git diff --stat` 合计 `22 files changed, +2032 / -267`；除上述 5 个文件外，其余文件内容与第 1 轮一致（`SkillSpecializationBaker.cpp` 仍为 `81` 行级差分，未出现额外区域改动）。本轮二进制 `bin\NoMoreDayTests.exe` 构建时间 `2026/9/17 10:50:06` 晚于源码修改时间 `10:47~10:48`，即下文 doctest 证据覆盖本轮修复代码。

## 14. 复查范围

1. 逐条核验 F1–F4 的修复是否真实落地并与设计/计划/实现/门禁四端一致。
2. F2 的 `else-if` 链重接是否引入分支错配：核对 case5/case6 全部节点分支是否仍在、链首替换是否导致节点落入错误分支、是否有空分支残留、`mech` 是否变为未使用变量。
3. 独立复跑静态门禁与 Python/doctest 用例（只读，不编译）。
4. 确认无越界文件改动。

## 15. 上一轮发现项：已解决与仍开启

| 编号 | 上轮severity | 状态 | 复核证据 |
|---|---|---|---|
| F1（KEPT 登记不一致/无门禁） | Low | **已解决** | `scripts/validate_skill_spec_modifiers.py:89-105`（`(4,470,counter_swords)` 登记于 `:99`）；`tests/python/SkillSpecBatch2GateTest.py:52-58`（`BATCH2_KEPT_KEYS` 于 `:53`）；门禁用例 `:164-174` 同时断言「键存在于机制表」与「键∈KEPT_MECHANICS_KEYS」；设计 §6.3 `:411` 与计划 §3.3 `:179` 均已补入，§5.1 与 §6.3 不再自相矛盾；`assets/data/skill_mechanics.json:332` 键仍在 |
| F2（空 `else-if` 死骨架） | Low | **已解决，且未引入新问题** | `SkillSpecializationBaker.cpp:592-596`（case5 链首为 501，500 已移除，注释说明迁移）、`:698-702`（case6 链首为 610，600-603 已移除）。逐分支核对：case5 现有 501/502/503/510/511/512/513/514/515/530/531/532/533/534/535/550/551/552/553/554/555/570/571/572/573/574/575 共 26 个分支，case6 基础/分支 A–D 全部保留，无节点丢失；两条链仍以 `break` 收尾（`:695` 与后续），被移除的节点现直接走 `break`，与空分支语义等价；链首由 500→501、600→610 只是去掉了一个恒不命中的前导条件，其余分支彼此互斥，不存在错误分支落入；每条分支体均有实质语句，无空分支残留；`mech` 在 case5（`:602/:656/:658/:668/:680`）与 case6（`:754+`）仍被使用，无未使用变量 |
| F3（门禁恒真断言） | Low | **已解决** | `tests/python/SkillSpecBatch2GateTest.py:105` 改为对 ID 解码的等式断言（`(record_id-2000000)//10 == node`），对 2005111/2006031 等 op_index 非 0 记录同样成立；`:107` 将恒真的 `range(10)` 收窄为 `(0, 1)` 合法索引集。两断言均可被错误数据证伪 |
| F4（Baker 注释失真） | Low | **已解决** | `SkillSpecializationBaker.cpp:569-572` 470 注释改为「剑数基准在步骤 1 显式写入 `del.sub_count = 5`（无 Profile 时回退机制表 `counter_swords`），本节点仅置 Keystone 标志位，无数值缩放」；`:639-643` 552 注释改为「不再篡改 `del.range`；环形半径仍由交付系统 `BeamChannelDeliverySystem` 单源读取机制表 `5/552 circle_radius`」。与实现（`BeamChannelDeliverySystem.cpp:1126-1129`）一致 |

**仍开启项：无。** 未发现本轮修复新引入的 Blocker / High / Medium 问题，亦未发现既有断言被弱化或旁路。

## 16. 独立复算与验证证据（第 2 轮）

| 命令 | 结果 |
|---|---|
| `git status --short` / `git diff --stat` | 文件边界与第 1 轮一致，无越界；`22 files changed, +2032 / -267` |
| `python scripts/validate_skill_spec_modifiers.py --check` | `id_decode / projectile_integer / skill_delivery_domain / migration_equivalence / retired_keys_absent / registry_reverse` 全 `[OK]`，`6/6 passed`，EXIT=0 |
| `python -m pytest tests/python/SkillSpecBatch2GateTest.py -q` | `14 passed in 0.03s`，EXIT=0 |
| `python scripts/gen_skill_mechanics_schema.py --check` | `[gen-schema] OK: skill_mechanics_schema.json up to date (453 entries, 65 unreferenced)`，EXIT=0 |
| `python scripts/gen_skill_spec_modifier_contract.py --check` | `skill_spec runtime contract is up to date.`，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*Skill 4*"` | 17 cases / 412 assertions，0 failed，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*Skill 5*"` | 21 cases / 307 assertions，0 failed，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*Sword Array*"` | 2 cases / 42 assertions，0 failed，EXIT=0 |
| `bin\NoMoreDayTests.exe --test-case="*Blade Ward UMR Baking*,*Infinite Blades UMR Baking*,*Sword Array UMR Baking*,*470 Counter Sword Count Parity*,*SkillBatch2Delivery*,*603 cast range parity*"` | 10 cases / 111 assertions，0 failed，EXIT=0 |

已提供的构建基线（`build.bat` RelWithDebInfo EXIT=0、0 warning/0 error；`ctest -L ci` 100% passed）仅作背景；上述关键项均为本轮独立复算。

## 17. 复查结论

**提交。**

F1–F4 四项 Low 发现项全部真实修复，修复方式与设计/计划/门禁/实现四端一致，未引入新的分支错配、未使用变量、空分支残留或越界改动；静态门禁 6/6 与 Python 门禁 14/14 通过，Skill 4/5/Sword Array 功能用例全绿。第 1 轮全部 4 项发现项关闭，无仍开启项。

## 18. 复查后剩余风险（第 2 轮）

1. `src/game/systems/skill/behaviors/BladeWard.cpp:132-136` 的 `static_cast<uint8_t>` 收窄仍无边界防护（第 1 轮风险 1 未变，当前值 5.0，影响可忽略）。
2. `tests/python/SkillSpecBatch2GateTest.py:51` 常量头注释仍写「已迁移节点下须保留的 mechanics 键」，而 `BATCH2_KEPT_KEYS` 现含非迁移节点 `4/470`；`validate_skill_spec_modifiers.py:97-98` 已就地注释澄清，属措辞级瑕疵，非阻塞，可在下次编辑该文件时顺手统一。
3. 技能 6 `delivery.range`（603 施法范围）当前仍无消费端，仅数值 parity 守护（第 1 轮风险 3 未变）；后续接入消费端时需补行为层用例。
4. 第 1 轮的范围外未覆盖项（非本批节点回归、GPU/渲染验证）依然适用。
