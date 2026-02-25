# Track: Combat Effectiveness Integration

**ID:** `combat_effectiveness_integration_20260225`  
**Status:** Not Started  
**Type:** feature/P0-convergence  
**Priority:** P0  
**Milestone:** M1  
**Series:** CS-M1-03

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 0/3 complete
- **Tasks:** 0/12 complete

## Scope Summary

- 将 `added_damage_effectiveness` 接入 DamagePipeline 主公式。
- 将 `trigger.effectiveness` 注入触发执行上下文并接入主公式。
- 目标公式: `FinalDamage = ((Base + Added * AddedEff) * (1 + Increased) * More * TriggerEff) * Mitigation`

## Dependencies

- **Upstream:** CS-M1-01 (`combat_single_damage_entry_20260225`)
- **Downstream:** CS-M2-04 (`combat_proc_budget_v1`)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review §2.4-2.5](../../analyzer/combat_system_review.md)
