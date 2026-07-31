# P0 渲染轨道整改实施方案（修订版）

> 状态：已通过终审（2026-07-31，`提交`）；v5 起仅修阻塞项，M1-M5/L1-L5 为已记录延后项
> 审计基线：HEAD `beace9f`（2026-07-31）
> 日期：2026-07-30
> 关联轨道：`gpu_rendergraph_resource_foundation_20260726`（M0-B）、`gpu_hardware_validation_gate_20260726`（M0-C）、`gpu_production_hdr_gi_closure_20260726`（M0-A）
> 关联主控规格：`conductor/specs/rendering_engine_v5_master_spec.md`

## 1. 背景：P0 阻塞真相

P0 渲染轨道（M0-A/B/C）此前被判定为"In Progress / production NO-GO"，表面原因是硬件 gate 未通过（`GPU_HARDWARE_GATE_RESULT status=NO_GO`，Cave paired GI delta `0.000193621 < 0.001`）。本次代码审计发现**第二层问题：轨道 plan.md / validation.md 的部分描述与 main 代码不一致——一部分整改已落地，另一部分声称的整改从未实现**。本方案以"能力矩阵"取代总括结论，逐项定位。

### 1.1 能力矩阵（2026-07-30 审计，基于 HEAD `beace9f`）

关键 M0 相关提交（省略中间 M1/M2 提交，如 `49eb2fa`、`2c51a37`）：`f0d4554`（M0-B complete 初版）→ `c90f96d`（M0-C complete 初版）→ **`f74d8a4`（2026-07-28，最后进入 main 的渲染提交，54 文件 / +1951 行，含大量 C++：RenderGraph.cpp +122、JFAPass +223、RadianceCascades +78、新增 VFXEmissionSnapshotPass、GPUParticleSystem +56、RenderResourceDescriptor +33、RenderSystem.cpp +7、gate +9、RenderGraphValidationTest +72、JFAPassProductionCorrectnessTest +47 等）**。`f74d8a4` 之后的提交仅 `ff2cd79`（移除无用 VFX 测试状态）与模块拆分系列（b061845..beace9f），无渲染整改。

| 能力 | 文档声称 | HEAD 实际（证据） |
|---|---|---|
| typed Read/Write access（Tag/Owner/Stage/Usage） | M0-B R1 已实现 | ✅ `RenderGraphBuilder::Read/Write(const TypedPassAccess&)` 存在 |
| compiled transitions + `stableResourceId` | M0-B R1 已实现 | ✅ `m_compiledPlan.transitions`、`transition.stableResourceId`（RenderGraph.hpp:197/243/256）、Execute 按 consumerPassIndex 应用 `MemoryBarrier` |
| 多写/cycle/missing producer 合同校验 | M0-B R1 | ✅ `ValidateBuildContracts`、NDEBUG 外 throw |
| `BeginCpuPass`/`EndCpuPass`（单一 GPU timer owner） | M0-B plan 2026-07-28 节 | ❌ 不存在（`git grep` src/tests 无匹配） |
| `BeginPassExecution`/`EndPassExecution`（pass 边界 flush 契约） | M0-B plan R4 | ❌ 不存在 |
| 生产路径单一 GL timer owner（消除 0x502） | M0-B validation | ❌ **`RenderProfiler::BeginPass` 仍调 `m_gpuApi.beginQuery(kGLTimeElapsed,...)`（debug/RenderProfiler.cpp:86-113），RenderGraph Execute 同时调 `GPUTimerQueryRing::BeginPass` 与 `context.renderProfiler->BeginPass` → 双 owner 仍在 HEAD** |
| `FixtureRenderDriver` / `GameplayRuntimeHarness` | M0-C plan R1.2 | ❌ 不存在（gate 用空 registry + 空 SharedContext） |
| `SetGiEnabledOverride` | M0-C plan R2 | ❌ 不存在 |
| paired GI on/off delta 判定 | M0-C validation R2.2 | ❌ `giIndirectPassed = giOn ? meanLuma>=0.01f : true`，非差分 |
| GL debug callback 安装 | M0-C plan R3 `[x]` | ⚠️ 仅能力探测（`DeviceCapabilityMatrix.cpp:30` `glfwGetProcAddress("glDebugMessageCallback")`），无 callback 安装；memory 审查记录亦记为"接受的低风险" |
| stable pass ID | M0-C validation R4 | ❌ `CompiledRenderPlan` 无 `stablePassId`（有 `stableResourceId`，不可混同） |
| 五秒 registry 快照 | M0-C validation R5 | ❌ 压力循环为逐帧 2MiB 容差比较，无快照序列 |

### 1.2 HEAD 关键路径现状（已核验）

