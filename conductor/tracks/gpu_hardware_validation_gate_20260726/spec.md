# GPU Hardware Validation Gate 规格说明书

> **Track ID**: `gpu_hardware_validation_gate_20260726`
> **类型**: P0 quality/release-gate
> **依赖**: `gpu_production_hdr_gi_closure_20260726`、`gpu_rendergraph_resource_foundation_20260726`
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)、[GPU 生产整改后续](../../../docs/designs/gpu-production-remediation-follow-up.md)、[MS-8 技术债整改设计](../../../docs/designs/ms-8-technical-debt-remediation-design.md)
> **状态**: 🚧 In Progress — 2026-08-02 W6 生产门禁机制落地

---

## 1. 问题与目标

历史 V5 验收只执行部分 Radiance pass，GPU timer 可能把未就绪 query 记录为 `0ms`，显存检查仅覆盖 FBO 代理。它们不能证明 Gameplay 离屏完整链、所有 GPU 资源、长稳运行和自动降级的生产行为。

本 Track 建立物理 GPU nightly/release 门禁，消费前两条 Track 的 compiled plan、resource registry 和有效计时器，生成可复查 artifact 与 `GO`/`NO-GO` 结论。无头 unit/integration 是前置条件，不替代硬件结果。

**测试分层（W6，2026-08-02）**：standalone 测试二进制（`NoMoreDayTests.exe`，1x1 hidden context + `GameplayRuntimeHarness`）只能作 contract/diagnostic，**不产生生产 GO**；生产门禁 = `NoMoreDay.exe --gpu-gate`（正常 Game/App 初始化后）。详见 §2.4。

## 2. 门禁范围

### 场景和矩阵

固定至少三个可重复 Gameplay fixture：洞穴颜色溢出、动态战斗遮挡/VFX emissive、室外高光源压力。每个 fixture 运行真实 `BeginTextureMode` 路径，覆盖 High、Ultra、GI-off 回退、window/target resize、tier 切换、capability fallback。

每次运行必须写入 revision、GPU/driver、OpenGL capability、分辨率、tier/config、scene seed、持续时间和 warmup。硬件前置不满足时结果是 `not-run`，不得转成 pass。

### 必须通过维度

| 维度 | 门禁 |
| --- | --- |
| 完整链功能 | plan/pass trace 含 external seed 到 Composite 的全部启用 pass；非黑 ROI、GI 间接光、回退输出均通过 |
| GI 正确性 | SDF sign readback、camera/zoom/resize、动态 occluder/emissive、history rejection、VFX 生产者通过 |
| 计时/性能 | 每关键 pass 至少 120 个 Valid GPU 样本；Pending/CPU fallback 单列；报告 mean/P95 和 V5 预算 |
| 资源/长稳 | registry 覆盖全部受管资源；1 分钟压力后 5 秒滑窗无单调增长，重建/切换无 live-resource leak |
| 回退/稳定 | GI on/off、tier、resize 各 100 次无黑帧、崩溃、GL high-severity 或资源净增长 |

resource registry 的字节数是引擎台账；只有驱动扩展可用才附加 driver VRAM telemetry，两者必须分开命名。未登记资源、无效 timer 性能结论、无法解释黑帧均为门禁失败。

### 运行参数与环境变量契约（S8）

runner（`scripts/gpu_hardware_validation_gate.py`）把 CLI 参数经环境变量注入 C++ gate（`RunGate`），跨系统契约如下，任何一侧变更须在本节同步：

| 环境变量 | 类型 | 默认 | C++ 生效参数 |
|---|---|---|---|
| `NMD_GATE_SAMPLES` | int | `120` | `sampleFramesPerFixture` |
| `NMD_GATE_TOGGLE_LOOPS` | int | `100` | `toggleLoops` |
| `NMD_GATE_STRESS` | `"1"`/`"true"`/`"TRUE"` 为真 | `true` | `stressTest1Min` |

- **超时联动**：`gate_timeout_seconds = GATE_BASE_TIMEOUT_SECONDS(120) + (stress ? GATE_STRESS_ADDED_SECONDS(60) : 0)`，即 stress 1min 时子进程预算 180s、禁 stress 时 120s，保证 60s 压力循环不撞超时。
- **归档路径**：默认 `artifacts/gpu-gate/<revision>/gpu_hardware_validation_artifact.json`（`--output-dir` 可覆盖）；`.gitignore` 排除 `artifacts/`。保留最近 20 次或 90 天。

### 测试分层与生产门禁（W6）

**standalone 测试二进制只作 contract/diagnostic，不产生生产 GO。**

