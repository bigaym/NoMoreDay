# Phase 1 / A1-2 Results

## Outcome

- Package status: `bounded-slice-complete`
- Converged runtime material GPU ABI surface to canonical `GPUMaterialDataV3` by removing legacy material payload definitions from active engine headers.
- Removed dead duplicate-generation structures (`GPUMaterialData` and `GPUMaterialDataV2`) from `GPUData.hpp`.

## Convergence change

- Canonical structure retained: `components::GPUMaterialDataV3` (128B, alignas(16)).
- Removed legacy structures:
  - `components::GPUMaterialData` (64B legacy schema)
  - `components::GPUMaterialDataV2` (intermediate 128B schema)
- Evidence of dead/duplicate path removal:
  - Repository C++ search for `\bGPUMaterialData\b` and `\bGPUMaterialDataV2\b` now returns no runtime code references.
  - Runtime material upload path in `MaterialManager` already uses `GPUMaterialDataV3`; this slice removes stale ABI definitions only.

## Verification

- `./build.bat` -> PASS (`NoMoreDay`, `NoMoreDayTests` built successfully)
- `./bin/NoMoreDayTests.exe --test-case="*RenderGraph V5 Contracts*"` -> PASS (`3` cases, `9` assertions, `0` failed)
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS (`1/1`, `2.63s`)

## Changed files

- `src/engine/render/GPUData.hpp`
- `docs/reports/four-pillars/phase-1/A1-2/results.md`
- `docs/reports/four-pillars/phase-1/A1-2/residual-risk.md`
