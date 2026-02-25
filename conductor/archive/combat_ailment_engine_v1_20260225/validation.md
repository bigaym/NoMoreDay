# Validation — combat_ailment_engine_v1_20260225

## Verification Evidence

- Ailment contract definitions → PASS  
  Evidence: `assets/data/ailment_contracts.json` defines Poison/Ignite/Bleed with full contract fields.
- Stack/refresh/overwrite tests → PASS  
  Evidence: `tests/unit/AilmentEngineTests.cpp` covers stack cap, refresh/extend/independent, strongest/newest/additive.
- DoT tick migration → PASS  
  Evidence: `src/game/systems/combat/EffectSystem.cpp` routes DoT handling to `AilmentTickDriver::Tick`; legacy DoT compatibility retained via `AilmentAdapter`.
- `build.bat` → PASS  
  Evidence: 2026-02-25 verify round completed after implementation and fix-up; pre-commit final regression round PASS.
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS  
  Evidence: includes `AilmentEngineTests` and existing `DoTDamageClosureTests`; pre-commit final regression round PASS.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS  
  Evidence: nonperf CI suite passed; pre-commit final regression round PASS.
