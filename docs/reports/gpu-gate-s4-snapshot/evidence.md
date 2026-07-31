# S4（M0-C R5.2）五秒 GPUResourceRegistry 快照 Evidence

> 关联计划：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S4
> 关联轨道：`conductor/tracks/gpu_hardware_validation_gate_20260726/`（plan.md R5.2 / validation.md §10）
> 实施日期：2026-08-01

## Changes

- `src/engine/render/resources/GPUResourceRegistry.hpp/.cpp`：新增 `GPUResourceSnapshot` 结构与 `TakeSnapshot()` / `GetFrameIndex()`。快照字段覆盖 S4 合同：**资源对象数**（`activeResourceCount`）、**字节数**（`currentTotalBytes`/`peakTotalBytes`）、**生命周期计数**（`totalCreatedCount`/`totalDestroyedCount`，作为 registry 可审计的 timer/resource 生命周期计数器）、**引用状态**（`liveReferenceCount`/`pendingReferenceCount`）、**时间戳**（registry `frameIndex` + 单调墙钟毫秒 `wallClockMs`，epoch 自首次快照/`Reset()` 起算）。`Reset()` 重置快照 epoch。
- `src/engine/render/validation/GPUHardwareValidationGate.cpp/.hpp`：
  - `StressTestReport` 新增 `std::vector<StressResourceSnapshot> resourceSnapshots`；`StressResourceSnapshot` 承载快照字段 + `pendingQueryOverageCount` + 窗口均值净差（`bytesNetGrowth`/`countNetGrowth`）+ 违规标志。
  - 压力循环重写：**临时 stress target 在基线前分配**；前 5s 为 baseline（滑动窗口均值学习合法 churn）；之后每 5s 在 frame 边界（render 完成 + `AdvanceFrame()` 后）采样快照，以**滑动窗口均值与基线均值的净差**（字节 > 2 MiB 或对象数 > 8）判定净增长，替换原逐帧 `currentTotalBytes > prevWindowBytes + 2 MiB` 单调比较。
  - **quiescence 采样点**：快照处 `GPUTimerQueryRing::PollReadyQueries()` 后统计压力窗口内曾产出 Valid 结果的 pass 是否在 `3 × kRingDepth = 9` 帧内未刷新（Pending 超龄，fail-closed）。压力开始前 `Shutdown()`+`Initialize()` 重置 ring，避免 matrix 遗留结果污染。
  - 最终泄漏计数改为 **baseline-diff**：压力基线后新建且 gate 结束时仍存活（未释放）的资源才计为 leak candidate，排除长期存活的 pass 持久目标。
  - `GateReport::ToJsonString()` 输出 `stress_test.resource_snapshots` 快照数组。
- `tests/unit/GPUResourceRegistrySnapshotTest.cpp`（新增）：3 个 TEST_CASE 覆盖快照字段、pending reference 窗口（9 帧）、epoch 时间戳单调/重置。
- `tests/integration/GPUHardwareValidationGateTest.cpp`：RunGate 测试新增 `resource_snapshots` schema 断言（数组 + 每项 14 个字段）；GL diagnostics schema 测试新增空数组契约断言。
- 计划/证据文档：主计划 S4 状态 `[x]` + 实施记录；本 evidence 文档。

## 快照 Schema（JSON）

```json
{
  "frame_index": 140022,
  "timestamp_ms": 4999,
  "active_resource_count": 4,
  "current_total_bytes": 18592000,
  "peak_total_bytes": 18592000,
  "total_created_count": 22,
  "total_destroyed_count": 18,
  "live_reference_count": 4,
  "pending_reference_count": 0,
  "pending_query_overage_count": 0,
  "bytes_net_growth": 0,
  "count_net_growth": 0,
  "net_growth_violation": false,
  "pending_overage_violation": false
}
```

## Verification

- `python scripts/check_module_boundaries.py`：PASS，`71/71`。
- `cmd.exe /c build.bat check`：PASS（Check mode，退出码 0）。
- `cmd.exe /c build.bat`（两次，含终版代码）：日志 `%TEMP%\opencode\s4-build.log`、`s4-build2.log`；均含 `[Build] Build completed successfully.` 与 `[Build] All steps completed successfully`，无 `error C`/`error LNK`/`fatal error`/`FAILED`。
- `bin\NoMoreDayTests.exe --test-case="*GPU Hardware Validation Gate*"`：PASS，3 cases / 240 assertions；`GPU_HARDWARE_GATE_RESULT status=NO_GO`（matrix 项在软件 GL/WARP 环境因 ROI 黑帧、GI delta、pass 预算不足而失败，属既有环境限制，非 S4 回归）。
  - `stress_test.resource_snapshots` 输出 13 个五秒边界快照（timestamp_ms 0..54999），全部 `net_growth_violation=false`、`pending_query_overage_count=0`、`active_resource_count=4` 恒定；`stress_1min_passed=true`；`toggle_100_loops_passed=true`；`leak_candidate_count=0`。
- `bin\NoMoreDayTests.exe --test-case="*GPUResourceRegistry*"`：PASS，3 cases / 15 assertions。
- `ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure`：15 项中 3 项失败，全部为**已知既有失败**：
  - `nmd.tests.integration`：`GIStabilityIntegrationTest`（`context.giEmissiveTexture != 0u` / `giRadianceTexture != 0u`，软件 GL 环境读回，任务明示可接受）。
  - `nmd.tests.unit` / `nmd.tests.skill.unit`：`[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining gaps`（`CHECK(hasFreeze)`，任务明示可接受；独立运行 `[Unit]*` 426/426 全通过，属 ctest 环境下的既有 flaky）。
- `git diff --check`：PASS（仅 CRLF warning，退出码 0）。

## Deferred Scope and Risks

- **Pending 超龄判定口径**：registry 不持有 timer query（S1b 范围禁止改动 ring），故 `pending_query_overage_count` 在 gate 侧经 ring 只读 API（`GetPassResult`）统计"压力窗口内曾 Valid 后超 9 帧未刷新"的 pass。从未产出 Valid 结果的 pass（如 GI-off 时未执行的 GI pass）不计入，避免误报；若 timer query 整体失效，仍会由 matrix 的 `validSampleCount>=120` 判定暴露。此为已记录的口径限制。
- **wallClockMs 语义**：registry epoch 时间戳从首次快照起算（首个快照为 0），随 `Reset()` 重置；gate 侧 JSON 的 `timestamp_ms` 直接取该值，压力循环的绝对耗时见 `duration_seconds`。
- 终版快照数量为 13（60s 内 5s 边界 + 终态去重），而非固定 12；具体数量取决于渲染帧率与边界对齐。
- 未 stage、未 commit；未触碰 RenderGraph/GPUTimerQueryRing/RenderProfiler/RenderSystem 的 S1b 逻辑，未改 CMake/build.bat/PCH。
