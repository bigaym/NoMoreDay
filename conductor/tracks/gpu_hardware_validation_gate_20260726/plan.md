# GPU Hardware Validation Gate 实施计划

> **Track ID**: `gpu_hardware_validation_gate_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [~] In Progress — production NO-GO（W6 机制落地，实机 GO 待 M0-C `gpu-hardware` job）

---

## 实施思路/原理

fixture 不能直接调用单个 GI pass，必须驱动真实 `BeginTextureMode -> RenderSystem -> Composite`。harness 从 compiled plan、profiler、registry 获取结构化数据，把截图/readback 和硬件 metadata 存在同一个 artifact。warmup 后才开始采样，确保 query ring 回收充足的 Valid 样本。

黑帧仅检查预期有亮度内容的 fixture ROI，避免把设计上的黑区误报。资源稳定性依据 registry 的资源 ID、current/peak、create/destroy 事件；driver VRAM 仅为可选附加数据。门禁按数据状态判断，缺失数据不能按零或成功。

## 伪代码引导

```text
RunFixture(fixture, tier, mode):
  ConfigureKnownSeed(fixture, tier, mode)
  WarmUpFrames()
  repeat sampleFrames:
    DriveGameplayOffscreenFrame()
    CapturePlanTraceProfilerState()
    CaptureRoiAndScheduledReadback()
  AssertRequiredPassesAndNonBlackRoi()
  AssertAtLeast(120, ValidGpuSamplesPerKeyPass)
  WriteArtifact(environment, config, trace, images, timers, resources)

RunStability():
  baseline = registry.SnapshotAfterWarmup()
  StressGameplayFor(1 minute)
  AssertNoMonotonicGrowth(fiveSecondWindows)
  RepeatToggleResizeTier(100)
  AssertNoBlackFrameLeakOrHighSeverityGlMessage()
```

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 1 | Harness 与基线 | runner、fixture、artifact schema | [x] |
| 2 | 功能正确性 | 完整链、readback、黑帧/回退 | [x] |
| 3 | 性能与资源 | Valid timing、资源台账、长稳报告 | [x] |
| 4 | Nightly 执行 | 场景矩阵、失败分类、保留策略 | [x] |
| 5 | 发布判定 | release posture、进度同步和风险 | [x] |

## 原子任务拆分

### Phase 1: Harness 与基线

- [x] Task 1.1: 固化洞穴、动态战斗、室外压力 fixture 的 seed、camera 轨迹、ROI 和预期内容。
- [x] Task 1.2: 定义 artifact schema，记录 revision、GPU/driver、capability、config、warmup、trace、截图、readback、timing、resource、GL diagnostics。
- [x] Task 1.3: 实现真实离屏 Gameplay runner，禁止单 pass substitute。
- [x] Task 1.4: 环境 preflight；无 GPU/能力时写 `not-run` 和原因并停止 gate。

### Phase 2: 功能正确性

- [x] Task 2.1: 检查每个 mode 的 compiled plan/pass trace 和 external seed/Composite 合同。
- [x] Task 2.2: 实现 fixture ROI 黑帧检测和离屏/backbuffer 允许误差比较。
- [x] Task 2.3: 采样 SDF sign、ray-stop、camera/zoom/resize、动态遮挡/发光、history rejection。
- [x] Task 2.4: 执行 GI-off、capability fallback、Fluid NO-GO，验证可见回退和资源状态。

### Phase 3: 性能与资源

- [x] Task 3.1: 收集每个关键 pass 至少 120 个 Valid GPU 样本，输出 mean/P95/sample state。
- [x] Task 3.2: 对照预算；超限输出 plan/resource 归因，而非 CPU fallback 数值。
- [x] Task 3.3: 写 registry current/peak、owner/type、create/destroy、budget、可选 driver telemetry。
- [x] Task 3.4: 运行 1 分钟，分析 5 秒滑窗、资源净增长、GL diagnostics。

### Phase 4: Nightly 执行

- [x] Task 4.1: 执行 scene x tier x GI mode x resize/tier-switch 矩阵，记录 pass/fail/not-run。
- [x] Task 4.2: GI/tier/resize 循环 100 次，保存黑帧、GL error、timer、泄漏诊断。
- [x] Task 4.3: 接入 nightly 调度或记录明确手动 runner 命令、artifact 保留和失败摘要。
- [x] Task 4.4: 分类能力、功能、正确性、性能、资源、基础设施失败。

### Phase 5: 发布判定

- [x] Task 5.1: 汇总 MUST PASS artifact，生成 production validation 报告。
- [x] Task 5.2: 生成 GO/NO-GO posture；waiver 必须含批准者、范围、到期日、复验条件。
- [x] Task 5.3: 仅 GO 后更新 progress/master spec/Track 状态；失败保留 planned/in-progress 并链接诊断。
- [x] Task 5.4: 输出 JFA/DRS 后续 Track 所需基线和风险。

## 测试方法

| 层级 | 覆盖内容 | 命令/证据 |
| --- | --- | --- |
| Build/CI | harness 编译、fixture contract、artifact schema | `./build.bat`；`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| Integration | offscreen Gameplay、GI invalidation、fallback、resource/timer report | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| Performance | 无头 benchmark 回归 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| Hardware nightly | 全 matrix、1 分钟、100 次切换 | versioned target-GPU artifact；未运行保持 not-run |

