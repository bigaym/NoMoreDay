# GPU Hardware Validation Gate 发布姿态报告

> **Track ID**: `gpu_hardware_validation_gate_20260726`  
> **更新日期**: 2026-08-02
> **姿态结论**: 🔴 **NO-GO**（W6 生产门禁机制已落地，实机 `gpu-hardware` job 证据待采集）

---

## 判定汇总

| 检查维度 | 门禁标准 | 判定结果 | 豁免 (Waiver) |
| --- | --- | :---: | :---: |
| 1. 硬件 Capability Preflight | GL 4.3+ Core / Compute / SSBO / Timer Queries / RGBA16F | 未复验（本地最小样本已验证机制，非实机矩阵） | 0 |
| 2. 真实离屏 Gameplay 完整链 | 固定 GameplayState fixture -> RenderSystem -> Composite | 未复验（`NoMoreDay.exe --gpu-gate` 机制已落地，实机矩阵待采集） | 0 |
| 3. GI / SDF 正确性 Readback | 真实资源的 GI 差分、SDF/occupancy 与 ray-stop probes | 未复验 | 0 |
| 4. GPU 计时与 Pass Budget | 由 compiled plan stable ID 产生的 >= 120 个不重复 Valid 样本 | 未复验（本地最小样本非 exhaustive） | 0 |
| 5. 1 分钟长稳与 100 轮 Toggle | 5 秒窗口 registry snapshot 无单调净增长与 GL diagnostics | 未复验（机制已落地，实机矩阵待采集） | 0 |
| 6. SPH 隔离机制 | Shipped Tiers (High/Ultra) fluidEnabled == false | 未复验 | 0 |

---

## 结论

本文件中的先前 `GO` 已被 [生产整改 Track 集成审查](../../../docs/reviews/2026-07-26-gpu-production-remediation-tracks-review.md) 否决。W6（2026-08-02）已将生产门禁机制落地为 `NoMoreDay.exe --gpu-gate`（正常 Game/App 初始化后的真实矩阵），并把 standalone 测试二进制硬件矩阵/S7b 重分类为 contract/diagnostic（不产生生产 GO）。但 release 仍为 🔴 **NO-GO**：所有 MUST PASS 项须由实机 `gpu-hardware` job 归档的目标 GPU artifact 重新证明（真实 GPU identity、High/Ultra/GI-off/resize/tier/capability 矩阵、每 pass >=120 个不同帧有效样本、零 high-severity GL、60s 压力 5s 窗口无净增长、100 次切换、可复现 artifact），实机 GO 由后续 M0-C 流程判定。
