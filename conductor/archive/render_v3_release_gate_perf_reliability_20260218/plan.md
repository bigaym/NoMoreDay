# Render V3 Release Gate Perf Reliability Plan

> **Track ID**: `render_v3_release_gate_perf_reliability_20260218`  
> **Policy**: TDD + evidence-first closure for Open bugs in bug registry.

## Phase 1: Foundation (TDD Baseline)

- [x] Reproduce `F4.3/F4.5/F4.6` current failures in batch gate and single-case rerun.
- [x] Add deterministic seed/log output for stress benchmark and clustered benchmark.
- [x] Add/extend tests to assert benchmark preconditions (validation flag, warmup consistency, pass/fail parse contract).

## Phase 2: Logic (Root Cause Fix)

- [x] Fix `F4.3` instability in `RenderGraph Contract Validation Guard` benchmark.
- [x] Fix `F4.5` derived comparator false-fail path caused by unstable inputs.
- [x] Optimize/adjust clustered benchmark path to satisfy `F4.6 >= 5%` target without weakening threshold.

## Phase 3: Integration (Gate + Waiver + Bug Sync)

- [x] Update `scripts/v3_release_gate.py` output/evidence fields for repeated-run stability proofs.
- [x] Run 3 consecutive Release gate runs and collect metrics snapshots.
- [x] Remove active waivers `WVR-20260218-F4.3-001`, `WVR-20260218-F4.5-001`, `WVR-20260218-F4.6-001` when criteria are met.
- [x] Update `conductor/bug_registry.md` status flow: `Open -> In Progress -> Resolved -> Verified`.

## Phase 4: Polish & Closeout

- [x] Execute `build.bat`.
- [x] Execute `build.bat analyze`.
- [x] Execute `ctest --test-dir build -C Release -L performance --output-on-failure`.
- [x] Sync `validation.md`, `tracks.md`, and track metadata evidence counters.
- [x] Prepare archive-ready closeout package.

## Definition of Done

- [x] No Open bug remains in `conductor/bug_registry.md`.
- [x] Release gate performance checks (`F4.3/F4.5/F4.6`) are stable without waiver.
- [x] Evidence is reproducible and traceable from command -> artifact -> bug status.
