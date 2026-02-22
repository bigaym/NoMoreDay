# Blade Ascendant VFX Design Freeze - Specification

> **Track ID**: `blade_ascendant_vfx_design_freeze_20260221`  
> **Status**: Pending

---

## 1. Goal

Freeze Blade Ascendant VFX design into an executable contract package for downstream tracks:

- `blade_ascendant_skill_rendering_integration_20260221` (T2)
- `blade_ascendant_skill_validation_gate_20260221` (T3)

## 2. Inputs and Outputs

### 2.1 Inputs

- `设计文档/职业设计草案_剑修.md`
- `设计文档/特效和UI/GPU_Rendering_Quick_Reference.md`
- Runtime integration surfaces:
  - `src/game/systems/skill/SkillSystem.cpp`
  - `src/engine/render/GPUSkillEffectSystem.cpp`
  - `src/engine/render/passes/VFXPass.cpp`
  - `src/engine/render/passes/DistortionPass.cpp`

### 2.2 Outputs (Frozen Package)

- Main spec document:
  - `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`
- Track artifacts:
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/spec.md`
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/plan.md`
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/validation.md`
- Evidence artifacts:
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md`
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`
  - `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/tier_fallback_matrix.md`

## 3. In Scope

- 9-skill VFX definitions: main/trigger/empowered feedback, concurrency caps, Low fallback.
- `SkillVfxEvent` data contract and lifecycle trigger points.
- RenderGraph ownership/resource contract and forbidden paths.
- Tier fallback sequence and budget thresholds.
- Explicit dependency handoff to T2/T3 specs/plans.

## 4. Out of Scope

- Shader/code implementation.
- Runtime ABI version migration implementation.
- Performance benchmark execution for runtime behavior changes.

## 5. Quality Gates

- Consistency: logic events and VFX trigger points map 1:1.
- Executability: no conflict with SSBO/FBO/pass-order constraints.
- Verifiability: each rule maps to evidence or test acceptance.
- Rollback-readability: Low/Medium still preserve skill readability.

## 6. Definition of Done

- [x] V2 package includes complete 9-skill entries with concurrency and Low fallback.
- [x] `SkillVfxEvent` contract and timing map are frozen.
- [x] RenderGraph forbidden paths (especially `FBO 0`) are explicit.
- [x] Budget thresholds and downgrade order are explicit.
- [x] Three evidence matrices are complete.
- [x] T2/T3 specs explicitly reference this frozen package.
- [x] All changed docs pass UTF-8 validation.
