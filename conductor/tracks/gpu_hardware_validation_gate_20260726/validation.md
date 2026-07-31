# GPU Hardware Validation Gate 生产验证报告

> **Track ID**: `gpu_hardware_validation_gate_20260726`  
> **验证日期**: 2026-07-26  
> **修订版本**: HEAD  
> **审查整改依据**: [2026-07-26 生产整改 Track 集成审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md)

---

## 1. 历史验证记录

以下记录仅说明曾执行的实现尝试，不能作为 production `GO` 证据。集成审查确认其并未驱动真实 Gameplay fixture，GI/SDF readback 与 GL diagnostics 不可信，timer 所有权不一致，ROI origin 与五秒稳定性合同也未满足。

- **R1: SDF 距离场离散采样读回 (SDF Sign Readback)**
  - 实装了对 SDF / 遮挡纹理的离散像素采样读回，验证遮挡内部与外部符号的离散读回判定。
- **R2: GI 间接光照差分读回 (GI Indirect Contribution)**
  - 实装了 GI-On 与 GI-Off 离屏渲染的亮度贡献比较。
- **R3: 1 分钟连续长稳与 5 秒滑窗检测 (1-Min Continuous Stress)**
  - 实装了 60 秒连续 Gameplay 离屏渲染循环与 5 秒滑窗 `GPUResourceRegistry` 内存增长监测（要求无单调净增长）。
- **R4: Pass Timing 条件严苛化 (Pass Timing AND Condition)**
  - 将 Pass 耗时判定条件从 `||` 改为严苛的 `&&`，要求 Valid 样本数 `>= 120` 且 `p95Ms <= budgetMs`。
- **R5: SPH Fluid 隔离机制 (Fluid Forced Off in Shipped Tiers)**
  - 强制确保 Shipped Tiers (`High` / `Ultra`) 中 `fluidEnabled == false`，恪守 SPH NO-GO 规则。
- **R6 & R7: 硬件能力与 GL 格式检测 (Capability & Diagnostic Checks)**
  - 补充了 OpenGL 版本与格式支持检测。

---

## 2. 当前验证结论

- **历史命令**: `./build.bat` 与 `python scripts/gpu_hardware_validation_gate.py` 的先前结果不再支持当前 revision 的 `GO`。
- **artifact**: 被忽略的 `bin/gpu_hardware_gate/` 输出不是可审计归档。
- **判定结果**: 🔴 **NO-GO**，待整改计划的真实目标硬件复验。

## 3. 2026-07-26 整改验证

- `python -m unittest tests/python/GpuHardwareValidationGateRunnerTest.py`：5 项通过，覆盖 `GO`、`NO_GO`、缺失 verdict、非零 C++ 返回码、CRLF 输出和 `NO_GO` 的非零 CLI 退出码。
- `./build.bat`：RelWithDebInfo `ALL_BUILD` 成功。
- 本地 runner 使用 `bin/NoMoreDayTests.exe` 执行时，artifact 记录 `return_code: 0`、`gate_status: "NO_GO"`、`meets_preflight: true`。这证明 runner 消费 C++ verdict，而非把 doctest 成功转换成 `GO`；runner 因 `NO_GO` 失败退出。

该结果只验证 R0 的 fail-closed 合同，不能作为 production `GO` 证据；R1-R6 和目标硬件复验仍为阻塞项。

## 4. R1.1 真实渲染器验证

- `./build.bat`：R1.1 修改后的 RelWithDebInfo `ALL_BUILD` 成功。
- `bin/NoMoreDayTests.exe --test-case="*GPU Hardware Validation Gate*"`：2 个门禁测试通过（667 个非目标测试跳过）。
- 测试进程创建实际 OpenGL context 并显式初始化 `RenderSystem`；gate 读取到 `NVIDIA Corporation`、`NVIDIA GeForce RTX 4070 SUPER/PCIe/SSE2` 与 `OpenGL 4.3.0 NVIDIA 591.86`。
- C++ verdict 为 `GPU_HARDWARE_GATE_RESULT status=NO_GO`，而 doctest 自身为 `Status: SUCCESS!`。这证明已安装的真实 HDR/GI 渲染器参与运行，且 R0 runner 合同不会把测试进程成功误判为 `GO`。

R1.1 只验证真实 GPU context、驱动身份和渲染器安装。当前门禁仍以空 `registry`/`SharedContext` 驱动，尚未替换为 R1.2 的固定 `GameplayState` fixtures；因此该 `NO_GO` 是正确且必须保留的结果。

## 5. R1.2.1 可重复地图输入

