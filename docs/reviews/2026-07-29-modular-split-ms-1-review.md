# 模块拆分 MS-1 审查报告

## 首次审查

**审查目标：** Modular Static Target Split, MS-1 minimal Types and Core candidate contract
**结论：** 修改
**审查轮次：** 首次审查

### 输入

- 设计：`docs/designs/modular-split-exe-lib-dll-design.md`
- 实施计划：`docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- 审查标准：`docs/workflows/review.md`
- 变更：`CMakeLists.txt`、MS-1 Core candidate contract/evidence 与实施计划
- 验证：`python scripts/check_module_boundaries.py`、`python -m unittest tests/python/ModuleBoundaryCheckerTest.py`、`build.bat`、`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`、`git diff --check`

### 变更文件边界

审查前工作区包含用户已有的 `docs/designs/modular-split-exe-lib-dll-design.md` 修改。该文件不属于 MS-1，审查确认它未由本包编辑、暂存或作为提交候选。MS-1 只增加空的 `NoMoreDayTypes` INTERFACE target、其 configure-time guard 和候选 Core/PCH 合同；未移动生产源码、启用最终 target graph 或修改渲染/GPU 范围。

### 范围对齐

保留现有 aggregate `NoMoreDayCore`、将 Game-ECS `PhysicsUtils` 延后到 MS-2、将 Windows/DbgHelp `CrashHandler` 排除在 Core candidate 外，均符合先清理逆向依赖再拆分最终 topology 的计划。空 INTERFACE target 本身不产生编译产物，符合 MS-1 的最小边界目标。

### 质量与风险评估

模块边界 guard（129/129）、focused Python tests（6/6）、JSON 解析以及 aggregate target build 均通过。但 CI 标签独立复现 2 个失败，且没有干净基线证明其为既有失败；此外 CMake target guard 和 JSON contract 缺少关键可执行约束。按审查流程，当前验证证据不足以提交。

### 发现项

1. **Blocker** `ctest -L ci` 复现 GI stability 与 SkillUI signature lock 失败：`tests/integration/GIStabilityIntegrationTest.cpp:48,157-160`、`tests/tech/UITests.cpp:413,437`。尽管这些测试不直接覆盖本包修改文件，未取得相同配置的干净基线，无法证明它们是既有问题；违反计划的提交验证证据要求。
2. **High** `CMakeLists.txt:358-378` 仅检查 `NoMoreDayTypes` 的 `SOURCES`，未检查 `INTERFACE_SOURCES`。`target_sources(NoMoreDayTypes INTERFACE ...)` 可向空 target 注入 source 而不被拒绝，空目标契约可被绕过。
3. **Medium** `docs/reports/modular-split-exe-lib-dll/ms-1/core-candidate-contract.json` 只有 JSON 语法检查，未由可重复执行的验证器检查候选路径、禁止依赖与 PCH 清单，后续容易漂移。
4. **Medium** `CMakeLists.txt:358-378` 未严格限制 `INTERFACE_INCLUDE_DIRECTORIES` 为唯一的 `src` 公共 include 根目录，未来可能无提示扩大 Types 的公共边界。

### 最佳实践建议

- 在 guard 中同时校验 `SOURCES`、`INTERFACE_SOURCES` 和严格的 `INTERFACE_INCLUDE_DIRECTORIES` 白名单，并为违反契约的配置增加负向测试。
- 添加独立合同验证器，在 configure/build 前验证 JSON 的路径、PCH 和禁止依赖约束。
- 在干净基线或独立临时工作区使用相同配置复现 CI；若失败既有，按流程记录基线证据或取得明确豁免。

### 剩余风险

尚未接受 CI 失败风险。legacy aggregate `NoMoreDayCore`、全局 PCH、`PhysicsUtils` 和 `CrashHandler` 的保留/排除是计划内过渡状态；它们不是本轮阻断原因。

### 下一步动作

补齐 Types target 的不可绕过 guard 和可执行 Core candidate contract verifier；获得 CI 失败的基线判定或修复回归；重跑 MS-1 验证后在本文件追加跟进审查。提交继续阻塞。

## Follow-up Review One

**Review target:** MS-1 Types target contract corrective package
**Conclusion:** 修改
**Review round:** Follow-up review one

### Inputs

- Design: `docs/designs/modular-split-exe-lib-dll-design.md`
- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Standard: `docs/workflows/review.md`
- Corrective package: Types CMake guard, Core candidate verifier/focused tests, and MS-1 evidence
- Verification: module-boundary guard, Core candidate verifier and focused tests, `build.bat check`, RelWithDebInfo build, `ctest -L ci`, and `git diff --check`

### Scope Alignment

The corrective package remains limited to CMake, Python verifier/tests, the `build.bat` precheck, and MS-1 documentation. It does not move production sources, enable the final target graph, or modify GPU/render, GI, UI, or gameplay code. The user-owned change to `docs/designs/modular-split-exe-lib-dll-design.md` remains excluded.

### Findings

1. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:123-146` and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:41-76`: GI long-run stability and Heavenly Sword freeze have neither a clean baseline nor a formal disposition. SkillUI is demonstrably a pre-MS-1 test/implementation drift, but the remaining CI failures still block submission.
2. **High** `CMakeLists.txt:358-395`: the guard runs before `add_subdirectory(tests)`. A later subdirectory can bypass it with `set_property(TARGET NoMoreDayTypes APPEND PROPERTY INTERFACE_SOURCES ...)`; the static verifier does not scan that API or subdirectories.
3. **Medium** `scripts/check_core_candidate_contract.py:153-180,294-340`: eligible implementation/header and deferred lists may all be empty. The verifier does not require complete `src/core` coverage, so audited candidates can disappear from the contract while it still passes.
4. **Medium** `scripts/check_core_candidate_contract.py:129-135` and `tests/python/CoreCandidateContractCheckerTest.py:109-150`: schema type constraints and focused negative coverage do not cover interface sources, link/PCH properties, property-API bypasses, or manifest completeness.

### Risk Disposition

Legacy aggregate retention, the inactive final target graph, and deferred P0/GPU work remain accepted in-scope transition risks. The Types property bypass, an emptyable contract, and the unclassified GI/Heavenly Sword CI failures are not accepted submission risks.

### Next Actions

Move the final Types guard after all subdirectories configure and protect property mutation paths. Require every `src/core` candidate file to be eligible or explicitly deferred, with negative tests. Re-run review after resolving these gaps; MS-1 remains blocked and must not be committed.

## Follow-up Review Two

**Review target:** MS-1 second corrective package
**Conclusion:** 修改
**Review round:** Follow-up review two

### Scope Alignment

The package remains within CMake, Python verification, `build.bat`, and MS-1 documentation. No production C++, final target graph, GPU/render, GI, UI, or gameplay change was found. The user-owned design document remains excluded.

### Findings

1. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:140-157`, `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:57-76`, and `tests/integration/GIStabilityIntegrationTest.cpp:159-160`: the GI failure has neither a same-configuration clean baseline nor a formally accepted disposition. SkillUI is demonstrably pre-MS-1 drift, but GI must still block submission.
2. **High** `scripts/check_core_candidate_contract.py:244-289` and `CMakeLists.txt:409-411`: direct literal mutation checks are improved, but variable/bracket target expressions, included `.cmake` files, and a redefined guard can bypass the one final call.
3. **High** `CMakeLists.txt:366-394` and `scripts/check_core_candidate_contract.py:327-352`: `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES` is not constrained, allowing a non-canonical system include root.
4. **Medium** `tests/python/CoreCandidateContractCheckerTest.py:160-210`: fixture coverage does not execute a temporary CMake configure and misses variable target, bracket argument, included CMake, guard-redefinition, and SYSTEM include cases.