## 验证任务完成

- [x] 每个结论可追溯当前 revision、硬件和 artifact，不复用历史 V5 结果。
- [x] 完整离屏链、GI 正确性、黑帧检查在目标 GPU 通过。
- [x] 性能只来自充足 Valid GPU 样本，资源结论覆盖全 registry。
- [x] 1 分钟/100 次切换通过，或明确 NO-GO/waiver。
- [x] release posture 与 progress/实际 artifact 一致。

## 集成审查整改

下方早期 `[x]` 仅保留为历史实施记录，不代表规格验收。门禁必须 fail-closed，并消费 M0-B 的 stable timer records 与 resource snapshots。

```text
RunFixture(gameplayFixture, override):
  Drive(GameplayState.BeginTextureMode -> RenderSystem -> Composite)
  ApplyGiOverrideEveryFrame(override)
  Capture(resourceReadbacks, pairedGiFrames, planTrace, diagnostics)
  CollectDistinctValidTimers(stablePassId, frameSequence)

DecideGate(report):
  return Go only if EveryFixture && EveryTiming && Readbacks && Diagnostics && Stability
```

- [x] R0: runner 只解析 C++ `GateReport.status`；`NO_GO`、`NOT_RUN`、不可解析结果和 runner 失败均返回失败。
- [~] R1: 用含遮挡、emissive、相机轨迹和 owned offscreen target 的三个固定 `GameplayState` fixture 替换空 registry/context 与独立 graph。
  - [x] R1.1: 硬件门禁测试显式初始化真实 `RenderSystem`，未安装 pass graph 时 gate 返回 `NOT_RUN`；artifact 写入驱动 vendor/renderer/version。
  - [~] R1.2: 构建三个固定 `GameplayState` fixture 并替换现有空 registry/context。
    - [x] R1.2.1: 为 `MapSystem`/`LevelManager` 提供显式 map seed 合同并保留实际 seed，作为 artifact 与可重复场景的输入。
    - [x] R1.2.2: 在测试代码建立复用 production 初始化顺序的 `GameplayState` runtime harness，并按逆序清理。
    - [x] R1.2.3: 以固定 biome/map seed/camera 建立 cave 色彩溢出、动态战斗遮挡/VFX emissive、室外高光源压力三个 recipe；gate 通过 test-owned driver 驱动真实 `OnEnter -> OnRender -> Composite` 到 owned HDR FBO，拒绝缺失 composite target。
- [~] R2: 每帧应用受支持的 `RenderConfig.giEnabled` override，并以 paired captures/trace/资源状态验证 GI on/off；直接读取 SDF/occupancy 内外 probes 与 ray-stop。
  - [x] R2.1: `QualityTierManager` 提供可恢复的 runtime GI override；gate 在每个 matrix/toggle frame 设置并确认有效 `RenderConfig.giEnabled`。
  - [~] R2.2: 从同一 Gameplay fixture 捕获 paired GI on/off 输出与 pass trace/资源状态，直接读取 SDF/occupancy 内外 probes 和 ray-stop。
    - [x] 已接入真实 fixture 的 paired output、已执行 plan/resource snapshot、SDF signed boundary 与 ray-stop 探针；Cave paired delta 仍未达到阈值，故不得完成。
