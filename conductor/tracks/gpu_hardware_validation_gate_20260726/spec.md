# GPU Hardware Validation Gate 规格说明书

> **Track ID**: `gpu_hardware_validation_gate_20260726`
> **类型**: P0 quality/release-gate
> **依赖**: `gpu_production_hdr_gi_closure_20260726`、`gpu_rendergraph_resource_foundation_20260726`
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
> **状态**: 🚧 In Progress — 2026-07-26 集成整改

---

## 1. 问题与目标

历史 V5 验收只执行部分 Radiance pass，GPU timer 可能把未就绪 query 记录为 `0ms`，显存检查仅覆盖 FBO 代理。它们不能证明 Gameplay 离屏完整链、所有 GPU 资源、长稳运行和自动降级的生产行为。

本 Track 建立物理 GPU nightly/release 门禁，消费前两条 Track 的 compiled plan、resource registry 和有效计时器，生成可复查 artifact 与 `GO`/`NO-GO` 结论。无头 unit/integration 是前置条件，不替代硬件结果。

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

## 3. 发布规则

- `GO` 要求所有 MUST PASS 项通过，artifact、硬件环境和残余风险写入 validation/release posture。
- `NO-GO` 在关键正确性、黑帧、泄漏、严重 GL 信息、无有效 GPU 性能数据或场景缺失时触发。
- 性能不达标不得用 CPU 时间或 2026-02 历史结果替代；只能 `NO-GO` 或由用户明确批准带范围/到期日的 waiver。
- SPH 始终 NO-GO，不属于生产功能；发现 shipped Tier 启用即为回退失败。

## 4. 验收标准

- [ ] harness 生成完整 pass trace、截图/ROI、SDF/history readback、timer、resource、GL diagnostics artifact。
- [ ] 三场景、High/Ultra、GI-off、resize、tier switch、capability fallback 均有 pass/fail/not-run 状态。
- [ ] 每个性能结论至少 120 个 Valid GPU 样本，Pending/Unavailable/CPU fallback 分离报告。
- [ ] 1 分钟和 100 次切换无黑帧、泄漏、崩溃或严重 GL debug 信息。
- [ ] release posture、progress、Track 状态仅在当前 artifact 支持时标记 production GO。

## 5. 风险

- CI 可能无显示 GPU；允许独立 runner，但运行环境和 artifact path 必须版本化。
- 截图会有 driver 细微差异；采用固定 seed/ROI/允许误差，而不是未校准全图像素比较。
- 驱动 VRAM extension 非通用；registry 覆盖率和释放检查是强制证据。
