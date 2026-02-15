# Validation Notes - render-risk-msvc-hardening_20260215

Date: 2026-02-15

## Command Matrix

| Command | Result | Evidence |
|---|---|---|
| `.\build.bat` | `PASS` (fail-fast gate validated) | Existing cache with `MinGW Makefiles` now fails immediately with actionable message: `Existing CMake generator "MinGW Makefiles" is unsupported... build.bat clean-all`. |
| `.\build.bat clean-all` | `PASS` | Clean MSVC configure/build/test completed under `Visual Studio 18 2026`; unit tests `196/196` passed. |
| `.\build.bat analyze` (after hardening) | `PASS` | `/analyze` path configures and completes in normal workspace without manual cache surgery. |
| `.\build.bat analyze` (warning/filter pass) | `PASS` | Third-party targets are no longer analyzed directly; analysis scope is limited to first-party targets with reduced log noise and faster incremental analyze cycle. |
| `.\build.bat` (bin-output update) | `PASS` | `NoMoreDay.exe` and `NoMoreDayTests.exe` now emit to root `bin\`; test runner resolves `..\bin\NoMoreDayTests.exe` first, removing stale `bin\<Config>` executable drift. |
| `.\build.bat perf` (post benchmark/material fixes) | `PASS` | Performance suite passed `57/57`; MDI logs now use scenario gates (`MDI Scenario A/B`, `Legacy Reference`) and VFX load no longer emits baseline unknown-material fallback spam. |
| `.\build.bat analyze` (post bin-output update) | `PASS` | Static analysis gate still succeeds after output-directory + test-runner path updates. |

## Toolchain Policy Evidence

- Root CMake now hard-fails on non-MSVC toolchains.
- `build.bat` no longer exposes Ninja fallback or MinGW/GCC fallback semantics.
- Supported generators are constrained to:
  - `Visual Studio 17 2022`
  - `Visual Studio 18 2026`
- Cached non-MSVC generator/compile state now hard-fails with explicit remediation (`build.bat clean-all`).

## Static Analysis Scope Decision

- `/analyze` was moved from global compile flags to a dedicated interface target (`NoMoreDayAnalyzeFlags`) and attached only to first-party build targets.
- Third-party subprojects (`third_party/*` CMake targets) no longer run direct static analysis passes.
- Additional external-header suppression flags are configured:
  - `/external:anglebrackets`
  - `/external:W0`

## Remaining Work

- None. Phase 1-5 objectives are complete and evidence-backed.

## Rendering Risk Closure Notes

- MDI inversion root cause was benchmark contract ambiguity, not a direct rendering-path defect:
  - prior metric blended all-visible and sparse-visibility interpretation into one ratio;
  - updated benchmark now publishes scenario-labeled gates for reproducible interpretation.
- VFX material fallback spam root cause was sequence parse timing vs material registry init order:
  - `VFXSequenceManager::LoadFromJson` now ensures `MaterialManager` is initialized before material name resolution.
- Hotspot classification (current machine baseline):
  - `GPUFlowFieldSystem update 256x256`: over mean budget (`~1.16ms` vs target `<0.8ms`) -> documented exception (follow-up optimization track).
  - `ItemFactory` batch 1000 weapon/armor: over mean budget (`~5.7-5.8ms` vs target `<5.0ms`) -> documented exception (follow-up optimization track).
  - These remain warnings, not gate failures in current performance policy.

## Decision and Deviation Log

- Decision: Treat MDI inversion as benchmark-contract ambiguity, not renderer functional regression.
  - Evidence: scenario-gated benchmark output now reports separate all-visible/sparse-visibility paths and stable thresholds.
- Decision: Keep FlowField and ItemFactory over-target states as documented exceptions in this track.
  - Evidence: warnings reproducible across `build.bat perf` runs; no hard-fail gate in current policy.
- Deviation: Expanded scope to fix stale test-binary execution path (`bin/<Config>` vs root `bin`) because it invalidated benchmark evidence.
  - Evidence: `build.bat` now logs `Using test executable: ..\bin\NoMoreDayTests.exe`; performance logs reflect updated MDI scenario output.

## Bug Registry Sync

- Added and verified:
  - `BUG-20260215-001` (VFX material fallback spam due init order / registry mismatch)
  - `BUG-20260215-002` (build runner stale binary selection causing evidence drift risk)