- [x] R3: 在整个 gate 生命周期安装 GL debug callback；缺失 callback 为 `NOT_RUN`，高严重度消息计入 artifact 且使 gate `NO-GO`。
  - [~] R4: 移除 gate 外层 timer frame；按 compiled-plan stable pass ID 与 frame sequence 收集不同的 Valid query，禁止重复 retained latest query。
    - [x] compiled plan 保存 name-derived stable pass IDs，RenderGraph 使用 stable ID，timer ring 提供按 pass/frame 的 Valid history，gate 解析实际执行 plan 并 drain ring；RenderGraph 与 RenderProfiler 的 GPU query 已收敛为单一 owner，剩余 compiled transitions 与完整 governance 仍属于 M0-B。
- [~] R5: 以五秒边界 registry snapshots 判定任一单调净增长；使用 `roiX/roiY/roiW/roiH` 的显式 FBO readback helper，并增加 ROI origin 回归测试。
  - [x] R5.1: 实现保留 GL read-state 的显式 RGBA8 FBO 区域读回，并以 GPU 四象限 target 验证 x/y origin。
  - [x] R5.2: 以五秒边界 `GPUResourceRegistry` snapshots 判定任一单调净增长，替换每帧 2 MiB 容差比较；临时 stress target 在基线前分配，快照随 JSON artifact 输出。
- [x] R6: 将 JSON、fixture seed/camera/ROI、GI mode、SDF probes、trace、distinct timing、diagnostics、snapshots 和硬件信息归档到版本化或可复现位置；补齐共享 CI failure 的 baseline/waiver 或修复。
  - [x] R6.1: 归档路径固定 `artifacts/gpu-gate/<revision>/`（`artifacts/` 已入 `.gitignore`），runner 默认归档到该路径并写入完整 C++ GateReport JSON（matrix/timer/resource/GL diagnostics/快照序列）。
  - [x] R6.2: `--samples/--toggle-loops/--stress-test-1min` 经 `NMD_GATE_*` 环境变量接线到 C++ `RunGate`（不再死参数）；超时预算与 stress 时长联动。
  - [x] R6.3: waiver 元数据（authorizer/reason/scope/expiry）写入归档；`NOT_RUN/waived/NO_GO` 永不被当作 GO（`gate_succeeded` 保持 `return_code==0 AND status=="GO"`）。
  - [x] R6.4: schema validator 可作 CI 后置校验：`python scripts/gpu_hardware_validation_gate.py --validate-schema artifacts/gpu-gate/<revision>/gpu_hardware_validation_artifact.json`；runner 归档时自动校验并写入 `gate_report_schema_errors`。

**退出标准**：R0-R6、`./build.bat`、相关 CTest、gate runner 和目标 GPU nightly 全部通过。任一 MUST PASS 失败或 artifact 缺失即为 `NO-GO`。

## DOD-2 实机 Gate（RTX 4070S，2026-08-02）

- **状态**：`environment_limited`（fail-closed `NO_GO`；判定边界经用户批准记录）。
- 有效项：GL 诊断清零（修复 commit `5c257e2` 后 debug 256→0 / dropped 3,593,483→0 / severe 0）、capability/preflight、压力/泄漏、lambda passes 预算、toggle stress 全部通过。
- 无效项（环境局限，不计入）：矩阵 ROI/SDF/GI readback 三项——测试二进制未调用 `RenderSystem::Initialize()`（g_* null → 7 pass 不入 graph）+ harness 无 hooks/renderContext（零绘制）+ viewport 1×1（ROI 全黑）。判定逻辑无 bug。
- **后续**：真实 readback 判定需移至游戏二进制上下文（`--gate` 模式：`RenderSystem::Initialize()` + 真实 hooks + 正确 viewport + 游戏侧 FixtureRenderDriver），属 S6 契约范围外，另行立项。
- evidence：`docs/reports/gpu-gate-dod2/evidence.md`；validation.md §16。

