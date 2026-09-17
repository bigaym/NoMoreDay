# UMR 技能收尾与契约加固 —— 第三轮（最终）实施审查

## 审查目标

`2026-09-17-umr-skill-wrapup-and-contract-hardening` 变更集的第三轮、也是最终一轮独立实施审查。审查以第二轮发现项（H-R2-1 / M-R2-1 / L-R2-1..4 / BP-R2-1）是否被正确、完整修复为首要目标，并核查修复本身是否引入新缺陷。

## 结论

**修改**（详见文末「轮次结论」）。

- H-R2-1（High，阻塞）已彻底关闭。
- M-R2-1（Medium）**未完全关闭**：计划 §2 文件清单仍未与实际 diff 对齐。
- L-R2-1 / L-R2-2 / L-R2-4 已关闭；L-R2-3 部分关闭；BP-R2-1 可接受但存在文档与代码不一致。

## 审查轮次

第三轮（最终）跟进审查。前两轮报告 `docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review.md`、`...-review-round2.md` 保持只读、未改动。

## 输入

- 设计：`docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md`（v1.2）
- 计划：`docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md`（v1.2）
- 审查标准：`docs/workflows/review.md`
- 规范：`AGENTS.md`、`conductor/code_standard.md`
- 前轮：`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review.md`（第一轮）、`...-review-round2.md`（第二轮，结论 `修改`）
- 工作树基线：git HEAD = `48618225`

## 变更文件边界

`git status --short`：**26 个已跟踪文件被修改（+271/-55）**，另 8 个未跟踪文件（4 份本轮文档 + 2 个新头文件 + 2 个新测试）。

已修改（实现相关，节选）：
`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/skill/SkillProfileResolve.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp`、`src/game/systems/skill/behaviors/{BloodSea,HeavenlySwordDescent,SevenStarSlash,SwordArray}.cpp`、`assets/data/mastery_skill_trees.json`、`assets/data/skill_mechanics_schema.json`、`src/game/application/states/GameplayState.cpp`、`build.bat`、`tests/TestCommon.hpp`、`tests/unit/{SkillProfileResolveTests,SkillSpecializationBakerTests,RenderSystemInitializeFailureTest,VFXSequencerTest}.cpp`、`tests/functional/{SwordArrayNodes,SevenStarSlashNodes}.cpp`、`tests/integration/{GPUABIBindingTierIntegrationTest,JFAPassUpsampleMaskTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`、`tests/performance/MaterialLightingBenchmark.cpp`、`docs/plans/2026-09-12-skill1-9-followup-backlog.md`。
另有一个工作树内既存用户文档改动：`设计文档/职业被动和技能设置.md`（+4，非本实现包产物）。

新增（未跟踪）：
`src/game/systems/skill/behaviors/BeamChannelShared.hpp`、`src/game/systems/skill/behaviors/SwordArrayShared.hpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`。

边界核查结论：

- `settings.json` **未出现在 `git status`**（`git status --short -- settings.json` 为空），未被本轮改动。
- 仓库根目录**无杂散新文件**：未跟踪项仅为上述 4 份文档、2 个新头文件、2 个新测试。
- `git status --short | rg "^\?\?"` 输出与计划 §2 声明的产物一致。

## 范围对齐

设计与计划对本次修复的范围（清缓存判定 `is_baked`、半径单一来源、`TestSetupScope` 写路径加固、node 1015 `stat_modifiers` 清空）与实际 diff 一致；**行为性改动文件全部落在清单内**。不对齐点集中在计划的文件清单元数据（见 Medium-1）。

三份生成/校验脚本与构建脚本门禁均通过：

- `python scripts/validate_skill_spec_modifiers.py --check` → `EXIT 0`，`skill_spec modifier offline gates passed: 6/6 passed.`
- `python scripts/gen_skill_mechanics_schema.py --check` → `EXIT 0`，`[gen-schema] OK: skill_mechanics_schema.json up to date (437 entries, 62 unreferenced)`
- `python scripts/gen_skill_contracts.py --check` → `EXIT 0`，`[OK] skill_contract blocks are up to date.`
- `build.bat check`（经 `%TEMP%` 包装 `.cmd` + `Start-Process -Wait -PassThru` 取 `.ExitCode`）→ **EXITCODE=0**，stdout/stderr 中 `is not recognized as an internal or external command` 匹配数 **0**，stdout 仅 `[Build]` 行。

