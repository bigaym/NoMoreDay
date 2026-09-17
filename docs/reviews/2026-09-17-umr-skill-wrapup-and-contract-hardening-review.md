# UMR 收尾与契约加固 —— 独立审查报告

## 审查目标

对一次已完成的改动包 `UMR-SKILL-WRAPUP-HARDENING`（未提交）做独立代码取证审查，核对其与 design v1.2 / plan v1.2 / backlog §1.2-1.4 的对齐程度，判断结论取 `提交` 或 `修改`。审查者未参与实现，结论以源码、数据、生成门禁输出与 git 元数据为准。

## 结论

**修改**

## 审查轮次

第 1 轮。

## 输入

- `docs/workflows/review.md`
- `docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md`（v1.2）
- `docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md`（v1.2）
- `docs/plans/2026-09-12-skill1-9-followup-backlog.md` §1.2/§1.3/§1.4
- `AGENTS.md`、`conductor/code_standard.md`（引用）
- 实现者提交的证据（离线门禁、build 日志、ctest、定向用例运行结果）
- 本次独立取证：`git diff`、`git status --short`、直接源码阅读、`python scripts/*.py --check`、数据统计脚本

## 变更文件边界

`git status --short` 与 `git diff --stat` 核对的边界与任务描述一致：21 个文件改动（+236/-54），2 个新增源文件 + 2 个新增测试文件。

- 已修改（源码/测试 16）：`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/skill/SkillProfileResolve.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp`、`src/game/systems/skill/behaviors/{BloodSea,HeavenlySwordDescent,SevenStarSlash,SwordArray}.cpp`、`src/game/application/states/GameplayState.cpp`、`tests/TestCommon.hpp`、`tests/unit/SkillProfileResolveTests.cpp`、`tests/functional/{SevenStarSlashNodes,SwordArrayNodes}.cpp`、`tests/integration/{GPUABIBindingTierIntegrationTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`
- 已修改（数据/构建/文档 5）：`assets/data/mastery_skill_trees.json`、`assets/data/skill_mechanics_schema.json`、`build.bat`、`docs/plans/2026-09-12-skill1-9-followup-backlog.md`
- 新增：`src/game/systems/skill/behaviors/BeamChannelShared.hpp`、`src/game/systems/skill/behaviors/SwordArrayShared.hpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`

`设计文档/职业被动和技能设置.md` 的改动为用户先前编辑，未计入本次实现产物；`settings.json` 当前未处于修改状态（见 HIGH-1）。

## 与设计与计划的范围对齐

| 计划任务 | 状态 | 事实依据 |
| --- | --- | --- |
| Task 1.1-1.3 `is_baked` 字段 + Bake 唯一出口 + Resolve 三步过滤 | 对齐 | `SkillDefs.hpp:677`、`SkillSpecializationBaker.cpp:27-32`（提前返回不置位）与 `:340`（唯一成功出口置位）、`SkillProfileResolve.cpp:18/:27/:36` |
| Task 1.4 手写档案补 `is_baked` | 对齐 | `SkillProfileResolveTests.cpp` 三处 |
| Task 1.5-1.6 删除哨兵专属 `skill != nullptr` + 新增哨兵测试 | 对齐 | `BloodSea.cpp:427-443`、`HeavenlySwordDescent.cpp:601-616/:654-659`、`SkillProfileResolveSentinelTests.cpp`（经真实 `RebakeSkillProfiles` 入口） |
| Task 2.1 技能10 半径单源 | 对齐 | `SevenStarSlash.cpp:417-431` |
| Task 2.2 技能6 钳制单源 | 对齐 | `SwordArray.cpp:121-145`、`SwordArrayShared.hpp:7` |
| Task 2.3 技能7 BeamChannel 守卫加固 | 对齐（且证伪了 C-1 的死分支前提，见设计 §2.2.4） | `BeamChannelDeliverySystem.cpp:224-230` |
| Task 2.4 指示圈单源 helper | 对齐 | `GameplayState.cpp:872-888`、`BeamChannelShared.hpp` |
| Task 3.0 证伪门禁 | **偏差** | `SkillWrapupHardeningTests.cpp:212` 由计划要求的"存在差异"改为"相等"（见 MEDIUM-1） |
| Task 3.2 节点1015 清空 | 对齐 | `mastery_skill_trees.json:576-584` |
| Task 5.1 两脚本接入 build.bat | 对齐 | `build.bat:313-318`，位于 `gen_modifier_runtime_v2.py`(:320) 之前 |
| Task 5.2 precheck 统一中止语义 | 代码对齐、**验证证据缺失** | 既有 9 处本就有中止（`build.bat:288-311`）；无"篡改数据→构建中止"证据（见 MEDIUM-2） |
| Task 5.3 删除 GITHUB_ACTIONS 死代码 | 不适用（该死代码不存在，backlog §1.4 已登记） | — |
| Task 5.4 TestSetupScope | **部分对齐** | 仅接入 3 个 GPU 集成测试（见 HIGH-1） |