- `MapSystem` 现在以显式 generation seed 初始化并公开实际 seed；没有提供 seed 的生产调用仍在构造时生成随机 seed。
- `LevelManager::loadNewLevel`/`prepareLevel` 接受可选 map seed，并在 `LevelData` 与当前 level 状态中保留实际值，供 fixture artifact 使用。
- `bin/NoMoreDayTests.exe --test-case="[Unit] MapSystem - AirWall Tile Marking"`：1 个测试通过，18,438 项断言通过；固定 seed 的两张 cave 地图逐 tile 一致。
- R1.2.1 修改后的 `./build.bat` RelWithDebInfo 成功。

该步骤只建立可重复输入，不构成 Gameplay fixture 或硬件 `GO` 证据。

## 6. R1.2.2 Runtime Harness 与固定启动配置

- `GameplayRuntimeHarness` 以 production 初始化顺序构建 registry、资源、关卡、任务执行器、UI 与 `StateManager`，通过 `ApplyPendingChanges()` 只提交状态切换，不在未安装动态 GPU 子系统时调用 `GameplayState::OnUpdate()`。
- `GameplayStartConfig` 公开 biome、地图尺寸、map seed 和初始 camera；默认仍为 Town，因此常规启动路径不变。测试以固定 `Cave`、`96x96` 和 `0x4E4D4455` 首帧渲染验证该合同。
- `./build.bat`：R1.2.2 与启动配置修改后的 RelWithDebInfo `ALL_BUILD` 成功（进程退出码 `0`）。
- `bin/NoMoreDayTests.exe --test-case="[Integration] GPU Hardware Validation Gate - GameplayState Runtime Harness"`：1 个测试通过、2 项断言通过；依次运行默认 Town 和固定 Cave 的真实 `GameplayState::OnEnter -> OnRender` 路径。

该步骤证明静态真实 Gameplay 首帧与固定输入可运行；三套带 GI 读回、动态遮挡/VFX 和室外灯光压力的 recipe 尚未全部建立，门禁保持 `NO-GO`。

## 7. R1.2.3 固定场景输入 smoke 验证

- 固定 cave recipe 在真实 `GameplayState` 中创建静态 `ShadowCasterComponent` 和红色 `LightComponent`；固定 combat recipe 在两帧之间移动 `dynamicFlag=1` 的遮挡物，并通过 `GPUSkillEffectSystem::SubmitSkillEvent()` 提交 Fire `CastImpact` VFX；固定 SunPrairie recipe 创建 `63` 个确定性点光源。
- `bin/NoMoreDayTests.exe --test-case="[Integration] GPU Hardware Validation Gate - Fixed Gameplay Recipes"`：1 个测试通过、3 项断言通过；每个 recipe 都经由真实 `OnEnter -> OnRender` 路径执行。
- `./build.bat`：recipe smoke 修改后的 RelWithDebInfo `ALL_BUILD` 成功（进程退出码 `0`）。

此 smoke 验证只证明真实场景输入能够驱动渲染链；gate 尚未消费这些 fixture，且没有 paired GI/SDF resource readback、稳定 timer records、GL diagnostics、五秒 snapshots 或归档，因此 R1.2 和 production `GO` 仍未满足。

## 8. R5.1 ROI Origin 读回

- `GPUHardwareValidationGate::ReadbackRgba8Region()` 以显式 framebuffer 与 `x/y/width/height` 调用 `glReadPixels`，并恢复 read framebuffer、read buffer 与 pack alignment；gate 的 ROI 检查已使用该 helper，不能再静默忽略 `roiX/roiY`。
- `bin/NoMoreDayTests.exe --test-case="*ROI*"`：1 个 GPU 测试通过、10 项断言通过；在真实 FBO 的四象限分别写入 red/green/blue/yellow，再以四个不同 x/y origin 读回验证颜色。
- `./build.bat`：R5.1 修改后的 RelWithDebInfo `ALL_BUILD` 成功（进程退出码 `0`）。

R5.2 的五秒资源快照合同见第 10 节；R5 仍依赖 gate 的真实 Gameplay fixture，production `NO-GO` 不变。

## 9. R2.1 Runtime GI Override

