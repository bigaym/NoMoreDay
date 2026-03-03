# C5-2 Results

## Package

- Package ID: C5-2
- Objective: End-to-end full validation report
- Scope used: `docs/reports/four-pillars/phase-5/C5-2/**`, `scripts/perf/**` (runner only), low-risk generator fix in `scripts/gen_tags.py` required to unblock matrix

## Matrix Outcome

| Command | Outcome | Notes |
|---|---|---|
| `./build.bat check` | PASS | Pre-check pipeline green. |
| `./build.bat debug` | PASS (after fix) | Initial failure due to missing `<array>` in generated `TagRegistry.hpp`. |
| `./build.bat` | PASS | RelWithDebInfo build successful. |
| `./build.bat release` | PASS | Release build successful. |
| `./build.bat analyze` | PASS | Static analysis warnings observed; no failing gate. |
| `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | PASS | 6/6 passed. |
| `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` | PASS | 4/4 passed. |
| `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | PASS | 3/3 passed. |
| `ctest --test-dir build -C Release -L performance --output-on-failure` | PASS | 1/1 passed. |

## Diagnostics + Low-Risk Fix Applied

- Initial blocker:
  - `src/game/data/TagRegistry.hpp(72,58): error C2027` undefined `std::array`.
- Root cause:
  - `scripts/gen_tags.py` generated a header using `std::array` without emitting `#include <array>`.
- Fix:
  - Added `#include <array>` to generated include block in `scripts/gen_tags.py`.
  - Regenerated `src/game/data/TagRegistry.hpp` via `python scripts/gen_tags.py`.
  - Re-ran `./build.bat debug` successfully.

## Performance and Stability Evidence

- Deterministic candidate run JSON:
  - `docs/reports/four-pillars/phase-5/C5-2/perf-deterministic-run.json`
  - Candidate median P95 metric: `0.006 ms`
- Phase 0 baseline JSON:
  - `docs/reports/four-pillars/phase-0/P0-2/perf-baseline-run.json`
  - Baseline median P95 metric: `0.006 ms`
- Delta vs baseline:
  - Absolute delta: `0.000 ms`
  - Relative delta: `0.00%`
  - Gate (`<= +5%`): PASS

- Long-run stability command:
  - `python scripts/perf/run_long_stability.py --profile phase5 --minutes 60`
  - Output JSON: `docs/reports/four-pillars/phase-5/C5-2/stability-phase5.json`
  - Summary:
    - Sample count: `6183`
    - Actual duration: `3600.013035 s`
    - Median: `0.006 ms`
    - P95: `0.007 ms`
    - Min/Max: `0.005 / 0.011 ms`

## Generated Artifacts

- `docs/reports/four-pillars/phase-5/C5-2/perf-deterministic-run.json`
- `docs/reports/four-pillars/phase-5/C5-2/stability-phase5.json`
- `docs/reports/four-pillars/phase-5/C5-2/perf-run-logs/` (raw deterministic run logs)
- `docs/reports/four-pillars/phase-5/C5-2/stability-run-logs/` (raw long-run logs)
