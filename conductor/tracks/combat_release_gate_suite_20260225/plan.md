# Combat Release Gate Suite — Implementation Plan

> Track ID: `combat_release_gate_suite_20260225` | Series: CS-M3-04  
> Depends on: CS-M2-05

## Phase 1: Gate Infrastructure
- [ ] Define gate config schema (thresholds, labels, pass criteria).
- [ ] Extend `build.bat` with `combat-gate` sub-command.
- [ ] Implement gate report generator.

## Phase 2: Gate Integration
- [ ] Wire CI gate (contract check + unit tests).
- [ ] Wire nightly gate (full regression + performance).
- [ ] Wire release gate (all dimensions + manual checklist).

## Phase 3: Baseline & Gate
- [ ] Establish M1 baseline metrics for comparison.
- [ ] Run first full release gate cycle.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
