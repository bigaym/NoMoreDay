# Phase 1 / A1-3 Results

## Outcome

- Package status: `bounded-slice-complete`
- Removed a concrete legacy render-pass route in `LightCullingPass` by deleting the `clusteredLightingV4Enabled` fallback branch that hard-capped culling inputs to `256` lights.
- Preserved clustered-lighting behavior for the converged path by always using canonical clustered capacity (`kMaxTotalClusteredLights`) and validating with render-focused contracts.

## Convergence change

- File: `src/engine/render/passes/LightCullingPass.cpp`
- Removed legacy toggle/path:
  - `useV4Clustering = config.clusteredLightingV4Enabled`
  - `if (!useV4Clustering) { lightCount = min(lightCount, 256u); }`
  - Uniform fallback `uMaxTotalClusteredLights = 256` when V4 toggle was off
- New behavior:
  - `lightCount` now always reflects active light records for clustered culling.
  - `uMaxTotalClusteredLights` now always uses `core::kMaxTotalClusteredLights`.

## Contract coverage

- Added render integration contract:
  - `tests/integration/ClusteredLightingIntegrationTest.cpp`
  - Case: `[Integration] Clustered Lighting - Legacy V4 gate removed for light culling`
- Contract assertion proves the removed path stays removed by forcing `clusteredLightingV4Enabled = false` and checking culling still uploads the full active light set (`>256`).

## Verification

- `./build.bat` -> PASS (`NoMoreDay`, `NoMoreDayTests` built successfully)
- `./bin/NoMoreDayTests.exe --test-case="*Legacy V4 gate removed for light culling*"` -> PASS (`1` case, `5` assertions, `0` failed)
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS (`1/1`, `2.37s`)

## Changed files

- `src/engine/render/passes/LightCullingPass.cpp`
- `tests/integration/ClusteredLightingIntegrationTest.cpp`
- `docs/reports/four-pillars/phase-1/A1-3/results.md`
- `docs/reports/four-pillars/phase-1/A1-3/residual-risk.md`
