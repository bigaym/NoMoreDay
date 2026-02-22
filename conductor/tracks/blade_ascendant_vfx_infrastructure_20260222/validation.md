# Validation - blade_ascendant_vfx_infrastructure_20260222

## Scope

- Track: `blade_ascendant_vfx_infrastructure_20260222`
- Goal: provide executable VFX infrastructure for Blade Ascendant VFX V3 (event contract + recipe skeleton + tier hooks).

## Evidence Checklist

- [ ] `SkillVfxEvent` extended with compatible defaults.
- [ ] Recipe-driven skeleton exists (smoke-tested on at least 1 skill).
- [ ] No new global SSBO binding introduced; RenderGraph ownership respected.
- [ ] Tier fallback knobs mapped (documented).

## Verification Commands

- `.\build.bat notest`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

## Notes

- This track is foundational; runtime visual completeness is verified in downstream feature tracks and the final validation gate.

