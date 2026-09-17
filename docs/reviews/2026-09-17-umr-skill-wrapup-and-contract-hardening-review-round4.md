# UMR 技能收尾与契约加固 — 第四轮（最终通过）独立复审报告

## 审查目标

`2026-09-17-umr-skill-wrapup-and-contract-hardening` 变更包的第四轮（收尾）独立验证复审。目标：确认第三轮结论 `修改` 的全部发现项（Medium-1、Low-1～Low-3）与 4 条最佳实践建议已闭合或已登记，且补救未引入新缺陷，判定可否 `提交`。

## 结论

`提交`

## 审查轮次

最终通过审查（第四轮）。独立复核，未编辑任何源码/测试/数据/文档（仅写入本报告）。

## 输入

- 设计：`docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md`（§3.3 覆盖门限见 :315）。
- 计划：`docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md`（v1.2，§2 清单 :52-91）。
- 审查标准：`docs/workflows/review.md`。
- 前序报告：`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review.md`、`...-review-round2.md`、`...-review-round3.md`（均未改动）。
- 后续登记册：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（§1.2～§1.4）。
- 验证证据：见「变更文件边界」「质量与风险评估」「范围对齐」各节内嵌命令输出。

## 变更文件边界

`git status --short` 摘要：26 个已跟踪文件修改（`+278 / -55`），9 个未跟踪新增。工作树内 `settings.json` **未**出现在 `git status` 中；仓库根目录**无**游离文件（`git status --short | Where-Object { $_ -match '^\?\? [^/\\]+$' }` 返回空）。

已跟踪修改（实测）：`assets/data/mastery_skill_trees.json`、`assets/data/skill_mechanics_schema.json`、`build.bat`、`docs/plans/2026-09-12-skill1-9-followup-backlog.md`、`src/game/application/states/GameplayState.cpp`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp`、`src/game/systems/skill/SkillProfileResolve.cpp`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/{BloodSea,HeavenlySwordDescent,SevenStarSlash,SwordArray}.cpp`、`tests/TestCommon.hpp`、`tests/functional/{SevenStarSlashNodes,SwordArrayNodes}.cpp`、`tests/integration/{GPUABIBindingTierIntegrationTest,JFAPassUpsampleMaskTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`、`tests/performance/MaterialLightingBenchmark.cpp`、`tests/unit/{RenderSystemInitializeFailureTest,SkillProfileResolveTests,SkillSpecializationBakerTests,VFXSequencerTest}.cpp`、`设计文档/职业被动和技能设置.md`。

未跟踪新增：`docs/designs/2026-09-17-...-design.md`、`docs/plans/2026-09-17-...-plan.md`、三份 `docs/reviews/2026-09-17-...-review*.md`、`src/game/systems/skill/behaviors/{BeamChannelShared.hpp,SwordArrayShared.hpp}`、`tests/unit/{SkillProfileResolveSentinelTests.cpp,SkillWrapupHardeningTests.cpp}`（实测字节数 5161 / 14102 / 1407 / 397，均存在且两个共享头均为 `namespace NoMoreDay`）。

## 范围对齐

### Medium-1（计划 §2 清单与实际 diff 对齐）— 已闭合

独立重算 diff 后逐项比对，`plan.md:52-91` §2 清单现已与实际工作树一致：

- `plan.md:76` 新增行 `现有测试 | tests/unit/SkillSpecializationBakerTests.cpp | 修改（+2）`，与 `git diff --stat` 的 `2 +` 一致，说明为「技能 7 射程字面量期望补注释」。
- `plan.md:77` / `plan.md:78` 将 `tests/unit/SkillBatch4NullProfileFallbackTests.cpp`、`tests/unit/SkillBatch4BloodSeaOrderingTests.cpp` 标为 `**未修改**`，并注明 `git diff` 为空、仅作哨兵/空档案回退语义回归证据（`plan.md:108` Task 1.7 复述）。实测二者确不在 `git status --short` 中，`plan.md` 原先的「补入清单为回归证据」（`plan.md:31`）指登记证据而非宣称文件变更，语义已澄清。
- `plan.md:91` 新增 `文档（**非本包**） | 设计文档/职业被动和技能设置.md | 工作树内既存用户改动 | 本包实施前已存在的未提交改动，不属本包变更边界，不纳入审查/回滚范围`，正确地将该文件排除出包边界。

