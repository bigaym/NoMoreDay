# V4 Release Posture - v4_validation_release_gate_20260219

## Decision
- `NO-GO` (as of 2026-02-19)

## Why Not GO
- Stability and rollback gates are not fully evidenced yet.
- Required blocker tasks still open: `4.1`, `4.2`, `4.3`, `5.1`, `5.2`, `5.7`.

## Evidence Snapshot
- Build pipeline: PASS (`build.bat`)
- CTest labels: PASS (`ci`, `unit`, `integration`, `performance`)
- Functional/performance/contract evidence reused from completed V4 feature tracks and preflight validation artifacts.

## Exit Criteria to Switch to GO
1. Finish and document 30-minute Ultra stress run with memory/frame-time deltas (`Task 4.1`).
2. Finish and document 10x runtime feature-flag switching without black-screen/path break (`Task 4.2`).
3. Finish and document tier degrade jitter threshold validation (`Task 4.3`).
4. Execute explicit `render.v4.enabled=false` rollback run and verify V3 behavior/perf floor (`Task 5.1`, `Task 5.2`).
5. Close blockers, complete track sync, then archive V4 tracks (`Task 5.7`).