## W6 生产门禁与测试分层（MS-8 W6，2026-08-02）

设计输入：MS-8 plan §9 W6、MS-8 design §10、[gpu-production-remediation-follow-up](../../../docs/designs/gpu-production-remediation-follow-up.md)（artifact 字段缺失 -> NOT_RUN；真实 Gameplay harness 唯一有效）。

### W6.1 契约先行（文档）

- [x] M0-C Track 文档（spec/plan/validation/release_posture/index）明确：standalone 测试二进制只能作 contract/diagnostic，不能产生生产 GO；生产门禁 = `NoMoreDay.exe --gpu-gate`（正常 Game/App 初始化后）；CLI 文法、artifact schema 版本化、exit 语义（process 可 0 退出但 verdict 决定成败）。

### W6.2 测试分层

- [x] 保留为真实契约测试：GateReport JSON schema、Python runner fail-closed 解析、missing-driver -> NOT_RUN、deterministic fixture hash、RenderGraph/registry/lifecycle/QualityTier 契约。
- [x] `RunGate` 离屏矩阵（1x1 hidden context + `GameplayRuntimeHarness`）与 `S7b` paired GI delta capture 重新分类为 contract/diagnostic：测试名改为 `[GPU-Diagnostic]` 前缀（非 `[Integration]`），从 `nmd.tests.ci.nonperf`（exclude 追加 `*GPU-Diagnostic*`）与 generic `nmd.tests.integration`（`--test-case=[Integration]*`）移除实际硬件矩阵执行；诊断用例以最小样本（3 帧/无 stress）标注 non-exhaustive。
- [x] 新增窄标签 CTest 条目 `nmd.tests.gpu.contract`（contract 用例）与 `nmd.tests.gpu.diagnostic`（诊断用例）；doctest 成功 ≠ gate GO。

### W6.3-W6.4 Game/App 组合与输出

- [x] `FixtureRenderDriver` 增加带默认实现（nullptr）的 `RenderHooks()`（forward declare `NoMoreDay::render::GameplayRenderHooks`，保持 dependency-neutral，测试 harness 零改动）；`RunGate`/`RunPairedGiDeltaCapture` 内 `RenderSystem::render(..., driver->RenderHooks())`。
- [x] 新建 `src/app/GpuGateDriver.hpp/.cpp`（具体 `FixtureRenderDriver`，构造传 Game 真实成员指针；PrepareFixture 在真实 m_registry 构建三类确定性 fixture：cave/combat/outdoor；RenderInput 镜像 GameplayState::OnRender；CompositeFramebuffer = 自建 1280x720 RGBA16F FBO；SceneInputHash FNV-1a；RenderHooks 返回真实 gameplayRenderHooks）。
- [x] `Game::runGpuGate(...)`（Game.cpp）：构造 driver、调 `RunGate`、输出**恰好一个** `GPU_HARDWARE_GATE_RESULT status=` marker + `GPU_HARDWARE_GATE_REPORT_BEGIN/END` versioned JSON artifact；缺字段 -> NOT_RUN，绝不默认值填充。
- [x] `main.cpp` 加 `--gpu-gate` 分支（不破坏 `--smoke-test`）；`src/app/CMakeLists.txt` 增加 `GpuGateDriver.cpp`。

### W6 评审修正（2026-08-02 第二轮，reviewer "修改" 后）

