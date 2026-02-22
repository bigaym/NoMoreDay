# Validation - blade_ascendant_vfx_transmutation_20260222

## Scope

- Track: `blade_ascendant_vfx_transmutation_20260222`
- Goal: implement element variants driven by `TransmuterSwitch` and `elementType`.

## Evidence Checklist

- [ ] Element textures generated procedurally (reproducible script).
- [ ] Runtime switch works; visuals align with palette.
- [ ] Tier fallback keeps readability.

## Verification Commands

- `python scripts/gen_blade_vfx_assets.py`
- `.\build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

