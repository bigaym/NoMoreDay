# Combat Summon Strategy V1 — Implementation Plan

> Track ID: `combat_summon_strategy_v1_20260225` | Series: CS-M2-03  
> Depends on: CS-M1-01, CS-M1-06

---

## Phase 1: Data Model & Component Refactor

- [ ] Create new component structs: SummonCombatProfile, SummonAIProfile, SummonRuntimeState.
- [ ] Replace `SummonComponent.name` with `archetype_id`.
- [ ] Migrate existing SummonComponent data to new schema.

## Phase 2: System Split

- [ ] Extract `SummonLifecycleSystem` from SummonSystem.
- [ ] Extract `SummonAISystem` from SummonSystem (targeting, behavior).
- [ ] Create `SummonCombatBridge` for damage routing through DamagePipeline.

## Phase 3: Attribution & Inheritance

- [ ] Implement three-element attribution (owner/summon/source_skill) in events.
- [ ] Implement `SummonInheritMode` (Snapshot/Dynamic/Mixed).
- [ ] Wire inheritance into DamagePipeline context.

## Phase 4: Spirit Sword Migration (Sample)

- [ ] Migrate spirit sword (ShadowCast path) to use SummonCombatBridge.
- [ ] Verify visual and numerical parity with pre-migration.
- [ ] Migrate melee orbit to use SummonCombatBridge.

## Phase 5: Testing & Gate

- [ ] Unit: attribution presence validation.
- [ ] Unit: inheritance mode correctness.
- [ ] Integration: spirit sword full regression.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables

- 3 new subsystems (Lifecycle/AI/CombatBridge).
- New summon component structs.
- Spirit sword migration as reference implementation.
