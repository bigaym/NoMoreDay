# GPU Adaptive Quality Control 实施计划

> **Track ID**: `gpu_adaptive_quality_control_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [ ] Planned

---

## 实施思路/原理

DRS 建立在 target descriptor/query ring 上。Gameplay world external target 与 HDR resource 同时按 scale 分配，Composite 拉伸到 native output；window 后 UI 保持 direct-to-screen。controller 读取 Valid GPU P95/window，以滞回/cooldown 确保调整远慢于采样。曝光从 HDR 而非 UI/tonemapped LDR 统计；其 histogram/reduction 是有 barrier/预算/生命周期的 RenderGraph pass。

## 伪代码引导

```text
UpdateScale(validGpuWindow):
  if userLocked or !validGpuWindow.ready: return KeepCurrentScale()
  if p95 > downThreshold for downWindow and cooldownExpired: return DecreaseWithinTier()
  if p95 < upThreshold for upWindow and cooldownExpired: return IncreaseWithinTier()
  if p95 > downThreshold and scale == minScale: return RequestExistingFeatureDegrade()

UpdateExposure(hdr):
  histogram = BuildLogLuminanceHistogram(hdr)
  target = Clamp(ExposureForPercentile(histogram), min, max)
  return Adapt(previous, target, brightenRate, darkenRate, dt)
```

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 1 | 配置与 target 合同 | scale/exposure config、离屏 extent 路由 | [ ] |
| 2 | DRS controller | Valid GPU、滞回、tier 协作 | [ ] |
| 3 | Auto exposure | HDR 统计、适应、tonemap 参数 | [ ] |
| 4 | 验证与启用 | 性能、视觉、稳定、默认策略 | [ ] |

## 原子任务拆分

### Phase 1: 配置与 target 合同

- [ ] Task 1.1: 定义 render scale、锁定、controller 阈值、fixed/auto exposure 默认配置与持久化。
- [ ] Task 1.2: 扩 external/HDR descriptor，明确 render/output extent、camera viewport、native UI 边界。
- [ ] Task 1.3: 实现 scale resize/recreate，验证 external scene、HDR、GI、postprocess、Composite 坐标/生命周期。
- [ ] Task 1.4: 添加 `1.0/fixed` 兼容基线测试。

### Phase 2: DRS controller

- [ ] Task 2.1: 从 query ring 读取 Valid GPU window，禁止 Pending/CPU fallback。
- [ ] Task 2.2: 实现阈值、连续窗口、cooldown、滞回、用户锁定。
- [ ] Task 2.3: 定义 scale 下限的 feature degrade 请求和恢复顺序，记录决策原因。
- [ ] Task 2.4: tier/resize/GI toggle 受控重置 controller，避免多源并发。
- [ ] Task 2.5: profiler/report 显示 scale、sample state、reason、target extent。

### Phase 3: Auto exposure

- [ ] Task 3.1: 定义 HDR histogram/reduction resource、pass、barrier、预算、registry owner。
- [ ] Task 3.2: 实现 percentile target、clamp、明暗适应速度、reset。
- [ ] Task 3.3: 在 tonemap 前绑定 exposure，确保 UI/final LDR 不参与统计。
- [ ] Task 3.4: debug 固定值/直方图、capability fallback，默认 fixed。

### Phase 4: 验证与启用

- [ ] Task 4.1: controller unit tests 覆盖无数据、阈值、滞回、cooldown、锁定、下限。
- [ ] Task 4.2: offscreen Gameplay integration tests 覆盖 scale/resize/tier/GI/native UI 坐标。
- [ ] Task 4.3: 亮暗 fixture 验证 clamp、收敛、最大单帧变化与 UI 截图不变。
- [ ] Task 4.4: 目标 GPU 采集 Valid P95、oscillation、资源、黑帧 artifact。
- [ ] Task 4.5: 全 gate 通过后选择默认启用；否则记录 fixed-default。

## 测试方法

| 层级 | 覆盖内容 | 命令/证据 |
| --- | --- | --- |
| Unit | controller、exposure math、extent contract | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| Integration | offscreen scale、GI/resize/tier、native UI、lifecycle | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| Performance | DRS/exposure pass 预算 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| Hardware | 压力 scale、恢复、亮暗曝光、资源/黑帧 | 扩展 hardware Gate artifact |

## 验证任务完成

- [ ] 默认固定配置和生产基线视觉一致。
- [ ] DRS 仅依据 Valid GPU 数据，稳定无抖动，压力按既定顺序降级。
- [ ] scale/exposure 切换维持 native UI、离屏输出、资源稳定。
- [ ] auto exposure 有 HDR-only 行为、clamp、fallback 证据。
- [ ] artifact 支持默认启用或明确维持关闭结论。
