# Phase 1 Gate Results

- run_timestamp_local: 2026-03-03 23:46:27 +08:00
- workspace: `D:\PRJ\NoMoreDay`

## Command outcomes

| # | Command | Status | Key results |
|---|---|---|---|
| 1 | `./build.bat check` | PASS | Pre-check scripts completed; no migration/ABI/JSON/contract drift detected; check mode skipped compilation. |
| 2 | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` | PASS | 9/9 tests passed, 0 failed, total time 6.32s. |
| 3 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | PASS | 6/6 tests passed, 0 failed, total time 6.01s. |
| 4 | `ctest --test-dir build -C Release -L performance --output-on-failure` | PASS | 2/2 tests passed, 0 failed, total time 13.81s (`nmd.tests.performance`, `nmd.tests.combat.perf.baseline`). |
| 5 | `ctest --test-dir build -C RelWithDebInfo -R "^nmd\.tests\.combat\.(parity\.unit|perf\.baseline)$" --output-on-failure` | PASS | 2/2 tests passed, 0 failed, total time 1.28s. |
| 6 | `./bin/NoMoreDayTests.exe --test-case="[Unit] TagDomainV2*"` | PASS | doctest filter matched 3 cases: 3 passed, 0 failed (8 assertions passed). |

## Failure details (blocking)

- None.

## Phase 1 status

- All required phase gates for current Phase-1 changes are green, including full performance label and scoped combat checks.
- Exception protocol was not needed for this run.
- Phase 1 is **complete**.