- `GPUHardwareValidationGate.cpp`（632 行）：空 `entt::registry` + 空 `SharedContext` + 合成 testGraph 驱动 `::RenderSystem::render`；GI 判定 `meanLuma>=0.01f`，非 paired delta；**gate:290/294 在 render 外层调用 `GPUTimerQueryRing::BeginFrame/EndFrame`，而 `RenderGraph.cpp:449/488` 已负责同一生命周期（ring 深度 3）→ 双重 BeginFrame 会额外推进并覆盖 slot**。
- `RenderSystem.cpp`：全局 `g_renderProfiler`（:197），:461 `BuildPassTimingSummary(g_renderProfiler->GetAllStats())`（GPU 均值/P95 输出），:1591/1787 初始化/重置，:2235 注入 `graphContext.renderProfiler`，:2268/2276 框架层 BeginFrame/EndFrame。
- `RenderProfiler` 位于 `src/engine/render/debug/`（非 `render/` 根目录）；`GPUTimerQueryRing` 同目录。

## 2. 目标与非目标

### 目标

1. 消除生产路径 0x502（GL_TIME_ELAPSED 双 owner）——当前最严重、最小可独立修复的缺陷，**作为独立生产缺陷修复，不改变 M0-A/B/C 的批准顺序**。
2. 使 M0-B/M0-C 轨道文档与 main 代码如实一致（按能力矩阵逐项同步，不抹除已落地实现）。
3. 按轨道设计补齐 M0-C 硬件 gate 缺失能力（GL debug callback、五秒快照、真实 Gameplay fixture、paired GI delta）。
4. 为最终硬件 GO（RTX 4070 实机）建立可复现的 gate 流程。

### 非目标

- 不重写 RenderGraph 架构（typed access / compiled transitions / stableResourceId 已存在且有效）。
- 不并行重构模块拆分（MS-6/7）范围内的渲染所有权；本方案与模块拆分无代码重叠，模块边界检查须保持 PASS。
- 不改变产品可见渲染行为，除非该行为本身是缺陷（如 0x502、双重 frame 推进）。
- 不在无 RTX 4070 实机时宣称硬件 GO。
- **不改变主控规格的 M0-A→M0-B→M0-C 排序**；S1 独立于该排序（生产缺陷修复），如后续需重排，须先更新主控规格并获得批准。

## 3. 约束

- 每步修改必须通过 `./build.bat`（重定向日志，检查 `[Build] Build completed successfully.` 与 `[Build] All steps completed successfully` 标记），不得以空输出判断成功。
- 每步修改必须通过模块边界检查：`python scripts/check_module_boundaries.py`（当前 71/71）。
- 每步修改同步更新所属轨道 `plan.md`/`validation.md`：`[~]`=计划中、`[x]`=已实现并验证；只陈述该步已验证的代码状态。
- 不修改 `docs/designs/modular-split-exe-lib-dll-design.md`（用户持有）。
- 渲染改动聚焦 `src/engine/render/` 与 `src/engine/resource/`；不触碰 `src/game/` 归属迁移。
- 提交须逐批获用户明确授权后执行。

## 4. 实施步骤

任务状态约定：`[ ]` 未开始、`[~]` 进行中、`[x]` 已完成并验证。每步遵循：审计调用点 → 最小改动 → 构建验证 → focused 测试 → 更新轨道文档 → 审查后提交。

### S0：stablePassId（M0-C R4 前置，稳定 pass identity）

- **状态**：`[ ]`
- **内容**：在 `CompiledRenderPlan` 定义确定性稳定 pass ID：
  - **identity source（与图拓扑顺序无关）**：canonical pass name（规范化规则：去除空白/统一大小写/唯一化重复名）+ 版本化 hash（FNV-1a 64，hash 算法与版本盐固定并记录于头文件注释）。
  - **fail-closed 合同**：ID 冲突（hash 碰撞/重复声明）、与保留 ID 冲突（如 `GPUTimerQueryRing::kFramePassId`）、溢出（保留 ID 空间/frameIndex 上界超限）→ 在 `RenderGraph::Build` 阶段运行时断言失败，禁止执行（非编译期）。
  - **条件 pass 规则**：分支图中未执行的 pass 保留其 ID 但无样本；ID 不随执行与否变化。
  - **重命名/迁移规则**：pass 重命名即新 ID，须同步更新测试与历史基线；禁止隐式沿用。
  - ring 历史按 `stablePassId + frameIndex` 保存；`GPUTimerQueryRing::BeginPass(stablePassId)`。
- **涉及文件**：`src/engine/render/graph/RenderGraph.hpp/.cpp`、`src/engine/render/debug/GPUTimerQueryRing.hpp/.cpp`、`RenderGraphValidationTest.cpp`。
- **验收**：确定性断言测试覆盖：pass 重排 ID 不变、条件 pass ID 保留、hash 冲突与保留 ID 冲突编译失败、溢出失败、硬件 trace 可用。
- **验证命令**：`./build.bat`（重定向日志）；`ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure`；RenderGraph 契约 focused tests。
- **风险**：中。

