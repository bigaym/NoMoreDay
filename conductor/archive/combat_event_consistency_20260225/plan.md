# Combat Event Consistency — Implementation Plan

> Track ID: `combat_event_consistency_20260225`  
> Series: CS-M1-04  
> Depends on: `combat_single_damage_entry_20260225` (CS-M1-01)

---

## Phase 1: Test Baseline (Day 1)

- [x] Create `tests/unit/EventConsistencyTests.cpp`:
  - [x] Test: single target → event damage == applied damage.
  - [x] Test: batch targets → all events use final_damage.
  - [x] Test: with mitigation → event reflects post-mitigation value.

---

## Phase 2: Event Payload Fix (Day 1-2)

- [x] Replace `res.damage` with `final_damage` at all 6 event construction sites (L931, L937, L943, L950, L955, L961).
- [x] Audit: search for any other event construction using pre-mitigation values.

Verification:

- [x] All Phase 1 tests pass.
- [x] `build.bat` PASS.

---

## Phase 3: Final Gate (Day 2)

- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Fixed event payloads in DamagePipeline.cpp.
- New event consistency unit tests.
- `validation.md` evidence.
