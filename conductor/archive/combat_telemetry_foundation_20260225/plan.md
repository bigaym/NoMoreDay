# Combat Telemetry Foundation — Implementation Plan

> Track ID: `combat_telemetry_foundation_20260225` | Series: CS-M2-05  
> Depends on: CS-M1-04

---

## Phase 1: Telemetry Infrastructure

- [x] Create `CombatTelemetry` singleton with metrics collection API.
- [x] Implement sliding window statistics (avg/p95/p99).
- [x] Add compile-time flag `COMBAT_TELEMETRY_ENABLED`.
- [x] Add runtime toggle for metrics output.

## Phase 2: Metrics Integration

- [x] Instrument `DamagePipeline::Calculate` with timing.
- [x] Instrument `StatsSystem::GetStatWithTags` with call/cache counters.
- [x] Instrument `CombatEventDispatcher` with per-frame counting.
- [x] Instrument trigger guards with interception statistics.
- [x] Instrument summon system with entity/event counting.

## Phase 3: Output & Gate

- [x] Implement log/terminal output formatter.
- [x] Verify overhead < 0.1ms (benchmark test).
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- CombatTelemetry subsystem.
- Instrumented combat systems.
- Performance overhead validation.
