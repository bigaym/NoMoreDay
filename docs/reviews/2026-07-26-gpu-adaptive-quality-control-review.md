# GPU Adaptive Quality Control 实施审查

## 审查目标

Track `gpu_adaptive_quality_control_20260726` (M2-E) 实施审查。

## 结论

`修改`

当前实现覆盖配置层、DRS 控制器与 GPU 计时聚合，controller 结构清晰、滞回与 Valid GPU 采样正确。但相对规格与计划的交付范围出现实质缺口：自动曝光 (Phase 3) 75% 未实现，验证与启用 (Phase 4) 60% 未完成，八项验收标准完全没有硬件证据。`metadata.json` 正确记录为 `in_progress`，不是已完成状态。

## 审查轮次

首次审查。

## 输入

- 设计规格：[conductor/tracks/gpu_adaptive_quality_control_20260726/spec.md](../conductor/tracks/gpu_adaptive_quality_control_20260726/spec.md)
- 实施计划：[conductor/tracks/gpu_adaptive_quality_control_20260726/plan.md](../conductor/tracks/gpu_adaptive_quality_control_20260726/plan.md)
- 架构审查：[docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md](./2026-07-26-gpu-rendering-engine-audit-review.md)
- 审查标准：[docs/workflows/review.md](../docs/workflows/review.md)
- 硬件门禁：[conductor/tracks/gpu_hardware_validation_gate_20260726/](../conductor/tracks/gpu_hardware_validation_gate_20260726/index.md)
- 验证证据：validation.md + git diff 源码检视 + 项目图谱检索

## 变更文件边界

```
 M src/engine/render/RenderSystem.cpp           | 142 ++++++++++++++++++++++--
 M src/engine/render/RenderSystem.hpp           |  12 +++
 M src/engine/render/core/QualityTierManager.cpp| 149 ++++++++++++++++++++++++++
 M src/engine/render/core/QualityTierManager.hpp|   6 ++
 M src/engine/render/core/RenderConstants.hpp   |  23 ++++
 M src/engine/render/debug/GPUTimerQueryRing.cpp|  94 ++++++++++++++--
 M src/engine/render/debug/GPUTimerQueryRing.hpp|  14 +++
 M src/engine/render/passes/PostProcessPass.cpp |   2 +-
 M src/game/states/GameplayState.cpp            |  37 +++++--
 M tests/CMakeLists.txt                         |   1 +
 M tests/unit/QualityTierManagerTest.cpp        |  41 +++++++
?? conductor/tracks/gpu_adaptive_quality_control_20260726/
?? src/engine/render/core/AdaptiveQualityController.cpp
?? src/engine/render/core/AdaptiveQualityController.hpp
?? tests/unit/AdaptiveQualityControllerTest.cpp
```

11 个 modified + 4 个 untracked 文件，未提交。

## 范围对齐

### 设计规格对齐

| 规格要求 | 状态 | 说明 |
|---------|------|------|
| render scale 只作用于 Gameplay world/HDR/GI/postprocess；UI 保持 native（§2.1） | ✅ | GameplayState 使用 `RenderTargetExtent` 创建 scene RT，native 拉伸展示 |
| controller 仅消费 Valid GPU timing（§2.1） | ✅ | `UpdateAdaptiveQualityPolicy` 检查 `QueryState::Valid` |
| 使用 target budget、上下阈值、最短窗口、cooldown、恢复滞回（§2.1） | ✅ | `AdaptiveQualityController` 实现完整滞回模型 |
| 压力顺序：tier scale → feature degrade（§2.1） | ✅ | `DecreaseScale` → `RequestFeatureDegrade` 顺序 |
| 无有效样本/锁定保持固定 scale（§2.1） | ✅ | `NoValidGpuSample`/`UserLocked`/`Disabled` 分支 |
| 默认 renderScale=1.0, fixed exposure 1.0，不改变画面（§2.2） | ✅ | 所有默认值安全：`dynamicResolutionEnabled=false`, `renderScaleLocked=true`, `exposure=1.0f` |
| optional auto exposure: HDR histogram/reduction（§2.2） | ❌ | 未实现 |
| 亮/暗不同适应速度、clamp、时间平滑（§2.2） | ❌ | 未实现 |
| resize/tier/scale/fallback 受控重置 exposure history（§2.2） | ❌ | 未实现 |
| exposure 不采样 UI/final LDR（§2.2） | ⚠️ | 部分：exposure 从 config 读取，但 auto exposure 本身未实现 |

