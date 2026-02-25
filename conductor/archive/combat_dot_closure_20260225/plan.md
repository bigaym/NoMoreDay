# Combat DoT Closure — Implementation Plan

> Track ID: `combat_dot_closure_20260225`  
> Series: CS-M1-02  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)  
> Workflow: TDD-first, minimal diff

---

## Phase 1: Test Baseline (Day 1)

- [x] Create `tests/unit/DoTDamageClosureTests.cpp`:
  - [x] Test: DoT tick reduces target HP by calculated damage amount.
  - [x] Test: DoT tick with `Tag::DamageOverTime` does NOT trigger `OnSkillHit`.
  - [x] Test: Poison DoT tick applies correct element tag.
  - [x] Test: Fire DoT (if applicable) applies correct element tag.
  - [x] Test: Multiple concurrent DoTs on same target tick independently.

Verification:

- [x] Tests compile and pass with fix applied.

---

## Phase 2: DoT Tick Fix (Day 2)

- [x] Fix `EffectSystem.cpp` L111: Change `Tag::None` to `Tag::DamageOverTime`.
- [x] Fix `EffectSystem.cpp` after L114: Add `CombatSystem::ApplyDamage(registry, entity, result.total_damage, effect.source)`.
- [x] Verify element-specific tag is preserved in DamagePool (now from `BuffEffect::tick_damage_tag`, default `Tag::Poison`).
- [x] Audit `DamagePipeline.cpp` L415/L584/L900 to confirm `Tag::DamageOverTime` correctly gates Hit-only branches.

Verification:

- [x] All Phase 1 tests pass.
- [x] `build.bat` PASS.
- [x] Manual smoke equivalent: headless DoT closure tests validate HP delta and event exclusion.

---

## Phase 3: Final Gate (Day 2-3)

- [x] Run full CI suite.
- [x] Verify no existing tests regressed.
- [x] Update `validation.md` with evidence.

Verification:

- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Fixed DoT tick with ApplyDamage call and correct Tag.
- New DoT unit tests.
- `validation.md` evidence.