DoD §1.1-§1.4 经离线门禁与定向用例复核后成立；§1.5 的"测试不污染 settings.json"未闭环。

## 质量与风险评估

- **内存/并发/安全**：本轮改动无裸 `new/delete`、无 `reinterpret_cast` 新增、无裸线程、无热路径加锁变更；`is_baked` 为值类型布尔字段，不引入生命周期问题。
- **is_baked 契约（Task 1.1 复查项）**：`BakedSkillProfile` 的 `operator==` 为 default（`SkillDefs.hpp:701`），新字段参与比较；`SkillSystem::RebakeSkillProfiles` 的幂等跳过依赖该 `operator==`（`SkillSystem.cpp:2739/:2759`），哨兵路径（`is_baked=false`）与正常路径（`is_baked=true`）不会误判相等。`static_assert` 仅约束 `is_standard_layout` / `is_trivially_destructible`（`:703-704`），加 `bool` 后仍成立；无 `sizeof` 断言；`ActiveSkillsComponent::to_json/from_json`（`SkillDefs.hpp:722-734`）不序列化 `baked_profiles`，存档兼容性未破坏；全仓无对 `BakedSkillProfile` 的 `memcpy/memcmp`。
- **哨兵过滤自洽性（Task 4 复查项）**：`ResolveBakedProfile` 三步语义一致（缓存命中、fallback 烘焙、槽位扫描均以 `is_baked` 为准）；`GetBakedSkillProfile`（`SkillSystem.cpp:2766-2779`）仍为无过滤直读口，`BeamChannelDeliverySystem.cpp:154` 直连后由 `ResolveBeamChannelMaxRange` 内部再判 `is_baked`，二者自洽（哨兵 `area_radius==1.0f` 不会越过 `> 1.0f` 数值守卫）。
- **空指针**：`BloodSea.cpp` / `HeavenlySwordDescent.cpp` 删除 `skill != nullptr` 后仍以 `skill ? ... : fallback` 三元保留空值回退，且 `profile` 由 `ResolveBakedProfile` 保证非哨兵；未见新增解引用风险。
- **单源**：技能6 常量 `kSwordArrayBaseCastRange`（`SwordArrayShared.hpp:7`）同时供 `SwordArray.cpp` 与 `SkillSpecializationBaker.cpp` case6 使用；技能7 指示圈与 BeamChannel 共用 `ResolveBeamChannelMaxRange`，`GameplayState.cpp` 已删除本地复算与 TODO。仅缺省值 `350.0f` 存在两处（见 LOW-4）。
- **数据**：节点1015 仅清空 `stat_modifiers`，`SKILL_DURATION_FLAT(2010150)` 仍保留并被用例断言（`SkillWrapupHardeningTests.cpp:305+`，0.59s）。离线生成门禁 `gen_skill_contracts.py --check` 通过，且图标同步/契约脚本均不读取 `stat_modifiers`。
- **测试质量**：新用例断言行为层可观测量（`InvulnerableComponent.shieldRadius`、Spawn 位置、helper 返回值、无敌时长），技能10 期望值 56/68/100 为硬编码而非复制实现公式；未发现恒真断言或必然失败断言。

## 发现项

### High

- **[High-1] `settings.json` 污染未闭环，DoD §1.5「测试不污染 settings.json」不成立。**
  事实依据：`tests/unit/VFXSequencerTest.cpp:130` 的 `[Unit] VFXSequencer - Hot Reload` 在 `:147` 调用 `qualityManager.Initialize("settings.json")`；`:439` 的 `[Unit] VFXSequencer - QualityTier Filtering` 在 `:458` 再次调用；`tests/performance/MaterialLightingBenchmark.cpp:103` 的 `[Performance]` 用例在 `:114` 调用同一接口。`QualityTierManager::Initialize` 在 `QualityTierManager.cpp:123` 无条件调用 `PersistSelectionMetadata`，后者在 `QualityTierManager_Settings.cpp:673` 写入 `detail["updatedAtUtc"] = MakeUtcTimestamp();` 并在 `:688` 执行 `WriteJsonAtomically`——即每次运行都改写被 git 跟踪的仓库根 `settings.json`。而 `TestSetupScope` 的实例化点仅有 3 个 GPU 集成测试（`GPUABIBindingTierIntegrationTest.cpp:14`、`MaterialLightingIntegrationTest.cpp:44/:83`、`RenderSystemPhaseDToggleSmokeTest.cpp:54`），`VFXSequencerTest.cpp` 与 `MaterialLightingBenchmark.cpp` 均未使用该夹具。因此 `ctest -L unit` 必然污染工作区；实现者提供的"内容哈希前后一致"证据只覆盖 3 个被守卫用例，当前工作区干净只能由人工 `git checkout -- settings.json`（计划 Task 5.4 兜底）达成，不构成可复现的"不污染"证据。

