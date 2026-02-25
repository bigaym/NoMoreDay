# Combat Effectiveness Integration — Implementation Plan

> Track ID: `combat_effectiveness_integration_20260225`  
> Series: CS-M1-03  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)

---

## Phase 1: Test Baseline (Day 1)

- [ ] Create `tests/unit/EffectivenessIntegrationTests.cpp`:
  - [ ] Test: AddedEff=1.0 → added damage unchanged (baseline).
  - [ ] Test: AddedEff=0.5 → added damage halved.
  - [ ] Test: AddedEff=0.0 → added damage = 0.
  - [ ] Test: TriggerEff=1.0 → direct cast damage unchanged.
  - [ ] Test: TriggerEff=0.5 → triggered damage halved.
  - [ ] Test: Combined AddedEff=0.4 + TriggerEff=0.7 → correct multiplication.

Verification:

- [ ] Tests fail for expected reasons (coefficients not yet wired).

---

## Phase 2: Pipeline Integration (Day 2-3)

- [ ] Add `float added_effectiveness = 1.0f` to `DamageRequest` (or equivalent context struct).
- [ ] Add `float trigger_effectiveness = 1.0f` to `DamageRequest`.
- [ ] In `DamagePipeline::Calculate`:
  - [ ] Apply `added_effectiveness` to flat added damage: `added_total *= req.added_effectiveness`.
  - [ ] Apply `trigger_effectiveness` as final multiplier before mitigation.
- [ ] In `SkillSystem.cpp` trigger dispatch:
  - [ ] Read `trigger.effectiveness` from `NodeContractData`.
  - [ ] Pass it into `DamageRequest.trigger_effectiveness`.
- [ ] Verify all direct-cast paths use default `trigger_effectiveness = 1.0`.

Verification:

- [ ] All Phase 1 tests pass.
- [ ] `build.bat` PASS.

---

## Phase 3: Final Gate (Day 3)

- [ ] Run full CI.
- [ ] Verify no existing tests regressed.
- [ ] Update validation.md.

Verification:

- [ ] `build.bat` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- DamageRequest fields for both effectiveness coefficients.
- Pipeline formula integration.
- Trigger dispatch wiring in SkillSystem.
- New unit tests.
- `validation.md` evidence.
