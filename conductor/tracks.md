# Project Tracks

This file tracks all major active tracks for the project.  
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## V3 渲染系统升级 — Track 依赖图

```
Step A: v3_baseline_contracts_20260216 (第1周)
    │
    ├─── Step B: v3_shadow_pipeline_20260215 (第2-4周)
    │       │
    ├─── Step C: v3_clustered_lighting_20260215 (第3-5周)
    │       │
    │       ▼
    ├─── Step D: v3_material_lighting_depth_20260215 (第4-6周)
    │       │    (depends on: baseline + shadow)
    │       │
    │       ▼
    ├─── Step E: v3_vfx_lighting_integration_20260215 (第6-8周)
    │            (depends on: baseline + shadow + clustered + material)
    │
    ▼
Step F: v3_validation_and_release_gate_20260215 (第8-10周)
         (depends on: ALL feature tracks)
```

---

## [x] Track: V3 Baseline Contracts (v3_baseline_contracts_20260216) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: foundation  
> **Step**: A (第 1 周)  
> **Focus**: RenderConfig V3 扩展、ABI V3 upgrade、Pass 顺序锁定、Binding 治理、Frame Ownership 合同、Feature Flag 基础设施。所有后续 V3 Track 的前置依赖。  
> **Tasks**: 20/20

---

## [x] Track: V3 Shadow Pipeline (v3_shadow_pipeline_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: feature  
> **Step**: B (第 2-4 周)  
> **Depends On**: `v3_baseline_contracts_20260216`  
> **Focus**: 2.5D Hybrid Shadow 系统，含 SDF + Atlas 分档、chunk 缓存、确定性淘汰与回退。  
> **Tasks**: 27/27

---

## [x] Track: V3 Clustered 2D Lighting (v3_clustered_lighting_20260215) [COMPLETED]

> **Status**: COMPLETED (Will be archived to `conductor/archive/`)  
> **Priority**: P0  
> **Type**: feature  
> **Step**: C (第 3-5 周)  
> **Depends On**: `v3_baseline_contracts_20260216`  
> **Focus**: Compute-driven light culling + z-layer mapping; no-regression gate enabled, uplift recovery delegated to Step F release gate.  
> **Tasks**: 25/25

---

## [x] Track: V3 Material Lighting Depth (v3_material_lighting_depth_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Step**: D (第 4-6 周)  
> **Depends On**: `v3_baseline_contracts_20260216`, `v3_shadow_pipeline_20260215`, `v3_clustered_lighting_20260215`  
> **Carry-Over Migration**: strict clustered 128-light `>=5%` uplift gate is moved to `v3_validation_and_release_gate_20260215` (`F4.6`).  
> **Carry-Over**: clustered+material coupling optimization and evidence are tracked in this track; release uplift gate moved to Step F.
> **Focus**: Material 2.0 schema、BRDF-lite shader、Texture2DArray 管理、双缓冲热重载。  
> **Tasks**: 30/30

---

## [x] Track: V3 VFX Lighting Integration (v3_vfx_lighting_integration_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Step**: E (第 6-8 周)  
> **Depends On**: `v3_baseline_contracts_20260216` + Shadow + Clustered + Material  
> **Focus**: VFX schema v3、3 类新事件、tierPolicy、预算估计器、预览工具、12 个模板序列。  
> **Tasks**: 33/33

---

## [x] Track: V3 Validation and Release Gate (v3_validation_and_release_gate_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: quality  
> **Step**: F (第 8-10 周)  
> **Depends On**: ALL V3 feature tracks  
> **Carry-Over Intake**: owns `v3_material_lighting_depth_20260215` D0.3 (`F4.6`).
> **Focus**: 4 层门禁（功能/契约/性能/稳定性）、截图差异回归、30 分钟压力测试、风险验证、回退演练。  
> **Tasks**: 37/37  
> **Closeout Note**: `F4.6` under temporary waiver (`WVR-20260218-F4.6-001`); `F6.2` screenshot strict gating carried to V4 preflight dependency checks (`GPU_Rendering_System_V4.md` §1.4).

---

