# Validation - blade_ascendant_vfx_validation_gate_20260222

## Scope

- Track: `blade_ascendant_vfx_validation_gate_20260222`
- Goal: final acceptance for Blade Ascendant VFX V3.

## Verification Commands (Baseline)

- `.\build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

