# Combat Defense Contract V1 — Implementation Plan

> Track ID: `combat_defense_contract_v1_20260225` | Series: CS-M2-02  
> Depends on: CS-M1-01

---

## Phase 1: Audit & Contract Design

- [x] Audit current defense calculation locations (DamagePipeline, CombatSystem).
- [x] Design `MitigationChain` with 6 ordered steps.
- [x] Add `COMBAT_DEFENSE_DEBUG` compile flag for step-by-step logging.

## Phase 2: Implementation

- [x] Implement `MitigationChain::Apply` in DamagePipeline.
- [x] Migrate scattered defense logic to unified chain.
- [x] Add debug logging at each step.

## Phase 3: Testing & Gate

- [x] Unit: each defense step in isolation.
- [x] Integration: armor, resistance, barrier tests.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- Unified `MitigationChain` in DamagePipeline.
- Debug logging infrastructure.
- Defense unit + integration tests.
