# Combat Release Gate Suite — Implementation Plan

> Track ID: `combat_release_gate_suite_20260225` | Series: CS-M3-04  
> Depends on: CS-M2-05

## Phase 1: Gate Infrastructure
- [x] Define gate config schema (thresholds, labels, pass criteria).
- [x] Extend `build.bat` with `combat-gate` sub-command.
- [x] Implement gate report generator.

## Phase 2: Gate Integration
- [x] Wire CI gate (contract check + unit tests).
- [x] Wire nightly gate (full regression + performance).
- [x] Wire release gate (all dimensions + manual checklist).

## Phase 3: Baseline & Gate
- [x] Establish M1 baseline metrics for comparison.
- [x] Run first full release gate cycle.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
