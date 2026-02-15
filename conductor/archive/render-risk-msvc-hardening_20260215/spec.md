# Render Risk Closure and MSVC Hard Cutover Specification

> Track ID: `render-risk-msvc-hardening_20260215`  
> Type: `bug+refactor`  
> Priority: `P0`  
> Status: `Resolved`  
> Date: `2026-02-15`

## 1. Context and Evidence

This track is created from combined static-analysis intent and real test evidence collected on 2026-02-15.

### 1.1 Test and Build Evidence

- `build.bat`: succeeded; non-performance tests passed `196/196`.
- `NoMoreDayTests.exe --test-case=*performance*`: passed `57/57`.
- `build.bat analyze`: blocked by generator mismatch (`Visual Studio 18 2026` vs cached `MinGW Makefiles`).
- `build.bat perf`: blocked by the same generator mismatch.

### 1.2 Risk Signals from Runtime Logs

- Rendering benchmark shows `MDI` slower than legacy in current scenario (`~0.72ms` vs `~0.12ms`).
- VFX runtime repeatedly reports material fallback (`unknown material`, invalid material IDs in sequence assets).
- Some benchmark targets exceeded (not failing tests, but failing intended budget intent), e.g. FlowField and ItemFactory batches.

### 1.3 Static-Analysis Alignment

Historical audits in `conductor/analyzer/` repeatedly flag rendering maintainability/performance fragility (MDI path complexity, contract drift risk).  
Current blocker: static analysis entrypoint cannot be run reliably due build generator state conflict.

## 2. Mandatory Decisions (Locked)

1. Toolchain is MSVC-only from now on.
2. No GCC/MinGW fallback remains in project CMake or build scripts.
3. If no supported Visual Studio generator/toolchain is found, build must fail immediately with explicit error.
4. Build entrypoints `build.bat`, `build.bat analyze`, `build.bat perf` must all work in a clean and previously-used workspace.

## 3. Scope

## In Scope

- Build system hard cutover to MSVC-only.
- Removal of GCC/MinGW-specific configuration paths in root build logic.
- Deterministic generator behavior and explicit failure rules.
- Rendering risk closure for issues confirmed by tests/logs:
  - MDI benchmark regression investigation and correction plan.
  - VFX material fallback elimination.
  - Budget-over-target hot paths tracked and gated.

## Out of Scope

- Engine migration away from CMake.
- Full renderer redesign.
- Non-render gameplay balancing changes unrelated to measured risk.

## 4. Technical Contract

## 4.1 Toolchain Contract

- Top-level CMake config must hard-fail when compiler is not MSVC.
- `build.bat` must only select supported Visual Studio generators.
- No Ninja/MinGW fallback path for generator selection.
- Existing mismatched cache/generator state must be handled deterministically:
  - either automatic clean+reconfigure with selected MSVC generator,
  - or hard-fail with actionable message.
  - This track adopts hard-fail by default for predictability.

## 4.2 Static Analysis Contract

- `build.bat analyze` is a required gate and must execute without manual cache surgery.
- `/analyze` path is validated on MSVC build.
- Failures in analysis stage block completion.

## 4.3 Rendering Risk Contract

- MDI benchmark path must have a justified and stable expectation:
  - either real optimization improvement in covered scenario, or
  - benchmark definition updated with rationale and explicit pass/fail criteria per scene type.
- VFX sequence material references must resolve without runtime fallback spam in normal asset set.
- Over-budget benchmarks must be either optimized or have tracked exceptions documented with mitigation.

## 5. Data Model and Ownership

No ECS gameplay component schema change is required for this track.  
This track introduces build/pipeline governance artifacts only.

Governance data additions:

- `ToolchainPolicy` (build-script/CMake enforced, code-level constants optional)
  - `allowed_compilers = ["MSVC"]`
  - `allowed_generators = ["Visual Studio 17 2022", "Visual Studio 18 2026"]`
  - `fallback_enabled = false`

- `PerformanceGateSnapshot` (test log export or markdown table)
  - `scenario`
  - `mean_ms`, `p99_ms`
  - `target_ms`
  - `status`

## 6. Acceptance Criteria

- [x] AC-01: Root CMake hard-fails on non-MSVC compiler.
- [x] AC-02: No GCC/MinGW fallback logic remains in `build.bat` and root `CMakeLists.txt`.
- [x] AC-03: `build.bat` fails immediately when no supported VS generator is available.
- [x] AC-04: `build.bat analyze` runs successfully from a normal developer workspace.
- [x] AC-05: `build.bat perf` runs successfully from a normal developer workspace.
- [x] AC-06: VFX runtime no longer emits unknown-material fallback warnings for shipped baseline assets.
- [x] AC-07: MDI benchmark expectation is corrected by optimization or by explicit scenario-specific gating policy.
- [x] AC-08: All risk/exception decisions are documented in track validation notes with evidence.

## 6.1 Acceptance Code Mapping

| Code | Summary | Planned Phase |
|---|---|---|
| AC-01 | CMake hard-fails on non-MSVC | Phase 1 |
| AC-02 | Remove GCC/MinGW fallback logic | Phase 1 |
| AC-03 | Missing VS generator => immediate fail | Phase 1 |
| AC-04 | `build.bat analyze` runnable | Phase 2 |
| AC-05 | `build.bat perf` runnable | Phase 4 |
| AC-06 | Baseline VFX assets no material fallback spam | Phase 3 |
| AC-07 | MDI benchmark contract corrected/justified | Phase 3 |
| AC-08 | Validation evidence complete | Phase 5 |

## 7. Risks and Mitigations

- Risk: Third-party CMake minimum-policy noise causes false confidence.
  - Mitigation: enforce clear top-level error policy and normalize build directory assumptions.
- Risk: Removing fallback paths breaks some local environments.
  - Mitigation: explicit prerequisite message; fail-fast instead of implicit fallback.
- Risk: Benchmark variance hides regression.
  - Mitigation: use fixed scenes/profile labels and record mean+p95/p99 over stable sample windows.
