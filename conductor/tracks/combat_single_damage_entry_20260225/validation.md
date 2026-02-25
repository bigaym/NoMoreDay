# Validation — combat_single_damage_entry_20260225

## Scope

- Unification of all damage calculation paths to `DamagePipeline`.
- Deprecation of `CombatSystem::CalculateDamage` legacy chain.
- Caller migration: CombatSystem, ProjectileSystem, SkillSystem, SwordArray.
- Compatibility flag: `COMBAT_LEGACY_CALC_ENABLED`.

## Test Coverage Added/Updated

- `tests/unit/DamagePipelineUnifiedEntryTests.cpp`
  - Pipeline equivalence with legacy formula.
  - Thorns damage via Pipeline.
  - Self-damage with skip_mitigation.
- `tests/integration/CombatDamageRegressionTests.cpp`
  - Melee combo HP delta regression.
  - AI enemy attack HP delta regression.
  - Projectile hit HP delta regression.

## Verification Evidence

_(To be filled during implementation)_

- `build.bat` → PENDING
- `build.bat analyze` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PENDING
- `Select-String -Path "src\**\*" -Pattern "CalculateDamage" -Recurse` audit → PENDING
- Numerical snapshot comparison → PENDING
