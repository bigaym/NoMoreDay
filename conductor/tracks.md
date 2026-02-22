# Project Tracks

This file tracks all major active tracks for the project.  
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## Blade Ascendant Skill Refactor Tracks (2026-02-21)

```
T0: blade_ascendant_skill_contracts_20260221 (P0, Foundation)
  -> T1: blade_ascendant_skill_behaviors_20260221 (P0, Logic)
    -> T1.5: blade_ascendant_vfx_design_freeze_20260221 (P0, Design Freeze)
      -> T2: blade_ascendant_skill_rendering_integration_20260221 (P1, Integration)
        -> T3: blade_ascendant_skill_validation_gate_20260221 (P0, Gate)
```

## [x] Track T0: Blade Ascendant Skill Contracts Refactor (blade_ascendant_skill_contracts_20260221)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: refactor/foundation  
> **Focus**: 技能合同化数据层、节点角色/触发/互斥/范围声明落地，作为后续行为与渲染重构基线。  
> **Tasks**: 18/18  
> **Location**: [`conductor/archive/blade_ascendant_skill_contracts_20260221/`](./archive/blade_ascendant_skill_contracts_20260221/index.md)

## [x] Track T1: Blade Ascendant Skill Behaviors Refactor (blade_ascendant_skill_behaviors_20260221)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: refactor/feature  
> **Depends On**: `blade_ascendant_skill_contracts_20260221`  
> **Focus**: 9 技能行为重构为合同驱动执行，统一剑意/御剑步/Trigger 防环与元素互斥逻辑。  
> **Tasks**: 24/24  
> **Location**: [`conductor/archive/blade_ascendant_skill_behaviors_20260221/`](./archive/blade_ascendant_skill_behaviors_20260221/index.md)

## [x] Track T1.5: Blade Ascendant VFX Design Freeze (blade_ascendant_vfx_design_freeze_20260221)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: design/spec  
> **Depends On**: `blade_ascendant_skill_behaviors_20260221`  
> **Focus**: 冻结剑修技能特效设计输入，明确事件契约、RenderGraph 接入、Tier 回退与预算门限。  
> **Tasks**: 18/18  
> **Location**: [`conductor/archive/blade_ascendant_vfx_design_freeze_20260221/`](./archive/blade_ascendant_vfx_design_freeze_20260221/index.md)

## [x] Track T2: Blade Ascendant Skill Rendering Integration (blade_ascendant_skill_rendering_integration_20260221)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature/integration  
> **Depends On**: `blade_ascendant_vfx_design_freeze_20260221`  
> **Focus**: 新技能行为与 RenderGraph/VFX 管线对齐，保证 FBO 归属合同、SSBO 约束与 Low Tier 回退路径。  
> **Tasks**: 20/20  
> **Location**: [`conductor/archive/blade_ascendant_skill_rendering_integration_20260221/`](./archive/blade_ascendant_skill_rendering_integration_20260221/index.md)

## [x] Track T3: Blade Ascendant Skill Validation Gate (blade_ascendant_skill_validation_gate_20260221)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: quality  
> **Depends On**: `blade_ascendant_skill_rendering_integration_20260221`  
> **Focus**: 功能/合同/性能/稳定性/回退五维门禁与证据闭环。  
> **Tasks**: 16/16  
> **Location**: [`conductor/archive/blade_ascendant_skill_validation_gate_20260221/`](./archive/blade_ascendant_skill_validation_gate_20260221/index.md)

---

## Blade Ascendant VFX Refactor Tracks (2026-02-22)

```
TVFX-A: blade_ascendant_vfx_infrastructure_20260222 (P0, Foundation)
  -> TVFX-B: blade_ascendant_vfx_base_forms_20260222 (P1, Base Forms)
    -> TVFX-C: blade_ascendant_vfx_transmutation_20260222 (P1, Element Variants)
      -> TVFX-D: blade_ascendant_vfx_keystone_trigger_20260222 (P1, Keystone/Trigger)
  -> TVFX-E: blade_ascendant_vfx_global_systems_20260222 (P1, Global Systems)
    -> TVFX-F: blade_ascendant_vfx_validation_gate_20260222 (P0, Gate)
```

