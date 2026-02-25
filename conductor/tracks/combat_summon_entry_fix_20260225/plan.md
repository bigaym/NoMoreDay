# Combat Summon Entry Fix — Implementation Plan

> Track ID: `combat_summon_entry_fix_20260225`  
> Series: CS-M1-06  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)

---

## Phase 1: Test Baseline (Day 1)

- [ ] Create `tests/unit/SummonDamageEntryTests.cpp`:
  - [ ] Test: melee orbit damage is calculated via Pipeline (not hardcoded).
  - [ ] Test: sword array damage is calculated via Pipeline.
  - [ ] Test: owner stats affect summon damage output.
- [ ] Record pre-migration DPS values for orbit and sword array.

---

## Phase 2: Migration (Day 1-2)

- [ ] `SummonSystem.cpp` L83: Replace `ApplyDamage(registry, target, 25.0f, summon.owner)` with:
  - [ ] Construct `DamagePool` from owner's base damage stats.
  - [ ] Call `DamagePipeline::Calculate(registry, summon.owner, target, skill_id, pool, tags)`.
  - [ ] Call `ApplyDamage` with Pipeline result.
- [ ] `SwordArray.cpp` L215: Replace `ApplyDamage(registry, target_ent, hp->max * 0.1f, ...)` with Pipeline call.
- [ ] Verify VFX remain unchanged.

Verification:

- [ ] All Phase 1 tests pass.
- [ ] `build.bat` PASS.

---

## Phase 3: Final Gate (Day 2)

- [ ] `Select-String` audit: no direct ApplyDamage in SummonSystem/SwordArray.
- [ ] `build.bat` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS
- [ ] DPS comparison: record post-migration values.

---

## Deliverables

- Migrated SummonSystem and SwordArray to Pipeline.
- New summon damage unit tests.
- Pre/post DPS comparison data.
- `validation.md` evidence.