### 实施计划对齐

| 阶段 | 完成度 | 状态 |
|------|--------|------|
| Phase 1: 配置与 target 合同 | 4/4 | ✅ |
| Phase 2: DRS controller | 4/5 | ⚠️ Task 2.5 profiler HUD 未实现 |
| Phase 3: Auto exposure | 1/4 | ❌ 仅 exposure config 读取完成 |
| Phase 4: 验证与启用 | 2/5 | ❌ 仅 controller unit test + config roundtrip 完成 |

整体：11/18 tasks（与 `metadata.json` 一致）。

### 验收标准对齐

| 验收标准 | 状态 |
|---------|------|
| 1.0/fixed 截图与硬件 Gate 基线一致 | ❌ 无证据 |
| controller 只在 Valid 样本持续超/低阈值后调节，稳定负载每分钟 ≤ 2 次变化 | ⚠️ 逻辑正确，无实机证据 |
| 最小 scale 前不关闭 feature；到下限才与 tier degrade 协作 | ✅ 实现正确 |
| 0.70-1.00 scale/resize/GI/tier/离屏无黑帧、泄漏、坐标错位或 UI 缩放 | ❌ 无硬件证据 |
| auto exposure 符合 clamp/收敛/最大单帧变化 fixture 阈值 | ❌ 未实现 |
| 有效 GPU/资源/视觉/fallback 证据通过后才默认启用 | ⚠️ 默认关闭策略符合要求，但无启用证据 |

## 质量与风险评估

### 已验证正面

- `AdaptiveQualityController` 状态机正确：有效 GPU 样本门控、sustain window、cooldown、滞回恢复、下限 feature degrade 请求。
- `GPUTimerQueryRing` 实现了帧级聚合、P95 计算、Valid/Pending/CpuFallback 状态区分。
- `UpdateAutoDegradePolicy` 已从 CPU profiler stats 迁移到 GPU timer ring，对齐硬件门禁的 Valid GPU 合同。
- `GameplayState` 离屏 RT 创建使用 `RenderTargetExtent`，UI 保持 native 分辨率。
- JSON 持久化完整：读写 + roundtrip 测试。
- 所有扩展点（auto exposure、DRS 主动调节）默认关闭，不改变现有画面。

### 发现项

#### High — Auto exposure (Phase 3) 75% 未实现

`spec.md §2.2` 要求 HDR log-luminance histogram/reduction pass、percentile target、明暗不同适应速度、clamp、时间平滑、resize/tier fallback 受控重置。`plan.md §3` 拆分 4 个原子任务 (3.1-3.4)。当前仅 Task 3.3 的 exposure config 读取部分完成（`PostProcessPass.cpp:400`）。不存在 HDR histogram 或 reduction pass、无 auto exposure adaptation 逻辑、无 capability fallback、无 debug 固定值/直方图。

default-off (`autoExposureEnabled=false`) 确保不破坏现有画面，但 Plan 交付范围缺口明确。

#### High — 验证证据 (Phase 4) 60% 未完成

八项验收标准中六项无硬件、视觉或 fixture 证据：
- 无 offscreen 集成测试覆盖 scale/resize/tier/GI 切换/native UI 坐标（Task 4.2）
- 无亮暗 fixture 验证 clamp、收敛、最大单帧变化与 UI 截图不变（Task 4.3）
- 无目标 GPU 采集 Valid P95、oscillation、资源或黑帧 artifact（Task 4.4）
- 无默认启用决策证据（Task 4.5）
- 唯一交付的验证是 5 个 controller unit test 和 1 个 JSON roundtrip test

