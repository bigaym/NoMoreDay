# Blade Ascendant VFX Global Systems - Implementation Plan

> **Track ID**: `blade_ascendant_vfx_global_systems_20260222`  
> **Status**: [x] Completed

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| **Phase 1** | Sword Intent System | 0-10 progression + peak + consume feedback | [x] |
| **Phase 2** | Yujian Step | 3-stage state VFX | [x] |
| **Phase 3** | Resist 1-5 Types | Distinguishable overlay markers | [x] |
| **Phase 4** | Fallback + Verify | Tier/budget consistency and evidence | [x] |

---

## Phase 1: Sword Intent System (6 Tasks)

- [x] Task 1.1: Clarify Sword Intent VFX ownership and remove duplicate emission path.
- [x] Task 1.2: Implement 1-9 stack progressive feedback with lightweight persistent cues.
- [x] Task 1.3: Implement stack-10 peak state with quality-gated distortion and higher density.
- [x] Task 1.4: Implement one-shot consume feedback burst when stacks are spent.
- [x] Task 1.5: Add persistent-effect cap so Sword Intent does not starve primary skill feedback.
- [x] Task 1.6: Keep Low-tier fallback with minimal halo/iconic readability cue.

## Phase 2: Yujian Step (4 Tasks)

- [x] Task 2.1: Bind state events (`BuffEnter`/`BuffExit`) to recipe-driven VFX chain.
- [x] Task 2.2: Implement enter/sustain/exit visual stages.
- [x] Task 2.3: Coordinate with `TrailSystem`/GPU trail and apply fallback stride sampling.
- [x] Task 2.4: Keep pressure-scene VFX budget within pass envelope.

## Phase 3: Resist 1-5 Types (6 Tasks)

- [x] Task 3.1: Ensure `resistDebuffType` is consumed by runtime overlay rendering path.
- [x] Task 3.2: Define distinguishable visual lexicon by shape (not color-only).
- [x] Task 3.3: Reuse `VFXPass` overlay path; do not add new render pass.
- [x] Task 3.4: Keep readability under transmutation element palette.
- [x] Task 3.5: Keep Low-tier minimal marker fallback without expensive effects.
- [x] Task 3.6: Build and CI verification (`build.bat` + `ctest -L ci`).

## Phase 4: Fallback + Verify (2 Tasks)

- [x] Task 4.1: Validate global effects do not override primary combat readability priority.
- [x] Task 4.2: Record implementation and verification evidence in `validation.md`.

---
_Updated by Feature Developer._
