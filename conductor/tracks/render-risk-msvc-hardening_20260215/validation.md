# Validation Notes - render-risk-msvc-hardening_20260215

Date: 2026-02-15

## Command Matrix

| Command | Result | Evidence |
|---|---|---|
| `.\build.bat` | `PASS` (fail-fast gate validated) | Existing cache with `MinGW Makefiles` now fails immediately with actionable message: `Existing CMake generator "MinGW Makefiles" is unsupported... build.bat clean-all`. |
| `.\build.bat clean-all` | `PASS` | Clean MSVC configure/build/test completed under `Visual Studio 18 2026`; unit tests `196/196` passed. |
| `.\build.bat analyze` (after hardening) | `PASS` | `/analyze` path configures and completes in normal workspace without manual cache surgery. |
| `.\build.bat analyze` (warning/filter pass) | `PASS` | Third-party targets are no longer analyzed directly; analysis scope is limited to first-party targets with reduced log noise and faster incremental analyze cycle. |

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

- Phase 3: MDI benchmark contract and VFX material fallback closure.
- Phase 4: `build.bat perf` end-to-end gate verification.
- Phase 5: final closeout evidence and decision log finalization.