### S1：单一 GPU timer owner（生产缺陷修复，消除 0x502）

**拆分声明**：S1 拆为 **S1a（可先行，独立于 S0）** 与 **S1b（依赖 S0 的 stable ID 集成与四态回填）**。S1a 属生产缺陷修复，优先级高于 M0-A→M0-B→M0-C 顺序，**须经用户授权为主控规格例外（§8 决策 4）后方可实施**；S1b 按正常顺序随 S0 之后执行。

#### S1a：单一 frame owner + RenderProfiler 去 GL query（先行）

- **状态**：`[x]`（2026-07-31 实施完成，独立审查 `提交`）
- **内容**：
  1. 移除 `GPUHardwareValidationGate.cpp:290/294` 外层 `BeginFrame/EndFrame`（由 `RenderGraph.cpp:449/488` 唯一负责）。
  2. `RenderProfiler` 新增 CPU 计时路径 `BeginCpuPass(const char* name)` / `EndCpuPass()`（仅 CPU 侧记录，不调 `beginQuery`）；`RenderGraph::Execute` 内部由 `BeginPass` 改为 `BeginCpuPass` 调用。**过渡态**：S1a 期间 `GetAllStats()` 的 GPU 均值/P95 标记 `Unavailable`（无回填），HUD/summary 显示 CPU 计时，避免展示伪 GPU 数据。
- **涉及文件**：`src/engine/render/debug/RenderProfiler.hpp/.cpp`、`src/engine/render/graph/RenderGraph.cpp`、`src/engine/render/validation/GPUHardwareValidationGate.cpp`、相关测试。
- **验收**：RenderGraph Execute 内不再调用 RenderProfiler 的 GL query；每 pass 仅 `GPUTimerQueryRing` 一个 `GL_TIME_ELAPSED` owner；生产路径无 0x502；gate 压力循环无 slot 覆盖。
- **验证命令**：`./build.bat`；`python scripts/check_module_boundaries.py`（71/71）；`ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure`；新增真实 GL 上下文回归测试（单 query owner、无 0x502、slot 不覆盖）。
- **风险**：中（GPU telemetry 中间态为 Unavailable，须在 HUD 正确展示，S1b 恢复）。

#### S1b：四态数据模型与延迟回填（依赖 S0）

- **状态**：`[x]`（2026-08-01 实施完成并验证，待独立审查 `提交`）
- **内容**：
  1. **四态模型**：`PassTimingStats` 扩展为 `{ Pending, Valid, Unavailable, CpuFallback }` + 来源 `frameIndex`。状态转换表：
     | 状态 | 进入条件 | 退出条件 |
     |---|---|---|
     | Pending | query 未 ready | 结果 ready → Valid；超龄 → Unavailable |
     | Valid | ready 结果到达且 frame 接受 | 新帧 ready 结果覆盖（同 stablePassId） |
     | Unavailable | 映射失败/无采样/超龄 | 后续 ready 结果到达 |
     | CpuFallback | 无 GPU 或显式降级 | 恢复 GPU 且 ready |
  2. **frame acceptance rule**：仅接受 `frameIndex` 严格递增的 ready 结果；旧帧/迟到结果拒绝（防 late 覆盖新帧）；`GPUTimerResult → PassTimingStats` 转换在回填点单次执行（单次回填：每个源结果仅回填一次）。
  3. **aggregate 规则**：Valid 参与均值/P95；Pending 沿用上帧值并在元数据标记来源 frame；Unavailable/CpuFallback 不参与 GPU 聚合（CpuFallback 走 CPU 采样聚合）。
  4. **RenderSystem 消费时序**：`RenderSystem::render` 末尾（graph 执行后）调用 `FlushRingToProfiler()`（Poll 唯一调用点），随后 `UpdateStats()`；HUD/`BuildPassTimingSummary`/DRS 均读取回填后的统计；DRS 决策仅使用 Valid（无 Valid 时沿用上帧且标记）。
  5. **API 草图**：
     ```
     void RenderProfiler::BeginCpuPass(const char* name);   // CPU 侧
     void RenderProfiler::EndCpuPass();
     void RenderProfiler::FlushRingToProfiler();            // 由 RenderSystem::render 末尾调用
     void RenderProfiler::UpdateStats();                    // 四态聚合
     const PassTimingStats& RenderProfiler::GetPassResult(stablePassId) const;
     ```
- **涉及文件**：`RenderProfiler.hpp/.cpp`、`GPUTimerQueryRing.hpp/.cpp`、`RenderGraph.hpp/.cpp`、`RenderSystem.cpp`、HUD/benchmark 消费者、相关测试。
- **验收**：delayed-ready 单次回填；旧帧拒绝；无 GPU → CpuFallback；映射失败 → Unavailable；HUD/summary/DRS 四态正确决策；每状态独立测试。
- **验证命令**：`./build.bat`；`ctest -L "unit|integration"`；新增四态与时序测试（delayed-ready、无 GPU、映射失败/重复、旧帧拒绝、HUD、DRS）。
- **风险**：高。

