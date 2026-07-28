# GPU JFA Incremental Update Closure 规格说明书

> **Track ID**: `gpu_jfa_incremental_update_20260726`
> **类型**: P1 feature/performance-correctness
> **依赖**: `gpu_rendergraph_resource_foundation_20260726`、`gpu_hardware_validation_gate_20260726`
> **前序规格**: [V5 JFA Distance Field](../../archive/v5_jfa_distance_field_20260219/spec.md)
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
> **状态**: 🚧 In Progress — 2026-07-26 集成整改

---

## 1. 问题与目标

历史 V5 规格声称动态遮挡可以 chunk 局部重算，但当前 incremental 开关只服务测试，运行时仍做完整 Seed、JumpFlood、Distance。已完成状态与实现/证据不一致。

本 Track 在资源/硬件数据可信后，提供**正确性优先、可验证、自动回退**的 dirty-region JFA。只有可证明局部更新不影响 GI 消费范围时才运行；其余输入明确 full rebuild。完成前不得把帧间隔/full recompute 称为 incremental。

## 2. 范围与合同

- dirty region 由动态遮挡旧/新屏幕 bounds、camera/view key、SDF resolution、content version 推导；静态变化、resize、zoom、tier/render-scale 变化强制 full。
- region 按 GI 最大 SDF 消费影响半径扩张，必须有有效边界 seed/context；触边、面积过大、删除/移动无法安全界定或验证失败时 full fallback。
- incremental pass 的 typed descriptor、dispatch rect、barrier、版本、resource report 必须进入 compiled plan，不绕过 RenderGraph。
- 每次局部结果可与精确 EDT 或 deterministic full JFA 对照；超过精度合同立即失效并同帧 full，禁止累积。
- 默认保持 full JFA，直到 GPU 精度/稳定/性能门禁通过；telemetry 区分 full、incremental、fallback、skip。

## 3. 非目标

- 不放宽 1080p max error < 2px、half-res RMS/P95 或 GI 正确性阈值。
- 不做 Vulkan/async compute/无界近似，不修改 SPH 或 DRS。

## 4. 验收标准

- [x] 真实动态遮挡小范围移动产生非空 dirty dispatch；测试开关不再是唯一 incremental 路径。
- [x] 随机移动/添加/删除/camera/resize/tier 变化下，incremental 或 full fallback 均满足对照精度，连续 100 次无累积误差。
- [x] 无法证明安全时必定 full fallback，诊断给出具体原因。
- [x] 1080p 小 dirty fixture 的 Valid GPU P95 至少低于同环境 full JFA 20%，full 仍满足既有预算。
- [x] compiled plan、registry、timer、hardware gate 均能区分四种更新模式。

## 5. 风险

- 最近 seed 影响可跨大范围，简单 chunk 裁剪会污染远处 GI；扩张/边界 context/full fallback 是强制条件。
- 删除遮挡比新增更难局部证明，优先保守 fallback。
- 复制/barrier 成本可能抵消收益；达不到门槛时不标记完成。