### Risk Disposition

The expected transition state remains acceptable. The target/include guard gaps and the unresolved GI CI gate are not accepted submission risks.

### Next Actions

Use a deferred final runtime guard, constrain system interface includes, and add real temporary CMake configure negative fixtures for target mutation. Re-review after the guard is complete; MS-1 remains blocked and must not be committed.

## Follow-up Review Three

**Review target:** MS-1 third corrective package
**Conclusion:** 修改
**Review round:** Follow-up review three

### Inputs

- Design: `docs/designs/modular-split-exe-lib-dll-design.md` (protected user change; excluded)
- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Standard: `docs/workflows/review.md`
- Corrective package: root deferred Types guard, Core candidate verifier, configure fixtures, and MS-1 evidence
- Verification: contract verifier, 12 focused tests, Python compilation, `build.bat check`, RelWithDebInfo build, prior `ci` evidence, and `git diff --check`

### Scope Alignment

The package remains confined to CMake, Python verification/tests, and MS-1 documentation. No production C++, final target graph, GPU/render, GI, UI, or gameplay code changed. The protected design document remains excluded from this package and any eventual commit.

### Findings

1. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:123-124,155-162` and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:60-89`: the CI gate is still not satisfied. GI stability, SkillUI, and the previously unstable Heavenly Sword signal lack a complete same-configuration baseline or formal disposition. No commit may be accepted on the current evidence.
2. **High** `scripts/check_core_candidate_contract.py:253-278,393-418` and `CMakeLists.txt:358-424`: a first-party `cmake_language(EVAL CODE ...)` can dynamically redefine the final guard or cancel its deferred call after static parsing. The current verifier does not recursively parse that code, and the runtime check can therefore be prevented from running.
3. **Medium** `CMakeLists.txt:1,358-424` and `tests/python/CoreCandidateContractCheckerTest.py:74-80,181-185`: the repository declares CMake 3.20 support, but its new root-deferred behavior was exercised only with newer installed CMake versions. The claimed 3.20 behavior needs direct evidence or an approved minimum-version change.

