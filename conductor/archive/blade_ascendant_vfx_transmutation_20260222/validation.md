# Validation - blade_ascendant_vfx_transmutation_20260222

## Scope

- Track: `blade_ascendant_vfx_transmutation_20260222`
- Goal: implement element variants driven by `TransmuterSwitch` and `elementType`.

## Implemented Artifacts

- `src/engine/render/GPUData.hpp`
  - `GPUSkillEffect.softness` replaced by `flags` (low 4 bits for `elementType`, struct size remains 64B).
- `src/engine/render/GPUSkillEffectSystem.cpp`
  - Added include-aware shader loader for local GLSL include resolution.
  - Encoded `elementType` to `GPUSkillEffect.flags` in recipe and legacy paths.
- `assets/shaders/sh_skill_effect.vs`
  - Added `flags` field to SSBO struct and passed `passFlags` to fragment stage.
- `assets/shaders/sh_skill_effect.fs`
  - Integrated `vfx/vfx_element_switch.glslinc`.
  - Applied shader-side palette switch from `passFlags & 0xF`.
- `assets/data/vfx/blade_ascendant_v3.json`
  - Added element variant recipes (Fire + Cold) for skills 1-9.
- `scripts/check_blade_vfx_recipe.py`
  - Added transmutation coverage validation for required element variants.
- `tests/unit/SkillVfxEventContractTest.cpp`
  - Added transmutation recipe smoke checks (skills 1-9 each include Fire + Cold entries).
- `scripts/gen_blade_vfx_assets.py`
  - Reworked to deterministic `numpy + stdlib` pipeline (no PIL dependency).
  - Generates required V3 transmutation textures.

## Generated Assets

Command:

- `python scripts/gen_blade_vfx_assets.py`

Generated / refreshed files under `assets/textures/vfx/`:

- `vfx_element_fire.png`
- `vfx_element_ice.png`
- `vfx_element_lightning.png`
- `vfx_element_void.png`
- `vfx_resist_crack.png`
- `vfx_frost_spread.png`
- `vfx_ember_trail.png`
- `vfx_electric_arc.png`
- plus deterministic refresh of existing placeholders (`vfx_trail_smooth.png`, `vfx_noise_cloud.png`, `vfx_circle_shockwave.png`, `vfx_scratch_mask.png`, `vfx_rune_array.png`)

## Evidence Checklist

- [x] Element textures generated procedurally via reproducible script.
- [x] Runtime switch path consumes `elementType` and applies palette in shader.
- [x] Tier fallback remains readability-first and pass-safe.
- [x] No new global SSBO binding, no FBO0 direct output path introduced.

## Verification Commands

- `python scripts/gen_blade_vfx_assets.py` -> PASS
- `python scripts/check_blade_vfx_recipe.py --check` -> PASS
- `.\build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS
- `.\build.bat analyze` -> PASS (existing warnings only)
- `ctest --test-dir build -C Release -L performance --output-on-failure` -> PASS
