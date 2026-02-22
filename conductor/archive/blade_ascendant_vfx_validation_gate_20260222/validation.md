# Validation - blade_ascendant_vfx_validation_gate_20260222

## Scope

- Track: `blade_ascendant_vfx_validation_gate_20260222`
- Goal: final V3 gate acceptance for Blade Ascendant VFX rollout tracks.

## Functional Coverage (From Completed Dependency Tracks)

- `blade_ascendant_vfx_base_forms_20260222`: Base Form 3.1-3.9 coverage.
- `blade_ascendant_vfx_transmutation_20260222`: element variant matrix and shader switch path.
- `blade_ascendant_vfx_keystone_trigger_20260222`: keystone/trigger/synergy differentiation and trigger-storm control.
- `blade_ascendant_vfx_global_systems_20260222`: Sword Intent, Yujian Step, resist 1-5 overlays.

## Contract and Safety Checks

- `VFXPass` ownership contract confirmed in `src/engine/render/passes/VFXPass.cpp`:
  - reads `SceneHdrColor` and `SceneDepth`
  - writes `SceneHdrColor`
- `VFXPass` compute check:
  - no compute dispatch in `src/engine/render/passes/VFXPass.cpp`.
  - compute dispatch remains in dedicated compute passes (`JFAPass`, `LightCullingPass`, `GICompositePass`, etc.).
- FBO0 usage review:
  - no Blade VFX track changes introduced direct default-framebuffer writes outside existing pass framework.
  - no new hardcoded FBO0 write path introduced by this rollout.
- SSBO governance review:
  - no new global SSBO binding introduced in Blade VFX tracks.
  - existing binding contract preserved through existing `RenderConstants::Binding`.

## Tier Fallback Evidence

- Recipe schema and gate check passed: `python scripts/check_blade_vfx_recipe.py --check`.
- Runtime fallback behavior remains aligned with existing quality policy:
  - particle scaling
  - distortion gating
  - trail stride fallback
  - secondary cue reduction

## Verification Commands

- `.\build.bat` -> initial run FAILED due stale Ninja cache (MSVC-only guard), then resolved via `.\build.bat clean-all`.
- `.\build.bat clean-all` -> PASS
- `.\build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS
- `ctest --test-dir build -C Release -L performance --output-on-failure` -> FAIL (single known case)
  - failing case: `[Performance] ParticleTrail - Scenario 4 SubEmitter 1k/frame`
  - metric: `dispatchOverheadMs=0.205426` (threshold `< 0.2`)
  - disposition: non-blocking for this track, linked to `BUG-20260219-004`
- `.\build.bat analyze` -> PASS (existing warnings only)

## Gate Conclusion

- Functional/contract/fallback acceptance: PASS
- Performance suite: conditional pass with one known non-blocking historical issue
- Final posture: **CONDITIONAL-GO** for Blade Ascendant VFX V3 gate

## Bug Registry Linkage

- Linked existing issue: `BUG-20260219-004` (`conductor/bug_registry.md`)
- This track adds the latest evidence point: `dispatchOverheadMs=0.205426` (2026-02-22)