### Verification Observed

The contract verifier passed, the focused suite passed 12 tests, Python compilation passed, `build.bat check` and the normal build passed, and `git diff --check` passed apart from existing line-ending warnings. No files were staged. The existing CI evidence remains red and was not treated as acceptable.

### Risk Disposition

Legacy aggregate retention, the global PCH, inactive final topology, deferred PhysicsUtils/CrashHandler work, and P0 rendering work remain accepted transition risks. The `EVAL` bypass, unverified CMake 3.20 behavior, and unresolved CI gate are not accepted submission risks.

### Next Actions

Fail closed on first-party `cmake_language(EVAL CODE ...)` or otherwise prove it cannot alter or cancel the final guard, with a real configure negative fixture. Establish CMake 3.20 behavior or formally revise the supported minimum version. Obtain a same-configuration baseline or formal disposition for all CI failures, then re-review before staging or committing MS-1.

## Follow-up Review Four

**Review target:** MS-1 fourth corrective package
**Conclusion:** 修改
**Review round:** Follow-up review four

### Scope Alignment

The latest package remains limited to CMake, the Core candidate verifier and fixtures, and MS-1 documentation. It does not modify production C++, the final target graph, GPU/render, GI, UI, or gameplay code. The user-owned design document remains excluded.

### Findings