## [x] Track: Render Risk Closure and MSVC Hard Cutover (render-risk-msvc-hardening_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: bug+refactor  
> **Focus**: Close test/analysis-backed render risks and enforce strict MSVC-only toolchain policy.

---

## [x] Track: Tier Detection & Auto-Degrade (tier_detection_autodegrade_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1

---

## [x] Track: VFX Material Pipeline Completion (vfx_material_pipeline_completion_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P2

---

## [x] Track: RenderGraph Contract Hardening (rendergraph_contract_hardening_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0

---

## [x] Track: GPU ABI & Binding Governance (gpu_abi_binding_governance_20260215) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1

---

## [x] Track: Particle and Trail Enhancement (particle_trail_enhancement_20260213) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Phase**: GPU Rendering System 2.0 - Phase 3

---

## [x] Track: Polishing and Advanced Features (polishing_advanced_features_20260213) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Phase**: GPU Rendering System 2.0 - Phase 5

---

## [x] Track: Render V3 Clustered Shader Hardening (render_v3_clustered_shader_hardening_20260218)

> **Status**: COMPLETED (待归档到 `conductor/archive/`)  
> **Priority**: P0  
> **Type**: bugfix  
> **Depends On**: `v3_clustered_lighting_20260215`, `v3_validation_and_release_gate_20260215`  
> **Focus**: recover clustered compute shader compile/execution path, restore integration stability, and keep deterministic fallback contract.

---

## [x] Track: Render V3 Material Phase Shift GPU Sync (render_v3_material_phase_shift_gpu_sync_20260218)

> **Status**: COMPLETED (待归档到 `conductor/archive/`)  
> **Priority**: P0  
> **Type**: bugfix  
> **Depends On**: `v3_material_lighting_depth_20260215`, `v3_vfx_lighting_integration_20260215`  
> **Focus**: ensure `MaterialPhaseShift` runtime events actually propagate to GPU material payload and restore baseline deterministically.

---

## [x] Track: Render V3 Release Gate Strict Closeout (render_v3_release_gate_strict_closeout_20260218)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: quality  
> **Depends On**: `render_v3_clustered_shader_hardening_20260218`, `render_v3_material_phase_shift_gpu_sync_20260218`, `v3_validation_and_release_gate_20260215`  
> **Focus**: close Step F strict-gate debt, synchronize waiver/bug status with evidence, and produce current release posture.

---

## Status Update (2026-02-18)

### V3 Active Tracks (Total: 0 tasks)

| Track | Step | Phase/Tasks | Status |
|---|---|---|---|
| (none) | - | - | - |

### Archived Tracks

- v3_clustered_lighting_20260215: **COMPLETED & ARCHIVED**
- v3_baseline_contracts_20260216: **COMPLETED & ARCHIVED**
- v3_shadow_pipeline_20260215: **COMPLETED & ARCHIVED**
- v3_material_lighting_depth_20260215: **COMPLETED & ARCHIVED**
- v3_vfx_lighting_integration_20260215: **COMPLETED & ARCHIVED**
- v3_validation_and_release_gate_20260215: **COMPLETED & ARCHIVED**
- render-risk-msvc-hardening_20260215: **COMPLETED & ARCHIVED**
- render_v3_release_gate_strict_closeout_20260218: **COMPLETED & ARCHIVED**
- render_v3_material_phase_shift_gpu_sync_20260218: **COMPLETED & ARCHIVED**
- render_v3_clustered_shader_hardening_20260218: **COMPLETED & ARCHIVED**
- rendergraph_contract_hardening_20260215: **COMPLETED & ARCHIVED**
- gpu_abi_binding_governance_20260215: **COMPLETED & ARCHIVED**
- tier_detection_autodegrade_20260215: **COMPLETED & ARCHIVED**
- vfx_material_pipeline_completion_20260215: **COMPLETED & ARCHIVED**
- polishing_advanced_features_20260213: **COMPLETED & ARCHIVED**
- particle_trail_enhancement_20260213: **COMPLETED & ARCHIVED**
- material_vfx_sequencer_20260213: **COMPLETED & ARCHIVED**
