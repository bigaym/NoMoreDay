# Combat Summon Strategy V1 — Implementation Plan

> Track ID: `combat_summon_strategy_v1_20260225` | Series: CS-M2-03  
> Depends on: CS-M1-01, CS-M1-06

---

## Phase 1: Data Model & Component Refactor

- [x] Create new component structs: SummonCombatProfile, SummonAIProfile, SummonRuntimeState.
- [x] Replace `SummonComponent.name` with `archetype_id`.
- [x] Migrate existing SummonComponent data to new schema.

## Phase 2: System Split

- [x] Extract `SummonLifecycleSystem` from SummonSystem.
- [x] Extract `SummonAISystem` from SummonSystem (targeting, behavior).
- [x] Create `SummonCombatBridge` for damage routing through DamagePipeline.

## Phase 3: Attribution & Inheritance

- [x] Implement three-element attribution (owner/summon/source_skill) in events.
- [x] Implement `SummonInheritMode` (Snapshot/Dynamic/Mixed).
- [x] Wire inheritance into DamagePipeline context.

## Phase 4: Spirit Sword Migration (Sample)

- [x] Migrate spirit sword (ShadowCast path) to use SummonCombatBridge.
- [x] Verify visual and numerical parity with pre-migration.
- [x] Migrate melee orbit to use SummonCombatBridge.

## Phase 5: Testing & Gate

- [x] Unit: attribution presence validation.
- [x] Unit: inheritance mode correctness.
- [x] Integration: spirit sword full regression.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- 3 new subsystems (Lifecycle/AI/CombatBridge).
- New summon component structs.
- Spirit sword migration as reference implementation.
