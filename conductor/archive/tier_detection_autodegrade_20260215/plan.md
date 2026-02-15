# Tier Detection & Auto-Degrade Plan

> **Track ID**: `tier_detection_autodegrade_20260215`

## Phase 1: Foundation

- [x] Add capability probe API and data model in `QualityTierManager`.
- [x] Add structured logging fields for probe and selection reason.
- [x] Add tests for settings override precedence.

## Phase 2: Logic

- [x] Implement hard-floor capability-to-tier mapping.
- [x] Add baseline micro-benchmark hook and final tier arbitration.
- [x] Persist detection output and final selected tier metadata.

## Phase 3: Integration

- [x] Introduce runtime budget pressure monitor using `RenderProfiler` stats.
- [x] Implement staged auto-degrade policy with cooldowns and hysteresis.
- [x] Wire degrade actions to render config knobs without changing default visuals.

## Phase 4: Polish & Tests

- [x] Extend `RenderingBenchmark` with reproducible tier/degrade profiles.
- [x] Add regression tests for degrade/recover transitions.
- [x] Run `build.bat` and performance suite; document thresholds and results.

## Phase: Review Fixes
- [x] Task: Apply review suggestions 513afda

## Acceptance Gates (DoD)

- [x] Quantified thresholds: degrade trigger and recover trigger use fixed budget numbers (ms/frame) per tier, with hysteresis >= 20% gap and cooldown >= 3 s; values are committed in config/docs and covered by tests.
- [x] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` including startup probe, user override precedence, resize rebuild, and Alt+Tab/context restore without oscillation.
- [x] ABI migration policy documented and enforced: capability/tier metadata version is explicit; old cache/config entries are either migrated or rejected with deterministic warning/error messaging.
