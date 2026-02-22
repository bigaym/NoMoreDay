# Blade Ascendant VFX Design Freeze - Plan

> **Track ID**: `blade_ascendant_vfx_design_freeze_20260221`

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| Phase 1 | Context Alignment | Constraints and interface mapping | [x] |
| Phase 2 | Contract Freeze | V2 spec + three evidence matrices | [x] |
| Phase 3 | Track Handoff | T2/T3 reference and dependency sync | [x] |
| Phase 4 | Freeze Validation | UTF-8 + consistency + validation proof | [x] |

---

## Phase 1: Context Alignment (4 Tasks)

- [x] Task 1.1: Extract 3.1-3.9 mechanics from `职业设计草案_剑修.md`.
- [x] Task 1.2: Extract hard constraints from `GPU_Rendering_Quick_Reference.md`.
- [x] Task 1.3: Map existing interfaces (`SkillSystem`, `GPUSkillEffectSystem`, `VFXPass`, `DistortionPass`).
- [x] Task 1.4: Draft `evidence/skill_vfx_matrix.md`.

## Phase 2: Contract Freeze (7 Tasks)

- [x] Task 2.1: Freeze `SkillVfxEvent` fields and trigger timing.
- [x] Task 2.2: Freeze 9-skill VFX entries (main + trigger/empowered).
- [x] Task 2.3: Freeze concurrency caps for all 9 skills.
- [x] Task 2.4: Freeze Low-tier fallback for all 9 skills.
- [x] Task 2.5: Freeze RenderGraph contract (owner/resource/forbidden paths).
- [x] Task 2.6: Freeze budget thresholds and downgrade sequence.
- [x] Task 2.7: Finalize `render_contract_matrix.md` and `tier_fallback_matrix.md`.

## Phase 3: Track Handoff (4 Tasks)

- [x] Task 3.1: Sync T2 spec references to frozen package.
- [x] Task 3.2: Sync T2 plan mapping to frozen package.
- [x] Task 3.3: Sync T3 spec validation baseline to frozen package.
- [x] Task 3.4: Sync `conductor/tracks.md` status/dependency counts.

## Phase 4: Freeze Validation (3 Tasks)

- [x] Task 4.1: Run UTF-8 checks on all changed track/design docs.
- [x] Task 4.2: Run consistency self-check (spec/evidence/handoff alignment).
- [x] Task 4.3: Record evidence and conclusion in `validation.md`.

---

## Required Deliverables

- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/spec.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/plan.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/validation.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`
- `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/tier_fallback_matrix.md`
