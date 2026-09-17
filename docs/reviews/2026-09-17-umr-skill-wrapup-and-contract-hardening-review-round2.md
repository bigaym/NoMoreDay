# UMR-SKILL-WRAPUP-HARDENING 第二轮（复检）审查报告

- 审查对象：`docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md`、`docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md` 及其实施产物
- 基准：HEAD = `48618225`，工作区为未提交变更
- 第一轮报告（结论 `修改`）：`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review.md`
- 审查方式：**只读核查**（`git diff` / `git show` / `rg` / 指定测试子集 / 三个 `--check` 脚本）。未运行完整 `build.bat`，未修改任何代码、数据、文档。
- 结束时 `git status --short` 与审查开始时一致（23 M + 7 ??），确认审查无副作用。

---

## 1. 变更文件边界

以 `git status --short` / `git diff --stat` 为准，实际变更集合为（HEAD=48618225）：

### 1.1 已修改文件（23 个，+251 / -55）

| 类别 | 文件 |
|---|---|
| 底座结构 | `src/game/foundation/components/SkillDefs.hpp` |
| 烘焙层 | `src/game/systems/skill/SkillSpecializationBaker.cpp` |
| 底座解析 | `src/game/systems/skill/SkillProfileResolve.cpp` |
| 行为层 | `src/game/systems/skill/behaviors/{BloodSea,HeavenlySwordDescent,SevenStarSlash,SwordArray}.cpp` |
| 行为层 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp` |
| 渲染/UI | `src/game/application/states/GameplayState.cpp` |
| 数据资产 | `assets/data/mastery_skill_trees.json`、`assets/data/skill_mechanics_schema.json`（生成物） |
| 构建脚本 | `build.bat` |
| 测试夹具 | `tests/TestCommon.hpp` |
| 测试 | `tests/unit/{SkillProfileResolveTests,VFXSequencerTest}.cpp`、`tests/functional/{SevenStarSlashNodes,SwordArrayNodes}.cpp`、`tests/integration/{GPUABIBindingTierIntegrationTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`、`tests/performance/MaterialLightingBenchmark.cpp` |
| 总账 | `docs/plans/2026-09-12-skill1-9-followup-backlog.md` |
| 既有用户改动 | `设计文档/职业被动和技能设置.md`（第一轮已认定为用户先前编辑，不计入实现产物） |

### 1.2 新增文件（7 个，均为 untracked）

`src/game/systems/skill/behaviors/BeamChannelShared.hpp`、`src/game/systems/skill/behaviors/SwordArrayShared.hpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`、`docs/designs/2026-09-17-...-design.md`、`docs/plans/2026-09-17-...-plan.md`、`docs/reviews/2026-09-17-...-review.md`

### 1.3 边界结论

- 无越界改动：所有改动均落在设计/计划主题范围内（技能哨兵契约收敛 + 半径单源 + 节点 1015 + settings.json 环境卫生）。
- **登记缺口**：`src/game/systems/skill/behaviors/SwordArrayShared.hpp`（新增源文件）、`tests/unit/VFXSequencerTest.cpp`、`tests/performance/MaterialLightingBenchmark.cpp`、以及 3 个 integration 测试文件，均**未**进入 `plan §2` 变更文件清单；其中 `VFXSequencerTest.cpp` / `MaterialLightingBenchmark.cpp` 是本次 High 整改的实际落点，**任何文档都未登记**。详见发现项 M-R2-1。

---

## 2. 范围对齐（设计 / 计划）

### 2.1 已对齐

| 设计要求 | 代码现状 | 判定 |
|---|---|---|
| §2.1 `BakedSkillProfile::is_baked` | `SkillDefs.hpp:677` `bool is_baked = false;`，注释（:670-679）明确不得以 `area_radius==1.0f` 推断 | 一致 |
| §2.1 Baker 唯一成功出口置位 | `SkillSpecializationBaker.cpp:342` 唯一写入点；`:340-342` 注释约束提前返回不得置位 | 一致 |
| §2.1 三步过滤 | `SkillProfileResolve.cpp:16-21 / 24-28 / 31-39`，未烘焙一律 `nullptr` | 一致 |
| §2.2.4 BeamChannel 加固 | `BeamChannelDeliverySystem.cpp:228` `profile != nullptr && profile->is_baked && profile->area_radius > 1.0f` | 一致 |
| §2.2.3 技能 7 默认值单源 | `BeamChannelShared.hpp:12` `kBeamChannelBaseRangeDefault = 350.0f`，被 `:26` 与 `SkillSpecializationBaker.cpp:115` 引用 | 一致 |
| §2.2.2 技能 6 单源 | `SwordArrayShared.hpp:8` `kSwordArrayBaseCastRange = 400.0f`，被 `SwordArray.cpp:132`、`SkillSpecializationBaker.cpp:106` 引用 | 一致 |
| §2.3 节点 1015 | `mastery_skill_trees.json:562-581` `id:1015` → `stat_modifiers: []` | 一致 |
| §2.3.1 Task 3.0 实证修订 | `SkillWrapupHardeningTests.cpp:185-216` 完全按修订后方案实现 | 一致 |
| §2.7 build.bat | `build.bat:288-311` 9 处 + `:313-321` 新增 2 处，均在 `:320 gen_modifier_runtime_v2.py` 之前 | 一致 |
| Task 5.4 夹具 | `TestCommon.hpp:68-132` 条件快照/按需还原 | 一致 |

### 2.2 未对齐（文档自身矛盾）

- `design §1.1`（`docs/designs/2026-09-17-...-design.md:25`）仍写「既有 9 处 precheck 调用均无失败中止判断（真正的门禁缺口）」，与同文档 `§2.7`（`:283-286`）的实证修订**直接矛盾**；`plan §0 C-8`（`docs/plans/2026-09-17-...-plan.md:25`）同样保留旧前提（其 `:132` 已修订）。见 L-R2-1。
- `design §2.7`（`:289`）称污染源「等 8 处」，与实测 literal 调用点 7 处（若含生产路径 `RenderSystem.cpp:969` 为 8）口径不一致。见 L-R2-2。
- `backlog F-07`（`docs/plans/2026-09-12-skill1-9-followup-backlog.md:113`）称「本轮以 TestSetupScope 覆盖已知 3 个 GPU 集成测试写入点」，已过时（实际 5 个文件的 7 处 literal 调用点）。

---

## 3. 质量与风险评估

### 3.1 测试与门禁实跑结果（本轮独立复现）

| 项目 | 命令 | 结果 |
|---|---|---|
| 单元子集 | `bin\NoMoreDayTests.exe --test-case=*SkillWrapup*,*SkillProfileResolve*,*SkillBatch4*` | **29 cases / 29 passed；224 assertions / 224 passed；SUCCESS** |
| 修饰符门禁 | `python scripts\validate_skill_spec_modifiers.py --check` | `[OK] ... 6/6 passed`，EXIT=0 |
| Schema 生成物 | `python scripts\gen_skill_mechanics_schema.py --check` | `[OK] ... up to date (437 entries, 62 unreferenced)`，EXIT=0 |
| 契约生成物 | `python scripts\gen_skill_contracts.py --check` | `[OK] skill_contract blocks are up to date.`，EXIT=0 |

### 3.2 第一轮 7 项整改声称的独立核实结论

| # | 声称 | 核实结论 |
|---|---|---|
| 1 | High：DoD §1.5 已闭环（新增 VFXSequencerTest 2 处 + MaterialLightingBenchmark 1 处） | **不成立（未闭环）**。声称的 3 处新增**属实**，但存在两个**未夹具化的写入点**（默认实参路径 + 间接路径）。见 H-R2-1。 |
| 2 | Medium：Task 3.0 已改为 REQUIRE + 灵敏度对照 + 同向 (2) | **成立**。机制论证（`Stats.hpp:399-406` 缺省 `Tag::None` + `StatsSystem.cpp:318/323` 跳过）经独立核对**正确**。见 3.3。 |
| 3 | Medium：负向中止证据已实测并回改文档 | **成立**。文档与 `build.bat` 代码一致、脚本证据链自洽、篡改文件已还原。见 3.4。 |
| 4 | Low：技能 7 默认值单源 | **成立（src 侧）**，但测试侧仍有字面量（L-R2-4）。 |
| 5 | Low：F-01 未同步已补 F-06/F-07/F-08 + §1.4 | **成立**，但 F-07 口径过时（L-R2-3）。 |
| 6 | Low：`skill_mechanics_schema.json` 为合法生成产物 | **成立**。`--check` EXIT=0，diff 仅 `dynamic_keys` 增 `base_range`。 |
| 7 | BestPractice：`TestCommon.hpp` `CHECK_FALSE` 为既有代码 | **成立**。`git show HEAD:tests/TestCommon.hpp` 的 `ResetSkillRegistries()` 内已有该断言，本轮 diff 未新增。 |

### 3.3 Task 3.0 机制论证复核（认定正确）

- `src/game/foundation/components/Stats.hpp:327` `Tag required_tags = Tag::None;`；`from_json`（`:399-406`）缺省即 `Tag::None`。
- `src/game/contracts/impl/StatsSystem.cpp:318` `bool is_baked = (mod.required_tags == Tag::None);`；`:323` `if (!is_baked) { ... ApplyStatCalculation ... }` → `required_tags == None` 时整体跳过，且 `:319-321` 只在 `!is_baked && player_tags != Tag::None` 时才可能翻回。
- Astrolabe 路径（`:337-346`）与 SkillModifierComponent 路径（`:348-353`）走同一 `apply_if_tags_match` lambda，故节点 1015 的修饰符**双重惰性**成立。
- 用例实现（`tests/unit/SkillWrapupHardeningTests.cpp:185-216`）：`:190` `CreateScopedCaster(..., 0.15f)` → `:194-196` `REQUIRE(baselineProbe == Approx(15.0f))`（灵敏度对照）；`:204-205` 全局域改 `REQUIRE`；`:214-215` 作用域断言由「必须不同」改为「与基准一致」。`:179-184` 注释如实记录正对照被证伪。
- 结论：断言**自洽且非自证**——门禁语义由 (1) 的 `REQUIRE` 承担，灵敏度对照排除「探针读不到任何来源」的假阴性；但 (2) 已退化为同向断言，**不再构成正对照**（已在文档与代码注释中如实登记，不构成虚假证据）。

### 3.4 Task 5.2 负向证据复核（认定成立）

- `build.bat:288-311` 9 处 precheck 每处均带 `if errorlevel 1 exit /b 1`；`git diff -- build.bat` 证明**非本轮新增**；本轮新增仅 `:313-321` 两条（`validate_skill_spec_modifiers.py --check`、`gen_skill_mechanics_schema.py --check`）。
- `build.bat:704-720 :run_quiet_step`：`:711 set "STEP_EXIT=%errorlevel%"`、`:716 endlocal & exit /b %STEP_EXIT%` → 退出码透传，失败前缀确为 `[Build] `。
- 脚本证据链：`scripts/validate_skill_spec_modifiers.py:262-266` 生成 `... param_f32 ... must be an integer`；`:510` `[FAIL] {name}: {n} issue(s)`；`:516-521` `[FAIL] ... ({passed}/{total} passed)` 并 `return 1`；脚本共 6 个 check → 与「5/6 passed、退出码 1」一致。
- 篡改目标已还原：`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json:606-648` 记录 `modifier_id 2002100`（`debug_name = RendingWave_Node210_Projectiles`），`:614`/`:644` 均为整数 `1.0`；`git status` 未列出该文件。
- 注：受「禁止运行完整 build.bat」约束，未独立重跑该负向实验；结论由脚本源码 + 已还原文件 + 文档记录交叉确证。

---

## 4. 发现项

### Blocker

无。

### High

#### H-R2-1　DoD §1.5（settings.json 环境卫生）仍未闭环 —— 存在 2 个未夹具化写入点

**文件:行号**
1. `tests/integration/JFAPassUpsampleMaskTest.cpp:217`（`qm.Initialize();`，`qm` 定义于 `:216`）
2. `tests/unit/RenderSystemInitializeFailureTest.cpp:37`（`RenderSystem::Initialize()`）→ `src/engine/render/RenderSystem.cpp:969`（`qualityManager.Initialize("settings.json")`）

**问题**
实施方只核对了**显式传参**的 `Initialize("settings.json")` 调用点，遗漏了**默认实参**与**间接调用**两条路径：

1. `src/engine/render/core/QualityTierManager.hpp:100`：
   `void Initialize(const std::string &settingsPath = "settings.json", bool forceRedetect = false);`
   —— 默认实参就是仓库根 `settings.json`，因此**无参 `Initialize()` 同样是写入点**。全仓 tests 扫描中，唯一命中 QualityTierManager 的无参调用即 `JFAPassUpsampleMaskTest.cpp:217`，而该文件**完全没有任何 `TestSetupScope`**。
2. `RenderSystemInitializeFailureTest.cpp:37` 以能力注入（`DeviceCapabilityMatrix::SetProbeOverrideForTesting`）触发失败，但 `RenderSystem.cpp` 的执行顺序是：`:961` ABI 校验 → `:969 qualityManager.Initialize("settings.json")` → `:1008-1026` 能力门禁（`ProbeCapabilities` / `CheckProductionRequirements` → `return false`）。即**能力门禁在写入之后**，真实 manifest 校验通过时该用例必然先写 `settings.json` 再失败返回。

**证据**

```
$ rg -n '"settings\.json"' src/ -g "*.cpp" -g "*.hpp"
src/engine/render/core/QualityTierManager.hpp:100:  void Initialize(const std::string &settingsPath = "settings.json",
src/engine/render/RenderSystem.cpp:969:  qualityManager.Initialize("settings.json");

$ rg -n "TestSetupScope" tests/integration/JFAPassUpsampleMaskTest.cpp
(无输出)

$ rg -n "RenderSystem::Initialize" tests/ -g "*.cpp"
tests/unit/RenderSystemInitializeFailureTest.cpp:37  (无 TestSetupScope)
tests/unit/RenderSystemInitializeFailureTest.cpp:71  (ABI mismatch，:961 即失败，安全)
tests/integration/GPUABIBindingTierIntegrationTest.cpp:37  (已夹具化)
tests/integration/RenderSystemPhaseDToggleSmokeTest.cpp:65  (已夹具化)

$ rg -n "DeviceCapabilityMatrix|ProbeCapabilities|return false" src/engine/render/RenderSystem.cpp
1008:  auto &capMatrix = ...DeviceCapabilityMatrix::Get();
1009:  const auto capabilityReport = capMatrix.ProbeCapabilities();
1011:      ...CheckProductionRequirements(...
1026:    return false;
```

写入确定性：`src/engine/render/core/QualityTierManager.cpp:123` 无条件调用 `PersistSelectionMetadata(settingsPath);`；`src/engine/render/core/QualityTierManager_Settings.cpp:673` 写入 `updatedAtUtc`、`:688` `WriteJsonAtomically(...)` → **时间戳必然变化，文件必被改写**。

覆盖面：`tests/CMakeLists.txt:4` `file(GLOB_RECURSE ...)` 保证两文件均被编译；`:54-61 nmd.tests.ci.nonperf`（`--test-case-exclude=*Performance*,*GPU-Diagnostic*`）与 `:81-88 nmd.tests.integration`（`--test-case=[Integration]*`）均会调度它们，`WORKING_DIRECTORY = ${CMAKE_SOURCE_DIR}` 即仓库根。

**说明（诚实边界）**：为避免污染工作区，本轮未实跑这两个用例做动态复现；上述为**代码路径静态确证**，路径唯一、无分支逃逸。

**建议**
1. 在 `JFAPassUpsampleMaskTest.cpp` 的该 TEST_CASE 内（`EnsureGpuContext()` 之后、`qm.Initialize()` 之前）加入 `NoMoreDay::test::TestSetupScope scope;`。
2. 在 `RenderSystemInitializeFailureTest.cpp` 的两个 `RenderSystem::Initialize()` 用例中各加入 `TestSetupScope`（或在 `RenderSystemInitializeFailureTest.cpp:37` 前）。
3. 更彻底的方案：让 `QualityTierManager::Initialize` 在测试构建（或提供 `SetSettingsPathOverrideForTesting`）下默认不落盘，从根上消除「默认实参」这类隐形写入面。
4. 复检方式：对 `rg -n "\.Initialize\(\)" tests/` 做**无参**形式的穷举，而非只 grep `"settings.json"` 字面量。
5. 在 `design §3.3` / `plan Task 5.4` 的验证清单中补入上述两个用例文件名。

---

### Medium

#### M-R2-1　High 整改的落点文件未登记，`plan §2` 文件清单与实际变更不一致

**文件:行号**
- `docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md:54-80`（§2 变更文件清单）
- `docs/plans/2026-09-12-skill1-9-followup-backlog.md:121`（§1.4）

**问题**
`plan §2` 清单缺失下列实际变更：
- `src/game/systems/skill/behaviors/SwordArrayShared.hpp`（**新增源文件**，清单只列了 `BeamChannelShared.hpp`）
- `tests/unit/VFXSequencerTest.cpp`
- `tests/performance/MaterialLightingBenchmark.cpp`
- `tests/integration/{GPUABIBindingTierIntegrationTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`

其中后 4 个恰是 DoD §1.5 整改的落点。`backlog §1.4` 只登记了 3 个 integration 测试为「范围扩展」，`VFXSequencerTest.cpp` / `MaterialLightingBenchmark.cpp` **在任何文档中均无登记**。

**证据**：`git status --short` 列出上述文件；`rg -n "VFXSequencer|MaterialLightingBenchmark|SwordArrayShared" docs/plans docs/designs` 在 plan §2 清单区域内无命中（`VFXSequencerTest.cpp` 仅在设计文档的 §1.5/§2.7 叙述中以文字出现，未进入文件清单）。

**建议**：将 4 个测试文件与 `SwordArrayShared.hpp` 补入 `plan §2` 清单，并在 `backlog §1.4` 同步更新「范围扩展」条目；同时把 H-R2-1 新增的两个文件一并登记。

---

### Low

#### L-R2-1　`design §1.1` 与 `§2.7` 自相矛盾，旧前提未清理

**文件:行号**：`docs/designs/2026-09-17-...-design.md:25` 与 `:283-286`；`docs/plans/2026-09-17-...-plan.md:25`（C-8）与 `:132`。

**问题**：§1.1 仍断言「既有 9 处 precheck 调用均无失败中止判断（真正的门禁缺口）」，而 §2.7 已实证修订为「9 处本就带 `if errorlevel 1 exit /b 1`」。同一文档内对同一事实给出相反陈述，会误导后续读者。

**证据**：`rg -n "无失败中止|errorlevel 1 exit /b 1" docs/designs/2026-09-17-...-design.md` → 两处并存。

**建议**：在 §1.1 处加「（已于 §2.7 修订，见 v1.2）」并改写为中性表述；`plan C-8` 的「修订前」列保持原样、增加指向 `:132` 的注记。

#### L-R2-2　`design §2.7` 污染源「8 处」口径与实测不一致

**文件:行号**：`docs/designs/2026-09-17-...-design.md:289`。

**问题**：称污染源「等 8 处」，而测试侧 literal 调用点实测为 7 处。若把生产路径 `RenderSystem.cpp:969` 计入则为 8，但文档未说明口径，且该处已由 `RenderSystemInitializeFailureTest` 间接触达（即 H-R2-1 第 2 点），口径混淆反而掩盖了风险。

**证据**：`rg -n "Initialize\(\"settings\.json\"" tests/ -g "*.cpp"` → 7 处命中（均有 TestSetupScope）；`rg -n 'Initialize\("settings\.json"\)' src/` → `RenderSystem.cpp:969`。

**建议**：明确写出「literal 调用点 7 处 + 默认实参路径 N 处」的分类计数，并与 H-R2-1 的整改结果同步。

#### L-R2-3　`TestCommon.hpp` 快照/还原的进程级副作用与修复覆盖面局限

**文件:行号**：`tests/TestCommon.hpp:68-132`（`:94-107` 快照、`:110-129` 还原）。

**问题**
1. 该夹具是**进程级全局副作用**：它只保护「已接入 scope」的用例。同一进程内若存在未夹具化写入点（正是 H-R2-1 的两个用例），其写入不会被还原，甚至可能被后续某个 `TestSetupScope` 的**快照**当作原始内容固化，导致修复对遗漏点完全无效——即「局部修复 + 全局快照」的组合会在测试顺序变化时产生**非确定性**。
2. `:110-129` 的还原逻辑在「文件被外部删除」时会主动写回快照字节，属磁盘副作用（虽然设计上可接受）。
3. 每用例完整读盘 + 条件写回，属可接受的性能代价（已由设计说明），但未在文档中量化。

**证据**：读 `tests/TestCommon.hpp:63-132`；`ResetSkillRegistries()` 中的 `CHECK_FALSE`（`:92` @ HEAD）在析构期归属不清（第一轮 BestPractice，实施方判定为既有代码，本轮已确认属实）。

**建议**：在 `plan Task 5.4` 的说明中补一句「本夹具只对已接入 scope 的用例有效；未接入的写入点不受保护，须逐一穷举接入」；并在 `design §3.3` 的验证清单中注明「应在**同一进程**内验证 settings.json 在整轮测试后无改动」。

#### L-R2-4　测试侧仍残留技能 7/6 默认值字面量

**文件:行号**：`tests/unit/SkillWrapupHardeningTests.cpp:286-287`（各写 `350.0f`）；`tests/unit/SkillSpecializationBakerTests.cpp:2141`（`350.0f`）。

**问题**：src 侧已单源化（`BeamChannelShared.hpp:12`），但测试期望值仍写死字面量。作为「期望值」可接受，但与原计划「单源」的意图存在落差；若默认值变更，需同时改多处。

**证据**：`rg -n "350\.0f" tests/ src/` → 仅上述测试命中；`rg -n "kBeamChannelBaseRangeDefault" src/ tests/` → `BeamChannelShared.hpp:12/26`、`SkillSpecializationBaker.cpp:115`。

**建议**：测试中对「默认值」的期望改为引用 `kBeamChannelBaseRangeDefault`；对「独立构造的期望」可保留字面量并加注释说明其独立性意图。

---

### BestPractice

#### BP-R2-1　`build.bat` `:run_quiet_step` 调用后的 `if errorlevel 1` 冗余

**文件:行号**：`build.bat:704-720`（`:716 endlocal & exit /b %STEP_EXIT%`）。

**问题**：`run_quiet_step` 已透传退出码，紧随其后的 `if errorlevel 1 exit /b 1` 属防御式冗余，但与本轮「统一中止语义」的目标一致，保留可读性更好；仅为可选清理项。

**建议**：维持现状即可，或在注释中说明为「双重保险」以免后续被误删。

---

## 5. 最佳实践建议

1. **写入面清点方法**：核查「测试不写仓库文件」时，必须同时穷举 (a) 显式字面量传参、(b) 默认实参调用（`.Initialize()`）、(c) 经由其它入口（如 `RenderSystem::Initialize`）的间接调用。本轮正是 (b)(c) 两类被遗漏。
2. **默认实参反模式**：把仓库根路径作为 `= "settings.json"` 默认实参，使「隐式写入」在调用点不可见。建议改为无默认值或默认空字符串，强制调用方显式决策。
3. **夹具的覆盖面声明**：`TestSetupScope` 这类进程级副作用夹具，应在头文件注释中显式声明「仅保护已接入 scope 的用例」，避免被误认为全局保险。
4. **文档口径一致性**：设计文档的「修订前/修订后」两处必须交叉引用；本轮 `§1.1` vs `§2.7` 的矛盾正是缺少交叉引用所致。
5. **边界登记**：凡新增源文件或超出计划清单的测试文件，必须同步 `plan §2` 清单与 `backlog §1.4`，否则复检无法判定边界合理性。

---

## 6. 剩余风险

| 风险 | 等级 | 说明 |
|---|---|---|
| 测试运行污染 `settings.json`（未夹具写入点） | **高** | H-R2-1：CI（`nmd.tests.ci.nonperf`）与本地 `ctest -L unit` 均可能触发，导致工作区脏化、`git checkout` 兜底被反复触发；DoD §1.5 无法在提交前兑现。 |
| 未登记文件的后续可维护性 | 中 | M-R2-1：`SwordArrayShared.hpp` 等未入清单，后续变更/回滚缺少依据。 |
| 文档自相矛盾导致误判 | 中 | L-R2-1 / L-R2-2：若后续复检只读 §1.1，会重新得出「9 处无中止」的错误结论。 |
| `GetBakedSkillProfile` 直连消费（约 30 处）仍不做 `is_baked` 过滤 | 中 | 已登记 F-02，属本轮明确接受的残留；哨兵防护依赖调用方空指针/数值守卫。 |
| 约 42 个专精节点属性修饰符惰性 | 中 | 已登记 F-06，非本轮范围。 |
| 节点 1015 `desc_key` 与技能机制不同步 | 低 | 已登记 F-01。 |
| `dynamic_keys` 三元组校验退化 | 低 | 已登记 F-08，生成物已 `--check` 通过。 |

---

## 7. 下一步动作

1. **【必须·阻塞提交】修复 H-R2-1**：为 `tests/integration/JFAPassUpsampleMaskTest.cpp` 的用例与 `tests/unit/RenderSystemInitializeFailureTest.cpp` 的两个 `RenderSystem::Initialize()` 用例加入 `TestSetupScope`；并考虑从 `QualityTierManager::Initialize` 默认实参侧根治。
2. **【必须】补登记 M-R2-1**：更新 `plan §2` 文件清单与 `backlog §1.4`。
3. **【建议】清理文档矛盾 L-R2-1 / L-R2-2**：修正 `design §1.1:25`、`plan C-8:25`，统一污染源计数口径。
4. **【建议】补强验证方法**：在 `design §3.3` 写明「同一进程内全量测试后比对 `settings.json` 内容不变」，并提供可复现的比对命令；将「无参 `Initialize()` 穷举」纳入复检清单。
5. **【建议】测试侧去字面量 L-R2-4、夹具覆盖面声明 L-R2-3、`build.bat` 冗余清理 BP-R2-1**。
6. 修复后需**再一轮复检**（至少对新增 `TestSetupScope` 的三个文件做静态确认 + 在可污染环境下实跑一次 `nmd.tests.ci.nonperf` 并比对 `settings.json`）。

---

## 8. 最终结论

# `修改`

判定依据（按优先级）：

1. **High 未闭环**：DoD §1.5 所依赖的「所有 `Initialize("settings.json")` 调用点均在 `TestSetupScope` 作用域内」这一前提**客观不成立**。存在 2 个未夹具化写入点（`JFAPassUpsampleMaskTest.cpp:217` 默认实参路径、`RenderSystemInitializeFailureTest.cpp:37` → `RenderSystem.cpp:969` 间接路径），且二者均会被 `nmd.tests.ci.nonperf` / `nmd.tests.integration` 调度。此时即使 `*SkillWrapup*` 等子集全绿、三个 `--check` 全绿，也无法支撑「环境卫生」这一验收项。
2. **整改真实性**：第一轮 Medium-1（Task 3.0）与 Medium-2（Task 5.2）的整改**经独立核实属实且机制论证正确**，Low-1/4/5/6 与 BestPractice 判定亦属实——本轮不否定这些工作的质量。
3. **不得以「测试通过」代替验收**：29/29 与 224/224 断言通过属必要条件，非充分条件；H-R2-1 恰是测试覆盖盲区，测试绿灯无法反证其不存在。

修复 H-R2-1 并补登记 M-R2-1 后，可进入第三轮复检；若无新增问题，可判 `提交`。

---

### 附：发现项计数

| 级别 | 数量 | 编号 |
|---|---|---|
| Blocker | 0 | — |
| High | 1 | H-R2-1 |
| Medium | 1 | M-R2-1 |
| Low | 4 | L-R2-1, L-R2-2, L-R2-3, L-R2-4 |
| BestPractice | 1 | BP-R2-1 |

第一轮 7 项发现项处置：**已修复 6 项**（Medium-1、Medium-2、Low-1、Low-4、Low-5、Low-6、BestPractice-1）／**未闭环 1 项**（High-1，见 H-R2-1）。