### H-R2-1 复核（High，已关闭）

两个被点名的写路径均已修复：

1. `tests/integration/JFAPassUpsampleMaskTest.cpp`：新增 `#include "TestCommon.hpp"`，在受影响用例（`TEST_CASE` @211）声明 `TestSetupScope settingsGuard;`（:219），紧邻默认实参写路径 `qm.Initialize();`（:223），并在 :217-218 注释写明默认实参即仓库根 `settings.json`。默认实参来源 `src/engine/render/core/QualityTierManager.hpp:100` 未变，但调用点已被夹具覆盖。
2. `tests/unit/RenderSystemInitializeFailureTest.cpp`：新增 `#include "TestCommon.hpp"`（:13），两个失败用例分别声明 `TestSetupScope`（:26、:72）。写路径 `src/engine/render/RenderSystem.cpp:969` `qualityManager.Initialize("settings.json")` 位于能力门禁 `:1008-1027` 之前，能力失败用例确实「先写后失败」，夹具覆盖正确。

**穷尽性独立重扫（三类写路径）**：

- (a) 显式字面量 `Initialize("settings.json")`：`tests/performance/MaterialLightingBenchmark.cpp:119`、`tests/integration/GPUABIBindingTierIntegrationTest.cpp:23`、`tests/integration/RenderSystemPhaseDToggleSmokeTest.cpp:63`、`tests/integration/MaterialLightingIntegrationTest.cpp:55`+`:86`、`tests/unit/VFXSequencerTest.cpp:152`+`:464` —— 共 **7 处 / 5 文件**，全部已声明 `TestSetupScope`（guards：`MaterialLightingBenchmark.cpp:109`、`GPUABIBindingTierIntegrationTest.cpp:16`、`RenderSystemPhaseDToggleSmokeTest.cpp:55`、`MaterialLightingIntegrationTest.cpp:43`+`:83`、`VFXSequencerTest.cpp:135`+`:445`）。
- (b) 无参默认实参 `Initialize()`：全仓库仅 `JFAPassUpsampleMaskTest.cpp:223` 一处（已覆盖）。
- (c) 间接入口：仅 `RenderSystem::Initialize()` → `RenderSystem.cpp:969`。测试侧调用点共 4 处（`RenderSystemPhaseDToggleSmokeTest.cpp:65`、`GPUABIBindingTierIntegrationTest.cpp:37`、`RenderSystemInitializeFailureTest.cpp:43`+`:80`），全部已覆盖；`src/` 内无其它测试可达的默认写路径。

补充穷尽性核查（第二轮未显式列出的旁路）：

- 同一管理器其它会持久化的 setter（`SetV3Enabled`/`SetClusteredLightingEnabled`/`SetNormalLightingEnabled`/`SetSpecularEnabled`/`SetLinearPipelineEnabled`，声明于 `QualityTierManager.hpp:137-145`，形参默认 `settingsPath = "settings.json"`）：测试侧调用点（`QualityTierManagerTest.cpp`、`ReleaseGateIntegrationTest.cpp`、`ColorSpaceLinearizationTest.cpp` 等）**全部显式传入临时路径 `settingsPath.string()`**；`SetGiEnabledOverride` 为运行时开关，不落盘。生产侧 `src/game/application/states/SettingsState.cpp:397-415` 使用默认路径属**预期的游戏内设置持久化**，非测试污染源。
- `src/game/foundation/Settings.hpp:70` `Save(filePath="settings.json")` / `:94` `Load(...)`：测试侧**零调用**（`rg "\.Save\(" tests` 无命中）；生产调用仅 `src/app/Game.cpp:201`、`SettingsState.cpp:103`。
- 其它 `.*.Initialize()` 无参调用（`ShadowPipelineTierFallbackIntegrationTest.cpp:118/179/192`、`RenderGraphTierMatrixIntegrationTest.cpp:121/122/176/177`、`ClusteredLighting*` 等）目标为 pass/lightingPass，不是 `QualityTierManager`，不写 `settings.json`。

**结论：`src/` 与 `tests/` 中已不存在未加夹具的 `settings.json` 写路径。**

### H-R2-1 环境洁净度实证（精确验收）

对仓库根 `settings.json` 做 SHA256 基线，运行后逐段复算：

