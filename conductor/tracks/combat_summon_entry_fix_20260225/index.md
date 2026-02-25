# Track: Combat Summon Entry Fix

**ID:** `combat_summon_entry_fix_20260225`  
**Status:** Not Started  
**Type:** bugfix/P0-convergence  
**Priority:** P0  
**Milestone:** M1  
**Series:** CS-M1-06

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 0/3 complete
- **Tasks:** 0/10 complete

## Scope Summary

- 消除 `SummonSystem.cpp` L83 中 `ApplyDamage(..., 25.0f)` 的直写路径。
- 召唤近战环绕伤害路由至 `DamagePipeline`。
- 消除 `SwordArray.cpp` L215 中 `hp->max * 0.1f` 硬编码直写。
- 保留现有视觉表现不变。

## Dependencies

- **Upstream:** CS-M1-01 (`combat_single_damage_entry_20260225`)
- **Downstream:** CS-M2-03 (`combat_summon_strategy_v1`)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review §2.7-2.8](../../analyzer/combat_system_review.md)
