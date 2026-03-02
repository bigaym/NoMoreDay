# P0-2 Baseline Measurement Scaffold

Status: draft template (fill with measured data before Phase 0 exit)

## Test status baseline

- Capture timestamp (UTC): `<YYYY-MM-DDTHH:MM:SSZ>`
- Branch/commit: `<branch>` / `<sha>`
- Command set:
  - `./build.bat check`
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
  - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
  - `ctest --test-dir build -C Release -L performance --output-on-failure`
- Results:
  - check: `<pass/fail>`
  - unit: `<pass/fail + totals>`
  - integration: `<pass/fail + totals>`
  - performance: `<pass/fail + totals>`
- Module mapping notes (ui/item/progression gaps): `<fill here>`

## Performance baseline placeholders

- Scene/profile id: `<profile-name>`
- Seed: `<seed>`
- Resolution: `<width>x<height>`
- Quality tier: `<tier>`
- Warmup runs: `1`
- Measured runs: `5`
- P95 frame time results (ms):
  - run1: `<value>`
  - run2: `<value>`
  - run3: `<value>`
  - run4: `<value>`
  - run5: `<value>`
- Median P95 (ms): `<value>`

## Hardware/profile placeholders

- OS build: `<value>`
- CPU: `<value>`
- GPU: `<value>`
- Driver version: `<value>`
- RAM: `<value>`
- Build config: `<RelWithDebInfo/Release>`
- Additional runtime flags: `<value>`

## Notes

- Keep this file as a stable template and add measured baseline snapshots in sibling reports.
