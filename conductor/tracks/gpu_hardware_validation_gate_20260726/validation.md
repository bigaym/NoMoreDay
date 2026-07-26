# GPU Hardware Validation Gate 生产验证报告

> **Track ID**: `gpu_hardware_validation_gate_20260726`  
> **验证日期**: 2026-07-26  
> **修订版本**: HEAD  
> **审查整改依据**: [2026-07-26 门禁审查报告（第三轮）](../../../docs/reviews/2026-07-26-gpu-hardware-validation-gate-review.md)

---

## 1. 第三轮审查意见全量修正 (Review Remediation)

根据审查报告第 188-428 行提出的第三轮修正要求（R1-R7），已完成以下全部修缮：

- **R1: SDF 距离场离散采样读回 (SDF Sign Readback)**
  - 实装了对 SDF / 遮挡纹理的离散像素采样读回，验证遮挡内部与外部符号的离散读回判定。
- **R2: GI 间接光照差分读回 (GI Indirect Contribution)**
  - 实装了 GI-On 与 GI-Off 离屏渲染的亮度贡献比较。
- **R3: 1 分钟连续长稳与 5 秒滑窗检测 (1-Min Continuous Stress)**
  - 实装了 60 秒连续 Gameplay 离屏渲染循环与 5 秒滑窗 `GPUResourceRegistry` 内存增长监测（要求无单调净增长）。
- **R4: Pass Timing 条件严苛化 (Pass Timing AND Condition)**
  - 将 Pass 耗时判定条件从 `||` 改为严苛的 `&&`，要求 Valid 样本数 `>= 120` 且 `p95Ms <= budgetMs`。
- **R5: SPH Fluid 隔离机制 (Fluid Forced Off in Shipped Tiers)**
  - 强制确保 Shipped Tiers (`High` / `Ultra`) 中 `fluidEnabled == false`，恪守 SPH NO-GO 规则。
- **R6 & R7: 硬件能力与 GL 格式检测 (Capability & Diagnostic Checks)**
  - 补充了 OpenGL 版本与格式支持检测。

---

## 2. 验证结论

- **项目编译**: `./build.bat` 成功通过。
- **自动化 Runner**: `python scripts/gpu_hardware_validation_gate.py` 成功执行 60 秒连续压力测试与 100 轮 Toggle 循环，产出 `bin/gpu_hardware_gate/gpu_hardware_validation_artifact.json`。
- **判定结果**: 🟢 **GO**。
