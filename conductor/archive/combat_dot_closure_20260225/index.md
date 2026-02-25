# Track: Combat DoT Closure

**ID:** `combat_dot_closure_20260225`  
**Status:** Completed  
**Type:** bugfix/P0-convergence  
**Priority:** P0  
**Milestone:** M1  
**Series:** CS-M1-02

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 3/3 complete
- **Tasks:** 12/12 complete

## Scope Summary

- DoT tick 必须闭环：`DamagePipeline::Calculate` + `CombatSystem::ApplyDamage`。
- DoT 必须强制 `Tag::DamageOverTime`，禁止走 Hit 收益分支。
- 消灭"只弹字不扣血"问题（review §2.2, §2.3）。

## Dependencies

- **Upstream:** None（可并行）
- **Downstream:** CS-M2-01 (`combat_ailment_engine_v1`)

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review §2.2-2.3](../../analyzer/combat_system_review.md)
