# Blade Ascendant Skill Rendering Integration - Plan

> **Track ID**: `blade_ascendant_skill_rendering_integration_20260221`  
> **Frozen Inputs**: `BladeAscendant_SkillVFX_Design_v2.md` + T1.5 evidence matrices

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| Phase 1 | Event Contract Wiring | `SkillVfxEvent` end-to-end delivery | [ ] |
| Phase 2 | 9-Skill VFX Implementation | Main + trigger/empowered feedback paths | [ ] |
| Phase 3 | RenderGraph Compliance | owner/resource/FBO ownership closure | [ ] |
| Phase 4 | Tier and Budget Validation | fallback order + budget evidence | [ ] |

---

## Phase 1: Event Contract Wiring

- [ ] Task 1.1: Emit `SkillVfxEvent` at required skill lifecycle points
- [ ] Task 1.2: Consume and stage events in `GPUSkillEffectSystem`
- [ ] Task 1.3: Wire `EmpoweredConsume/Buff*` events for sword-intent visuals
- [ ] Task 1.4: Add silent downgrade behavior for missing/noncritical events

## Phase 2: 9-Skill VFX Implementation

- [ ] Task 2.1: Implement 3.1 / 3.2 using frozen matrix definitions
- [ ] Task 2.2: Implement 3.3 / 3.4 using frozen matrix definitions
- [ ] Task 2.3: Implement 3.5 / 3.6 using frozen matrix definitions
- [ ] Task 2.4: Implement 3.7 / 3.8 / 3.9 using frozen matrix definitions
- [ ] Task 2.5: Enforce per-skill concurrency caps from frozen matrix

## Phase 3: RenderGraph Compliance

- [ ] Task 3.1: Validate `VFXPass` read/write/owner mapping
- [ ] Task 3.2: Validate `DistortionPass` read/write/owner mapping
- [ ] Task 3.3: Verify no skill path writes to `FBO 0` directly
- [ ] Task 3.4: Verify resize rebuild path for VFX resources

## Phase 4: Tier and Budget Validation

- [ ] Task 4.1: Implement Low/Medium fallback behavior
- [ ] Task 4.2: Collect stress budget samples (`VFXPass`, `DistortionPass`)
- [ ] Task 4.3: Capture evidence for fallback and budget acceptance
- [ ] Task 4.4: Update track validation with proof and residual risks
