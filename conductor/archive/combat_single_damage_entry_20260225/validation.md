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

Date: `2026-02-25`

- `build.bat` → PASS
- `build.bat analyze` → PASS (existing analyzer warnings remain, no new blocking warnings)
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
- `Select-String -Path "src\**\*" -Pattern "CalculateDamage" -Recurse` audit → PASS
  - Remaining hits only:
    - `src/game/systems/combat/CombatSystem.hpp` declaration
    - `src/game/systems/combat/CombatSystem.cpp` legacy body
- Numerical snapshot comparison → PASS
  - `tests/unit/DamagePipelineUnifiedEntryTests.cpp` verifies legacy-equivalence and thorns/self-damage semantics with epsilon checks.
  - `tests/integration/CombatDamageRegressionTests.cpp` verifies melee/AI/projectile HP delta regression against pipeline simulation.

## Additional Gate Fixes During Verification

- `ctest -L integration` initially failed at `SkillContractRegistryTests` because `assets/data/skills.json` contract bounds were stale versus current `talent_tree` node counts, and skill 1 used an invalid transmuter node id.
- Synced `skills.json` contract values (`min_nodes/max_nodes` for skills 2..9; skill 1 `transmuter_node_ids` corrected), then reran full gate.
- Final gate status after fix: all required commands PASS.
