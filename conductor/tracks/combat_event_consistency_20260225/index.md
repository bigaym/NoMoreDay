# Track: Combat Event Consistency

**ID:** `combat_event_consistency_20260225`  
**Status:** Not Started  
**Type:** bugfix/P0-convergence  
**Priority:** P0  
**Milestone:** M1  
**Series:** CS-M1-04

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 0/3 complete
- **Tasks:** 0/10 complete

## Scope Summary

- `CalculateBatch` 中所有事件载荷统一使用 `final_damage`（经反制/拦截后的实际值）。
- 消除事件值（`res.damage`）与实际承伤值（`final_damage`）的脱节。
- 统一事件载荷字段规范。

## Dependencies

- **Upstream:** CS-M1-01 (`combat_single_damage_entry_20260225`)
- **Downstream:** CS-M2-05 (`combat_telemetry_foundation`)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review §2.6](../../analyzer/combat_system_review.md)
