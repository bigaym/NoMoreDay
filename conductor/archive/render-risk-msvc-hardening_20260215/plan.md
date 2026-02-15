# Render Risk Closure and MSVC Hard Cutover Plan

> Track ID: `render-risk-msvc-hardening_20260215`  
> Spec: `./spec.md`  
> Workflow: TDD + evidence-first verification

## Phase 1: Toolchain Hard Cutover (P0)

- [x] Task 1.1: Remove GCC/MinGW fallback semantics from `build.bat`.
- [x] Task 1.2: Remove Ninja fallback selection path from `build.bat`.
- [x] Task 1.3: Enforce fail-fast when supported VS generator is unavailable.
- [x] Task 1.4: Add explicit mismatch handling for cached non-MSVC generator state (hard-fail with actionable message).
- [x] Task 1.5: Root `CMakeLists.txt` hard-fails if compiler is not MSVC.
- [x] Task 1.6: Remove non-MSVC ASan branch in root CMake to eliminate GCC-specific options.

Verification:
- [x] `build.bat` on MSVC environment configures and builds.
- [x] Simulated non-MSVC or missing-generator setup fails with explicit message.
- [x] Acceptance: AC-01, AC-02, AC-03.

## Phase 2: Static Analysis Entrypoint Reliability (P0)

- [x] Task 2.1: Fix `build.bat analyze` generator/cache flow so it works without manual cleanup.
- [x] Task 2.2: Verify `/analyze` path is actually activated under MSVC.
- [x] Task 2.3: Capture and archive analysis run summary under track artifacts.

Verification:
- [x] `build.bat analyze` exits success on a normal workspace.
- [x] Static analysis output is reproducible and documented.
- [x] Acceptance: AC-04.

## Phase 3: Rendering Risk Closure from Test Evidence (P1)

- [x] Task 3.1: Investigate MDI vs legacy benchmark inversion; identify whether issue is implementation, scene setup, or metric design.
- [x] Task 3.2: Apply fix or redefine benchmark contract with scenario labels and acceptance thresholds.
- [x] Task 3.3: Eliminate baseline VFX material fallback warnings by reconciling sequence material references with registry IDs.
- [x] Task 3.4: Re-check over-target hotspots (FlowField, ItemFactory) and classify as fix-now vs documented exception.

Verification:
- [x] Performance suite passes with updated/validated budget policy.
- [x] Baseline VFX asset load has no unknown-material fallback spam.
- [x] Acceptance: AC-06, AC-07.

## Phase 4: Test and Gate Consolidation (P1)

- [x] Task 4.1: Ensure full sequence is stable: `build.bat`, `build.bat analyze`, `build.bat perf`.
- [x] Task 4.2: Add regression guard(s) to prevent reintroduction of GCC/MinGW paths.
- [x] Task 4.3: Add/extend tests for toolchain policy and benchmark gate assumptions where feasible.

Verification:
- [x] Non-performance tests pass.
- [x] Performance tests pass.
- [x] Toolchain policy checks pass.
- [x] Acceptance: AC-05.

## Phase 5: Documentation and Closeout (P2)

- [x] Task 5.1: Record all decisions and deviations (if any) with concrete evidence.
- [x] Task 5.2: Update `conductor/bug_registry.md` when fixes correspond to tracked bug entries.
- [x] Task 5.3: Produce track `validation.md` with command matrix and outcomes.

Verification:
- [x] Evidence package is complete and reproducible.
- [x] Track can be moved to `Resolved` with objective proof.
- [x] Acceptance: AC-08.

## Estimated Task Count

- Total phases: `5`
- Total tasks: `18`
