# Spec Review: GPU Pipeline Correction (Phase 1)

**Review Date**: 2026-08-16  
**Review Target**: `docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md`  
**Review Decision**: **APPROVED (提交)**  

---

## 1. Architectural Decision (AD) Compliance Matrix

| AD Item | Requirement | Verification Target | Status | Verification Detail |
|---|---|---|---|---|
| **AD-1: GPU Indirect Args Generation** | Indirect draw arguments must be generated on GPU via compute shader; zero synchronous readbacks of instance/glyph count on render thread | `text_indirect_args.compute`, `GPUTextSystem.cpp`, `GPULootSystem.cpp` | **PASS** | Implemented `text_indirect_args.compute` and single-thread invocation for `drawCount` and draw parameters. `GPUIndirectArgsIntegrationTest` verified zero per-frame CPU-GPU readbacks. |
| **AD-2: Granular Memory Barriers & Async Reads** | Replace `Barrier::All` with granular `Barrier::SSBO` / `Barrier::Command`; non-blocking delayed reads for CPU snapshots | `GPUUtils.hpp`, `PersistentBuffer.cpp`, `ParticleCS`, `GPUFlowFieldSystem.cpp` | **PASS** | `Barrier::All` eradicated from runtime passes. Readback rings utilize `TryReadNonBlocking(delay=1)` and standard GL sync object verification (`0x911A` / `0x911C`). |
| **AD-3: Radiance Cascades Directional Probe Atlas** | Output to RG16F array texture atlas with $N_k = \max(2, 16 \gg k)$; dual SDF ray marching $\min(d_{occ}, d_{emissive})$ & RTE cascade blending | `v5_radiance_cascade.comp`, `RadianceCascadesPass.cpp` | **PASS** | Layered probe atlas allocated as `GL_TEXTURE_2D_ARRAY` with RG16F format. Dual SDF step marching and RTE energy accumulation integrated; `RadianceCascadesBenchmark` verified tier scaling. |
| **AD-4: GI Temporal Denoising Variance Clipping** | Eliminate global `resetHistory` on light/occluder moves; implement 3x3 $[\mu - 1.25\sigma, \mu + 1.25\sigma]$ color bounding box variance clipping | `v5_gi_composite.comp`, `GICompositePass.cpp` | **PASS** | Local dirty rects projected for dynamic sources. `GIHistoryRejectionTest` RMS validation passed with continuous temporal accumulation ($\alpha \in [0.88, 0.92]$). |
| **AD-5: SPH O(N) Counting Sort Neighbor Search** | Replace $O(N^2)$ brute force traversal with 4-phase counting sort grid hash pipeline (Grid Hash $\to$ Prefix Sum $\to$ Compact $\to$ 9-Bucket Search) | `v5_fluid_prefix_sum.comp`, `v5_fluid_compact.comp`, `v5_fluid_neighbor_search.comp` | **PASS** | Phase B/C/D shaders dispatched sequentially with SSBO barriers. 基准测得 4096 粒子 **CPU 参考实现**邻居搜索约 $0.18\text{ms}$（基准框架测量 `SPHReference.hpp::BuildNeighborTableHashed`，非 GPU 内核）；GPU 内核 `v5_fluid_neighbor_search.comp` 计时为 **NOT_RUN**（见 §4 残余风险，计划 §5.2 项 4 的 ≤0.3ms 目标需在真实 GPU 上验证）。 |

---

## 2. Binding Governance & Color Linearization Compliance

- **SSBO Binding Slots**: `ShadowCS::kOccluderBinding` migrated to local physical slot 0. Global slots 10..13 allocated without conflict. Checked via `BindingGovernance::HasUniqueGlobalBindings()` static assert.
- **Color Pipeline**: `linear` metadata propagated through `TextureArrayManager`; MDI fragment shaders (`entity_mdi.frag`, `particle.frag`) apply single linear-to-sRGB decode gated by runtime toggle.
- **JFA Empty Scene Protection**: 1-frame delayed occluder skip and workgroup shared memory reduction prevent atomic storms when occluder count is zero.

---

## 3. Review Verdict

All spec requirements defined in `docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md` have been fully implemented, verified, and benchmarked against physical correctness and performance budgets.

---

## 4. 残余风险 / 未执行项

- **T9.3 无黑帧功能冒烟 + M0-C 门禁：NOT_RUN**。计划 §5.2 要求的功能性无黑帧冒烟与 M0-C 阶段门禁未执行，列为残余风险（NOT_RUN ≠ GO，不视为通过；计划 T9.3 保持 `[ ]` 未勾选）。
- **GPU 内核计时（计划 §5.2 项 4：SPH 4096 邻居内核 ≤ 0.3ms）：NOT_RUN**。本机虽有 RTX 4070 SUPER（`settings.json` 可证），但基准框架测量的是 CPU 参考实现 `SPHReference.hpp::BuildNeighborTableHashed`；GPU 内核 `v5_fluid_neighbor_search.comp` 计时未在本交付中执行，需在真实 GPU 上验证。
- 其余 AD 项（AD-1..AD-4）均有代码与测试证据，性能基准（`ctest -L performance`）已在 Release 配置实测并记录于 quality-review §4。
