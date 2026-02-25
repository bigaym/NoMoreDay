# Combat Effectiveness Integration — Implementation Plan

> Track ID: `combat_effectiveness_integration_20260225`  
> Series: CS-M1-03  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)

---

## Phase 1: Test Baseline (Day 1)

- [x] Create `tests/unit/EffectivenessIntegrationTests.cpp`:
  - [x] Test: AddedEff=1.0 → added damage unchanged (baseline).
  - [x] Test: AddedEff=0.5 → added damage halved.
  - [x] Test: AddedEff=0.0 → added damage = 0.
  - [x] Test: TriggerEff=1.0 → direct cast damage unchanged.
  - [x] Test: TriggerEff=0.5 → triggered damage halved.
  - [x] Test: Combined AddedEff=0.4 + TriggerEff=0.7 → correct multiplication.

Verification:

- [x] Tests compiled and passed after coefficient wiring.

---

## Phase 2: Pipeline Integration (Day 2-3)

- [x] Add `float added_effectiveness = 1.0f` to `DamageRequest` (or equivalent context struct).
- [x] Add `float trigger_effectiveness = 1.0f` to `DamageRequest`.
- [x] In `DamagePipeline::Calculate`:
  - [x] Apply `added_effectiveness` to flat added damage: `added_total *= req.added_effectiveness`.
  - [x] Apply `trigger_effectiveness` as final multiplier before mitigation.
- [x] In `SkillSystem.cpp` trigger dispatch:
  - [x] Read `trigger.effectiveness` from `NodeContractData`.
  - [x] Propagate trigger effectiveness via trigger execution context (`SkillExecution` + cast_id mapping) so `DamagePipeline` can consume it from `source_entity`.
- [x] Verify all direct-cast paths use default `trigger_effectiveness = 1.0`.

Verification:

- [x] All Phase 1 tests pass.
- [x] `build.bat` PASS.

---

## Phase 3: Final Gate (Day 3)

- [x] Run full CI (track scope: unit + ci labels).
- [x] Verify no existing tests regressed.
- [x] Update validation.md.

Verification:

- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- DamageRequest fields for both effectiveness coefficients.
- Pipeline formula integration.
- Trigger dispatch wiring in SkillSystem.
- New unit tests.
- `validation.md` evidence.
