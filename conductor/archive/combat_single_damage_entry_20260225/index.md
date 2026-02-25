# Track: Combat Single Damage Entry

**ID:** `combat_single_damage_entry_20260225`  
**Status:** Completed (Archived)  
**Type:** refactor/P0-convergence  
**Priority:** P0-Critical  
**Milestone:** M1（口径收敛与正确性闭环）  
**Series:** CS-M1-01

## Core Documents

- [Specification](./spec.md)
- [Implementation Plan](./plan.md)
- [Validation Evidence](./validation.md)

## Progress

- **Phases:** 4/4 complete
- **Tasks:** 18/18 complete

## Scope Summary

- Unify ALL damage calculation paths to `DamagePipeline` as single source of truth.
- Deprecate `CombatSystem::CalculateDamage` with `[[deprecated]]` + compile guard.
- Migrate remaining callers in `CombatSystem.cpp`, `ProjectileSystem.cpp`, `SwordArray.cpp`, `SkillSystem.cpp`.
- Preserve `CombatSystem::ApplyDamage` as the sole HP settlement function (no formula logic).
- Establish compatibility shim with kill-switch for rollback safety.

## Dependencies

- **Upstream:** None (root track)
- **Downstream:** CS-M1-02, CS-M1-03, CS-M1-04, CS-M1-06, all M2/M3 tracks

## Quick Links

- [Back to Tracks](../../tracks.md)
- [Combat System Review](../../analyzer/combat_system_review.md)
- [Combat System Improvement Plan](../../analyzer/combat_system_improvement_plan.md)
- [Combat System Capability Roadmap](../../analyzer/combat_system_capability_roadmap.md)
