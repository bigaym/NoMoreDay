# Phase 0 / P0-2 Results

## Outcome

- Package status: `complete`
- Deterministic perf protocol captured with fixed scenario, one warmup run, and five measured runs.
- Host profile capture is now automated and recorded in package artifacts.

## Key verification

- `python scripts/perf/capture_host_profile.py` -> PASS (writes `host-profile.json`)
- `python scripts/perf/run_deterministic_p95_baseline.py` -> PASS
  - warmup: `1`
  - measured runs: `5`
  - measured values (`p95_proxy_p99_ms`): `0.006`, `0.006`, `0.006`, `0.007`, `0.007`
  - median (`p95_proxy_p99_ms`): `0.006`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS (`1/1`, `1.51s`)
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS (`1/1`, `2.35s`)
- `ctest --test-dir build -C Release -L performance --output-on-failure` -> PASS (`1/1`, `13.10s`)

## Gap signal (heuristic)

- `progression`: 0 matching test files
- `ui`: 6 matching test files
- `item`: 8 matching test files

## Linked artifacts

- `baseline-template.md`
- `baseline-2026-03-03.md`
- `test-map.md`
- `test-map.json`
- `perf-baseline-run.json`
- `host-profile.json`
