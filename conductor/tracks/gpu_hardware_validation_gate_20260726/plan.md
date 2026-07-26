# GPU Hardware Validation Gate 实施计划

> **Track ID**: `gpu_hardware_validation_gate_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [x] Completed

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
