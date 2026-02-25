# Track: Combat Contract CI Gate

**ID:** `combat_contract_ci_gate_20260225`  
**Status:** Completed  
**Type:** tooling/governance  
**Priority:** P0  
**Milestone:** M1（口径收敛与正确性闭环）  
**Series:** CS-M1-05

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 3/3 complete
- **Tasks:** 10/10 complete

## Scope Summary

- Fix current `gen_skill_contracts.py --check` failure (contract drift detected 2026-02-25).
- Harden contract generation script for robustness and determinism.
- Integrate contract check into CI pipeline as blocking gate.
- Establish nightly validation for continuous contract compliance.

## Dependencies

- **Upstream:** None (independent from CS-M1-01, can run in parallel)
- **Downstream:** All M2/M3 tracks benefit from CI-enforced contract compliance

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review §2.9](../../analyzer/combat_system_review.md)
- [Script: gen_skill_contracts.py](../../../scripts/gen_skill_contracts.py)
