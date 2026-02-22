# Blade Ascendant VFX Base Forms (3.1 - 3.9) - Implementation Plan

> **Track ID**: `blade_ascendant_vfx_base_forms_20260222`  
> **Depends On Spec**: `spec.md`  
> **Status**: [x] Completed

---

## Phase Overview

| Phase | Name | Core Deliverable | Status |
|---|---|---|---|
| **Phase 1** | Recipe rollout | Base-form recipes for skills 1-9 | [x] |
| **Phase 2** | Runtime tuning | Action parameter tuning and runtime behavior fixes | [x] |
| **Phase 3** | Tier fallback | Low/Medium readability fallback and cap alignment | [x] |
| **Phase 4** | Verify + sync | Build/test evidence and track sync | [x] |

---

## Phase 1: Recipe Rollout (4 Tasks)

- [x] Task 1.1: Added base-form recipe entries for skills 1-9 in `assets/data/vfx/blade_ascendant_v3.json`.
- [x] Task 1.2: Declared low-tier readability critical events per skill and enforced coverage checks in `scripts/check_blade_vfx_recipe.py`.
- [x] Task 1.3: Kept per-skill concurrency alignment via existing `kSkillCaps` + `ResolveSkillCap` path in `src/engine/render/GPUSkillEffectSystem.cpp`.
- [x] Task 1.4: Recorded expected output components in recipe actions (`Overlay`, `ParticleBurst`, `TrailStroke`, `DistortionPulse`) and validation notes.

## Phase 2: Runtime Implementation (9 Tasks)

- [x] Task 2.1: Implemented skill 3.1 base-form recipe (dash trail + impact ring).
- [x] Task 2.2: Implemented skill 3.2 base-form recipe (crescent edge + light glow).
- [x] Task 2.3: Implemented skill 3.3 base-form recipe (blade body + launch feedback).
- [x] Task 2.4: Implemented skill 3.4 base-form recipe (ward ring + rotating hit feedback).
- [x] Task 2.5: Implemented skill 3.5 base-form recipe (warning circle + blade rain burst).
- [x] Task 2.6: Implemented skill 3.6 base-form recipe (formation ring + boundary pulse).
- [x] Task 2.7: Implemented skill 3.7 base-form recipe (beam body + impact spark).
- [x] Task 2.8: Implemented skill 3.8 base-form recipe (spinning flight + return trail).
- [x] Task 2.9: Implemented skill 3.9 base-form recipe (edge glow + enter/exit dissolve feedback).

## Phase 3: Tier Fallback (4 Tasks)

- [x] Task 3.1: Aligned low-tier behavior by preserving critical readability events while reducing particle/trail pressure through fallback policy.
- [x] Task 3.2: Aligned medium-tier behavior with reduced but stable GPU effects and per-skill cap control.
- [x] Task 3.3: Synced fallback with `QualityTierManager` switches: `vfxSequenceDetail`, `distortionEnabled`, and `trailEnabled`.
- [x] Task 3.4: Added 9-skill fallback coverage guard in recipe validator and unit test smoke matrix.

## Phase 4: Verify and Sync (3 Tasks)

- [x] Task 4.1: Build passed with MSVC generator after cache reset (`build.bat clean-all`, then `build.bat`).
- [x] Task 4.2: CI tests passed (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`).
- [x] Task 4.3: Synced track docs and global track registry with completion evidence.

---
_Updated during feature implementation closeout._