## [x] Track TVFX-A: Blade Ascendant VFX Infrastructure (blade_ascendant_vfx_infrastructure_20260222)

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: refactor/foundation  
> **Depends On**: `blade_ascendant_skill_rendering_integration_20260221`  
> **Focus**: 事件契约扩展 + 配方驱动骨架 + Tier/回退钩子（复用粒子/尾迹/扭曲管线，不新增全局 SSBO binding）。  
> **Tasks**: 22/22  
> **Location**: [`conductor/archive/blade_ascendant_vfx_infrastructure_20260222/`](./archive/blade_ascendant_vfx_infrastructure_20260222/index.md)

## [x] Track TVFX-B: Blade Ascendant VFX Base Forms (blade_ascendant_vfx_base_forms_20260222)

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Depends On**: `blade_ascendant_vfx_infrastructure_20260222`  
> **Focus**: 3.1-3.9 基础形态 VFX 完整落地 + Low/Medium 回退矩阵。  
> **Tasks**: 20/20  
> **Location**: [`conductor/archive/blade_ascendant_vfx_base_forms_20260222/`](./archive/blade_ascendant_vfx_base_forms_20260222/index.md)

## [x] Track TVFX-C: Blade Ascendant VFX Transmutation (blade_ascendant_vfx_transmutation_20260222)

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Depends On**: `blade_ascendant_vfx_base_forms_20260222`  
> **Focus**: 元素变转视觉差异（事件驱动切换 + 色板/纹理/行为联动），不新增 SSBO 结构体。  
> **Tasks**: 18/18  
> **Location**: [`conductor/archive/blade_ascendant_vfx_transmutation_20260222/`](./archive/blade_ascendant_vfx_transmutation_20260222/index.md)

## [ ] Track TVFX-D: Blade Ascendant VFX Keystone / Trigger / Synergy (blade_ascendant_vfx_keystone_trigger_20260222)

> **Status**: 🚧 Pending  
> **Priority**: P1  
> **Type**: feature  
> **Depends On**: `blade_ascendant_vfx_transmutation_20260222`  
> **Focus**: Keystone/Trigger/Synergy 的强判读反馈 + 高频触发场景 cap/采样策略。  
> **Tasks**: 0/16  
> **Location**: [`conductor/tracks/blade_ascendant_vfx_keystone_trigger_20260222/`](./tracks/blade_ascendant_vfx_keystone_trigger_20260222/index.md)

## [ ] Track TVFX-E: Blade Ascendant VFX Global Systems (blade_ascendant_vfx_global_systems_20260222)

> **Status**: 🚧 Pending  
> **Priority**: P1  
> **Type**: feature  
> **Depends On**: `blade_ascendant_vfx_infrastructure_20260222`  
> **Focus**: 剑意/御剑步/抗性削弱五型全局体系（可回退、可区分、预算可控）。  
> **Tasks**: 0/18  
> **Location**: [`conductor/tracks/blade_ascendant_vfx_global_systems_20260222/`](./tracks/blade_ascendant_vfx_global_systems_20260222/index.md)

## [ ] Track TVFX-F: Blade Ascendant VFX Validation Gate (blade_ascendant_vfx_validation_gate_20260222)

> **Status**: 🚧 Pending  
> **Priority**: P0  
> **Type**: quality  
> **Depends On**: `blade_ascendant_vfx_base_forms_20260222`, `blade_ascendant_vfx_transmutation_20260222`, `blade_ascendant_vfx_keystone_trigger_20260222`, `blade_ascendant_vfx_global_systems_20260222`  
> **Focus**: 功能/合同/Tier 回退/预算/稳定性 五维门禁与证据闭环（对齐 V3 验收清单）。  
> **Tasks**: 0/16  
> **Location**: [`conductor/tracks/blade_ascendant_vfx_validation_gate_20260222/`](./tracks/blade_ascendant_vfx_validation_gate_20260222/index.md)

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

