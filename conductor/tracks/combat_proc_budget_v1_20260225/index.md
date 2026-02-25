# Track: Combat Proc Budget V1

**ID:** `combat_proc_budget_v1_20260225`  
**Status:** Not Started  
**Type:** feature/governance  
**Priority:** P1  
**Milestone:** M2  
**Series:** CS-M2-04

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 0/4 complete
- **Tasks:** 0/14 complete

## Scope Summary

- 建立 `ProcBudgetManager`：按秒/帧预算控制击回/触发/异常/事件。
- 超预算按策略降采样或延迟，不可无限放行。
- 参数可配置（不硬编码）。

## Dependencies

- **Upstream:** CS-M1-01, CS-M1-03
- **Downstream:** CS-M3-01 (anti_meta)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Improvement Plan §3.6](../../analyzer/combat_system_improvement_plan.md)
