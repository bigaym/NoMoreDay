# Validation - blade_ascendant_vfx_base_forms_20260222

## Scope

- Track: `blade_ascendant_vfx_base_forms_20260222`
- Goal: implement Base Form VFX for skills 3.1-3.9 with low/medium fallback readability.

## Implemented Artifacts

- `assets/data/vfx/blade_ascendant_v3.json`
  - Added base-form recipe entries for skills 1-9.
  - Preserved generic transmuter/keystone/resist-overlay entries.
- `src/engine/render/GPUSkillEffectSystem.cpp`
  - Fixed recipe `ParticleBurst` count semantics to avoid repeated N*N emission.
  - Synced fallback policy to `trailEnabled`.
- `scripts/check_blade_vfx_recipe.py`
  - Added required base-form event coverage checks for skills 1-9.
- `tests/unit/SkillVfxEventContractTest.cpp`
  - Expanded recipe smoke coverage from skill 1 only to 9-skill core event matrix.

## Evidence Checklist

- [x] 9 skills have distinct base-form recipe entries on High tier.
- [x] Low/Medium fallback keeps core readability events per skill.
- [x] Concurrency caps remain enforced via `ResolveSkillCap` + `TrySubmitCapped`.
- [x] Recipe validator and unit smoke checks cover required event matrix.

## Verification Commands

- `python scripts/check_blade_vfx_recipe.py --check` -> PASS
- `.\build.bat` -> first run failed due existing unsupported `Ninja` cache; resolved with `.\build.bat clean-all`
- `.\build.bat clean-all` -> PASS
- `.\build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS

## Notes

- The `build.bat` failure was non-code and environment-cache related (existing generator mismatch), fixed in-session before final verification.
