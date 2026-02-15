# Project Tracks

This file tracks all major active tracks for the project.  
Completed tracks are archived in [./archive/tracks_archive.md](./archive/tracks_archive.md).

---

## [ ] Track: RenderGraph Contract Hardening (rendergraph_contract_hardening_20260215) [ACTIVE]

> **Goal**: Enforce render target ownership, pass resource contract checks, and stable pass boundaries.  
> **Docs**: [spec.md](./tracks/rendergraph_contract_hardening_20260215/spec.md) | [plan.md](./tracks/rendergraph_contract_hardening_20260215/plan.md)  
> **Status**: PENDING  
> **Priority**: P0

---

## [ ] Track: Tier Detection & Auto-Degrade (tier_detection_autodegrade_20260215) [ACTIVE]

> **Goal**: Add hardware capability probe and runtime budget-driven auto-degrade policies.  
> **Docs**: [spec.md](./tracks/tier_detection_autodegrade_20260215/spec.md) | [plan.md](./tracks/tier_detection_autodegrade_20260215/plan.md)  
> **Status**: PENDING  
> **Priority**: P1

---

## [ ] Track: GPU ABI & Binding Governance (gpu_abi_binding_governance_20260215) [ACTIVE]

> **Goal**: Establish ABI versioning/generation and binding collision governance.  
> **Docs**: [spec.md](./tracks/gpu_abi_binding_governance_20260215/spec.md) | [plan.md](./tracks/gpu_abi_binding_governance_20260215/plan.md)  
> **Status**: PENDING  
> **Priority**: P1

---

## [ ] Track: VFX Material Pipeline Completion (vfx_material_pipeline_completion_20260215) [ACTIVE]

> **Goal**: Complete missing VFX/material runtime wiring (MaterialSwap, distortion cap policy, lighting contract cleanup).  
> **Docs**: [spec.md](./tracks/vfx_material_pipeline_completion_20260215/spec.md) | [plan.md](./tracks/vfx_material_pipeline_completion_20260215/plan.md)  
> **Status**: PENDING  
> **Priority**: P2

---

## [x] Track: Particle and Trail Enhancement (particle_trail_enhancement_20260213) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Phase**: GPU Rendering System 2.0 - Phase 3

---

## [x] Track: Polishing and Advanced Features (polishing_advanced_features_20260213) [COMPLETED]

> **Status**: COMPLETED (Archived in `conductor/archive/`)  
> **Phase**: GPU Rendering System 2.0 - Phase 5

---

## Status Update (2026-02-15)

- rendergraph_contract_hardening_20260215: **ACTIVE**
  - mode: stability-first, strong compatibility.
- tier_detection_autodegrade_20260215: **ACTIVE**
  - mode: capability probe + runtime auto-degrade.
- gpu_abi_binding_governance_20260215: **ACTIVE**
  - mode: ABI contract and binding governance.
- vfx_material_pipeline_completion_20260215: **ACTIVE**
  - mode: close functional gaps without visual regressions.
- polishing_advanced_features_20260213: **COMPLETED & ARCHIVED**
  - delivered: Color Grading (LUT 16/32), Volumetric Light, Render Profiler HUD, Shader Hot Reload.
- particle_trail_enhancement_20260213: **COMPLETED & ARCHIVED**
  - delivered: Textured particles, GPU Trails, Sub-emitters, Force fields.
- material_vfx_sequencer_20260213: **COMPLETED & ARCHIVED**
  - delivered: material system, vfx sequencer, distortion pass.