| 类别 | 载体 | 内容 | CTest/标签 |
| --- | --- | --- | --- |
| 真实契约测试（保留） | `NoMoreDayTests.exe` | GateReport JSON schema、Python runner fail-closed 解析、missing-driver -> NOT_RUN、deterministic fixture hash、RenderGraph/registry/lifecycle/QualityTier 契约 | 沿用既有 `[Integration]`/`[Unit]` 与 `ci`/`integration` 标签 |
| contract/diagnostic（重新分类） | `NoMoreDayTests.exe` | `RunGate` 离屏矩阵（1x1 hidden context + `GameplayRuntimeHarness`）、`S7b` paired GI delta capture | `[GPU-Diagnostic]` 前缀；从 broad `ci;nonperf` 与 generic `integration` 移除；独立窄标签 `nmd.tests.gpu.diagnostic`/`nmd.tests.gpu.contract` |
| **生产门禁（唯一 production evidence）** | **`NoMoreDay.exe --gpu-gate`** | 正常 Game/App 初始化后的真实矩阵（真实 GL context/registry/render hooks/render path） | `gpu-hardware` opt-in serial resource-locked job |

- **doctest 成功 ≠ gate GO**：任何测试/文档/输出不得把 doctest 状态转换为生产门禁结论；runner 只认 `NoMoreDay.exe --gpu-gate` 的 artifact。

#### 生产门禁执行契约（`NoMoreDay.exe --gpu-gate`）

- **命令行文法**：
  ```
  NoMoreDay.exe --gpu-gate [--revision REV] [--samples N] [--toggle-loops N]
                [--stress-test-1min | --no-stress-test-1min]
  ```
  - `--revision REV`：artifact revision（默认 `HEAD`）。
  - `--samples N`：每 fixture 采样帧数；argv 优先，缺省读 `NMD_GATE_SAMPLES`，再缺省 `120`。
  - `--toggle-loops N`：GI/tier/resize toggle 循环数；argv 优先，缺省读 `NMD_GATE_TOGGLE_LOOPS`，再缺省 `100`。
  - `--stress-test-1min` / `--no-stress-test-1min`：60s 压力开关；argv 优先，缺省读 `NMD_GATE_STRESS`，再缺省启用。
- **执行上下文**：正常 Game/App 启动初始化完成后运行（`main.cpp --gpu-gate` 分支复用现有 `--smoke-test` 参数模式扩展，不破坏 `--smoke-test`）。真实 GL context、ResourceManager、`RenderSystem::Initialize()`、真实 render hooks（`GameplayRenderHooks`）与 Gameplay state；由 `src/app/GpuGateDriver`（composition root 的具体 `FixtureRenderDriver`）以真实 registry/SharedContext/render context、标准 render path 与 owned real-resolution RGBA16F offscreen target 驱动 `RunGate`。Engine 的 `FixtureRenderDriver` 保持 dependency-neutral（Engine 不 include Game/App 头）。
- **输出契约**：**恰好一个** `GPU_HARDWARE_GATE_RESULT status=GO|NO_GO|NOT_RUN` 行 + 一对 `GPU_HARDWARE_GATE_REPORT_BEGIN/END` 包裹的 versioned JSON artifact（含 GPU identity、revision、fixture/camera/ROI/extent/tier/GI、pass trace、timer samples、diagnostics、resource snapshots、failures、verdict）。缺必填字段/数据 -> `NOT_RUN`，绝不默认值填充（见 [gpu-production-remediation-follow-up](../../../docs/designs/gpu-production-remediation-follow-up.md)）。
- **exit 语义**：process 退出码与 verdict 分离——gate 正常完成即 0 退出，成败由 artifact `gate_status` 决定；runner 仅 `return_code==0 AND schema valid AND status=="GO"` 通过（NO_GO/NOT_RUN 均为失败）。

#### 生产门禁执行契约（W6 修正补丁，2026-08-02）

- **样本/切换参数语义**：显式传入低于生产下限（`--samples < 120` 或 `--toggle-loops < 100`）的参数按 **requested** 执行（诊断/机制验证），artifact `run_config` 记录 `requested_*` 与 `actual_*`（相等）并置 `non_exhaustive=true`；**`non_exhaustive` 禁 GO**（fail-closed）。仅在未显式传入（走缺省 `120`/`100`）时按生产下限执行。
- **GPU 身份**：`capabilities.vendor` / `driver_version` / `renderer` 读取真实 `glGetString(GL_VENDOR/GL_VERSION/GL_RENDERER)`；任一缺失（空串）→ preflight fail → `NOT_RUN`（GPU identity 不完整，fail-closed）。runner 对空 `vendor` / `driver_version` 按 schema 校验拒绝。
- **render hooks 必填**：production driver（`GpuGateDriver`，`IsProductionDriver()==true`）必须通过 `RenderHooks()` 提供真实 `GameplayRenderHooks`；缺失（null）→ `NOT_RUN`（渲染为空壳，fail-closed）。诊断 harness 豁免（非 production driver）。
- **pass trace 真实性**：matrix cell 的 `executed_pass_order` 来自真实 `RenderSystem::render` 最后帧 `RenderGraph::CompiledRenderPlan.passOrder`（`pass_trace_source` 注明来源）；空列表 → cell fail，绝不使用模拟 graph。
- **GI paired delta 逐 cell 进 verdict**：每 matrix cell 独立执行 `RunPairedGiDeltaCapture`，`gi_paired_delta` / `gi_paired_passed` 进入 cell `overall_passed` 与全局 verdict。
- **SDF readback 必填**：GI-on cell 必须对真实 JFA 距离场（GL_R16F）执行 `glGetTexImage` + 5 点 sign probe，`sdf_readback_status=="passed"` 才算过；GI-off cell 为 `not_applicable`。`missing`/`failed` → cell fail。
- **occupancy fail-closed**：M0-A R3 occupancy readback 未实现，artifact `occupancy.status="missing_pending_m0a"`、`blocks_go=true`；**当前任何 revision 都禁 GO**，直至实机 job 在 M0-A R3 落地后重新产出含 occupancy 证据的 artifact。
- **异常路径报告**：初始化异常时也输出完整 `NOT_RUN` JSON report（含失败原因与可用 provenance）到 stdout——保持"每次调用单 marker + 单报告"。

