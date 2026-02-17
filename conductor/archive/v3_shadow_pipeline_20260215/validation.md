# V3 Shadow Pipeline Validation

## 1. Contract Checks

1. ABI version is `3` in both CPU and shader generated include.
2. RenderGraph order includes: `LightCulling -> ShadowPrepare -> ShadowBuild -> ShadowResolve -> Lighting`.
3. No pass except final composite writes to `FBO 0`.

## 2. Functional Matrix

1. Tiers: `Low/Medium/High/Ultra`.
2. Frame targets: default framebuffer and offscreen framebuffer.
3. Runtime events: startup, resize, Alt+Tab/context restore, hot reload.

Expected:

1. `Medium` shadows are disabled by policy.
2. `High` uses SDF shadow without atlas dependency.
3. `Ultra` uses hybrid path with stable fallback when atlas overflows.

## 3. Determinism

1. Key-light selection order is deterministic for same input.
2. Atlas eviction is deterministic and logged with counters.
3. Shadow fallback path emits one structured warning category.

## 4. Performance

1. Capture per-pass `mean/p95/p99`.
2. Track overhead relative to V2 baseline scene.
3. Gate:
   - High <= `0.8 ms`
   - Ultra <= `1.3 ms`

## 5. Evidence Checklist

- [x] Unit tests report attached.
- [x] Integration tests report attached.
- [x] Performance JSON+CSV attached.
- [x] Visual diff captures attached.

## 6. Verification Evidence (2026-02-17)

1. Build and CI quick regression:
   - `.\build.bat` (pass)
2. Static analysis gate:
   - `.\build.bat analyze` (pass)
3. Performance gate:
   - `.\build.bat perf` (pass)
   - `.\bin\NoMoreDayTests.exe --test-case="[Performance] Shadow Pipeline - Tier budgets (mean/p95/p99)"` (pass)
     - High: `mean=0.0062ms`, `p95=0.0062ms`, `p99=0.0073ms`
     - Ultra: `mean=0.0144ms`, `p95=0.0148ms`, `p99=0.0167ms`
4. Targeted integration verification:
   - `.\bin\NoMoreDayTests.exe --test-case="[Integration] Shadow Pipeline*"` (3 passed, 0 failed)
