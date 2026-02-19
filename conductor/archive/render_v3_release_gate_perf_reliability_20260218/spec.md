# Render V3 Release Gate Perf Reliability Spec

> **Track ID**: `render_v3_release_gate_perf_reliability_20260218`  
> **Type**: `bugfix+quality`  
> **Priority**: P0  
> **Depends On**: `v3_validation_and_release_gate_20260215`, `render_v3_release_gate_strict_closeout_20260218`  
> **Targets**: `BUG-20260218-001`, `BUG-20260218-004`

## 1. Goal

Close all Open bugs in `conductor/bug_registry.md` that block strict release-gate closure:
1. `BUG-20260218-001` (`F4.6` clustered uplift gate)
2. `BUG-20260218-004` (`F4.3/F4.5` batch-gate non-determinism)

## 2. Data Model

```cpp
struct PerfGateSample {
  std::string checkId;             // F4.3/F4.5/F4.6
  double value = 0.0;              // parsed metric value
  double threshold = 0.0;          // gate threshold
  uint64_t runSeed = 0;            // deterministic seed for perf scenario
  uint32_t warmupFrames = 0;
  uint32_t measureFrames = 0;
};

struct PerfGateStabilitySummary {
  std::string profileId;           // stress_144 / clustered_128
  double mean = 0.0;
  double p95 = 0.0;
  double coefficientOfVariation = 0.0;
  uint32_t consecutivePasses = 0;  // for waiver exit criteria
  bool stable = false;
};
```

Alignment/ownership constraints:
1. CPU-side benchmark structs remain POD/standard-layout.
2. No heap allocation in hot benchmark loops.
3. Do not change GPU ABI layout for this track.

## 3. ECS Components / Systems

1. **Component**: no new gameplay components.
2. **System**: adjust performance benchmark and gate evaluation behavior in:
   - `tests/performance/RenderGraphContractBenchmark.cpp`
   - `tests/performance/ClusteredLightingBenchmark.cpp`
   - `scripts/v3_release_gate.py`
3. **Singleton/Global services**:
   - `render::core::QualityTierManager`
   - `render::lighting::LightManager`
   - `render::lighting::ClusteredLightingState`

## 4. Persistence

Waiver and evidence synchronization must remain JSON-compatible:

```json
{
  "waiverId": "WVR-20260218-F4.3-001",
  "status": "retired",
  "retiredAtUtc": "2026-02-18T00:00:00Z",
  "evidence": {
    "gateRuns": 3,
    "stress_144_pass": true,
    "regression_compare_pass": true
  }
}
```

## 5. Scope

1. Stabilize `F4.3` stress benchmark in batch-gate context.
2. Remove false coupling that causes `F4.5` derived regression check to flap.
3. Recover `F4.6` clustered uplift to meet `>=5%` target with reproducible evidence.
4. Remove/retire related waivers and close linked bugs after verification.

## 6. Acceptance Criteria

1. `build.bat` and `build.bat analyze` pass.
2. `ctest --test-dir build -C Release -L performance --output-on-failure` passes.
3. Three consecutive runs of  
   `python scripts/v3_release_gate.py --build-dir build --config Release --allow-missing-screenshots --final-verification`  
   satisfy:
   - `F4.3` = pass (no waiver)
   - `F4.5` = pass (no waiver)
   - `F4.6` = pass (`clustered_128_improvement_pct >= 5.0`)
4. `conductor/validation/v3_gate_waivers.json` has no active waiver linked to `BUG-20260218-001` or `BUG-20260218-004`.
5. `conductor/bug_registry.md` updates both bugs to at least `Verified` with command evidence.
