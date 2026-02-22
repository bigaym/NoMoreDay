# Blade Ascendant VFX Validation Gate (V3) - Implementation Plan

> **Track ID**: `blade_ascendant_vfx_validation_gate_20260222`  
> **Status**: [x] Completed (Conditional, non-blocking performance issue linked)

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| **Phase 1** | Functional Acceptance | §12.1 feature matrix coverage | [x] |
| **Phase 2** | Contract Acceptance | RenderGraph/FBO0/SSBO/VFXPass compute checks | [x] |
| **Phase 3** | Tier Fallback | Low/Medium readability and degrade sequence evidence | [x] |
| **Phase 4** | Budget and Stability | Build/test evidence + non-blocking performance linkage | [x] |

---

## Phase 1: Functional Acceptance (6 Tasks)

- [x] Task 1.1: Confirm Base Form visibility/distinguishability coverage.
- [x] Task 1.2: Confirm transmutation variant coverage per skill matrix.
- [x] Task 1.3: Confirm Keystone shape-change visuals and sustained-state split.
- [x] Task 1.4: Confirm Trigger/Empowered event-to-feedback alignment.
- [x] Task 1.5: Confirm resist debuff Type1-5 distinguishability.
- [x] Task 1.6: Confirm Sword Intent/Yujian Step global systems completeness.

## Phase 2: Contract Acceptance (4 Tasks)

- [x] Task 2.1: RenderGraph ownership contract checked (`VFXPass` read/write ownership).
- [x] Task 2.2: FBO0 write constraints reviewed and no new violation introduced by Blade VFX tracks.
- [x] Task 2.3: SSBO governance reviewed; no new global SSBO binding introduced by this rollout.
- [x] Task 2.4: No compute dispatch introduced in `VFXPass`.

## Phase 3: Tier Fallback (3 Tasks)

- [x] Task 3.1: Low/Medium readability maintained via fallback cue paths.
- [x] Task 3.2: AutoDegrade order remains active in runtime logs and systems.
- [x] Task 3.3: Fallback evidence captured in `validation.md`.

## Phase 4: Budget and Stability (3 Tasks)

- [x] Task 4.1: `build.bat`, `ctest -L ci/unit/integration` passed.
- [x] Task 4.2: Performance suite executed; known non-blocking failure recorded.
- [x] Task 4.3: Validation evidence and bug-registry linkage completed.

---
_Updated by Feature Developer._