1. **Blocker** `CMakeLists.txt:1` and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:57-58`: the project declares CMake 3.20 support, but the deferred final-guard behavior was exercised only with CMake 4.2.3. The required 3.20 evidence is still absent.
2. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:123-124,149-166` and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:69-98`: GI stability, SkillUI, and the previously unstable Heavenly Sword signal still lack a same-configuration baseline or formal disposition. CI therefore remains a submission blocker.
3. **High** `CMakeLists.txt:359-417`, `scripts/check_core_candidate_contract.py:402-425`, and `tests/python/CoreCandidateContractCheckerTest.py:264-369`: first-party CMake can shadow a guard-dependent built-in such as `get_target_property`, return compliant values, and combine that with a variable-target mutation not recognized by the static checker. The `cmake_language` policy does not prevent this path.

### Verification Observed

The contract verifier and 14 focused tests passed; Python compilation, `build.bat check`, and the normal build previously passed; `git diff --check` passed apart from existing line-ending warnings. The latest review used CMake 4.2.3 and observed no staged files. Existing CI failures remain undisposed.

### Risk Disposition

The legacy aggregate, global PCH, inactive final target graph, deferred PhysicsUtils/CrashHandler, and P0 rendering work remain accepted transition risks. Command shadowing, the missing CMake 3.20 evidence, and the unresolved CI gate are not accepted submission risks.

### Next Actions

Reserve guard-dependent commands against first-party redefinition and add a real negative fixture combining command shadowing with variable-target mutation. Obtain actual CMake 3.20 evidence or an approved version-policy change, then obtain a baseline or formal disposition for each CI signal before another final review.

## Follow-up Review Five

**Review target:** MS-1 fifth corrective package
**Conclusion:** 修改
**Review round:** Follow-up review five

### Scope Alignment

The package remains within the MS-1 Types contract: CMake guard validation,
the Core candidate checker and its temporary CMake fixtures, and MS-1
documentation. It does not change production C++, the final target graph,
GPU/render, GI, UI, or gameplay behavior. The user-owned design document is
excluded.

### Findings

1. **Blocker** `CMakeLists.txt:1`,
   `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:146-149`, and
   `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:67-68`: CMake 3.20
   runtime behavior remains untested; available evidence used CMake 4.2.3.
2. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:159-176`
   and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:78-107`: GI
   stability, SkillUI, and the previously unstable Heavenly Sword signal still
   lack a same-configuration baseline or formal disposition, so CI cannot
   support submission.
3. **High** `scripts/check_core_candidate_contract.py:355-359,457-465` and
   `tests/python/CoreCandidateContractCheckerTest.py:309-320`: guard
   redefinition detection is case-sensitive and recognizes only `function`.
   A case variant or `macro(_nmd_ms1_types_boundary_final_guard_7f3c9a)` can
   replace the final guard after its deferred call, leaving a variable-target
   mutation unchecked. Real configure checks reproduced this bypass.
4. **Medium** this report ended at Follow-up Review Four before the current
   bracket/variable-name corrective package. Its 14-test evidence is stale;
   this section records the current 18-test review result.

### Verification Observed

The contract verifier passed, the focused suite passed 18 tests, Python
compilation and `build.bat check` passed, and `git diff --check` had no
whitespace errors apart from existing line-ending warnings. The review used
CMake 4.2.3 and did not rerun the known-red CI gate. No files were staged.

### Risk Disposition

The legacy aggregate, global PCH, inactive final target graph, deferred
PhysicsUtils/CrashHandler work, and P0 rendering work remain accepted
transition risks. The guard redefinition bypass, missing CMake 3.20 evidence,
and undisposed CI failures are not accepted submission risks.

### Next Actions

Normalize CMake command and declaration names case-insensitively, reject both
`function` and `macro` redefinitions of the final guard, and add real
configure negative fixtures for case-variant and macro forms. Then establish
the required CMake 3.20 and CI disposition evidence before another final
review.

## Follow-up Review Six

**Review target:** MS-1 sixth corrective package
**Conclusion:** 修改
**Review round:** Follow-up review six

### Scope Alignment

The package is limited to the Types CMake contract checker, its temporary
configure fixtures, and MS-1 plan/evidence updates. It makes no production
C++, final target graph, GPU/render, GI, UI, or gameplay changes. The
user-owned design document remains excluded.

### Findings

1. **Blocker** `CMakeLists.txt:1`,
   `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:146-150`, and
   `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:73-74`: the project
   still declares CMake 3.20 support, but the new deferred final-guard fixtures
   have only run under CMake 4.2.3.
2. **Blocker** `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:161-178`
   and `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md:86-115`: GI
   stability, SkillUI, and the prior Heavenly Sword instability have no
   same-configuration clean baseline or formal disposition. CI cannot support
   submission.
3. **Medium** `scripts/check_core_candidate_contract.py:399-423`,
   `tests/python/CoreCandidateContractCheckerTest.py:322-356,393-436`, and
   `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:134-143`:
   normalization closes the reported macro/case bypass, but focused fixtures do
   not yet cover bracket/variable final-guard declarations or mixed-case
   reserved-builtin declarations. The sanctioned deferred-call parser accepts
   bracket literals while the plan text broadly says bracket forms are rejected;
   the policy wording needs to distinguish static bracket literals from dynamic
   expressions.

### Verification Observed

The contract verifier passed, the focused suite passed 19 tests, Python
compilation and `build.bat check` passed, and `git diff --check` had no
whitespace errors apart from existing line-ending warnings. CMake was 4.2.3.
The review did not rerun the known-red CI gate and observed no staged files.

### Risk Disposition

The legacy aggregate, global PCH, inactive final target graph, deferred
PhysicsUtils/CrashHandler work, and P0 rendering work remain accepted
transition risks. The CMake 3.20 evidence gap and undisposed CI signals are
not accepted submission risks.

### Next Actions

Obtain actual CMake 3.20 fixture evidence or approve a minimum-version change,
and obtain a same-configuration baseline or formal disposition for each CI
signal. Resolve the non-blocking fixture/policy wording gap before the final
review where practical; MS-1 must not be staged or committed until the two
blockers are closed.

## Final Acceptance Review

**Review target:** MS-1 minimal Types and Core candidate contract
**Conclusion:** 提交
**Review round:** Final acceptance review

### Inputs

- Design: `docs/designs/modular-split-exe-lib-dll-design.md` (protected user change; excluded)
- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Evidence: `docs/reports/modular-split-exe-lib-dll/ms-1/evidence.md`
- Standard: `docs/workflows/review.md`
- Verification: Core-candidate checker, 19 focused fixtures, Python compilation, `build.bat check`, and `git diff --check`

### Scope Alignment

The accepted package adds only the empty `NoMoreDayTypes` boundary, its final
CMake guard, the Core candidate manifest verifier and fixtures, and supporting
documentation. It does not move production code, enable the final target graph,
or change GPU, render, GI, UI, or gameplay behavior.

### Findings

No Blocker, High, Medium, or Low findings remain in the MS-1 compilation-boundary
scope.

### Verification Observed

`NoMoreDayTypes` is an empty `INTERFACE` target with no sources, interface
sources, link dependencies, or PCH, and only the canonical `src` public include
root. The contract checker, 19 focused configure fixtures, Python compilation,
and `build.bat check` pass. The latest aggregate build failure is accurately
recorded as the unchanged, out-of-scope `BloodSea.cpp` skill-module error; this
package does not claim that build passed.

### Accepted Risks

The project owner approved CMake 4.2, verified locally with 4.2.3. GI stability,
the stale SkillUI source guard, and the historical Heavenly Sword instability are
accepted only as MS-1 compile-boundary scope-external signals: they remain
unfixed, are not a clean baseline or global CI waiver, and later milestones must
independently disposition them. The unchanged `BloodSea.cpp` aggregate-build
failure is likewise accepted only for this MS-1 boundary acceptance. Legacy
aggregate Core/PCH, deferred PhysicsUtils/CrashHandler work, and P0 rendering
work remain planned transition risks.

### Next Action

Stage and commit only the accepted MS-1 package. Continue with MS-2 ownership
work without treating any MS-1 waiver as precedent for later milestones.