补充核对：以脚本将 `git status --short` 的每条路径回查 `plan.md` 全文，全部实现类路径命中；未命中项仅为 5 份本包文档（design / plan / 三份 review，属包自身产物，非 §2 清单项）与一条中文路径的 shell 八进制转义假阴性（`plan.md:91` 已含该行）。**无遗漏、无虚假归属。**

## 质量与风险评估

### Low-1（覆盖门限双处登记）— 已闭合

- 设计 `design.md:315` §3.3 现含 `**覆盖门限（必须知晓）**：TestSetupScope 是进程级且仅保护显式声明作用域的用例……` 及其 (a)/(b)/(c) 枚举。
- `tests/TestCommon.hpp` 中 `struct TestSetupScope` 上方注释块同步加入「覆盖门限：本夹具是进程级且仅保护显式声明 TestSetupScope 的用例……」，两处一致。
- 夹具实现仍为构造时先 `SnapshotSettingsFile()`、析构末 `RestoreSettingsFileIfChanged() const`（仅当字节不同才回写），未因注释改动改变行为。

### Low-2（build.bat 中文注释）— 已闭合

- `build.bat:313-317` 技能门禁预检块现为全 ASCII（`REM ... skill gates before the runtime binary ...`），`build.bat:318/320` 调用 Python 门禁。
- 实测 `build.bat check` 经 `Start-Process -Wait -PassThru` 读取 `.ExitCode`：**EXITCODE=0**，输出中 `is not recognized as an internal or external command` 匹配数 **0**，脚本解析正常。
- `build.bat:64` 仍有中文 REM，但经 `git show HEAD:build.bat` 比对，该行为本条改动前既存内容，非本轮引入；不构成对后续登记册「本轮改为 ASCII 注释」的虚假主张（该主张仅针对本次新增块）。

### Low-3（RenderSystemInitializeFailureTest 注释）— 已闭合

`tests/unit/RenderSystemInitializeFailureTest.cpp:71-72` 注释现为：「本用例在 ABI 校验处即返回（`RenderSystem.cpp:961-966` 先于 `:968-969` 的写入），本身并非污染源；夹具为防御性保留」。与源码逐行核对：`src/engine/render/RenderSystem.cpp:961-966` 为先 ABI 校验并 `return false`，`:968-969` 才 `qualityManager.Initialize("settings.json")`，注释与实现完全一致。首个用例 `:24-25` 关于 capability gate 先写入的注释保持正确未改。

### 独立再验证（只读）

- 离线门禁（EXIT 0）：`validate_skill_spec_modifiers.py --check`（`6/6 passed`）、`gen_skill_mechanics_schema.py --check`（`437 entries, 62 unreferenced`）、`gen_skill_contracts.py --check`（`[OK] skill_contract blocks are up to date.`）。
- 环境洁净度（内容哈希贯穿不变）：
  - 基准 SHA256 = `6C446ED35D44A0F432F4072686E11DD74E1FB736D37D25685C23BB6074298485`；
  - (a) `NoMoreDayTests.exe --test-case="*VFXSequencer*,*JFAPass*,*GPU ABI*,*Material Lighting*,*RenderSystem*"` → exit 0，42 passed / 0 failed，1143/1143 断言；
  - (b) `ctest -C RelWithDebInfo -L unit` → exit 0，8/8，18.77s；
  - (c) `-L integration` → exit 0，6/6，7.53s；
  - (d) `-R nmd.tests.ci.nonperf` → exit 0，1/1，13.70s；
  - 四次运行后重新哈希均等于基准（仅 mtime 可变），`settings.json` 内容始终未被污染。
- 代码 diff 抽查（`tests/TestCommon.hpp`、`build.bat`、`tests/unit/RenderSystemInitializeFailureTest.cpp` 及其余技能/Render 变更）语义自洽：`SkillProfileResolve.cpp` 三步统一 `is_baked` 拒签；`SkillSpecializationBaker.cpp` 仅在唯一成功出口置 `is_baked = true`；行为层仅移除哨兵专属 `skill != nullptr` 合取，数值/除零守卫（`area_radius > 0.0f`、`base_field_radius > 0.0f`）保留；`SwordArray.cpp` 仅在 `ownerPos` 存在且非移动光环时钳制落点，`distSq` 超距才开方；未发现热路径堆分配、裸 `new/delete`、`dynamic_cast`、裸线程或字符串键分支，符合 `code_standard.md` §2.1/§2.2/§5.2/§5.3/§6.1/§7.2/§8.1 与 `tech-stack.md` 约束。