### Medium

- **[Medium-1] Task 3.0 正对照被翻转为同向断言，门禁失去正向判别力（实证修订本身可接受，但设计与计划未回改）。**
  事实依据：`tests/unit/SkillWrapupHardeningTests.cpp:203` 与 `:212` 现均断言 `allocated == baseline`；原计划要求 `:212`（skill_id=10，作用域正对照）观察到差异。根因核实**正确**：`src/game/contracts/impl/StatsSystem.cpp:318` `bool is_baked = (mod.required_tags == Tag::None);`，`:323` 对 `is_baked==true` 直接跳过，而技能专精路径在 `:487` 复用了同一 lambda；`src/game/foundation/stats/AttributePipeline.cpp` 只折叠 `AstrolabeComponent::nodePoints`（`:615-644`），不遍历 `ActiveSkillsComponent::allocated_points`，故专精节点修饰符既未被预处理折叠、又被 StatsSystem 跳过。实现者结论与 F-06 一致且影响面**未被夸大**（见 LOW-2）。但灵敏度对照 `REQUIRE(baselineProbe == 15.0f)`（`:196`）只能证明"基础闪避读数可用"（`StatsSystem.cpp:271-273` `base = combat->dodge_chance * 100`），不能证明"节点修饰符施加路径可被观测"；修改后全仓不再存在任何"专精节点 stat_modifiers 生效"的正对照。另：`docs/designs/...-design.md` §2.3.1 与 plan Task 3.0（约 :109-112）仍写"skill_id=10 存在差异"的已被证伪预期，仅 backlog §1.4 记录了修订。
  建议：复用 `required_tags != None` 且 `combined_query_tags` 命中的合成节点修饰符构造真正正对照；并把 design/plan 的预期回改为"已证伪"。

- **[Medium-2] Task 5.2 要求的负向中止验证证据缺失，且设计/计划对既有 build.bat 的前提描述失实。**
  事实依据：`build.bat:288-311` 的 9 处既有 precheck **本就有** `if errorlevel 1 exit /b 1`（`git diff` 中这些是该行 context，非新增），设计 C-8/§2.7 与 plan Task 5.2 的"9 处无中止"前提不成立；新增两项（`:315-317` / `:317-318`）沿用同一模式，全块共 11 处一致，且位置在 `gen_modifier_runtime_v2.py`(`:320`) 之前，符合 Task 5.1。计划 Task 5.2 明确要求以"临时篡改数据 → 构建中止"作为验证证据，提交的证据清单中无此项。设计/计划未回改该事实，仅 backlog §1.4 记录。
  建议：补一次可复现的负向中止证据（篡改副本数据后构建观察非零退出，随后还原），并同步修订 design/plan 的 C-8 描述。

### Low

- **[Low-1] 节点1015 数据清理正确，但其展示文案与 F-01 未同步。** `mastery_skill_trees.json:576-584` 将 `stat_modifiers` 由 `[{type:35,mode:1,value:20.0}]` 改为 `[]`，符合设计 §2.3 方案甲；`assets/data/modifier_v2/skill_spec_modifiers.json:1774-1803` 的 `2010150 / node 1015 / SevenStarSlash_Node1015_VoidTreadDuration` 仍保留。`UISkillTalentTree.cpp`、`SkillDisplayPreviewService.cpp` 会读取 `node.stat_modifiers` 展示（清理后不再显示错误闪避行，属预期）；`SkillBatch4DeliveryOpTests.cpp:121-133`、`GeneratedSpecStateTests.cpp:157`、`SevenStarSlashNodes.cpp:127`、`tests/python/SkillSpecBatch4GateTest.py:24,37` 均与闪避无关，无回归。desc 语义与 F-01 已登记待办。

- **[Low-2] F-06 影响面核实：未夸大，实为全技能范围。** `assets/data/skills.json` 255 个节点中 10 个 `stat_modifiers` 非空、0 处声明 `required_tags`；`assets/data/mastery_skill_trees.json` 76 个节点中 32 个非空、0 处 `required_tags`。合计约 42 个节点属性修饰符在当前实现下全部惰性。全仓行为消费者仅 `StatsSystem.cpp`（UI 侧仅展示）。F-06 的系统性结论成立，建议升级为独立专项并覆盖技能1-9。