| 步骤 | 命令 | 结果 | hash 是否一致 |
|---|---|---|---|
| 基线 | `(Get-FileHash settings.json).Hash` | `6C446ED35D44A0F432F4072686E11DD74E1FB736D37D25685C23BB6074298485` | — |
| 1 | `bin\NoMoreDayTests.exe --test-case="*VFXSequencer*,*JFAPass*,*GPU ABI*,*Material Lighting*,*RenderSystem*"`（含全部 7 个字面量写点 + 2 个失败用例） | EXIT 0，`42 passed`，`1143/1143 assertions` | 一致 |
| 2 | `ctest --test-dir build -C RelWithDebInfo -L unit` | EXIT 0，8 tests / 17.94s | 一致 |
| 3 | `ctest --test-dir build -C RelWithDebInfo -L integration` | EXIT 0，6 integration tests / 7.42s | 一致 |
| 4 | `ctest --test-dir build -C RelWithDebInfo -R nmd.tests.ci.nonperf` | EXIT 0，1/1 passed / 13.32s | 一致 |

四次比对后哈希恒为 `6C446E…8485`。**并且 `settings.json` 的 `LastWriteTime = 2026/9/17 18:52:58`**（与步骤 1 运行时刻吻合），证明写路径**确实发生**、随后被夹具还原（若夹具缺失，内容会漂移）。此为 H-R2-1 修复有效的直接反证，而非仅凭测试全绿。

功能回归：`bin\NoMoreDayTests.exe --test-case="*SkillWrapup*,*SkillProfileResolve*,*SkillBatch4*"` → `29 passed | 0 failed`，`224/224 assertions`。

### Task 3.0 门禁复核（证伪是否诚实、是否掩盖缺陷）

- **机制正确性**：`src/game/foundation/components/Stats.hpp:399-406` 在缺键时令 `required_tags = Tag::None`（默认值 `Stats.hpp:327`）；`src/game/contracts/impl/StatsSystem.cpp:318` `bool is_baked = (mod.required_tags == Tag::None);`、`:323` `if (!is_baked) { … ApplyStatCalculation(…) }`。故 `required_tags == Tag::None` 的修饰符在**所有作用域**（含 `SkillOnly`）都被跳过。节点 1015 的 DodgeChance 修饰符正好缺 `required_tags`，因此在 skill_id=0 与 skill_id=10 下都惰性；`StatsSystem.cpp:446-495` 的专精节点路径（`:463` 取 `NodeContractData`、`:465-468` 默认 `SkillOnly`、`:384-407` `can_apply_scope`、`:487` 调同一 lambda）会使它在 `SkillOnly` 下被 `skill_id != 0 && source_skill_id == skill_id` 再挡一次。**双重惰性成立**。
- **灵敏度对照可信**：`tests/unit/SkillWrapupHardeningTests.cpp:196` `REQUIRE(baselineProbe == Approx(15.0f))` 是真实正对照——若探针路径（`dodge_chance=0.15`）失效会立刻失败，因此 :214-215 的「清理后 scoped 值 == baseline」不是自证式恒真断言。
- **证伪记录诚实**：测试 :179-184 注释、计划 Task 3.0（plan:118-123）与 backlog §1.4 均已记录「原正对照 `skill_id=10 应不同` 被实证证伪」。
- **判定**：将断言由「必须不同」改为「等于 baseline」**是正当的**——它精确断言了设计 §2.3.3 选定的 option A 语义（清空 `stat_modifiers` 后无任何增益），**不构成掩盖缺陷**。

## 质量与风险评估

对修改行逐一检视（非依赖实现者自述），未发现内存安全、并发、热路径字符串分支等 `conductor/code_standard.md` 硬否决问题；`TestSetupScope` 新增代码使用 `std::filesystem`/`std::ifstream`/`std::vector<char>` RAII，无裸 `new`/`delete`。

- 缓存判定改以 `bool is_baked` 显式位（`SkillDefs.hpp`）替代原先「`area_radius == 1.0f` 哨兵」推断，`SkillProfileResolve.cpp` 三处统一返回 null，语义清晰。
- 半径单一来源：`BeamChannelShared.hpp:20` 定义 `NoMoreDay::ResolveBeamChannelMaxRange`，`SwordArrayShared.hpp` 定义 `kSwordArrayBaseCastRange`，Baker 与运行时共用，避免双份维护（符合 review.md §5「不重复造轮子」）。
- `BeamChannelDeliverySystem.cpp` 保留了 `area_radius > 1.0f` 数值护栏并按设计 §1.2「保留数值/除零护栏」要求附证伪注释；删除的仅是哨兵专用 `profile != nullptr` 类冗余合取。

