# Validation - blade_ascendant_vfx_keystone_trigger_20260222

## Scope

- Track: `blade_ascendant_vfx_keystone_trigger_20260222`
- Goal: implement Keystone/Trigger/Synergy VFX differentiation with trigger-storm-safe caps and sampling.

## Implemented Artifacts

- `src/engine/render/GPUSkillEffectSystem.hpp`
  - Added trigger-frame control state (`m_triggerFrameCounts`, carry blend, dedupe keys).
  - Added helper interfaces for trigger cap resolution, trigger budget consumption, and duplicate culling.
- `src/engine/render/GPUSkillEffectSystem.cpp`
  - Added role-mask precedence in recipe selection (`Keystone > Trigger > Synergy > Base`) when recipe priority ties.
  - Added per-skill TriggerProc cap table and sampling/merge policy under high-frequency trigger storms.
  - Added cast-linked trigger dedupe (`castId + skill + quantized target`) to suppress duplicate feedback playback.
  - Added synergy-secondary attenuation for TriggerProc when only synergy role is active.
  - Kept render safety contract unchanged: no new compute dispatch in `VFXPass`.
- `assets/data/vfx/blade_ascendant_v3.json`
  - Added 3 skill-scoped KeystoneActivate templates (`skill6`, `skill8`, `skill9`).
  - Added Keystone sustained-state recipes (`BuffEnter`/`BuffExit`) to separate persistent vs burst feedback.
  - Added reusable Trigger/Synergy TriggerProc role templates.
- `scripts/check_blade_vfx_recipe.py`
  - Added Keystone/Trigger/Synergy coverage validation:
    - minimum skill-scoped keystone templates
    - required Keystone sustained recipe
    - required Trigger-role and Synergy-role TriggerProc recipes
- `tests/unit/SkillVfxEventContractTest.cpp`
  - Added recipe smoke checks for Keystone/Trigger/Synergy coverage.

## Evidence Checklist

- [x] KeystoneActivate feedback is distinct from base-form events.
- [x] TriggerProc remains readable on Low/Medium (fallback + sampled emission).
- [x] Trigger storm caps/sampling prevent runaway event amplification.
- [x] No compute work added to `VFXPass`; update-stage preparation contract preserved.

## Verification Commands

- `python scripts/check_blade_vfx_recipe.py --check` -> PASS
- `.\build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS
- `.\build.bat analyze` -> PASS (existing static-analysis warnings only)
- `ctest --test-dir build -C Release -L performance --output-on-failure` -> PASS