- `QualityTierManager::SetGiEnabledOverride()` 以可恢复的 optional override 重新生成当前 tier 的 `RenderConfig`；关闭时将 cascade 数量和 GI intensity 归零，开启时恢复该 tier 的有效 cascade/intensity。
- gate 在每个 matrix 行及每次 GI/tier/resize toggle 前显式设置 override，并在有效 `RenderConfig.giEnabled` 与期望不符时 fail-closed。
- `bin/NoMoreDayTests.exe --test-case="*Runtime GI Override*"：1 个 GPU 测试通过、4 项断言通过，验证 High tier 的 on/off 及恢复合同。
- `./build.bat`：R2.1 修改后的 RelWithDebInfo `ALL_BUILD` 成功（进程退出码 `0`）。

尚未建立同 fixture 的 paired GI capture、pass trace、SDF/occupancy probes 与 ray-stop 读回；R2 保持进行中，production `NO-GO` 不变。

## 10. R5.2 五秒资源快照

- `GPUResourceRegistry` 新增 `GPUResourceSnapshot` 快照 API（`TakeSnapshot()`/`GetFrameIndex()`）：字段含资源对象数（`activeResourceCount`）、字节数（`currentTotalBytes`/`peakTotalBytes`）、生命周期计数（`totalCreatedCount`/`totalDestroyedCount`）、引用状态（`liveReferenceCount`/`pendingReferenceCount`）、时间戳（registry `frameIndex` + 单调墙钟 `wallClockMs`）。
- 60 秒压力循环：临时 stress target 在**基线前**分配；前 5s 为 baseline（滑动窗口均值学习合法 churn）；之后每 5s 在 frame 边界（render 完成 + `AdvanceFrame()` 后）采样快照，以**滑动窗口均值与基线均值的净差**（字节 > 2 MiB 或对象数 > 8）判定净增长，替换原逐帧 2 MiB 容差比较，容忍合法延迟释放。
- **quiescence 采样点**：快照处 drain `GPUTimerQueryRing` 后统计压力窗口内曾产出 Valid 结果的 pass 是否在 `3 × kRingDepth = 9` 帧内未刷新（Pending 超龄，fail-closed）；压力开始前重置 ring 防 matrix 遗留结果污染。
- 最终泄漏计数改为 baseline-diff（基线后新建且未释放的资源），排除长期存活的 pass 持久目标误报。
- `GateReport` JSON 的 `stress_test.resource_snapshots` 保留每个边界值；快照 schema 记录于 `docs/reports/gpu-gate-s4-snapshot/evidence.md`。
- `./build.bat`：S4 修改后的 RelWithDebInfo `ALL_BUILD` 成功（日志 `%TEMP%\opencode\s4-build.log`、`s4-build2.log`），双成功标记齐全。
- `bin/NoMoreDayTests.exe --test-case="*GPU Hardware Validation Gate*"`：3 cases / 240 assertions 通过；`stress_test.resource_snapshots` 输出 13 个五秒边界快照（timestamp_ms 0..54999），全部 `net_growth_violation=false`、`pending_query_overage_count=0`、`active_resource_count=4` 恒定；`stress_1min_passed=true`、`toggle_100_loops_passed=true`、`leak_candidate_count=0`。C++ verdict 仍为 `NO_GO`（matrix 项在软件 GL/WARP 环境因 ROI 黑帧、GI delta、pass 预算不足失败，属既有环境限制）。
- `bin/NoMoreDayTests.exe --test-case="*GPUResourceRegistry*"`：3 cases / 15 assertions 通过（快照字段、pending 窗口、epoch 时间戳）。
- `ctest --test-dir build -C RelWithDebInfo -L "unit|integration"`：除既有 `GIStabilityIntegrationTest` 与 `HeavenlySwordClosureTests` 外全部通过。

该验证确认快照语义、净增长合同与 artifact 字段；R1.2、R2.2、R4/R6 未完成，production `NO-GO` 不变。

## 11. R3 OpenGL Diagnostics

- gate 在 preflight 之后、所有 fixture/stress/toggle 工作之前安装同步 `glDebugMessageCallback`，并在退出时恢复此前的 debug-output enable state；无法安装 callback 时立即返回 `NOT_RUN`。
- 回调累计全部消息与 `GL_DEBUG_SEVERITY_HIGH` 消息，分别写入 `gl_diagnostics.debug_message_count` 和 `severe_error_count`；任意高严重度消息都会使最终 verdict 为 `NO_GO`。
- `./build.bat`：R3 修改后的 RelWithDebInfo `ALL_BUILD` 成功，且未发现编译错误。
- `bin/NoMoreDayTests.exe --test-case="*RunGate Offscreen Matrix*"`：1 个测试通过、15 项断言通过；验证报告包含 GL diagnostics 与五秒快照字段。C++ verdict 为 `NO_GO`，符合其余 MUST PASS 项尚未满足的状态。

R3 只覆盖诊断采集和 fail-closed 判定；它不替代真实 Gameplay fixture、paired GI/SDF probe、stable timer 或 artifact/CI 闭环，production `NO-GO` 不变。

## 12. R1.2.3 Gate 驱动真实 Gameplay fixture

- `GameplayState::OnRender` 在其内部 scene target 结束后恢复调用者原有 framebuffer/viewport。因此正常窗口路径仍恢复到 framebuffer 0，而测试可捕获完整 `GameplayState -> RenderSystem -> Composite -> UI` 输出。
- `GPUHardwareValidationGate::RunGate` 现在要求 `FixtureRenderDriver`。未提供 driver 时返回 `NOT_RUN`，不再构造空 registry/`SharedContext` 或独立 synthetic graph。driver 由测试持有三个 production-order `GameplayRuntimeHarness` recipe 和对应 RGBA16F owned FBO；invalid target 会成为 fixture failure。
- `bin\\NoMoreDayTests.exe --test-case="*RunGate Offscreen Matrix*"`：通过，1 case、59 assertions，C++ 输出 `GPU_HARDWARE_GATE_RESULT status=NO_GO`。结果来自 3 fixture x 3 tier/GI mode 的真实 composite callback；回归断言要求 matrix 有 9 项且没有 `GameplayState fixture driver did not provide a valid composite target`。

本项消除 gate 的空 ECS/synthetic render graph 路径，但不会把未捕获 compiled trace、SDF/occupancy probes、paired GI differential、stable timer records 和 artifact/CI 缺口伪装为通过；因此 production 仍为 NO-GO。

## 13. R4 Stable Pass Identity 与 Valid Timer History

- `RenderGraph::CompiledRenderPlan` 现在保存与 `passOrder` 对齐的 name-derived stable pass IDs；同名或 hash collision 会产生 validation error，计划保持 invalid。`RenderGraph::Execute` 使用该 stable ID，不再把 insertion index 当作 timer identity。
- `GPUTimerQueryRing` 保留每个 pass 的 Valid `GPUTimerResult`（含 `frameIndex`），提供 `GetValidPassResultsSince()`；gate 以实际 compiled plan 的 pass name/ID 映射收集不同 frame 的结果，并额外 drain ring，禁止重复读取 retained latest。
- `tests/unit/RenderGraphValidationTest.cpp` 的 compiled-plan contract 验证 pass ID 对齐与历史查询接口；`*RenderGraph*`：22 cases、162 assertions 全部通过。
- `./build.bat`：RelWithDebInfo `ALL_BUILD` 成功，日志 `%TEMP%\\NoMoreDay_r4_active_pass_timer_build.log` 无 compiler/fatal/FAILED。
- 真实 RTX 4070 `bin/NoMoreDayTests.exe --test-case=*RunGate Offscreen Matrix*`：C++ verdict `GPU_HARDWARE_GATE_RESULT status=NO_GO`；doctest `124/129` assertions，5 项失败。stable-ID/history 已消除此前重复 latest/缺失有效样本的假采样问题，但 Cave paired GI delta 仅 `0.000193621`，Ultra 的 `PostProcessPass` 仍未取得 120 个 Valid 样本，故 NO-GO 正确。
- 同一硬件日志仍记录 `RenderSystem: composite target capture left GL error=0x502 framebuffer=3` 与未初始化 `GPUEntitySystem` 警告；两项均未豁免，待后续生产生命周期/同步整改。

本节只证明 stable identity 与 timer history 的闭环已接入；M0-B immutable transitions/typed resource governance、Cave GI differential、R6 artifact/CI 仍未完成，不能发布 `GO`。

## 14. 单一 GPU timer owner 与 GL 状态回归

- 根因：RenderGraph 的 `GPUTimerQueryRing` 与 `RenderProfiler` 同时开启 `GL_TIME_ELAPSED` query，第二个 `glBeginQuery` 产生 `GL_INVALID_OPERATION (0x502)`，导致每个 pass 的错误状态污染下一帧。
- 修复：RenderGraph 内 RenderProfiler 改为 `BeginCpuPass/EndCpuPass`，GPU query 只由 `GPUTimerQueryRing` 创建和结束；RenderProfiler 独立 benchmark 的 GPU API 保持可用。
- 构建：`./build.bat` RelWithDebInfo 成功，证据日志 `%TEMP%\\NoMoreDay_single_timer_owner_final_build.log`。
- 合同测试：`bin\\NoMoreDayTests.exe --test-case=*RenderGraph*` 通过 23 cases/173 assertions；`bin\\NoMoreDayTests.exe --test-case=*Gameplay Fixture Target Capture*` 通过 1 case/3 assertions，未出现 `0x502` 或 `GPUEntitySystem::Get()`。
- 真实 RTX 4070 门禁：`bin\\NoMoreDayTests.exe --test-case=*RunGate Offscreen Matrix*`，日志 `%TEMP%\\NoMoreDay_single_timer_owner_full_gate.log`，C++ verdict `GPU_HARDWARE_GATE_RESULT status=NO_GO`；doctest 114/119 assertions，5 项失败。timer valid sample shortage 已消失；剩余失败为 Cave paired GI delta `0.000193621 < 0.001` 及 Ultra 对同 fixture paired 结果的依赖。
- 该结果修复了 GL query ownership 污染，但没有满足 Cave GI differential、M0-B 全部 governance 或 R6 artifact/CI 退出标准，生产继续 NO-GO。