## [x] Track: Render V3 Release Gate Perf Reliability (render_v3_release_gate_perf_reliability_20260218)

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: bugfix+quality  
> **Depends On**: `v3_validation_and_release_gate_20260215`, `render_v3_release_gate_strict_closeout_20260218`  
> **Focus**: close remaining Open bugs `BUG-20260218-001` and `BUG-20260218-004`, remove F4.3/F4.5/F4.6 waivers, and restore strict release-gate reliability.
> **Closeout Note**: 2026-02-19 三次连续 Release gate 均 `pass`，`WVR-20260218-F4.6-001` 已退役；仅保留 `F6.2` 截图 warning（V4 依赖项）。

---

## Status Update (2026-02-19)

### V3 Active Tracks (Total: 0 tasks)

| Track | Step | Phase/Tasks | Status |
|---|---|---|---|
| render_v3_release_gate_perf_reliability_20260218 | Step F carry-over | 4/4 phases, 15/15 tasks | COMPLETED |

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

---

## V4 渲染引擎升级 — Track 依赖图

> **主控规格书**: [`rendering_engine_v4_master_spec.md`](./specs/rendering_engine_v4_master_spec.md)  
> **设计文档**: [`GPU_Rendering_System_V4.md`](../设计文档/特效和UI/GPU_Rendering_System_V4.md)

```
Track 0: v4_preflight_v3_closure_20260219 (Week 0-1)
    │
    ├──→ Track 1: v4_gpu_text_rendering_20260219    ─┐
    │                                                 │ V4-A (Week 1-3, 可并行)
    ├──→ Track 2: v4_gpu_loot_rendering_20260219    ─┤
    │                                                 │
    │    ┌────────────────────────────────────────────┘
    │    ↓
    ├──→ Track 3: v4_pbr_material_pipeline_20260219  (V4-B, Week 3-6)
    │        │
    │        ↓
    ├──→ Track 4: v4_advanced_lighting_20260219      (V4-C, Week 6-9)
    │        │
    │        ↓
    └──→ Track 5: v4_validation_release_gate_20260219 (Week 9-11)
```

---

## [~] Track 0: V4 Pre-flight — V3 Debt Closure (v4_preflight_v3_closure_20260219) [IN PROGRESS]

> **Status**: 🚧 In Progress  
> **Priority**: P0  
> **Type**: quality/chore  
> **Phase**: V4 Pre-flight (Week 0-1)  
> **Focus**: 闭环 V3 遗留依赖（DEP-V3-F4.6 性能豁免, DEP-V3-F6.2 截图基线），确认 V3 风险项，为 V4 开工亮绿灯。  
> **Tasks**: 7/8  
> **Location**: [`conductor/tracks/v4_preflight_v3_closure_20260219/`](./tracks/v4_preflight_v3_closure_20260219/index.md)

---

## [~] Track 1: V4 GPU Text Rendering — MSDF (v4_gpu_text_rendering_20260219) [IN PROGRESS]

> **Status**: 🚧 In Progress  
> **Priority**: P0  
> **Type**: feature  
> **Phase**: V4-A (Week 1-3)  
> **Depends On**: `v4_preflight_v3_closure_20260219`  
> **Focus**: MSDF 字体图集 + Compute Shader 排版 + MDI 绘制 + GPU 文字动画（飘字/重力/暴击放大），消灭 CPU 文字渲染瓶颈。  
> **Tasks**: 21/21  
> **Location**: [`conductor/tracks/v4_gpu_text_rendering_20260219/`](./tracks/v4_gpu_text_rendering_20260219/index.md)

---

## [x] Track 2: V4 GPU Loot Rendering (v4_gpu_loot_rendering_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: feature  
> **Phase**: V4-A (Week 1-3)  
> **Depends On**: `v4_preflight_v3_closure_20260219`  
> **Focus**: MDI 自动合批 + FrustumCull + GPU 力导向标签避让（GridHash → Repulsion → PositionUpdate），支持同屏 1000+ 战利品。  
> **Tasks**: 18/18  
> **Location**: [`conductor/archive/v4_gpu_loot_rendering_20260219/`](./archive/v4_gpu_loot_rendering_20260219/index.md)

---

