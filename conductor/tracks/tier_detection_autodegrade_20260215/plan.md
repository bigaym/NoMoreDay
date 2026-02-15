# Tier Detection & Auto-Degrade Plan

> **Track ID**: `tier_detection_autodegrade_20260215`

## Phase 1: Foundation

- [ ] Add capability probe API and data model in `QualityTierManager`.
- [ ] Add structured logging fields for probe and selection reason.
- [ ] Add tests for settings override precedence.

## Phase 2: Logic

- [ ] Implement hard-floor capability-to-tier mapping.
- [ ] Add baseline micro-benchmark hook and final tier arbitration.
- [ ] Persist detection output and final selected tier metadata.

## Phase 3: Integration

- [ ] Introduce runtime budget pressure monitor using `RenderProfiler` stats.
- [ ] Implement staged auto-degrade policy with cooldowns and hysteresis.
- [ ] Wire degrade actions to render config knobs without changing default visuals.

## Phase 4: Polish & Tests

- [ ] Extend `RenderingBenchmark` with reproducible tier/degrade profiles.
- [ ] Add regression tests for degrade/recover transitions.
- [ ] Run `build.bat` and performance suite; document thresholds and results.

## Acceptance Gates (DoD)

- [ ] Quantified thresholds: degrade trigger and recover trigger use fixed budget numbers (ms/frame) per tier, with hysteresis >= 20% gap and cooldown >= 3 s; values are committed in config/docs and covered by tests.
- [ ] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` including startup probe, user override precedence, resize rebuild, and Alt+Tab/context restore without oscillation.
- [ ] ABI migration policy documented and enforced: capability/tier metadata version is explicit; old cache/config entries are either migrated or rejected with deterministic warning/error messaging.