- **[Low-3] `dynamic_keys` 新增 `base_range` 属合理生成物更新，但全局放宽并掩盖 F-08 扫描器缺陷。** `assets/data/skill_mechanics_schema.json:2193` 新增 `"base_range"`；`BeamChannelShared.hpp` 形参刻意命名 `beamSkillId` 以规避扫描器把 `skillId` 解析到 `CombatSystem.cpp:198` 的文件级常量。`SkillMechanicsRegistry.cpp:247-257` 将 `dynamicKeys` 并入 `knownKeys`，使 `base_range` 对任意 `(skill,node)` 元组都被接受，三元组校验退化。建议后续按 F-08 收敛扫描器作用域后收窄 `dynamic_keys`。

- **[Low-4] 技能7 缺省值 350.0f 仍为双源。** `BeamChannelShared.hpp:22` 的 `GetFloat(beamSkillId,0u,"base_range",350.0f)` 与 `SkillSpecializationBaker.cpp:113` 的 `GetFloat(7,0,"base_range",350.0f)` 各持一份缺省。运行时同读一份 JSON，漂移风险低，但非严格单源。

- **[Low-5] Task 3.0 门禁用 `CHECK` 而非 `REQUIRE`，与计划"若(1)失败即停止 3.2/3.3"的前置阻断语义不符。** `SkillWrapupHardeningTests.cpp:203`。后果有限（失败不早停），但与计划措辞不一致。

### BestPractice

- `tests/TestCommon.hpp:143` 在 `~TestSetupScope` 调用的 `ResetSkillRegistries()` 内使用 `CHECK_FALSE`，析构期断言归属不清；建议改为标志位 + 用例层断言，或在注释中说明。
- `TestSetupScope` 每用例构造/析构各做一次完整文件读取（`:99-105`、`:115-121`），对用例量大的 unit 套件有轻微 I/O 开销；当前"按需实例化"策略合理，可考虑在套件级缓存基线字节。
- `build.bat:704-720` 的 `run_quiet_step` 已内含 `exit /b %STEP_EXIT%`，紧随其后的 `if errorlevel 1 exit /b 1` 属冗余防御；保留无害，但可统一为单一模式以减少后续读者困惑。

## 最佳实践建议

1. 为 `settings.json` 建立"全部已知写入点"的清单化守卫（当前清单至少 5 个文件 7 个调用点），并将其纳入 DoD 验证脚本，而非依赖人工 `git checkout`。
2. 把 F-06 提升为独立专项：为专精节点属性修饰符补齐"预处理折叠或 StatsSystem 直施"的单一路径，并补一个 `required_tags != None` 的正对照用例，防止再次出现"数据存在但永不生效"。
3. 对所有"证伪型门禁"约定记录规范：证伪即修订 design/plan 对应断言文本，而非只写 backlog，避免后续读者按失效预期行事。
4. 扫描器（F-08）应限定解析作用域而非靠命名规避；命名规避应作为临时手段登记到期。

## 剩余风险

- DoD §1.5 未闭环期间，任何开发者/CI 执行 `ctest -L unit` 都会弄脏工作区；若提交前漏执行 `git checkout -- settings.json`，会把测试写回噪声带入提交。
- F-06 未修复前，技能1-12 的全部专精节点属性修饰符继续静默失效，配置调整会被误判为"已生效"。
- 指示圈路径仍依赖 `SkillSystem::GetBakedSkillProfile` 的直读（约 30 处残余，F-02），本轮仅在哨兵治理范围内加了 `is_baked` 合取守卫；未做全量审计。
- 本轮未重新构建、未跑完整测试套件（遵守审查约束），故"build 零新 warning"与"ctest 全绿"依赖实现者证据，未获独立复现。

## 下一步动作

1. 将 `TestSetupScope` 接入 `tests/unit/VFXSequencerTest.cpp`（`:130`、`:439` 两个 `[Unit]` 用例）与 `tests/performance/MaterialLightingBenchmark.cpp:103` 用例；或经用户裁决把 DoD §1.5 收窄为"本轮改动文件不污染"并把剩余写入点登记为 F-07 子项。
2. 为 Task 3.0 补一个 `required_tags != None` 的正对照；并把 design §2.3.1 与 plan Task 3.0 的"存在差异"预期修订为已证伪。
3. 补一次"篡改数据 → 构建中止"的负向证据，并修订 design/plan C-8 关于 9 处 precheck 的失实描述。
4. 将上述 Low 项与 F-06 专项登记进 backlog，随后重跑离线门禁与定向用例。

---

审查者：独立审查（read-only）。本轮未修改任何代码、数据或文档，仅生成本报告。
