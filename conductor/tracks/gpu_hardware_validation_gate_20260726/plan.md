# GPU Hardware Validation Gate 实施计划

> **Track ID**: `gpu_hardware_validation_gate_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [~] In Progress — production NO-GO

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
- [ ] R6: 将 JSON、fixture seed/camera/ROI、GI mode、SDF probes、trace、distinct timing、diagnostics、snapshots 和硬件信息归档到版本化或可复现位置；补齐共享 CI failure 的 baseline/waiver 或修复。

**退出标准**：R0-R6、`./build.bat`、相关 CTest、gate runner 和目标 GPU nightly 全部通过。任一 MUST PASS 失败或 artifact 缺失即为 `NO-GO`。
