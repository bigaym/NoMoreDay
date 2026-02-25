# Combat Summon Entry Fix — Implementation Plan

> Track ID: `combat_summon_entry_fix_20260225`  
> Series: CS-M1-06  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)

---

## Phase 1: Baseline Recheck (Done)

- [x] Recheck current code path:
  - [x] Melee orbit: `SummonAISystem -> SummonCombatBridge -> DamagePipeline::Calculate -> ApplyDamage`
  - [x] Sword execute: `SwordArray -> DamagePipeline::Calculate -> ApplyDamage`
- [x] Confirm residual gaps are semantic constants, not pipeline bypass:
  - [x] Orbit `25.0f` constant remains.
  - [x] Sword execute `hp->max * 0.1f` inline constant remains.

---

## Phase 2: Residual Convergence (Done)

- [x] Orbit base damage convergence:
  - [x] Replace inline orbit call parameters with `SummonCombatProfile` contract fields.
  - [x] Keep attack cadence `0.2f` and current VFX behavior unchanged.
- [x] Sword execute convergence:
  - [x] Replace inline `hp->max * 0.1f` with `SwordArrayComponent` ratio field.
  - [x] Keep execute gameplay intent and current pipeline routing unchanged.
- [x] Test convergence:
  - [x] Extend summon tests to lock profile-driven orbit damage semantics.
  - [x] Extend behavior-guard checks for execute ratio contract fields.

Verification:

- [x] Updated summon tests PASS.
- [x] `build.bat` PASS.

---

## Phase 3: Final Gate & Closeout (Done)

- [x] `Select-String` audit:
  - [x] Orbit caller path no longer passes inline `25.0f` constant.
  - [x] Sword execute no longer uses inline `hp->max * 0.1f`.
- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS
- [x] Validation evidence synchronized.

---

## Deliverables

- Baseline-calibrated CS-M1-06 track docs.
- Converged summon damage semantics (no inline orbit/execute constants in runtime path).
- Regression tests for summon damage source semantics.
- Final validation evidence ready for archive/closeout.
