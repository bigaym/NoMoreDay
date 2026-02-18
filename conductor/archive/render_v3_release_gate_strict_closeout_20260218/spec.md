# Render V3 Release Gate Strict Closeout Spec

> **Track ID**: `render_v3_release_gate_strict_closeout_20260218`  
> **Type**: `quality`  
> **Priority**: P1  
> **Depends On**: `render_v3_clustered_shader_hardening_20260218`, `render_v3_material_phase_shift_gpu_sync_20260218`, `v3_validation_and_release_gate_20260215`

## 1. Goal

Close remaining Step F strict-gate debt and move V3 release quality from "pass with warnings" to "strictly auditable pass" where possible.

## 2. Target Gaps

1. `F4.6` clustered uplift waiver (`WVR-20260218-F4.6-001`) with open bug `BUG-20260218-001`.
2. `F6.2` screenshot strict gate currently warning path.
3. Need refreshed gate artifact evidence after corrective tracks.

## 3. Scope

1. `scripts/v3_release_gate.py` and related matrix/profile artifacts.
2. `conductor/validation/v3_gate_matrix.json`
3. `conductor/validation/v3_perf_profiles.json`
4. `conductor/validation/v3_gate_waivers.json`
5. `conductor/bug_registry.md` status synchronization.

## 4. Gate Contract

### 4.1 Required outputs
1. Artifact schema-valid gate report.
2. Explicit `fallbackTriggered` and reasons.
3. Metrics including `clustered_128_improvement_pct`.

### 4.2 Strictness policy
1. No silent downgrade from fail to warning without documented waiver.
2. Waiver must have expiry and measurable exit criteria.
3. If strict screenshot gate still cannot be fully closed, dependency transfer must be explicit and dated.

## 5. Acceptance Criteria

1. Post-fix gate run produces reproducible artifact set.
2. Waiver and linked bug statuses are updated consistently with latest evidence.
3. Decision on `F6.2` is explicit: closed in V3 or carried with concrete V4 preflight ownership.
4. Validation document includes command lines, timestamps, and result summary.
