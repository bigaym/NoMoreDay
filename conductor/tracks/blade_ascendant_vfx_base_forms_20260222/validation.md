# Validation - blade_ascendant_vfx_base_forms_20260222

## Scope

- Track: `blade_ascendant_vfx_base_forms_20260222`
- Goal: implement Base Form VFX for skills 3.1-3.9 with tier fallback.

## Evidence Checklist

- [ ] 9 skills are visually distinct on High tier.
- [ ] Low/Medium fallbacks preserve readability (no loss of core hit/cast feedback).
- [ ] Concurrency caps enforced; no runaway allocations.

## Verification Commands

- `.\build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

