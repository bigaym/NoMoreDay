# Render V3 Clustered Shader Hardening Validation

## 1. Required Commands

1. `build.bat`
2. `build.bat analyze`
3. `build.bat perf`
4. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## 2. Mandatory Evidence

- [x] Integration report includes `ClusteredLightingIntegrationTest` pass.
- [x] No compute shader compile errors for `light_culling.comp`.
- [x] Fallback diagnostics verified (negative scenario).
- [x] Perf benchmark emits clustered metric line(s).

## 3. Risk Regression Checklist

- [x] No binding conflict introduced.
- [x] No pass-order violation introduced.
- [x] No crash when clustered path is disabled.

## 4. Evidence Summary

1. Reproduction (before fix):
   - `build.bat clean-all` (2026-02-18) reported compute shader compile failure:
     - `light_culling.comp` error at `0(277)` / `0(289)`, token `.` unexpected.
   - Root cause pinned to local variable name `packed` (GLSL reserved word conflict in driver parser).
2. Fix:
   - Renamed local variable `packed` -> `packedLight` in `assets/shaders/lighting/light_culling.comp`.
   - Added deterministic negative-path test hook and assertion:
     - `LightCullingPass::SetComputeShaderPathForTesting(...)`
     - New integration case: forced missing shader path must report reason
       `failed to initialize light culling shader`.
   - Added binding alignment assertions for `BindingRegistry::LightCulling` symbols
     (`0/1/2/3/4/5`).
3. Validation commands:
   - `build.bat` -> PASS (CI nonperf pass).
   - `build.bat analyze` -> PASS (existing warnings only).
   - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS.
   - `build.bat perf` -> FAIL due unrelated pre-existing gate:
     - `[Performance] ParticleTrail - Scenario 4 SubEmitter 1k/frame`
     - observed `dispatchOverheadMs=0.261125` vs threshold `<0.2`.
   - Clustered-only performance rerun:
     - `bin\NoMoreDayTests.exe -tc="[Performance] Clustered Lighting*"` -> PASS.
     - Metrics emitted: `clustered_128_v2_mean_ms`, `clustered_128_mean_ms`,
       `clustered_128_improvement_pct`.

## 5. Final Verdict

- Status: `DONE (track scope)` with external perf-suite blocker tracked separately.
- Reviewer: Codex
- Date: 2026-02-18