## [x] Track 3: V4 2D PBR Material Pipeline (v4_pbr_material_pipeline_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Phase**: V4-B (Week 3-6)  
> **Depends On**: `v4_preflight_v3_closure_20260219`, `v4_gpu_text_rendering_20260219`, `v4_gpu_loot_rendering_20260219`  
> **Focus**: Material Schema V3 (GPUMaterialDataV3 128B) + 四层贴图规范 + BRDF-Lite (GGX/Schlick-GGX/Schlick Fresnel) + 美术资产工具链，实现"同光异材"。  
> **Tasks**: 25/25  
> **Location**: [`conductor/archive/v4_pbr_material_pipeline_20260219/`](./archive/v4_pbr_material_pipeline_20260219/index.md)

---

## [x] Track 4: V4 Advanced Lighting (v4_advanced_lighting_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P1  
> **Type**: feature  
> **Phase**: V4-C (Week 6-9)  
> **Depends On**: `v4_pbr_material_pipeline_20260219`  
> **Focus**: Clustered Forward+ V4 (4096 光源 + Area/Line Light + 球体/锥体精筛) + HeightShadowPass (64-step Raymarching + Self-Shadow) + POM (Ultra 专属)。  
> **Tasks**: 28/28  
> **Location**: [`conductor/archive/v4_advanced_lighting_20260219/`](./archive/v4_advanced_lighting_20260219/index.md)

---

## [x] Track 5: V4 Validation & Release Gate (v4_validation_release_gate_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: quality  
> **Phase**: V4 Gate (Week 9-11)  
> **Depends On**: ALL V4 feature tracks  
> **Focus**: 5 维度门禁（功能/性能/契约/稳定性/回退），V4→V3 回退验证，V4 发布判定。  
> **Tasks**: 30/30  
> **Location**: [`conductor/archive/v4_validation_release_gate_20260219/`](./archive/v4_validation_release_gate_20260219/index.md)

---

## V5 渲染引擎 — 次世代 GI — Track 依赖图

> **主控规格书**: [`rendering_engine_v5_master_spec.md`](./specs/rendering_engine_v5_master_spec.md)  
> **设计文档**: [`GPU_Rendering_System_V5.md`](../设计文档/特效和UI/GPU_Rendering_System_V5.md)
> **2026-02-19 全面回顾与重规划**: [`v5_rendering_review_20260219.md`](./tracks/v5_rendering_review_20260219.md)

```
V4 验收完成 (v4_validation_release_gate_20260219)
    │
    ├──→ Track 6: v5_jfa_distance_field_20260219     (V5-A, Week 0-3)
    │        │
    │        ↓
    ├──→ Track 7: v5_radiance_cascades_gi_20260219   (V5-A/B, Week 3-8)
    │
    ├──→ Track 8: v5_sph_fluid_exploration_20260219  (V5-B, Week 5-8, ⚠️探索性)
    │
    └──→ Track 9: v5_validation_release_gate_20260219 (V5 Gate, Week 8-10)
```

---

## [x] Track 6: V5 JFA Distance Field (v5_jfa_distance_field_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: feature  
> **Phase**: V5-A (Week 0-3 after V4)  
> **Depends On**: `v4_validation_release_gate_20260219`  
> **Early-Start Exception**: `v4_pbr_material_pipeline_20260219`（含 Emission 通道）完成后，可提前启动 JFA 预研分支  
> **Focus**: Jump Flood Algorithm O(log N) 距离场生成 — OccluderExtract + Seed Init + JFA 传播 + JFA+1 补偿 + DistanceCS，为 GI 提供空间加速结构。  
> **Tasks**: 22/22  
> **Location**: [`conductor/archive/v5_jfa_distance_field_20260219/`](./archive/v5_jfa_distance_field_20260219/index.md)
> **Closeout Note**: `ctest -C Release -L performance` fail is non-blocking and linked to existing `BUG-20260219-004` (ParticleTrail Scenario 4 threshold).

---

## [x] Track 7: V5 Radiance Cascades GI (v5_radiance_cascades_gi_20260219) [COMPLETED]