**新引入缺陷排查（针对修复本身）**：

- (i) `TestSetupScope` 现含 Logger/ItemFactory/ProcBudget/StatsSystem/技能表重载等重初始化，被放入渲染/GPU 用例。逐一核查无顺序/生命周期冲突：`tools::Logger::Init()`、`ItemFactory::initialize()`、`StatsSystem::Reset()`、`ResetSkillRegistries()`（`SkillRegistry::Get().LoadFromJson("assets/data/skills.json")`，`TestCommon.hpp:142`）均为 CPU 侧、不依赖 GL 上下文，且在 `GPUUtils::Initialize()` 之前调用无碍。
- (ii) `RenderSystemInitializeFailureTest` 中 `TestSetupScope` 不触碰 `DeviceCapabilityMatrix` / GPU ABI 清单，失败注入（能力 override / ABI manifest override）保持原样；实测两个 `CHECK_FALSE(initResult)` 仍通过，**未掩盖或改变失败行为**。
- (iii) 快照污染风险：已确认 7 个写点各自**每用例仅一个** `TestSetupScope`，无同用例重叠/嵌套作用域（`VFXSequencerTest.cpp:135`/`:445` 分属两个不同 `TEST_CASE`；`MaterialLightingIntegrationTest.cpp:43`/`:83` 同理），故构造期快照不会快照到已被污染的中间态。该风险仅在**未来**出现「无夹具写点 + 嵌套作用域」组合时才会显形（见 Low-1）。
- (iv) `TestSetupScope` 快照先于其余初始化执行（`TestCommon.hpp:70`），析构中还原置于最后（`:86`），顺序正确。

## 发现项

### Blocker

无。

### High

无。**H-R2-1 已关闭**（见上）。第一轮、第二轮 High 均已闭环。

### Medium

**Medium-1（M-R2-1 残留）：计划 §2 文件清单仍未与实际 diff 对齐。**
第二轮点名的缺失文件（`SwordArrayShared.hpp`、`VFXSequencerTest.cpp`、`MaterialLightingBenchmark.cpp`、三个 `tests/integration/*`、两个 H-R2-1 文件）现均已补入（plan:68、80-86），但清单整体仍不匹配：

- **漏列修改文件**：`tests/unit/SkillSpecializationBakerTests.cpp`（`git diff --stat` = `+2`，正是本轮 L-R2-4 修复所改）未出现在清单中（全计划仅 plan:154 的 Task 6.3 正文提及）。`设计文档/职业被动和技能设置.md`（工作树 `+4`）亦未列入清单边界。
- **虚假「新增」行**：plan:76 `tests/unit/SkillBatch4NullProfileFallbackTests.cpp`、plan:77 `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp` 标注为「新增」，但两者在 HEAD 已被跟踪且**无任何改动**（`git status --short` 为空、`git diff --stat` 为空）。

证据：

```
git status --short -- tests/unit/SkillBatch4NullProfileFallbackTests.cpp tests/unit/SkillBatch4BloodSeaOrderingTests.cpp
(无输出)
git diff --stat -- tests/unit/SkillSpecializationBakerTests.cpp "设计文档/职业被动和技能设置.md"
 tests/unit/SkillSpecializationBakerTests.cpp | 2 ++
 设计文档/职业被动和技能设置.md               | 4 ++++
```

影响：计划清单是本包变更边界的权威记录，漏列 + 虚假「新增」会误导后续审查与回滚（review.md §6「范围对齐」、§禁止内容#3「不得隐瞒偏差」）。修复：补入 `tests/unit/SkillSpecializationBakerTests.cpp` 行，修正 76/77 行为「未修改」或删除，并显式标注 `设计文档/职业被动和技能设置.md` 为「工作树内既存用户改动、不属本包」。

### Low

