# Render V3 Material Phase Shift GPU Sync Plan

> **Track ID**: `render_v3_material_phase_shift_gpu_sync_20260218`  
> **TDD Policy**: unit first for payload correctness, integration second.

## Phase 1: Repro and Assertion

- [x] Add failing unit assertion that compares pre-shift vs active-shift GPU payload values.
- [x] Add failing unit assertion for post-expiry baseline restoration.

## Phase 2: Material Manager Fix

- [x] Implement/extend GPU payload rebuild path that reapplies runtime multipliers.
- [x] Trigger rebuild on `SetRuntimePhaseShift` state change.
- [x] Trigger rebuild on `ResetRuntimePhaseShift`.
- [x] Ensure `SyncToGPU` uploads rebuilt cache only, with bounded cost.

## Phase 3: VFX Runtime Integration

- [x] Verify runtime life-cycle in `VFXSequencerSystem` still calls set/reset at correct boundaries.
- [x] Add integration assertion for visible state transition through sequence update ticks.

## Phase 4: Regression and Performance Safety

- [x] Run `build.bat`.
- [x] Run targeted tests for VFX/material unit+integration.
- [x] Run `build.bat analyze`.
- [x] Capture evidence in `validation.md`.

## DoD

- [x] Runtime phase shift mutates GPU payload while active.
- [x] Runtime phase shift fully restores baseline on end.
- [x] No regression in existing VFX/material compatibility tests.