> **Status**: ✅ Completed (Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: feature  
> **Phase**: V5-A/B (Week 3-8 after V4)  
> **Depends On**: `v5_jfa_distance_field_20260219`  
> **Focus**: 辐射级联全局光照核心 — Emissive Buffer + 6 级联射线追踪 + SDF 加速 + GI Composite + 时域稳定 + Holographic RC 探索。  
> **Tasks**: 35/35  
> **Location**: [`conductor/archive/v5_radiance_cascades_gi_20260219/`](./archive/v5_radiance_cascades_gi_20260219/index.md)
> **Latest Validation**: `.\build.bat` / `.\build.bat analyze` / `ctest -C RelWithDebInfo -L unit|integration|ci` / `ctest -C Release -L performance` all PASS.

---

## [x] Track 8: V5 SPH Fluid Simulation — Exploration (v5_sph_fluid_exploration_20260219) [COMPLETED]

> **Status**: ✅ Completed (NO-GO, Archived in `conductor/archive/`)  
> **Priority**: P2 ⚠️ 探索性质，不阻断 V5 核心交付  
> **Type**: feature (exploration)  
> **Phase**: V5-B (Week 5-8 after V4)  
> **Depends On**: `v5_jfa_distance_field_20260219`  
> **Focus**: GPU SPH 流体模拟（血液/水面/岩浆）— NeighborSearch + Density + Force + Leapfrog + Render + GI 交互。含 GO/NO-GO 决策点。  
> **Tasks**: 18/18  
> **Location**: [`conductor/archive/v5_sph_fluid_exploration_20260219/`](./archive/v5_sph_fluid_exploration_20260219/index.md)
> **Closeout Note**: Exploration baseline `fluid_reference_10k_mean_ms=1.4305` (`target=0.80ms`) not met; decision = `NO-GO`.

---
.
## [x] Track 9: V5 Validation & Release Gate (v5_validation_release_gate_20260219) [COMPLETED]

> **Status**: ✅ Completed (Core GO, SPH NO-GO, Archived in `conductor/archive/`)  
> **Priority**: P0  
> **Type**: quality  
> **Phase**: V5 Gate (Week 8-10 after V4)  
> **Depends On**: `v5_jfa_distance_field_20260219`, `v5_radiance_cascades_gi_20260219`  
> **Optional Input**: `v5_sph_fluid_exploration_20260219` 仅作为 GO/NO-GO 决策输入，不阻断 V5 核心发布门禁  
> **Focus**: 核心交付门禁（JFA 精度 + GI 间接光照 + 时域稳定 + 性能 + 契约 + 稳定性 + 回退）+ 可选 SPH 决策 + 架构前瞻评估。  
> **Tasks**: 31/31  
> **Location**: [`conductor/archive/v5_validation_release_gate_20260219/`](./archive/v5_validation_release_gate_20260219/index.md)
> **Release Decision**: `GO (V5 core GI released)`; optional SPH branch stays `NO-GO`.

---

## V4/V5 Active Tracks Summary (2026-02-21)

| Track | Phase | Tasks | Priority | Status |
|---|---|---:|:---:|---|
| v4_preflight_v3_closure_20260219 | V4 Pre-flight | 7/8 | P0 | 🚧 In Progress |
| v4_gpu_text_rendering_20260219 | V4-A | 21/21 | P0 | 🚧 In Progress |
| v4_gpu_loot_rendering_20260219 | V4-A | 18/18 | P0 | ✅ Completed |
| v4_pbr_material_pipeline_20260219 | V4-B | 25/25 | P1 | ✅ Completed |
| v4_advanced_lighting_20260219 | V4-C | 28/28 | P1 | ✅ Completed |
| v4_validation_release_gate_20260219 | V4 Gate | 30/30 | P0 | ✅ Completed |
| v5_jfa_distance_field_20260219 | V5-A | 22/22 | P0 | ✅ Completed |
| v5_radiance_cascades_gi_20260219 | V5-A/B | 35/35 | P0 | ✅ Completed |
| v5_sph_fluid_exploration_20260219 | V5-B | 18/18 | P2 | ✅ Completed (NO-GO) |
| v5_validation_release_gate_20260219 | V5 Gate | 31/31 | P0 | ✅ Completed |
| **合计** | — | **235/236** | — | — |
