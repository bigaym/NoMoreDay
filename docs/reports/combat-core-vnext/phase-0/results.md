# Phase 0 Gate Results

- run_timestamp_local: 2026-03-03 23:17:58 +08:00
- workspace: `D:\PRJ\NoMoreDay`

## Command outcomes

| # | Command | Status | Key results |
|---|---|---|---|
| 1 | `./build.bat check` | PASS | Pre-check scripts completed; no drift/regression gates failed; check mode skipped compilation. |
| 2 | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` | PASS | 9/9 tests passed, 0 failed, total time 6.22s. |
| 3 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | PASS | 6/6 tests passed, 0 failed, total time 7.15s. |
| 4 | `ctest --test-dir build -C Release -L performance --output-on-failure` | FAIL | 1/2 tests passed; `nmd.tests.performance` failed; doctest: 69 cases (67 passed, 2 failed, 454 skipped); 30731 assertions (2 failed); total time 19.85s. |
| 5 | `ctest --test-dir build -C RelWithDebInfo -R "^nmd\.tests\.combat\.(parity\.unit|perf\.baseline)$" --output-on-failure` | PASS | 2/2 tests passed, 0 failed; scoped module-gate checks green for this phase package. |

## Failure details (blocking)

- `tests/performance/ParticleTrailBenchmark.cpp` scenario "SubEmitter 1k/frame": `dispatchOverheadMs = 0.373679` exceeded gate `< 0.2`.
- `tests/performance/RenderGraphContractBenchmark.cpp` contract guard: `overheadP95Ms = 0.0329` exceeded gate `<= 0.03`.
- CTest summary: `nmd.tests.performance` failed under `performance` label.
- Root-cause investigation: see `docs/reports/combat-core-vnext/phase-0/performance-blocker-analysis.md`.

## Phase 0 status

- Full performance suite is red due to pre-existing failures outside touched Phase-0 package files.
- Scoped module-gate tests for this package are green.
- Under exception protocol, Phase 0 is **provisionally complete** and blocked only on unrelated suite-level perf issues.
