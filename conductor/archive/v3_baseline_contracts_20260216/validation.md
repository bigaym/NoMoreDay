# V3 Baseline Contracts Validation

## 1. RenderConfig and Feature Flag Validation

1. V3 fields serialize/deserialize correctly with safe defaults.
2. Invalid config values are rejected with diagnostics.
3. `render.v3.enabled` runtime toggle does not crash or leak resources.

## 2. ABI Contract Validation

1. `GPU_ABI_VERSION=3` is enforced.
2. Layout snapshot checks pass for all V3 baseline structs.
3. C++/GLSL struct generation remains single-source and auditable.

## 3. RenderGraph Contract Validation

1. V3 pass order contract is locked and violations fail tests.
2. Frame Ownership assertions detect illegal target writes.
3. Only `Composite` is allowed to write final screen target (`FBO 0`).

## 4. Binding and GL State Validation

1. Binding registry conflict checks pass for global/pass-local domains.
2. Literal binding usage is rejected by gate checks.
3. Compute-to-fragment synchronization template includes required `glMemoryBarrier`.

## 5. Tier and Budget Validation

1. V3 capability matrix (`Low/Medium/High/Ultra`) queries are correct.
2. Degrade sequence matches locked order in design baseline.
3. Pass budget constants are present and consumable by gate runners.

## 6. Evidence Checklist

- [x] Unit tests report attached.
- [x] Integration checks report attached.
- [x] ABI snapshot output attached.
- [x] RenderGraph contract report attached.

## 7. Verification Evidence (2026-02-17)

1. Build and CI quick regression:
   - `.\build.bat` (pass)
2. Static analysis gate:
   - `.\build.bat clean-all analyze` (pass)
3. Perf gate:
   - `cmake --build build --config Release --target NoMoreDayTests --parallel` (pass)
   - `ctest --test-dir build -C Release -L performance --output-on-failure` (pass)
4. Unit/integration gates:
   - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` (pass)
   - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` (pass)
