# Track: Combat Ailment Engine V1

**ID:** `combat_ailment_engine_v1_20260225`  
**Status:** Completed  
**Type:** feature/system  
**Priority:** P1  
**Milestone:** M2  
**Series:** CS-M2-01

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 4/4 complete
- **Tasks:** 16/16 complete

## Scope Summary

- 建立 `AilmentEngine` 统一异常状态合同。
- 每种异常声明 `max_stacks`、`refresh_policy`、`overwrite_policy`、`immunity`、`tick_interval`、`damage_pool_policy`。
- 替代 EffectSystem 中的临时 DoT 处理逻辑。

## Dependencies

- **Upstream:** CS-M1-01, CS-M1-02
- **Downstream:** CS-M3-02 (endgame_linker), CS-M3-03 (boss_framework)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Improvement Plan §3.4](../../analyzer/combat_system_improvement_plan.md)
