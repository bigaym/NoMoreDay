# Performance Baseline

## Run Metadata
- Date: 2026-02-10
- Command: `./bin/NoMoreDayTests.exe -tc="[Performance]*"`
- Result: `41 passed, 0 failed, 133 skipped`

## New Benchmarks (Track: performance_test_expansion)

| Benchmark | Mean (ms) | P99 (ms) | Target | Status |
|---|---:|---:|---|---|
| DamagePipeline Single Calculate | 0.000 | 0.000 | Mean < 0.01 | PASS |
| DamagePipeline Batch 200 | 0.035 | 0.122 | Mean < 1.0, P99 < 2.0 | PASS |
| PhysicsSystem updateAll 10K | 0.183 | 0.476 | Mean < 3.0, P99 < 5.0 | PASS |
| PhysicsSystem high density | 0.369 | 0.822 | Stress baseline | PASS |
| PhysicsSystem force fields | 0.193 | 0.287 | Mean < 0.5 | PASS |
| ProjectileSystem 500 | 0.507 | 1.003 | Mean < 1.0, P99 < 2.0 | PASS |
| ProjectileSystem 2000 | 1.593 | 2.672 | Mean < 4.0 | PASS |
| AISystem update 5000 | 0.148 | 0.302 | Mean < 2.0, P99 < 4.0 | PASS |
| SkillSystem update 100 | 0.002 | 0.004 | Mean < 0.5, P99 < 1.0 | PASS |
| SkillSystem UpdateCooldowns batch | 0.135 | 0.673 | Mean < 0.3 | PASS |
| HazardSystem update 200 | 0.025 | 0.354 | Mean < 0.5, P99 < 1.0 | PASS |
| MonsterAffixSystem update 500 | 0.015 | 0.053 | Mean < 0.5, P99 < 1.0 | PASS |
| GPUFlowFieldSystem update 256x256 | 0.785 | 1.398 | Mean < 0.8, P99 < 1.5 | PASS |
| GPUFlowFieldSystem crowd density 5000 | 0.469 | 1.367 | Mean < 0.5 | WARN (P99 high) |
| EnemySpawnSystem batch spawn 100 | 0.108 | 0.290 | < 5.0 total | PASS |
| EnemySpawnSystem updateEnemySpawning | 0.000 | 0.000 | Baseline | PASS |
| SaveManager createSnapshot 1000 | 0.085 | 0.133 | Mean < 10.0 | PASS |
| SaveManager restoreFromSnapshot 1000 | 0.083 | 0.096 | Mean < 15.0 | PASS |
| ItemFactory createWeapon 1000 | 6.485 | 8.450 | < 5.0 total | WARN |
| ItemFactory createArmor 1000 | 6.479 | 8.362 | < 5.0 total | WARN |
| ItemFactory legendary affix stress | 14.972 | 17.192 | Baseline | PASS |
| FogOfWar updateVisibility 256x256 | 0.438 | 1.234 | Mean < 0.3, P99 < 0.8 | WARN |
| FogOfWar syncToCPU | 0.479 | 1.734 | Baseline | PASS |

## Existing Performance Benchmarks (selected)
- DropSystem mass drop: `224 us` (now enabled, no longer skipped).
- Rendering Scenario C (Entities): `Mean 0.594 ms`, `P99 1.615 ms`.
- GPUEntitySync PhysicsSync: `Mean 0.297 ms`, `P99 0.420 ms`.
- GPUEntitySync VisualSync: `Mean 0.306 ms`, `P99 1.822 ms`.

## Notes
- This baseline is environment-dependent (GPU/driver/CPU sensitive).
- Full suite remains green; target overruns are reported as `WARN` by design.