`conductor/code_standard.md` §9 验证要求未满足。

#### Medium — Profiler HUD 显示未实现 (Task 2.5)

Plan 要求 profiler/report 显示 scale、sample state、reason、target extent。`RenderSystem` 仅在 log 输出 DRS decision，未在 HUD/overlay 显示。

#### Medium — `GetRenderScale()` 耦合 controller 配置副作用

`RenderSystem::GetRenderScale()` (`RenderSystem.cpp:1458-1465`) 在 getter 内调用 `ConfigureAdaptiveQualityController()`，后者包含 `LOG_INFO` 输出。`GameplayState` 在构造函数和 `UpdateSceneRT` 中调用此 getter。Getter 不应产生日志或重配置。应改为在 `Initialize` 时一次性配置 + `GetRenderScale()` 纯查询。

#### Medium — Auto-degrade 日志丢失 per-pass timing 明细

`UpdateAutoDegradePolicy` 将 `timingSummary` 硬编码为 `"frame_gpu_aggregate"`（`RenderSystem.cpp:459`），不再传递原始 per-pass 统计。虽然 GPU aggregate 更准确，但降低了离线调试质量。

#### Low — Track 元数据与文档未同步

- `index.md`: Status=Planned, Tasks=0/18（应为 In Progress, 11/18）
- `conductor/tracks.md`: 显示 `[ ]` 与 📋 Planned
- `rendering_system_progress.md`: 显示 📋 Planned
- 仅 `metadata.json` 正确记录 `in_progress` + 11/18
- 变更未提交

#### Best Practice — `GetValidFrameP95Ms` 每次调用全量排序

`GPUTimerQueryRing.cpp:258-272` 每次调用拷贝 120 个条目到栈 `std::array` 然后 `std::sort`。在 120 样本量下开销可忽略，但用于每帧 `UpdateAdaptiveQualityPolicy` 的决策路径，可改用按需排序标记或维护有序窗口。

## 最佳实践建议

| 发现项 | 可执行修复建议 |
|--------|------------|
| Auto exposure 未实现 | 按 Plan §3 创建后续子任务（HDR histogram pass、percentile math、adaptation、test），逐条实现。不阻塞当前 `修改`。 |
| 验证证据缺失 | 优先补充 offscreen scale/resize/GI 集成测试（Task 4.2）和 GPU 采集（Task 4.4），这是启用 DRS 的前提。 |
| Profiler HUD | 在 `DrawProfilerHud` 或 `RenderSystem` 渲染循环中增加 DRS 状态行（scale、reason、sample state）。 |
| `GetRenderScale` 副作用 | 将 `ConfigureAdaptiveQualityController` 移至 `RenderSystem::render` 或显式初始化步骤，`GetRenderScale` 返回 `g_adaptiveQualityController.GetCurrentScale()` 纯查询。 |
| Auto-degrade 日志 | 恢复 per-pass 明细或附加到 aggregate 日志中，保留调试诊断能力。 |
| 元数据同步 | 更新 `index.md`、`tracks.md`、`rendering_system_progress.md` 状态；变更提交。 |
| P95 排序优化 | 在 `m_frameHistory` 上使用脏标记 + 按需排序，或使用增量 percentile 维持（`kahan`+`t-digest` 推荐后补，非当前阻塞）。 |

## 剩余风险

- 当前默认全部关闭的生产安全性：`dynamicResolutionEnabled=false`、`renderScaleLocked=true`、`autoExposureEnabled=false`、`exposure=1.0f`，不改变现有画面，零生产风险。
- 已实现部分（controller 逻辑、GPU timer P95、Gameplay RT 伸缩）结构正确，通过单元测试验证。
- 真实 GPU 负载下的 DRS 振荡特征未知——Task 4.4 采集完成前无法确认每分钟 scale 变化 ≤2 次的验收标准。
- Auto exposure 的 HDR histogram 会增加 GPU work/资源（spec §4），在实现前需进入 pass 预算与 registry。