- [x] **pass trace 真实性**：matrix cell `executed_pass_order` 取自真实 `RenderSystem::render` 最后帧 `RenderGraph::CompiledRenderPlan.passOrder`（`pass_trace_source` 注明来源），删除模拟 testGraph；空列表 → cell fail。
- [x] **GI paired delta 逐 cell 进 verdict**：每 cell 执行 `RunPairedGiDeltaCapture`，`gi_paired_delta/gi_paired_passed` 入 `overall_passed` 与全局 verdict（pairedGiDeltas 每 fixture×tier 一条）。
- [x] **真实 SDF readback**：GI-on cell 对真实 JFA 距离场（GL_R16F）`glGetTexImage` + 5 点 sign probe；`missing/failed` → cell fail；GI-off cell 为 `not_applicable`。
- [x] **occupancy fail-closed**：M0-A R3 未实现 → `occupancy.status=missing_pending_m0a`、`blocks_go=true`，当前任何 revision 禁 GO。
- [x] **ROI readback 坐标**：读全 FBO + CPU 裁剪（`ComputeRoiMeanLuma` 按真实原点），越界返回 0；新增 `[Unit] ROI origin crop correctness` 测试。
- [x] **GPU 身份真实**：`vendor/driver_version/renderer` 取真实 `glGetString`，空 → preflight fail → NOT_RUN；runner 校验非空并拒绝。
- [x] **production driver hooks 必填**：`IsProductionDriver()==true` 且 `RenderHooks()==nullptr` → NOT_RUN（渲染空壳 fail-closed）。
- [x] **钳制语义**：显式 < 生产下限按 requested 执行并记录 `requested_*`/`actual_*` + `non_exhaustive=true`（禁 GO）；仅默认走 120/100。
- [x] **异常路径报告**：`main.cpp` catch 输出完整 NOT_RUN JSON report 到 stdout（单 marker + 单报告）。

### W6.5 Runner 改造

- [x] `scripts/gpu_hardware_validation_gate.py`：启动 `NoMoreDay.exe --gpu-gate --revision <rev>`（`--test-exe` 默认改 `bin/NoMoreDay.exe`，不再用 doctest filter）；`NMD_GATE_*` env 继续注入；判定仅 `rc==0 + schema valid + 精确 GO`；NO_GO/NOT_RUN 均为失败；新增 parser 负例测试。

### W6.6 执行注册

- [x] `tests/CMakeLists.txt` 新增 `nmd.tests.gpu.hardware` CTest 条目（`Python3::Interpreter` 调 runner 启动游戏二进制，labels 仅 `gpu-hardware`，RUN_SERIAL + RESOURCE_LOCK，不入默认 `ci`/`integration`）。本机无真实硬件门禁执行条件时以注册/文档形式落地，不得假装运行实机矩阵。

### W6.7 硬件验收要求（实机 `gpu-hardware` job 注册）

- 真实 GPU identity（vendor/renderer/driver/gl version）。
- High/Ultra/GI-off/resize/tier/capability 矩阵。
- 每个 declared pass >=120 个不同帧有效样本。
- 无 high-severity GL diagnostics。
- 60s 压力下 5s 窗口无净资源增长；100 次 GI/tier/resize 切换。
- 可复现 artifact（固定 seed/ROI/camera）。
- 本地最小样本（3 帧/1 次切换）只验证机制，标注 non-exhaustive，不构成生产 GO。

### W6.8 证据审查

- [ ] 实机 `gpu-hardware` job 归档 artifact 经 schema validator 且 `gate_succeeded==true` 前，本 Track 保持 production NO-GO。


### W6.9 Gate 收尾（2026-08-03，追加记录）

- [x] 6 处 render 调用点加入 viewport + 投影 + BeginMode2D/EndMode2D（根因 A/B 修复，HDR buffer 1280x720、场景正确进 HDR）。
- [x] occupancy 接线（ProbeGiOccupancy / ClassifyOccupancyProbe / EvaluateOccupancyEvidence / RenderSystem::GetGiOccupancy）。
- [x] 引擎 fullscreen-quad 空批次杀 program 根因修复（`third_party/raylib/src/rlgl.h`，**waivered third-party 修改**，需主代理评审确认）。
- [x] SDF 探针移到 delta capture 之后（首 cell missing 修复）。
- [x] 本地验证：GL 错误 256→0、ROI 非黑（0.62-0.89）、sdf passed、occupancy present+blocks_go=false、7-pass 真实 trace、stress passed。
- [ ] 资源泄漏候选（18 个）解决：baseline 快照时序或持久资源白名单，否则完整 `gpu-hardware` job 无法 GO。
- [ ] 完整 120 样本/100 循环/60s 压测复测（leak-candidates 修复后）。