### 被声明接受残余的登记核对（明确结论）

- **约 30 处 `GetBakedSkillProfile` 直连调用点**：已在 `design.md:29` 与 `design.md:118` 显式登记为「已知残余 / 已登记残余」，说明该接口不参与 `is_baked` 过滤、须保留空指针与数值守卫；并在 `docs/plans/2026-09-12-skill1-9-followup-backlog.md` F-02 登记为「本轮仅登记，不在范围内修改」。**登记有效、范围外明确。**
- **42 条惰性专精节点 `stat_modifiers`**：已在 `design.md:203` 与 backlog F-06 登记（`required_tags == Tag::None` 致 `StatsSystem.cpp:318` 快路径对所有作用域跳过，`AttributePipeline` 亦不折叠），量化「mastery 32 + skills 10 = 42」，明确「不在本轮 UMR 收尾内一并修复，建议独立立项」。**登记有效、范围外明确。**

## 发现项

本轮**未发现**任何 `Blocker` / `High` / `Medium` / `Low` 级别问题。第三轮发现的 Medium-1 与 Low-1～Low-3 全部闭合，且未因补救引入新缺陷。

仅记录两条非阻塞观察（不计入发现项计数）：

- `Low`（历史遗留，非本轮引入）— `plan.md:67` 仍保留「（或既有共享头）」的措辞，而 `BeamChannelShared.hpp` 实际已作为新文件创建。仅为计划文字的历史保守表述，不影响实现与清单对齐，可在后续文档整理时顺带收敛。
- 信息性 — 5 份本包文档（design / plan / 三份 review）未列入 §2 清单，属既有约定（§2 面向实现变更文件），非遗漏。

## 最佳实践建议

第三轮的 4 条最佳实践均已闭合或登记，本轮无新增建议：

- **BP-1（文档命名空间漂移）— 已修复**：`design.md` §2.2.3 已统一为 `命名空间 NoMoreDay`；两个新增共享头亦为 `NoMoreDay`。
- **BP-2（专精节点修饰符路径缺失正向对照）— 已登记**：backlog F-06 内「测试盲区登记（2026-09-17 三轮复审）」明确登记该正向对照盲区为独立跟催项。
- **BP-3（默认参数根因）— 已登记**：backlog F-07(b) 记录 1 处默认参数写入面，并给出 `InitializeForTesting()` 根本解候选。
- **BP-4（轻量 settings guard）— 已登记**：backlog F-07(c) 记录 1 处间接写入面，并给出轻量化 guard 候选。

## 剩余风险

通过审查后显式接受的风险：

1. **行为层 `is_baked` 覆盖不完整**：`GetBakedSkillProfile` 的约 30 处直连仍不过滤未烘焙档案，依赖既有空指针/数值守卫兜底（`design.md:29`、`design.md:118`、backlog F-02）。属显式登记的包外残余。
2. **42 条惰性专精节点修饰符**：声明缺失 `required_tags` 时被 `StatsSystem` 快路径全作用域跳过、`AttributePipeline` 不折叠，表现为静默无效（`design.md:203`、backlog F-06）。属显式登记的独立跟催项，本轮不修。
3. **`settings.json` 写入面**：7/5 文件、1 默认参数、1 间接写入（backlog F-07）；本轮以 `TestSetupScope` 进程级夹具 + 覆盖门限文档（`design.md:315`、`tests/TestCommon.hpp`）缓解，加根因根治（`InitializeForTesting()`）仍为候选未落地。
4. **文档措辞**：`plan.md:67` 的「（或既有共享头）」历史措辞残留，纯文档清晰度，无正确性/集成影响。

## 下一步动作

`提交`。第三轮全部发现项闭合、4 条最佳实践已修复或登记、离线门禁与四路环境洁净度验证全绿且 `settings.json` 内容哈希贯穿不变；两条被声明接受残余均已正确登记为包外跟催，未隐藏于通过结论。建议在后续独立文档整理中顺带收敛 `plan.md:67` 的历史措辞。

# 提交
