# Phase-2 P2-C Evidence Results

Date: 2026-03-04
Workspace: `D:\PRJ\NoMoreDay`

## Gate outcomes

1. `./build.bat check` -> PASS
   - Pre-check pipeline completed; check mode skipped compilation as expected.

2. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS
   - 9/9 tests passed, 0 failed.

3. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS
   - 6/6 tests passed, 0 failed.

4. `ctest --test-dir build -C Release -L performance --output-on-failure` -> PASS
   - 2/2 tests passed, 0 failed.

5. `ctest --test-dir build -C RelWithDebInfo -R "^nmd\.tests\.combat\.(unit|parity\.unit|perf\.baseline)$" --output-on-failure` -> PASS
   - 3/3 tests passed, 0 failed.

6. `./bin/NoMoreDayTests.exe --test-case="[Unit] ConditionIR*"` -> PASS
   - doctest cases: 4 passed, 0 failed (526 skipped).

## Exception protocol status

- Full performance suite completed successfully; provisional exception protocol not required.

## Phase decision

- Phase-2 package P2-C status: COMPLETE (all requested gates passed).