**Low-1（L-R2-3 残留）：`TestSetupScope` 进程级覆盖门限只在计划侧记录，设计侧缺失。**
计划 Task 5.4（plan:145）已完整写明「该夹具**进程级**且仅保护**显式声明的 `TestSetupScope` 作用域**；同一进程内未被夹具覆盖的写点不会被还原，反而可能被后续作用域的『原始内容』固化」及 (a)/(b)/(c) 三类枚举。但 `docs/designs/...-design.md §3.3`（design:312-319）只写了「同进程比对」验收命令，未记录该覆盖门限；`tests/TestCommon.hpp:61-67` 的契约注释也只提「进程被强杀 → `git checkout` 兜底」。建议在设计 §3.3 与 `TestCommon.hpp` 注释同步该门限，避免后续新增写点再次成为盲区。

**Low-2（BP-R2-1 附带）：`build.bat` 中文 `REM` 与 backlog 声明的「已改 ASCII」不一致。**
`build.bat:315-316` 澄清注释已加入且措辞准确（`run_quiet_step already forwards the exit code; the trailing if errorlevel 1 exit /b 1 is intentional belt-and-braces. Do not remove.`）。但 `build.bat:313-314` **仍为中文 `REM`**（内容为「这两步为只读门禁…」），而 `docs/plans/2026-09-12-skill1-9-followup-backlog.md:123`（F-08）声称「本轮改为 ASCII 注释」。即：本轮实际做法是**追加 ASCII 副本**而非替换中文行，文档与代码不符。解析完整性实测无虞（`build.bat check` = EXIT 0，且无 `is not recognized as an internal or external command`，`build.bat:2` 有 `chcp 65001`），故当前无功能影响；但既然 F-08 已知中文 `REM` 会在其它代码页下被截断为命令，建议要么真正替换为 ASCII，要么修正 backlog 表述。

**Low-3（新发现）：`tests/unit/RenderSystemInitializeFailureTest.cpp:71` 注释与事实不符。**
注释称「ABI 清单校验失败也要在写入 `settings.json` 之后才返回」。实际顺序为 `src/engine/render/RenderSystem.cpp:961-966` 先做 ABI 校验并 `return false`，`:968-969` 才 `qualityManager.Initialize("settings.json")`。因此 ABI 失配用例（`RenderSystemInitializeFailureTest.cpp:65-99`）**在写入之前即返回**，从不是污染源；此处新增的夹具属无害的防御性冗余，但注释结论错误。建议改为「本用例在写入前返回，夹具为防御性保留」。

### BestPractice

**BP-1：设计/计划中的命名空间与实现不符。** design §2.2.3 与 plan 写作 `skills::ResolveBeamChannelMaxRange`，实现位于 `src/game/systems/skill/behaviors/BeamChannelShared.hpp:20`，命名空间为 `NoMoreDay`（非 `skills`）。不影响行为，但会误导检索。

**BP-2：Task 3.0 缺「专精节点修饰符路径为正」的正对照。** 现有 :196 正对照只证明全局修饰符探针路径存活；对专精节点路径（`StatsSystem.cpp:487`）无法区分「节点 1015 修饰符被正确忽略」与「该路径整体失效」。建议补一条 `required_tags != Tag::None` 的节点修饰符正对照（或在 F-06 中显式登记该测试盲区）。

**BP-3：默认实参根因未消除。** `QualityTierManager.hpp:100` 与 `:137-145` 的 `settingsPath = "settings.json"` 默认实参是 H-R2-1、L-R2-2 两类盲区的共同根因；F-07 已登记候选根治方案（`InitializeForTesting()`）但未采纳。属可接受的技术债，建议排期。

**BP-4：重夹具下沉到渲染/性能用例。** 为使 `settings.json` 快照/还原可用，`MaterialLightingBenchmark.cpp:109` 等性能/GPU 用例现在也会执行技能表 `LoadFromJson` 与 `ItemFactory` 初始化。功能无碍，但增加了与技能子系统的非必要耦合及基准启动开销。可考虑拆分一个仅做 settings 快照/还原的轻量 guard。

## 最佳实践建议

