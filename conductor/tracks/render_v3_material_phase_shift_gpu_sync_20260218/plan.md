# Render V3 Material Phase Shift GPU Sync Plan

> **Track ID**: `render_v3_material_phase_shift_gpu_sync_20260218`  
> **TDD Policy**: unit first for payload correctness, integration second.

## Phase 1: Repro and Assertion

- [ ] Add failing unit assertion that compares pre-shift vs active-shift GPU payload values.
- [ ] Add failing unit assertion for post-expiry baseline restoration.

## Phase 2: Material Manager Fix

- [ ] Implement/extend GPU payload rebuild path that reapplies runtime multipliers.
- [ ] Trigger rebuild on `SetRuntimePhaseShift` state change.
- [ ] Trigger rebuild on `ResetRuntimePhaseShift`.
- [ ] Ensure `SyncToGPU` uploads rebuilt cache only, with bounded cost.

## Phase 3: VFX Runtime Integration

- [ ] Verify runtime life-cycle in `VFXSequencerSystem` still calls set/reset at correct boundaries.
- [ ] Add integration assertion for visible state transition through sequence update ticks.

## Phase 4: Regression and Performance Safety

- [ ] Run `build.bat`.
- [ ] Run targeted tests for VFX/material unit+integration.
- [ ] Run `build.bat analyze`.
- [ ] Capture evidence in `validation.md`.

## DoD

- [ ] Runtime phase shift mutates GPU payload while active.
- [ ] Runtime phase shift fully restores baseline on end.
- [ ] No regression in existing VFX/material compatibility tests.
