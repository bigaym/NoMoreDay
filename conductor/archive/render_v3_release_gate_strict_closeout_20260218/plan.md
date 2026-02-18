# Render V3 Release Gate Strict Closeout Plan

> **Track ID**: `render_v3_release_gate_strict_closeout_20260218`  
> **Policy**: evidence-driven quality closure.

## Phase 1: Baseline and Inputs

- [x] Re-run full release gate with current codebase and collect fresh artifacts.
- [x] Diff against previous `checks/pass/warning/fail` summary.

## Phase 2: F4.6 Handling

- [x] Re-measure clustered uplift metric after shader hardening.
- [x] Update waiver state and linked bug status based on measured trend.
- [x] If still below target, tighten remediation plan with dated milestones.

## Phase 3: F6.2 Handling

- [x] Validate screenshot gate prerequisites and manifest completeness.
- [x] Decide and document one of:
- [ ] A) strict gate closed in this track, or
- [x] B) explicit carry-over to V4 preflight with bounded ownership.

## Phase 4: Finalization

- [x] Run `build.bat`.
- [x] Run `build.bat analyze`.
- [x] Run `ctest --test-dir build -C Release -L performance --output-on-failure` (project policy replacement for `build.bat perf`).
- [x] Run `build.bat gate`.
- [x] Update validation evidence and bug registry linkage.

## DoD

- [x] Gate status is technically consistent with artifacts.
- [x] Waiver/bug/docs are synchronized.
- [x] Final recommendation for V3 release posture is explicit.