1. **（对应 Medium-1）** 更新 `docs/plans/...-plan.md §2` 文件清单：补 `tests/unit/SkillSpecializationBakerTests.cpp`（修改 +2）；将 `tests/unit/SkillBatch4NullProfileFallbackTests.cpp` / `SkillBatch4BloodSeaOrderingTests.cpp`（plan:76-77）由「新增」改为「未修改」或删除；显式标注 `设计文档/职业被动和技能设置.md` 为工作树内既存用户改动。使清单与 `git status --short` + `git diff --stat` 逐条一致。
2. **（对应 Low-1）** 在 design §3.3 与 `tests/TestCommon.hpp:61-67` 补写 `TestSetupScope` 的进程级覆盖门限（仅保护显式作用域；未覆盖写点会被后续作用域固化为「原始内容」）。
3. **（对应 Low-2）** 将 `build.bat:313-314` 中文 `REM` 真正替换为 ASCII（或将 backlog:123 的「本轮改为 ASCII 注释」更正为「已追加 ASCII 注释，中文行保留并经实测可解析」）；替换后重跑 `build.bat check` 验证无 `is not recognized` 行。
4. **（对应 Low-3）** 更正 `tests/unit/RenderSystemInitializeFailureTest.cpp:71` 注释，或保留夹具但注明其为防御性冗余。
5. **（对应 BP-2）** 增加专精节点修饰符路径的正对照测试。
6. **（对应 BP-3/BP-4）** 评估 F-07 的 `InitializeForTesting()` 根治方案与轻量 settings guard；在采纳前继续保留现有 `git checkout -- settings.json` 人工兜底说明。

## 剩余风险

1. **已知接受残留（非本轮引入）**：`SkillSystem::GetBakedSkillProfile`（`SkillSystem.cpp:2766-2779`）为缓存直通、不过滤 `is_baked`，全仓约 **30 处**直接调用点（含 `BeamChannelDeliverySystem.cpp:153`、`PlayerHUD.cpp:144`、`GameUiSnapshotBuilder.cpp:406` 等）仍可能读到未烘焙哨兵；设计 §1.2 已登记为接受项。
2. **已知接受残留**：**42 个惰性天赋节点修饰符**（mastery 32 + skills 10），因 `required_tags` 缺省为 `Tag::None` 而在所有作用域被跳过（F-06）；node 1015 本轮仅清空 `stat_modifiers`，系统性根因未动。
3. **`settings.json` 兜底**：`TestSetupScope` 在进程被强杀时不生效，仍依赖提交前人工 `git checkout -- settings.json`（`TestCommon.hpp:66-67` 已注明）。
4. **中文 `REM` 解析依赖代码页**：`build.bat:313-314` 在本机 `chcp 65001` 下解析正常（已实证），但在其它代码页/CI 环境下是否稳定未验证（F-08 曾观察到失败）。
5. **热路径字符串键**：`ResolveBeamChannelMaxRange` 回退经 `SkillMechanicsRegistry::GetFloat(beamSkillId, 0u, "base_range", …)` 做字符串键查询，被 `GameplayState.cpp:877` 指示圈与技能运行时路径调用；该模式为设计 §2.2.3 显式授权的既有机制，未发现新增性能回退证据，但未做帧级性能剖析。

## 下一步动作

1. **只修文档元数据即可放行（无需改代码）**：按「最佳实践建议 1」补齐/修正计划 §2 文件清单，使其与 `git status --short`、`git diff --stat` 完全一致。这是当前唯一的 Medium。
2. 顺带处理 Low-1（设计 §3.3 覆盖门限）、Low-2（`build.bat` 中文 `REM` 与 backlog 表述）、Low-3（失败用例注释）。
3. 重新提交并请求最终确认；因仅涉及文档，预期无需重跑完整构建，但 `build.bat check` 若改动 `build.bat` 注释则须重跑并确认无解析噪声。
4. 完整构建/测试证据（本报告已记录）无需重做。

## 轮次结论

- H-R2-1（High）：**已关闭**。两处写路径已夹具化；三类写路径穷尽重扫无遗漏；`settings.json` 哈希在 4 段运行后恒为 `6C446E…8485`，且 mtime 变化证明「写入确已发生并被还原」。
- L-R2-1、L-R2-2、L-R2-4：**已关闭**。BP-R2-1：可接受但伴随 Low-2 文档不符。
- L-R2-3：**部分关闭**（计划侧已记录，设计侧缺失 → Low-1）。
- M-R2-1：**未完全关闭**（计划 §2 清单仍不对齐 → Medium-1）。
- 新引入缺陷：仅 Low-3（注释事实错误），未发现 Blocker/High 级新缺陷。

按 `docs/workflows/review.md` 判定规则 2，仍残留 Medium，不得 `提交`。

# 修改
