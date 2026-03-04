# Phase 0 Performance Blocker Analysis

Date: 2026-03-03

## Scope

This analysis investigates why `ctest --test-dir build -C Release -L performance --output-on-failure`
fails during Phase 0, and whether the failures are caused by current Phase-0 package changes.

## Reproduction summary

1. Full perf suite gate (`-L performance`) fails with two benchmark assertions:
   - `tests/performance/ParticleTrailBenchmark.cpp`: `dispatchOverheadMs` over `< 0.2` gate.
   - `tests/performance/RenderGraphContractBenchmark.cpp`: `overheadP95Ms` over `<= 0.03` gate.
2. Focused repeat runs:
   - ParticleTrail Scenario 4 reproduces intermittently in isolation and fails consistently under full-suite load.
   - RenderGraph contract failure did not reproduce in focused repeats and appears near-margin/environment-sensitive.
3. Scoped module gate for current package passes:
   - `ctest --test-dir build -C RelWithDebInfo -R "^nmd\.tests\.combat\.(parity\.unit|perf\.baseline)$" --output-on-failure`
   - Result: 2/2 pass.

## Root-cause hypothesis

1. ParticleTrail benchmark has load/order sensitivity around a strict absolute threshold.
2. RenderGraph contract benchmark has micro-margin volatility close to threshold.
3. These failures are pre-existing relative to this phase package:
   - Current package touched only docs/tests for combat-v2 phase scaffolding.
   - Failing benchmark source files are outside touched scope.

## Conclusion

- Full perf suite remains red and must stay tracked as blocker debt.
- Under the phase exception protocol for untouched pre-existing perf failures,
  Phase 0 can be treated as **provisionally complete** because scoped module-gate tests pass
  and investigation evidence is recorded.

## Follow-up

1. Keep running full `-L performance` at each phase checkpoint.
2. Stabilize ParticleTrail/RenderGraph benchmark gating behavior in dedicated perf-hardening work.
3. Remove provisional status once full performance suite is green.
