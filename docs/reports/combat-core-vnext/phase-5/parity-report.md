# Combat V2 Parity Report - Phase 5 (P5-C)

## Scope

- Gate command: `ctest --test-dir build -C RelWithDebInfo -R "^nmd\.tests\.combat\.(integration|unit|parity\.unit|perf\.baseline)$" --output-on-failure`
- Focused parity command: `./bin/NoMoreDayTests.exe --test-case="[Integration] CombatV2DualRunParity*"`

## Outcomes

- Combat module gate set: PASS (4/4 test groups passed)
- `nmd.tests.combat.parity.unit`: PASS
- `CombatV2DualRunParity*`: PASS (4 test cases, 30 assertions, 0 failed)

## Evidence Notes

- Dual-run mismatch artifacts were observed as expected in strict checks, with tolerant checks classified as tolerance matches.
- No blocker conditions were observed for parity acceptance in this package; emitted mismatches align with current stub facade behavior.
