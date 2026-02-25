# Combat Single Damage Entry — Implementation Plan

> Track ID: `combat_single_damage_entry_20260225`  
> Series: CS-M1-01  
> Depends on: None (root track)  
> Workflow: TDD-first, caller-by-caller migration with regression snapshot

---

## Phase 1: Audit & Test Baseline (Day 1-2)

- [x] Full grep audit of `CalculateDamage` and non-pipeline `ApplyDamage` call sites.
- [x] Create numerical snapshot: run each damage path with fixed inputs, record `FinalDamage` values.
- [x] Create `tests/unit/DamagePipelineUnifiedEntryTests.cpp`:
  - [x] Test: Pipeline output matches legacy CalculateDamage for same inputs (melee, ranged, AoE).
  - [x] Test: Pipeline handles thorns damage (Tag::Thorns, skip_mitigation_for_source).
  - [x] Test: Pipeline handles self-damage (skip_mitigation = true).
- [x] Create `tests/integration/CombatDamageRegressionTests.cpp`:
  - [x] Test: Full melee combo → target HP delta matches expected.
  - [x] Test: AI enemy attack → target HP delta matches expected.
  - [x] Test: Projectile hit → target HP delta matches expected.

Verification:

- [x] New tests compile and fail for expected reasons (calling paths not yet migrated), then pass after migration.

---

## Phase 2: DamageRequest Extension & Deprecation (Day 2-3)

- [x] Extend `DamageRequest` struct in `DamagePipeline.hpp`:
  - [x] Add `bool skip_mitigation = false` field (for self-damage).
  - [x] Add `Tag thorns_tag` support or equivalent flag (`thorns_like_damage`).
- [x] Mark `CombatSystem::CalculateDamage` with `[[deprecated("Use DamagePipeline::Calculate")]]`.
- [x] Add `#ifdef COMBAT_LEGACY_CALC_ENABLED` guard around legacy function body.
- [x] Verify `DamagePipeline::Calculate` produces same output as legacy for equivalent inputs.

Verification:

- [x] `build.bat` compiles with deprecation warnings (not errors yet).
- [x] Unit tests for Pipeline equivalence pass.

---

## Phase 3: Caller Migration (Day 3-6)

### 3a: CombatSystem.cpp — Player Melee (L260-330)

- [x] Replace `CalculateDamage` calls at L260, L272 with `DamagePipeline::Calculate`.
- [x] Update `ApplyDamage` at L318 to use Pipeline result.
- [x] Verify thorns at L330 goes through Pipeline with `Tag::Thorns`.

### 3b: CombatSystem.cpp — AI Enemy Attack (L428-457)

- [x] Replace `CalculateDamage` at L428 with `DamagePipeline::Calculate`.
- [x] Update `ApplyDamage` at L457 to use Pipeline result.

### 3c: ProjectileSystem.cpp (L601, L639)

- [x] Refactor L601 (self-damage/explosion) to use `DamagePipeline::Calculate` with `skip_mitigation` (already pipeline-based in current codebase, audited and verified).
- [x] Refactor L639 (projectile hit) to use `DamagePipeline::Calculate` (already pipeline-based in current codebase, audited and verified).

### 3d: SkillSystem.cpp — Self-Damage (L878)

- [x] Refactor self-damage `ApplyDamage` to use `DamagePipeline::Calculate` with `skip_mitigation` (current L878 path already pipeline-based and verified).

### 3e: SwordArray.cpp — Spirit Sword Array (L215)

- [x] Replace hardcoded `hp->max * 0.1f` with proper `DamagePipeline::Calculate` call.
- [x] Ensure `source_skill` attribution is preserved.

**NOTE:** `SummonSystem.cpp` L83 is explicitly OUT OF SCOPE — handled by CS-M1-06.

Verification:

- [x] Each sub-phase verified individually before proceeding.
- [x] `build.bat` compiles clean (deprecated warnings only from legacy declaration).
- [x] All Phase 1 regression tests pass.

---

## Phase 4: Final Audit & Gate (Day 6-7)

- [x] Full `Select-String -Path "src\**\*" -Pattern "CalculateDamage" -Recurse` → only declaration + body remain.
- [x] Verify no new non-pipeline `ApplyDamage` calls bypass Pipeline.
- [x] Run full CI test suite.
- [x] Numerical snapshot comparison: all values within ±0.01% (unit/integration regression with epsilon checks).

Verification:

- [x] `build.bat` PASS
- [x] `build.bat analyze` PASS (no new warnings)
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Migrated caller code in CombatSystem, ProjectileSystem, SkillSystem, SwordArray.
- `DamageRequest` extension (skip_mitigation, thorns support).
- `[[deprecated]]` marker on `CombatSystem::CalculateDamage`.
- Compatibility flag `COMBAT_LEGACY_CALC_ENABLED`.
- New unit + integration regression test suites.
- `validation.md` evidence in track folder.