## 3. 发布规则

- `GO` 要求所有 MUST PASS 项通过，artifact、硬件环境和残余风险写入 validation/release posture。
- `NO-GO` 在关键正确性、黑帧、泄漏、严重 GL 信息、无有效 GPU 性能数据或场景缺失时触发。
- 性能不达标不得用 CPU 时间或 2026-02 历史结果替代；只能 `NO-GO` 或由用户明确批准带范围/到期日的 waiver。
- SPH 始终 NO-GO，不属于生产功能；发现 shipped Tier 启用即为回退失败。

### Waiver 与 gate_succeeded 语义（S8）

- **waiver 字段 schema**：CLI `--waiver-authorizer/--waiver-reason/--waiver-scope/--waiver-expiry` 四字段任意非空即生成 artifact 的 `waiver` 对象（authorizer / reason / scope / expiry）。waiver 是**纯元数据**，只作可追溯记录，**不参与判定**。
- **`gate_succeeded` 语义**：`gate_succeeded = (return_code == 0) AND (status == "GO")`。`NOT_RUN`/`waived`/`NO_GO` 永不通过为 GO；waiver 不改变该判定。`meets_preflight`（含 schema 校验）另列，同样不受 waiver 影响。

## 4. 验收标准

- [ ] harness 生成完整 pass trace、截图/ROI、SDF/history readback、timer、resource、GL diagnostics artifact。
- [ ] 三场景、High/Ultra、GI-off、resize、tier switch、capability fallback 均有 pass/fail/not-run 状态。
- [ ] 每个性能结论至少 120 个 Valid GPU 样本，Pending/Unavailable/CPU fallback 分离报告。
- [ ] 1 分钟和 100 次切换无黑帧、泄漏、崩溃或严重 GL debug 信息。
- [ ] release posture、progress、Track 状态仅在当前 artifact 支持时标记 production GO。
- [ ] 归档经过 schema validator：`python scripts/gpu_hardware_validation_gate.py --validate-schema artifacts/gpu-gate/<revision>/gpu_hardware_validation_artifact.json`（exit 0/1）；归档时自动执行 `validate_gate_report_schema` 并写入 `gate_report_schema_errors`，CI 在实机 runner 归档后检查该字段为空且 `gate_succeeded == true`。
- [ ] W6 生产门禁：`NoMoreDay.exe --gpu-gate` 在正常初始化后输出恰好一个 status marker 与一个 versioned JSON artifact；runner 只认 `rc==0 + schema valid + 精确 GO`；NO_GO/NOT_RUN 均为失败。
- [ ] W6 测试分层：standalone 二进制硬件矩阵与 S7b paired capture 从 broad `ci;nonperf` 与 generic `integration` 移除；contract 契约测试保留；doctest 成功不被当作 gate GO。
- [ ] W6 硬件验收（实机 `gpu-hardware` job）：真实 GPU identity、High/Ultra/GI-off/resize/tier/capability 矩阵、每个 declared pass >=120 个不同帧有效样本、零 high-severity GL diagnostics、60s 压力下 5s 窗口无净资源增长、100 次切换、可复现 artifact。

## 5. 风险

- CI 可能无显示 GPU；允许独立 runner，但运行环境和 artifact path 必须版本化。
- 截图会有 driver 细微差异；采用固定 seed/ROI/允许误差，而不是未校准全图像素比较。
- 驱动 VRAM extension 非通用；registry 覆盖率和释放检查是强制证据。


## 6. Gate 收尾验收补充（2026-08-03）

- 本地最小样本（samples=3）验收标准更新：`debug_message_count==0`、每 cell `roi_mean_brightness>0.02`、GI-on cell `sdf_readback_status=passed`、`occupancy.status=present && occupancy.blocks_go==false`、7-pass 真实 pass trace、恰 1 个报告块。以上全部实测通过。
- **完整生产 GO 仍被 18 个资源泄漏候选阻塞**（门禁 fail-closed watchdog 现有行为；GI 持久 pass 资源在 baseline 后创建未释放）。本地验证通过 ≠ 生产 GO。
- `third_party/raylib/src/rlgl.h` 空批次 flush 杀 program 修复为 waivered deviation（不在本 Track 允许文件清单），待主代理评审并入或回退。