- **实施记录（2026-08-01）**：
  - `PassTimingStats` 新增 `frameIndex`；`RenderProfiler` 新增 `FlushRingToProfiler()`（Poll 唯一调用点）、`GetPassResult(stablePassId)`（映射失败返回 Unavailable）、真实 `IsGpuTimingAvailable()`（由 ring 能力决定）。
  - `FlushRingToProfiler` 每 pass 仅接受 frameIndex 严格递增的 ready 结果（旧帧/重复源拒绝，单次回填）；Pending 超龄（6 帧）→ Unavailable；Unavailable 后续 ready 可恢复；无 GPU → 全 pass CpuFallback。
  - `GetStats` 改为固定栈数组聚合（无热路径堆分配）：Valid 参与 GPU 均值/P95，Pending 沿用上帧值并标记来源 frame，Unavailable/CpuFallback 不参与 GPU 聚合。
  - `RenderSystem::render` 在 graph 执行后、DRS/adaptive 策略读取前调用 `FlushRingToProfiler()`。
  - `GPUTimerQueryRing` 新增只读 `IsGpuTimerSupported()` 与测试钩子 `DebugInjectPassResult`/`DebugSetGpuTimerSupported`（Shutdown 时清除）。
  - 测试：新增 `tests/unit/RenderProfilerFourStateTest.cpp`（7 用例：四态转换/延迟 ready 单次回填/旧帧拒绝/超龄与恢复/无 GPU/映射失败/DRS-HUD 决策输入）；更新 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp`（S1a 断言改为 S1b 四态回填语义）。
  - 验证：边界 71/71；build.bat 双成功标记；新增 focused 测试全绿；ctest 仅剩既有 GIStability 与 HeavenlySword flaky 失败；`git diff --check` exit 0。详见 `docs/reports/gpu-s1b-timer-backfill/evidence.md`。

### S2：文档-代码脱节修正

- **状态**：`[ ]`（依赖 S1/S3/S4 部分落地后方可最终收口，但可先行启动）
- **内容**：按 §1.1 能力矩阵逐项同步：已落地项保持 `[x]` 并补提交/证据；未实现项改 `[~]` 并注明"代码未在 main，待 S1/S3-S8 落地"；validation.md 中 0x502/paired delta/五秒快照/stable ID 相关记录改写为"计划中"，不保留超前结论。
- **涉及文件**：`conductor/tracks/gpu_rendergraph_resource_foundation_20260726/{plan.md,validation.md,index.md,metadata.json}`、`conductor/tracks/gpu_hardware_validation_gate_20260726/{plan.md,validation.md,index.md,metadata.json}`、`conductor/rendering_system_progress.md`、`conductor/tracks.md`（如需）。
- **验收**：能力矩阵每条与 HEAD 源码/测试/提交交叉验证一致；进度文档不再超前。
- **验证**：`git grep <符号> -- src tests`（带 pathspec，避免命中文档）抽查。
- **风险**：**高**（文档混杂"已实施/未实施"条目，错误同步会污染轨道状态；须逐条源码+测试+提交证据审计）。

### S3：M0-C R3 GL debug callback 安装（fail-closed）

- **状态**：`[ ]`（无依赖）
- **内容**：
  1. **capability 传导**：`HardwareCapabilityReport`（Gate 侧）新增 debug callback 支持字段，从 `DeviceCapabilityMatrix` 的能力报告传导；preflight 不满足（字段为假或 `GL_DEBUG_OUTPUT` 启用失败）→ gate 结论 `NOT_RUN`（不得降级通过）。
  2. **callback 生命周期**：GL context 下安装 `glDebugMessageCallback` → 启用 `GL_DEBUG_OUTPUT` → 运行期过滤（仅 ERROR/HIGH，防刷屏）→ 运行结束恢复原始 callback/禁用；线程约束：callback 在 GL 线程调用，收集队列无锁化（单线程断言）。
  3. **诊断 schema 与状态传播**：`gl_diagnostics` 字段扩展为结构化数组（severity/type/source/message/id/time）；高严重度消息（ERROR/HIGH）→ NO-GO；能力缺失 → `NOT_RUN` 传播到最终 Gate 状态。**完整 JSON 输出**：gate 输出完整 GateReport JSON（stdout 或输出文件，含 matrix/timer/resource/GL diagnostics/快照序列），Python runner `gpu_hardware_validation_gate.py` 从该源头解析并归档（不只解析 status 行）。
- **涉及文件**：`GPUHardwareValidationGate.cpp/.hpp`、`DeviceCapabilityMatrix.cpp/.hpp`（能力传导）、`scripts/gpu_hardware_validation_gate.py`、gate 相关测试（含 callback 缺失 failure-path、schema validator 测试）。
- **验收**：gate 报告含结构化诊断；callback 安装/恢复成对；能力缺失 `NOT_RUN`；ERROR/HIGH → NO-GO；runner 归档完整报告。
- **验证命令**：`./build.bat`；`python -m unittest tests/python/GpuHardwareValidationGateRunnerTest.py`；gate focused 测试；RTX 4070 实机采集。
- **风险**：中。

### S4：M0-C R5.2 五秒 registry 快照

- **状态**：`[x]`（2026-08-01 实施完成并验证）
- **内容**：
  1. `GPUResourceRegistry` 增加快照 API（字段：资源对象数、字节数、timer 生命周期计数、引用状态、时间戳）。
  2. **基线窗口**：前 5s 为 warmup/baseline（含合法资源 churn 学习，滑动窗口平均）；之后每 5s 快照，**净增长判定**（窗口内对象数/字节数净增超过容差阈值即失败；考虑合法延迟释放，用滑动窗口均值而非瞬时单调比较）。
  3. **quiescence 采样点**：快照在 frame 边界（graph 执行完成、AdvanceFrame 后）采样；query Pending 超龄阈值（如 3×ring 深度帧数）纳入失败判定。
- **涉及文件**：`GPUResourceRegistry.hpp/.cpp`、`GPUHardwareValidationGate.cpp`、相关测试。
- **验收**：五秒快照序列输出；泄漏检测基于净增长合同；快照 schema 记录于 evidence。
- **验证命令**：`./build.bat`；gate focused 测试；实机压力循环。
- **风险**：中（churn 与泄漏区分）。

**S4 实施记录（2026-08-01）**：
- `GPUResourceRegistry` 新增 `GPUResourceSnapshot` 结构（`frameIndex` + `wallClockMs` 时间戳、`activeResourceCount` 对象数、`currentTotalBytes`/`peakTotalBytes` 字节数、`totalCreatedCount`/`totalDestroyedCount` 生命周期计数、`liveReferenceCount`/`pendingReferenceCount` 引用状态）与 `TakeSnapshot()`；`Reset()` 重置快照 epoch。
- 压力循环（`GPUHardwareValidationGate.cpp`）重写：临时 stress target 在基线前分配；前 5s 为 baseline（滑动窗口均值学习合法 churn）；之后每 5s 在 frame 边界（render + `AdvanceFrame` 后）快照，以**滑动窗口均值与基线均值的净差**（字节 > 2 MiB 或对象数 > 8）判定净增长，替换原逐帧 `> prevWindowBytes + 2MiB` 单调比较；query Pending 超龄（> 3×ring 深度 = 9 帧）纳入 fail-closed 判定。
- 最终泄漏计数改为 baseline-diff（压力窗口期间新建且未释放的资源），排除长期存活的 pass 持久目标误报。
- 快照序列输出到 `GateReport` JSON `stress_test.resource_snapshots`；schema 记录于 evidence 文档。
- 验证：`check_module_boundaries.py` 71/71；`build.bat check` 通过；`build.bat` 双成功标记；gate focused 测试通过；ctest unit|integration 除已知既有失败（GIStabilityIntegrationTest、HeavenlySwordClosureTests）外通过；`git diff --check` 干净。

### S5：M0-B R1/R2 legacy access 收敛审计

- **状态**：`[x]`（2026-07-31 实施完成并验证）
- **内容**：产出可审计 inventory（`conductor/tracks/gpu_rendergraph_resource_foundation_20260726/legacy-access-inventory.md`）：每个 string-based `Read`/`Write` 使用点的**处置结论**（收敛到 typed access / 已批准例外），例外须由 M0-B spec 批准并记录 owner、原因、到期版本、测试与 fail-closed 行为；**默认拒绝**生产 legacy access，不得以名称回退或手工 barrier 旁路。
- **涉及文件**：`RenderGraph.hpp/.cpp` 及相关 pass（按 inventory 逐点收敛）；inventory 文档。
- **验收**：inventory 全量覆盖（0 个未处置使用点）；未收敛且未批准项为失败项。
- **验证命令**：`git grep` 审计对照 inventory；`./build.bat`；RenderGraph 契约测试。
- **风险**：中。

**S5 实施记录（2026-07-31）**：
- 生产 pass 38 访问点 + 6 typed descriptor 经审计全部 typed；需收敛的 string-based 调用点 1 处（`tests/unit/RenderGraphValidationTest.cpp` S0 stable-id 测试）已收敛为 `TypedPassAccess`（同名/同 stableResourceId/同 stage/usage，barrier 语义等价）。
- 默认拒绝落地：`ResourceAccess::isStringBasedAccess` + `ValidateBuildContracts` fail-closed Error；新增 `[Unit] RenderGraph - string-based access is denied by default` 测试。
- 验证：`check_module_boundaries.py` 71/71；`build.bat check` 通过；`build.bat` 双成功标记；`*RenderGraph*` 31 cases/217 assertions；ctest unit|integration 14/15（唯一失败为既有 GI Long-run Stability Proxy 硬件读回，接受）；`git diff --check` 干净；`check_legacy_reintroduction.py` PASS。
- 例外：0 已批准、0 待批准；未处置使用点 0。

**S5 审查整改（2026-08-01，结论 `修改`，M1）**：
- **M1（fail-closed 不抵抗 NDEBUG/validation 开关）修复**：string-deny 由新增 `RenderGraph::RejectLegacyStringAccess` 在 `Build` 无条件执行（不受 NDEBUG、`s_validationEnabled` 影响），发现 string-based access 即抛 `std::logic_error` 拒绝执行，与 S0 `identityContractFailed` 无条件 throw 同构；RelWithDebInfo/NDEBUG 发布构建同样无法执行 string access（`m_isBuilt` 不会被置位，`Execute` 亦不可能运行）。
- deny 测试改为无条件断言 `Build` 抛 `std::logic_error`（不再区分 NDEBUG 分支）。
- 修正 inventory：§8 措辞改为无条件 fail-closed/拒绝执行；§5 收敛后调用点改为「生产 0，测试 2 处为 deny 负例夹具」（`RenderGraphValidationTest.cpp:120/124`）；§1 集成测试路径笔误改为平铺 `RenderGraphV3ContractsIntegrationTest.cpp`/`RenderGraphV5ContractsIntegrationTest.cpp`。
- 验证：`check_module_boundaries.py` 71/71；`build.bat check` 通过；`build.bat` 双成功标记（日志 `%TEMP%\opencode\s5fix-build.log`）；`*RenderGraph*` focused 测试通过；ctest unit|integration 除既有 GIStability/HeavenlySword 失败外通过；`git diff --check` 干净。

### S6：M0-C R1.2 真实 Gameplay fixture（硬件 GO 前提）

- **状态**：`[x]`（2026-08-01 实施完成并验证）
  - T6.1 `GameplayRuntimeHarness`：真实 ECS/SharedContext/资源构造（owner 与生命周期合同）；`[x]`
  - T6.2 `FixtureRenderDriver`：固定 fixture 驱动 `RenderSystem::render`；`[x]`
  - T6.3 三个 fixture 配方（cave_color_bleed、dynamic_combat_emissive、outdoor_light_pressure）：场景数据来源（现有测试 fixture 复用或新建关卡快照）、版本与 provenance 合同；`[x]`
  - T6.4 render target 所有权（离屏 target 生命周期与 gate 交互）；`[x]`
  - T6.5 artifact/version 合同（fixture 输入哈希、输出与证据对应）。`[x]`
- **验收**：gate 在真实 ECS/场景数据上运行；fixture 输入哈希可复现；数据版本有来源记录。
- **验证命令**：`./build.bat`；gate 测试；实机运行。
- **风险**：高。

**S6 实施记录（2026-08-01）**：
- 审计结论：`tests/` 无可直接复用的完整关卡快照；LightingStabilityTest.cpp 的 `Position+LightComponent` 批量构造模式与 GameplaySystems.cpp 的真实 emplace 模式可借鉴，但 recipe 实体需按三个 fixture 语义重新构造 → 按 §8 决策 3 新建最小真实场景配方（真实组件类型，不要求完整关卡）。
- T6.1：`GameplayRuntimeHarness`（`tests/integration/GameplayRuntimeHarness.hpp`，header-only，测试侧持有）：构造真实 `entt::registry`（PlayerTag/EnemyTag/Position/PrevPosition/Velocity/Radius/ColliderComponent/VisionComponent/ShadowCasterComponent/LightComponent/MapTileComponent/ColorComponent/VisualEffect/AttackEffect）+ 最小 `SharedContext`（registry/settings/renderAlpha 接线，其余字段 nullptr，RenderSystem 全部 nullptr 安全）+ RAII 生命周期（析构先 Destroy composite FBO 再释放 registry/context，copy 删除）。
- T6.2：`FixtureRenderDriver` 抽象接口（`src/engine/render/validation/FixtureRenderDriver.hpp`，仅依赖引擎类型，避免 engine→game 反向依赖破坏模块边界 71/71）：`PrepareFixture/Registry/Context/CompositeFramebuffer/CompositeWidth/CompositeHeight/SceneInputHash/FixtureVersion/SceneSource`。`GPUHardwareValidationGate::RunGate` 新增 `FixtureRenderDriver* driver` 参数；driver==nullptr 时 fail-closed 返回 `NOT_RUN`（轨道契约 validation.md L121：不再构造空 registry/SharedContext 或独立 synthetic graph）；driver 存在时用其 registry/context 替换空构造点，矩阵/采样/压力/toggle 循环用 driver 的真实场景驱动 `RenderSystem::render`。
- T6.3：三个配方（确定性 xorshift32 RNG，**不使用 std::srand/rand**；本平台确定性——cave 环形遮挡坐标用 std::cos/sin，末位 ULP 因 libm 而异，不承诺跨编译器位级复现）：cave_color_bleed（16 box shadow + 40 emissive crystals + 13 暖/彩光源 + floor tiles）、dynamic_combat_emissive（player + 10 enemies + 8 moving occluders + 24 VisualEffect + 6 AttackEffect + 10 combat lights）、outdoor_light_pressure（按循环放置的 treeline occluders，中心 r<160 留空过滤、实际数量视布局而定 + wide floor + 60 ground patches + 220 光源压力网格）。版本 `s6-v1.1`（v1.0 冻结后经 9×9→15×15 网格修正，版本提升 v1.0 → v1.1），来源记录 `tests/integration/GameplayRuntimeHarness.hpp`。
- T6.4：离屏 RGBA16F composite FBO 由 harness 持有（`FramebufferManager::Create`，`PrepareFixture` 时按 fixture 尺寸重建，`ReleaseCompositeTarget` 销毁）；gate 不再创建/销毁 fixture target，仅经 `driver->CompositeFramebuffer()` 引用；gate 的 stress/toggle 临时压力 FBO 仍归 gate 自有，与 harness target 无冲突。
- T6.5：`FixtureExecutionResult` 增加 `sceneInputHash/fixtureVersion/sceneSource`，写入 GateReport JSON（`scene_input_hash/fixture_version/scene_source`）；输入哈希为 FNV-1a 64，对 recipe 名 + seed + 每个实体的标识数据（tag/坐标）确定性累计。
- 新增 `[Unit] S6 GameplayRuntimeHarness - ...` 5 个用例（各配方组件计数、输入哈希确定性/差异性、未知 recipe 拒绝）；gate 集成测试新增 "Missing driver fails closed NOT_RUN" 用例，并在 RunGate 用例断言 matrix 单元格含 fixture 信息。
- 验证：`check_module_boundaries.py` 71/71；`build.bat check` 通过；`build.bat` 双成功标记（日志 `%TEMP%\opencode\s6-build.log`）；gate 集成测试 4 cases/299 assertions 通过；harness 单元测试 5 cases/24 assertions 通过；ctest unit|integration 13/15（唯一失败为既有 GIStabilityIntegrationTest 硬件读回与 HeavenlySwordClosureTests 概率断言）；`git diff --check` 干净。
- 备注：WARP/无头环境下 gate 结论 `NO_GO` 属预期（无独立 GPU，GI/Lighting pass 无有效采样）；JSON 报告已含 fixture 信息证明 harness 真实驱动（cave/dynamic/outdoor 输入哈希分别为 9635526039250172466 / 1224310844868084887 / 12786560374606737554）。

### S7：M0-C R2.2 paired GI delta

- **状态**：`[ ]`（依赖 S6；前置子任务 S7a）
- **S7a（前置）：真实 GI runtime override**——`QualityTierManager.hpp/.cpp` 增加 `SetGiEnabledOverride`（owner/调用线程/apply-restore 生命周期与异常退出恢复保证）；**优先级合同**：运行时 setter 覆盖 `settings.json` override（`render.gi.enabled`），paired capture 前置确定性配置（禁用 settings override 注入，防两腿均 off 致 delta≈0 误判）；**生效链验证**：override 必须改变 `RenderSystem` 有效配置 → GI pass 加入/移除 → GI 资源尺寸化（GI passes 定义 `EnsureSized`/显式 `OnResize` 契约，**同分辨率 false→true 时也校验资源尺寸**）→ `GICompositePass` temporal history 失效 → warmup；测试 true→false→true、false→true、异常退出恢复、history 隔离与两组不同 pass/resource trace（证明 graph 实际变化，而非仅管理器状态）。
- **S7b（主体）：paired capture 差分**——同 seed/camera/frame/FBO/色彩空间/ROI，仅 giEnabled 翻转；delta 算法与阈值（0.001）记录于证据；复现并判定 Cave `0.000193621 < 0.001`（阈值缺陷 or 真实 bug）；阈值调整须记录依据并经用户批准（§8 决策 1）。
- **涉及文件**：`GPUHardwareValidationGate.cpp`、`QualityTierManager.hpp/.cpp`、`RenderSystem.cpp`（配置读取合同）、`GICompositePass`（history 失效）、捕获工具。
- **验收**：S7a 生效配置可证（pass/resource trace 差异）；S7b paired delta 判定落地；Cave 有明确结论。
- **验证命令**：`./build.bat`；实机 gate 复跑。
- **风险**：高（依赖 S6；结论可能触发 M0-A 范围工作）。

### S8：M0-C R6 artifact 归档 + CI baseline

- **状态**：`[ ]`（依赖 S3 起的 artifact schema）
- **内容**：
  1. **归档**：固定路径 `artifacts/gpu-gate/<revision>/`；归档完整 C++ GateReport JSON（matrix/timer/resource/GL diagnostics/快照序列，不止 status）+ 截图 + 日志；schema/version 与保留策略（如最近 20 次或 90 天）记录于轨道文档。
  2. **runner 接线**：`scripts/gpu_hardware_validation_gate.py` 从 C++ 侧完整 GateReport JSON（S3 定义）解析并归档（matrix/timer/resource/GL diagnostics/快照序列，不止 status）；schema validator（JSON 校验）作为 CI 步骤；`GpuHardwareValidationGateRunnerTest.py` 增加 runner JSON 解析回归测试。
  3. **baseline/waiver 机制**：无实机环境显式标记 `skipped`/`waived`（不假装通过）；waiver 字段：authorizer、reason、scope、expiry、re-validation 结果；**禁止** `NOT_RUN`/`waived`/`NO_GO` 通过为 GO。
- **涉及文件**：`scripts/gpu_hardware_validation_gate.py`、CI 配置、轨道文档、`tests/python/GpuHardwareValidationGateRunnerTest.py`。
- **验证命令**：schema validator 通过；归档产物可追溯；CI 配置检查。
- **风险**：低。

### M0-A（gpu_production_hdr_gi_closure_20260726）

保持主控规格顺序（M0-A→M0-B→M0-C）不动；M0-A R1-R4 的实际前置依赖（M0-B 资源所有权收口）满足后，单独立项评估，不纳入本方案主路径。

## 5. 与模块拆分的关系

- 本方案不修改目标拓扑（CMake）、不移动源文件归属、不触碰 `src/game/` 迁移边界。
- MS-6（GPUEntitySystem/RenderSystem 适配）仍依赖 P0 先行；本方案 S1/S3/S4/S6 落地后，MS-6 的适配前提（timer owner、fixture、registry 快照）即具备。
- 模块边界检查必须全程保持 PASS（当前 71/71）。

## 6. 提交策略

- S1 独立提交（生产缺陷修复）；S2 文档修正独立提交（只陈述已落地状态，不与 S1 混批）。
- S3-S8 按完成顺序各自成批；每批先独立审查，结论 `提交` 后才提交。
- 每个提交前记录 `git status --short` 与 `git diff --check`；提交须用户授权。

## 7. 完成定义

拆分为三组，不得互相替代：

- **DOD-1 源码/测试完成**：S0、S1a/S1b、S3-S8 合同全部落地；能力矩阵与 HEAD 代码一致；模块边界检查与全量构建/测试全绿（既有豁免除外）；轨道 index/metadata 如实反映 `[x]`/`[~]`。
- **DOD-2 硬件 Gate 结论**：RTX 4070 实机 gate 输出 `GO`（timer shortage 消失、Cave paired delta 有结论、无未豁免失败）；或显式 `NOT_RUN`（能力缺失）——两者不可混淆，`NOT_RUN` 不得视为 GO。
- **DOD-3 waiver 记录**：任何 waiver 具备 authorizer/reason/scope/expiry/re-validation 字段并归档；waiver 不掩盖失败。

## 8. 决策记录（2026-07-31 用户已拍板）

1. **S7 Cave delta**：接受调整阈值（方向已授权；S7b 实机采集后记录具体值与依据，不扩范围到 M0-A，除非实机数据表明为真实 bug）。
2. **实机 gate 执行**：本机 RTX 4070 Super（用户确认本机具备）执行；运行频率实施时确定。
3. **S6 fixture 数据来源**：优先复用现有测试 fixture；若无法实现复用则新建关卡快照（据此拆分执行 S6/T6.1-T6.5）。
4. **S1a 主控规格例外授权**：**已授权**。S1a 作为独立生产缺陷修复优先于 M0-A→M0-B→M0-C 顺序实施，记录为主控规格例外（§2 声明不变；S1b 仍随 S0 正常排期）。

## 9. 设计/规格同步与性能约束

1. **合同变更需先入 spec**：S0（stable pass identity）、S1b（timer 四态与跨帧历史）、S7a（GI runtime lifecycle）、S8（artifact/waiver schema）均属跨系统合同变更，须在实施前更新对应轨道 `spec.md`（M0-B/M0-C）并经用户批准；`plan.md`/`validation.md` 同步更新。
2. **热路径约束**：`GetStats/GetAllStats`（RenderProfiler.cpp:155-178 现用 vector 构造）与 S1b 回填/聚合不得引入热路径堆分配：固定容量预分配、按 stablePassId 索引的定长数组；性能验收：现有 profiler 基准无回退（`RenderingBenchmark.cpp` 等）。
3. **轨道文档同步规则**：每步完成后同步对应轨道 plan/validation/index/metadata；禁止超前标记。
