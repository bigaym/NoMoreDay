# Combat Defense Contract V1 — Implementation Plan

> Track ID: `combat_defense_contract_v1_20260225` | Series: CS-M2-02  
> Depends on: CS-M1-01

---

## Phase 1: Audit & Contract Design

- [ ] Audit current defense calculation locations (DamagePipeline, CombatSystem).
- [ ] Design `MitigationChain` with 6 ordered steps.
- [ ] Add `COMBAT_DEFENSE_DEBUG` compile flag for step-by-step logging.

## Phase 2: Implementation

- [ ] Implement `MitigationChain::Apply` in DamagePipeline.
- [ ] Migrate scattered defense logic to unified chain.
- [ ] Add debug logging at each step.

## Phase 3: Testing & Gate

- [ ] Unit: each defense step in isolation.
- [ ] Integration: armor, resistance, barrier tests.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- Unified `MitigationChain` in DamagePipeline.
- Debug logging infrastructure.
- Defense unit + integration tests.
