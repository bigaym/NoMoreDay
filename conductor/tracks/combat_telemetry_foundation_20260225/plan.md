# Combat Telemetry Foundation — Implementation Plan

> Track ID: `combat_telemetry_foundation_20260225` | Series: CS-M2-05  
> Depends on: CS-M1-04

---

## Phase 1: Telemetry Infrastructure

- [ ] Create `CombatTelemetry` singleton with metrics collection API.
- [ ] Implement sliding window statistics (avg/p95/p99).
- [ ] Add compile-time flag `COMBAT_TELEMETRY_ENABLED`.
- [ ] Add runtime toggle for metrics output.

## Phase 2: Metrics Integration

- [ ] Instrument `DamagePipeline::Calculate` with timing.
- [ ] Instrument `StatsSystem::GetStatWithTags` with call/cache counters.
- [ ] Instrument `CombatEventDispatcher` with per-frame counting.
- [ ] Instrument trigger guards with interception statistics.
- [ ] Instrument summon system with entity/event counting.

## Phase 3: Output & Gate

- [ ] Implement log/terminal output formatter.
- [ ] Verify overhead < 0.1ms (benchmark test).
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- CombatTelemetry subsystem.
- Instrumented combat systems.
- Performance overhead validation.
