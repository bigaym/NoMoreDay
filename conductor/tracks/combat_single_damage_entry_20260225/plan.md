# Combat Single Damage Entry — Implementation Plan

> Track ID: `combat_single_damage_entry_20260225`  
> Series: CS-M1-01  
> Depends on: None (root track)  
> Workflow: TDD-first, caller-by-caller migration with regression snapshot

---

## Phase 1: Audit & Test Baseline (Day 1-2)

- [ ] Full grep audit of `CalculateDamage` and non-pipeline `ApplyDamage` call sites.
- [ ] Create numerical snapshot: run each damage path with fixed inputs, record `FinalDamage` values.
- [ ] Create `tests/unit/DamagePipelineUnifiedEntryTests.cpp`:
  - [ ] Test: Pipeline output matches legacy CalculateDamage for same inputs (melee, ranged, AoE).
  - [ ] Test: Pipeline handles thorns damage (Tag::Thorns, skip_mitigation_for_source).
  - [ ] Test: Pipeline handles self-damage (skip_mitigation = true).
- [ ] Create `tests/integration/CombatDamageRegressionTests.cpp`:
  - [ ] Test: Full melee combo → target HP delta matches expected.
  - [ ] Test: AI enemy attack → target HP delta matches expected.
  - [ ] Test: Projectile hit → target HP delta matches expected.

Verification:

- [ ] New tests compile and fail for expected reasons (calling paths not yet migrated).

---

## Phase 2: DamageRequest Extension & Deprecation (Day 2-3)

- [ ] Extend `DamageRequest` struct in `DamagePipeline.hpp`:
  - [ ] Add `bool skip_mitigation = false` field (for self-damage).
  - [ ] Add `Tag thorns_tag` support or equivalent flag.
- [ ] Mark `CombatSystem::CalculateDamage` with `[[deprecated("Use DamagePipeline::Calculate")]]`.
- [ ] Add `#ifdef COMBAT_LEGACY_CALC_ENABLED` guard around legacy function body.
- [ ] Verify `DamagePipeline::Calculate` produces same output as legacy for equivalent inputs.

Verification:

- [ ] `build.bat` compiles with deprecation warnings (not errors yet).
- [ ] Unit tests for Pipeline equivalence pass.

---

## Phase 3: Caller Migration (Day 3-6)

### 3a: CombatSystem.cpp — Player Melee (L260-330)

- [ ] Replace `CalculateDamage` calls at L260, L272 with `DamagePipeline::Calculate`.
- [ ] Update `ApplyDamage` at L318 to use Pipeline result.
- [ ] Verify thorns at L330 goes through Pipeline with `Tag::Thorns`.

### 3b: CombatSystem.cpp — AI Enemy Attack (L428-457)

- [ ] Replace `CalculateDamage` at L428 with `DamagePipeline::Calculate`.
- [ ] Update `ApplyDamage` at L457 to use Pipeline result.

### 3c: ProjectileSystem.cpp (L601, L639)

- [ ] Refactor L601 (self-damage/explosion) to use `DamagePipeline::Calculate` with `skip_mitigation`.
- [ ] Refactor L639 (projectile hit) to use `DamagePipeline::Calculate`.

### 3d: SkillSystem.cpp — Self-Damage (L878)

- [ ] Refactor self-damage `ApplyDamage` to use `DamagePipeline::Calculate` with `skip_mitigation`.

### 3e: SwordArray.cpp — Spirit Sword Array (L215)

- [ ] Replace hardcoded `hp->max * 0.1f` with proper `DamagePipeline::Calculate` call.
- [ ] Ensure `source_skill` attribution is preserved.

**NOTE:** `SummonSystem.cpp` L83 is explicitly OUT OF SCOPE — handled by CS-M1-06.

Verification:

- [ ] Each sub-phase verified individually before proceeding.
- [ ] `build.bat` compiles clean (deprecated warnings only from legacy declaration).
- [ ] All Phase 1 regression tests pass.

---

## Phase 4: Final Audit & Gate (Day 6-7)

- [ ] Full `Select-String -Path "src\**\*" -Pattern "CalculateDamage" -Recurse` → only declaration + body remain.
- [ ] Verify no new non-pipeline `ApplyDamage` calls bypass Pipeline.
- [ ] Run full CI test suite.
- [ ] Numerical snapshot comparison: all values within ±0.01%.

Verification:

- [ ] `build.bat` PASS
- [ ] `build.bat analyze` PASS (no new warnings)
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Migrated caller code in CombatSystem, ProjectileSystem, SkillSystem, SwordArray.
- `DamageRequest` extension (skip_mitigation, thorns support).
- `[[deprecated]]` marker on `CombatSystem::CalculateDamage`.
- Compatibility flag `COMBAT_LEGACY_CALC_ENABLED`.
- New unit + integration regression test suites.
- `validation.md` evidence in track folder.