## 下一步动作

1. **按本报告的结论执行修改计划**，关闭 High 和 Medium 级别发现项。
2. 对 Phase 3 (auto exposure) 创建后续独立子 Track 或挂入 backlog。
3. 补充 Phase 4 验证证据，尤其是 offscreen 集成测试 (Task 4.2) 与硬件采集 (Task 4.4)。
4. 同步 Track 元数据并提交当前变更。
5. 修复 Medium 级别 CQS 违规与日志回归后，进入下一轮复审查。

---

## 第二轮跟进审查

### 本轮审查范围

审查方确认第一轮报告指出的可操作代码级发现项已修复。本轮仅复查已解决项与仍开启项，不重新评估范围对齐表。

### 本轮变更内容

| 变更文件 | 变更内容 |
|---------|---------|
| `conductor/tracks.md` | 状态从 `[ ]` 改为 `[~]`，📋 Planned → 🚧 In Progress，Tasks 0/18 → 11/18 |
| `conductor/rendering_system_progress.md` | 📋 Planned → 🚧 In Progress，描述增加"/Auto Exposure HDR pass 待实现" |
| `src/engine/render/RenderSystem.cpp:1455` | `GetRenderScale()` 移除 `ConfigureAdaptiveQualityController` 副作用，改为纯查询 |
| `src/engine/render/RenderSystem.cpp:2293-2307` | 新增 DRS 状态 HUD：scale、enabled、locked、autoExp、GPUstate、P95 |
| `src/engine/render/RenderSystem.cpp:457-459` | Auto-degrade timing 日志恢复 per-pass 明细（`g_renderProfiler` 存在时） |

### 上一轮发现项解决状态

| 发现项 | 严重度 | 状态 | 说明 |
|--------|--------|------|------|
| `GetRenderScale()` CQS 违规 | Medium | ✅ 已解决 | 纯查询，不再调用 ConfigureAdaptiveQualityController |
| Profiler HUD (Task 2.5) | Medium | ✅ 已解决 | `RenderSystem::render` 末行增加 DRS 状态行 |
| Auto-degrade 日志 per-pass 明细丢失 | Medium | ✅ 已解决 | `g_renderProfiler` 非空时恢复 per-pass 明细 |
| Track 元数据未同步 | Low | ✅ 已解决 | tracks.md、rendering_system_progress.md 已更新 |
| Auto exposure Phase 3 未实现 | High | 🔴 仍开启 | 默认 `autoExposureEnabled=false`，无生产影响。计划挂入 backlog |
| 验证证据 Phase 4 未完成 | High | 🔴 仍开启 | 集成测试、硬件采集、fixture 测试待后续。默认关闭策略安全 |
| P95 排序优化 | Best Practice | ⚪ 未处理 | 非阻塞，120 样本开销可忽略 |

### 本轮结论

`提交`

本轮修复了全部三个 Medium 代码质量发现项（GetRenderScale 副作用、Profiler HUD、Auto-degrade 日志）和一个 Low 文档同步项。剩余两个 High 发现项为计划内 scope gap，均受安全默认值保护（auto exposure 与 DRS 均默认关闭/锁定），不产生生产风险。`metadata.json` 正确记录为 `in_progress`（11/18 tasks），Track 状态真实可信。

### 本轮剩余风险

与上轮一致，无新增风险：
- Auto exposure HDR pass 未实现，默认 `autoExposureEnabled=false`，零画面影响
- Validation evidence 未完整收集，默认 `dynamicResolutionEnabled=false` + `renderScaleLocked=true`，生产中 DRS 不会激活
- Phase 3/4 剩余任务按 P2 优先级后续完成

### 下一步动作

1. 确认本轮审查通过后提交当前变更。
2. 将 auto exposure (Phase 3) 放入 backlog，作为后续独立子 Track。
3. 积累硬件验证数据后，再评估 DRS 默认启用决策。
