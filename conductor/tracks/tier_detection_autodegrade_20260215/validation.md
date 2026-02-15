# Tier Detection & Auto-Degrade Validation

## Fixed Threshold Contract

Source: `src/engine/render/core/QualityTierManager.cpp::GetAutoDegradeBudgetThresholds`.

| Tier | Degrade Trigger (ms) | Recover Trigger (ms) | Hysteresis Gap | Sustain (s) | Cooldown (s) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Low | 7.2 | 5.6 | 22.2% | 3.0 | 3.0 |
| Medium | 10.5 | 8.2 | 21.9% | 3.0 | 3.0 |
| High | 13.5 | 10.5 | 22.2% | 3.0 | 3.0 |
| Ultra | 16.0 | 12.5 | 21.9% | 3.0 | 3.0 |

Contract checks are covered by:
- `tests/unit/QualityTierManagerTest.cpp` (`AutoDegrade Threshold Contract`)
- `tests/unit/QualityTierManagerTest.cpp` (`Legacy Metadata Version Migration`)

## Cross-Tier Regression Evidence

- Startup probe + user override precedence:
  - `tests/unit/QualityTierManagerTest.cpp`
- Low/Medium/High/Ultra matrix with resize + context restore:
  - `tests/integration/RenderGraphTierMatrixIntegrationTest.cpp`
- Auto-degrade sequence/recover behavior:
  - `tests/unit/QualityTierManagerTest.cpp`

## Benchmark Evidence (2026-02-15)

Command:
- `build.bat ninja perf`

Relevant outputs:
- `Scenario G (TierAutoDegrade): Mean=0.000ms, P99=0.002ms`
- `bench_rendering_system: ... p95_ratio=0.989`

These runs include reproducible tier/degrade profiles in:
- `tests/performance/RenderingBenchmark.cpp` (`Scenario G`)
