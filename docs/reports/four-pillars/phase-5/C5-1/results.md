# C5-1 Results

## Scope Delivered

- Expanded CTest module-gate registration in `tests/CMakeLists.txt` for `ui`, `item`, and existing `progression` coverage.
- Added explicit CI-friendly gate entries discoverable by dedicated CTest names and label filters.
- Preserved existing broad `unit` and `integration` gate labels while layering module labels on top.
- Verified registration and execution through build, `ctest -N` discovery, and label-scoped runs.

## Gate Mapping

- `nmd.tests.ui.unit`
  - Command: `NoMoreDayTests --test-case=[Unit]*UI*`
  - Labels: `unit;ui;ci`
- `nmd.tests.ui.integration`
  - Command: `NoMoreDayTests --test-case=[Integration]*UI*`
  - Labels: `integration;ui;ci`
- `nmd.tests.item.unit`
  - Command: `NoMoreDayTests --test-case=[Unit]*Item*`
  - Labels: `unit;item;ci`
- Existing progression gates retained:
  - `nmd.tests.progression.unit` -> labels `unit;progression;ci`
  - `nmd.tests.progression.integration` -> labels `integration;progression;ci`

## Verification Commands

1. `./build.bat notest`
2. `ctest --test-dir build -C RelWithDebInfo -N`
3. `ctest --test-dir build -C RelWithDebInfo -N -L ui`
4. `ctest --test-dir build -C RelWithDebInfo -N -L item`
5. `ctest --test-dir build -C RelWithDebInfo -N -L progression`
6. `ctest --test-dir build -C RelWithDebInfo -N -L unit`
7. `ctest --test-dir build -C RelWithDebInfo -N -L integration`
8. `ctest --test-dir build -C RelWithDebInfo -L ui --output-on-failure`
9. `ctest --test-dir build -C RelWithDebInfo -L item --output-on-failure`
10. `ctest --test-dir build -C RelWithDebInfo -L progression --output-on-failure`
11. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
12. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## Verification Results

- Build passed (`./build.bat notest`).
- `ctest -N` discovery shows 9 registered tests including new module gates:
  - `nmd.tests.ui.unit`, `nmd.tests.ui.integration`, `nmd.tests.item.unit`
- Label dry-run discovery:
  - `-L ui` -> 2 tests
  - `-L item` -> 1 test
  - `-L progression` -> 2 tests
  - `-L unit` -> 4 tests
  - `-L integration` -> 3 tests
- Label execution runs:
  - `-L ui` passed (`2/2`)
  - `-L item` passed (`1/1`)
  - `-L progression` passed (`2/2`)
  - `-L unit` passed (`4/4`)
  - `-L integration` passed (`3/3`)
