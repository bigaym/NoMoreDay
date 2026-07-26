# GPU Adaptive Quality Control 规格说明书

> **Track ID**: `gpu_adaptive_quality_control_20260726`
> **类型**: P2 feature/quality
> **依赖**: `gpu_hardware_validation_gate_20260726`
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
> **状态**: 📋 Planned

---

## 1. 问题与目标

当前自动降级主要关闭功能，`RenderConfig` 没有 render scale/exposure 策略，旧 profiler 也不能可靠区分 GPU 样本。此 Track 仅在硬件 Gate 证明计时和资源可信后，增加带滞回的动态分辨率与可配置曝光。压力下优先连续降低 scene 工作分辨率，UI 保持原生清晰。

## 2. 范围与合同

### 动态分辨率

- render scale 只作用于 Gameplay world/HDR/GI/postprocess target 及其 viewport；HUD、菜单、overlay、文本、screen UI 在最终 scene 后保持 native resolution。
- Gameplay external scene target 与 RenderSystem internal HDR target 共享 descriptor 的 render/output extent，保持 world-to-screen、filter、鼠标/相机和离屏 Composite 正确。
- controller 仅消费 `Valid` GPU timing，使用 target budget、上下阈值、最短窗口、cooldown、恢复滞回。无有效样本、能力缺失或用户锁定时保持固定 scale。
- 压力顺序：先在 tier scale 范围内下调；持续超限且到下限才请求既有 feature degrade；恢复先经过滞回再逐步上调。

### 自动曝光

- 默认 `renderScale=1.0`、fixed exposure `1.0`，升级不改变现有画面。
- optional auto exposure 在 GI composite 后、tonemap 前从 HDR log-luminance histogram/reduction 求目标值，使用亮/暗不同适应速度、clamp、时间平滑。
- resize、tier/scale、GI history reset、feature fallback 受控重置 exposure history，避免闪白/闪黑。
- exposure 不采样 UI 或最终 LDR。

### 非目标

- 不以 DRS 掩盖 P0 正确性/预算失败，不改变 V5 Hard Gate 定义。
- 不实施 TAA、HZB、Vulkan、新 Fluid；无 Valid GPU timing 时不以 CPU 时间自适应。

## 3. 验收标准

- [ ] `1.0/fixed` 截图与硬件 Gate 基线一致。
- [ ] controller 只在 Valid GPU 样本持续超/低阈值后调节，稳定负载每分钟 scale 变化不超过 2 次。
- [ ] 最小 scale 前不关闭 feature；到下限才与既有 tier degrade 协作并可恢复。
- [ ] 0.70-1.00（由基准最终确认）的 scale、resize、GI/tier switch、离屏路径无黑帧、泄漏、坐标错位或 UI 缩放。
- [ ] auto exposure 符合 clamp/收敛/最大单帧变化 fixture 阈值，UI 颜色不变。
- [ ] 有效 GPU/资源/视觉/fallback 证据通过后才能默认启用，否则保留 fixed default。

## 4. 风险

- 缩 external scene target 影响 camera viewport/raylib origin；必须先扩 descriptor，禁止只缩 HDR。
- DRS 与 auto-degrade 同时动作会抖动；单一 controller 决定先后并记录原因。
- histogram 增加 GPU work/资源；必须进入 plan、预算、registry，默认关闭 auto exposure。
