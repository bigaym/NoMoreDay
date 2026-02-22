# Blade Ascendant VFX Transmutation (Element Variants) - Implementation Plan

> **Track ID**: `blade_ascendant_vfx_transmutation_20260222`  
> **Depends On Spec**: `spec.md`  
> **Status**: [x] Completed

---

## Phase Overview

| Phase | Name | Core Deliverable | Status |
|---|---|---|---|
| **Phase 1** | Assets + palette | Procedural element/debuff textures and shader include integration | [x] |
| **Phase 2** | Event driven path | `TransmuterSwitch` + `elementType` flow connected to runtime rendering | [x] |
| **Phase 3** | Recipe expansion | Per-skill element variant recipes and fallback-compatible tuning | [x] |
| **Phase 4** | Verify + fallback | Build/test pass and tier fallback readability confirmation | [x] |

---

## Phase 1: Assets + Palette (5 Tasks)

- [x] Task 1.1: Extended `scripts/gen_blade_vfx_assets.py` to procedurally generate element textures (`fire/ice/lightning/void`) deterministically.
- [x] Task 1.2: Added procedural generation for new placeholders: `vfx_resist_crack.png`, `vfx_frost_spread.png`, `vfx_ember_trail.png`, `vfx_electric_arc.png`.
- [x] Task 1.3: Integrated `assets/shaders/vfx/vfx_element_switch.glslinc` into `sh_skill_effect.fs`.
- [x] Task 1.4: Added include-aware shader loading path in `GPUSkillEffectSystem` for local `#include` resolution.
- [x] Task 1.5: Recorded generated asset list and commands in `validation.md`.

## Phase 2: Event Driven (4 Tasks)

- [x] Task 2.1: Confirmed `SkillSystem` emits `TransmuterSwitch` with `elementType` (existing infra path).
- [x] Task 2.2: `GPUSkillEffectSystem` now encodes element to `GPUSkillEffect.flags` (low 4 bits) and shader reads it.
- [x] Task 2.3: Runtime color switching now uses shader-side palette selector (`NmdSelectElementPalette`).
- [x] Task 2.4: Build check passed (`build.bat`).

## Phase 3: Recipe Expansion (7 Tasks)

- [x] Task 3.1: Added per-skill element variants in `assets/data/vfx/blade_ascendant_v3.json` (skills 1-9 each include Fire + Cold variants).
- [x] Task 3.2: Element behavior differentiation layered as `Overlay + ParticleBurst + Distortion/Trail` action patterns.
- [x] Task 3.3: Implemented `elementType -> GPUSkillEffect.flags` encoding and shader-side switch.
- [x] Task 3.4: Palette linkage now consistently affects beam/trail/overlay paths through shared color source.
- [x] Task 3.5: Existing per-skill caps (`ResolveSkillCap` + `TrySubmitCapped`) remain active for variant entries.
- [x] Task 3.6: Conflict rule preserved: runtime uses `elementType` first, tags only as fallback.
- [x] Task 3.7: Validation scripts/tests expanded and passed (`check` + `ctest -L unit|ci`).

## Phase 4: Fallback + Verify (2 Tasks)

- [x] Task 4.1: Low/Medium fallback policy remains pass-free and readability-first (no new pass, no FBO0 bypass, no new global SSBO binding).
- [x] Task 4.2: Validation evidence captured in `validation.md` and global track registry synced.

---
_Updated during feature implementation closeout._
