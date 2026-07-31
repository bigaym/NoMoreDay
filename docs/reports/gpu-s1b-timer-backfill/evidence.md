# S1b 单一 GPU timer owner 四态数据模型与延迟回填 Evidence

> 关联计划：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S1b（依赖 S0）
> 关联轨道：`conductor/tracks/gpu_rendergraph_resource_foundation_20260726/`（S0 stable pass identity 合同）
> 实施日期：2026-08-01

## Changes

- `src/engine/render/debug/RenderProfiler.hpp/.cpp`：
  - `PassTimingStats` 扩展为四态（复用 `QueryState`：`Pending/Valid/Unavailable/CpuFallback`）+ 来源 `frameIndex`。
  - 新增 `FlushRingToProfiler()`（渲染路径 Poll 唯一调用点：`PollReadyQueries()` + 回填 + 四态推进）、`GetPassResult(uint32_t stablePassId)`（未跟踪 ID 映射失败返回默认 `Unavailable`）、真实 `IsGpuTimingAvailable()`（由 ring 能力决定，有 GL timer 时为 true）。
  - 四态推进（frame-acceptance rule）：仅接受 `frameIndex` 严格递增的 ready 结果；重复同帧源（单次回填）与旧帧/迟到结果拒绝；Pending 超龄（`kPendingOverageFrames=6` 帧）→ Unavailable；Unavailable 后续 ready 结果可恢复 Valid；无 GPU → 全 pass CpuFallback。
  - 聚合（aggregate rule）：`GetStats` 改为固定栈数组（`kWindowSize` scratch）聚合，移除原 vector 热路径堆分配；Valid 参与 GPU 均值/P95，Pending 沿用上帧值并标记来源 frame，Unavailable/CpuFallback 不参与 GPU 聚合（CpuFallback 走 CPU 采样聚合）。
  - 构造期对 16 个派生 stablePassId 做冲突检查：碰撞则记录 error 并对该 pass 禁用回填（gate-side 缺席 pass 派生 ID 别名风险）。
- `src/engine/render/debug/GPUTimerQueryRing.hpp/.cpp`：
  - 新增只读 `IsGpuTimerSupported()`（GL query 入口全部解析则 true）。
  - 测试钩子 `DebugInjectPassResult` / `DebugSetGpuTimerSupported`（`Shutdown()` 时清除，不影响生产路径）。
- `src/engine/render/RenderSystem.cpp`：`render()` 在 `graph.Execute()` 之后、`policyNow`/DRS/adaptive 策略读取**之前**调用 `FlushRingToProfiler()`（graphContext.renderProfiler 非空时）；HUD/`BuildPassTimingSummary` 读回填后统计，`PickPassCostMs` 仅 `Valid && gpuMeanMs>0` 用 GPU 否则 CPU（维持既有规则）。
- `tests/unit/RenderProfilerFourStateTest.cpp`（新增，7 用例）：Pending→Valid 接受与来源帧、延迟 ready 单次回填、旧帧拒绝、超龄 Unavailable 与恢复、无 GPU CpuFallback、映射失败 Unavailable、DRS/HUD 决策输入。
- `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp`：S1a 用例 #1 改为 S1b 语义（真实 GL 下 `IsGpuTimingAvailable()==true`；执行 pass 回填为 Valid、frameIndex>0；未执行 pass Unavailable；仍断言无 0x502 单 owner 回归；用例开始处重置 ring 单例避免跨用例污染）；用例 #2 不变并复跑通过。
- `docs/plans/2026-07-30-p0-rendering-remediation-plan.md`：S1b 状态 `[x]` + 实施记录。
- 本 evidence 文档。

## Verification

- `python scripts/check_module_boundaries.py`：PASS，`71/71`，files 20。
- `cmd.exe /c build.bat check`：PASS（Check mode，退出码 0，全部门禁检查 OK）。
- `cmd.exe /c build.bat > %TEMP%\opencode\s1b-build.log 2>&1`（两次，含终版代码）：退出码 0；日志含 `[Build] Build completed successfully.`（1 处）与 `[Build] All steps completed successfully`（1 处），`error C`/`error LNK`/`fatal error`/`FAILED` 0 处。
- 新增 focused 测试真实运行：
  - `bin\NoMoreDayTests.exe --test-case="*RenderProfiler four-state*"`：7 cases / 43 assertions 全 PASS。
  - `bin\NoMoreDayTests.exe --test-case="*S1b - RenderProfiler single timer owner*"`（真实 GL 集成）：1 case / 51 assertions 全 PASS。
  - `bin\NoMoreDayTests.exe --test-case="*Gate loop single ring frame owner*"`（既有 S1a 集成）：1 case / 6 assertions 全 PASS。
  - `bin\NoMoreDayTests.exe --test-case="[Unit] RenderGraph - S0*"`（S0 回归）：7 cases / 40 assertions 全 PASS。
  - `bin\NoMoreDayTests.exe --test-case="*Debug - Scenario F Profiler HUD Overhead*"`（热路径约束）：Mean=0.000ms < 0.15ms 目标，2 assertions PASS。
  - `bin\NoMoreDayTests.exe --test-case="*Heavenly Sword element nodes*"`：1 case / 4 assertions PASS（独立运行确认 ctest 下 flaky 属既有环境问题）。
- `ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure`：15 项中 3 项失败，全部为**已知既有失败**（与 S1a/S4 evidence 一致，非 S1b 回归）：
  - `nmd.tests.integration`：`GIStabilityIntegrationTest`（`context.giEmissiveTexture != 0u` / `giRadianceTexture != 0u`，软件 GL 环境 GI 纹理读回为 0）。
  - `nmd.tests.unit`（438 cases / 1 failed）与 `nmd.tests.ai.unit`：同一 `[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining gaps`（`CHECK(hasFreeze)`，ctest 环境既有 flaky；独立运行通过）。
- `git diff --check`：PASS（仅 CRLF warning，退出码 0）。

## Deferred Scope and Risks

- **Pending 判定口径**：四态中 Pending 的"沿用上帧值"由 `lastAcceptedFrameIndex` 与 ring 当前 frame 计数驱动（每帧 flush 时若 ring 未返回更新 ready 结果且帧号已推进，则该 pass 进入 Pending-carry）；ring 本身在无新 ready 结果时继续返回旧 Valid 结果，故 Pending 的进入依赖帧号推进而非 ring 内部 in-flight 槽位状态。实测与契约表一致。
- **超龄阈值**：`kPendingOverageFrames = 6`（2×ring 深度）。慢驱动（query ready 延迟 >6 帧）会暂时落入 Unavailable 并随 ready 到达恢复，属 fail-closed 行为。
- **GetStats 热路径**：已移除 vector 分配（固定栈数组），满足计划 §risk-2；Scenario F benchmark Mean=0.000ms。
- **gate 侧读取不变**：`GPUHardwareValidationGate.cpp`（S6 域）未触碰；其 `GetPassResult(stableId).state==Valid` 语义保持（ring `GetPassResult` 未改动）。
- **别名冲突路径**：16 个派生 stablePassId 的碰撞需构造期记录 + 禁用回填，正常情况不可达（32 位 FNV 低 32 位），仅作防御性日志。
- 未 stage、未 commit；未触碰 `GPUHardwareValidationGate.cpp/.hpp`、`GPUHardwareValidationGateTest.cpp`、FixtureRenderDriver/GameplayRuntimeHarness（S6 未提交改动保留）；未改 CMake/build.bat/PCH；未读/改受保护的 `docs/designs/modular-split-exe-lib-dll-design.md`。等待独立审查。
