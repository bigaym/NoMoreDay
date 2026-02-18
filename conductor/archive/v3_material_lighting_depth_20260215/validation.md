# V3 Material Lighting Depth Validation

## 1. Schema Compatibility

1. v2 materials parse and validate correctly.
2. v1 materials auto-map to v2 defaults without crash.
3. Unsupported schema version fails with explicit error.

## 2. Rendering Consistency

1. Same light setup shows intended differences for:
   - smooth vs rough materials,
   - high vs low specular,
   - normal map on/off.
2. No severe temporal shimmer under camera movement.

## 3. Tier Behavior

1. Low/Medium disable expensive branches and remain stable.
2. High/Ultra enable full Material 2.0 path.
3. Tier transitions do not corrupt material state.

## 4. Performance

1. Measure `entity_mdi` and `particle` pass impact.
2. Gate: High-tier increase <= `0.6 ms`.
3. Clustered carry-over gate:
   - keep 128-light A/B no-regression hard gate (`clustered <= baseline * 1.05`);
   - recover `>=5%` mean improvement target for the agreed benchmark profile
     (migrated to `v3_validation_and_release_gate_20260215` / `F4.6`).

Status (2026-02-18):
- No-regression gate: pass.
- `>=5%` uplift gate: not yet recovered under current agreed profile
  (latest sampled run: `-2.99417%`), and is carried into Step F release gate.

## 5. Evidence Checklist

- [x] Unit tests attached.
- [x] Integration matrix attached.
- [x] Perf results attached.
- [x] Visual comparisons attached.

## 6. Acceptance Run Log (2026-02-18)

1. Build and static/perf gates:
   - `build.bat` -> pass
   - `build.bat analyze` -> pass
   - `build.bat perf` -> pass
2. CTest matrix:
   - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> pass
   - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> pass
   - `ctest --test-dir build -C Release -L performance --output-on-failure` -> pass
3. Material/ABI/visual contract evidence:
   - `[Unit] Material - BRDF-lite same-light different-material divergence`:
     `diff = 1.08197 > 0.05` (same-light different-material divergence proven).
   - `[Integration] Material Lighting - Cross-tier particle shader ABI path`:
     confirms `pbrLite/detailParams` ABI fields and cross-tier render no-throw.
   - `[Integration] Material Lighting - Schema v2 hot reload updates runtime data`:
     runtime material update (`roughness/specular/ao/heightBias`) verified.
4. Performance evidence:
   - `[Performance] Material Lighting - Tier overhead budgets`:
     `highOverhead(ms)=8.5e-06 <= 0.6`
     `mediumDelta(ms)=3.25e-06 <= max(0.15, low*0.25)`

## 7. Carry-Over D0 Validation Log (2026-02-18)

1. Optimization implementation:
   - Added packed clustered light payload write/read path (compute->fragment),
     preserving deterministic index/header outputs and fallback path.
2. Verification commands:
   - `build.bat` -> pass
   - `build.bat analyze` -> pass
   - `build.bat perf` -> pass
   - `NoMoreDayTests --test-case="[Integration] Clustered Lighting - Deterministic overflow and index output"` -> pass
   - `NoMoreDayTests --test-case="[Integration] Clustered Lighting - Culling to lighting consumption path"` -> pass
   - `NoMoreDayTests --test-case="[Performance] Clustered Lighting - Low-light no regression"` -> pass
   - `NoMoreDayTests --test-case="[Performance] Clustered Lighting - 128 lights A/B no regression"` ->
     sampled `Clustered mean(ms)=0.142065`, `V2 mean(ms)=0.137935`, `improvement=-2.99417%`
3. Conclusion:
   - D0.1 and D0.2 completed.
   - D0.3 (`>=5%`) ownership moved to `v3_validation_and_release_gate_20260215` / `F4.6`.
