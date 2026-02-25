# Combat DoT Closure — Implementation Plan

> Track ID: `combat_dot_closure_20260225`  
> Series: CS-M1-02  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)  
> Workflow: TDD-first, minimal diff

---

## Phase 1: Test Baseline (Day 1)

- [ ] Create `tests/unit/DoTDamageClosureTests.cpp`:
  - [ ] Test: DoT tick reduces target HP by calculated damage amount.
  - [ ] Test: DoT tick with `Tag::DamageOverTime` does NOT trigger `OnSkillHit`.
  - [ ] Test: Poison DoT tick applies correct element tag.
  - [ ] Test: Fire DoT (if applicable) applies correct element tag.
  - [ ] Test: Multiple concurrent DoTs on same target tick independently.

Verification:

- [ ] Tests compile and fail for expected reasons before fix.

---

## Phase 2: DoT Tick Fix (Day 2)

- [ ] Fix `EffectSystem.cpp` L111: Change `Tag::None` to `Tag::DamageOverTime`.
- [ ] Fix `EffectSystem.cpp` after L114: Add `CombatSystem::ApplyDamage(registry, entity, result.total_damage, effect.source)`.
- [ ] Verify element-specific tag is preserved in DamagePool (already uses `Tag::Poison`).
- [ ] Audit `DamagePipeline.cpp` L415/L584/L900 to confirm `Tag::DamageOverTime` correctly gates Hit-only branches.

Verification:

- [ ] All Phase 1 tests pass.
- [ ] `build.bat` PASS.
- [ ] Manual smoke: DoT visibly reduces enemy HP bar.

---

## Phase 3: Final Gate (Day 2-3)

- [ ] Run full CI suite.
- [ ] Verify no existing tests regressed.
- [ ] Update `validation.md` with evidence.

Verification:

- [ ] `build.bat` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Fixed DoT tick with ApplyDamage call and correct Tag.
- New DoT unit tests.
- `validation.md` evidence.
