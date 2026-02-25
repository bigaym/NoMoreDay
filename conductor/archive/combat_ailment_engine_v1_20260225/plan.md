# Combat Ailment Engine V1 — Implementation Plan

> Track ID: `combat_ailment_engine_v1_20260225` | Series: CS-M2-01  
> Depends on: CS-M1-01, CS-M1-02

---

## Phase 1: Contract Design & Data Model

- [x] Define `AilmentContract` struct with all parameters from spec.
- [x] Define `AilmentRegistry` to load contracts from config.
- [x] Define enum types: `RefreshPolicy`, `OverwritePolicy`, `DamagePoolPolicy`.
- [x] Write contract definition data for at least: Poison, Ignite, Bleed.

## Phase 2: Engine Core

- [x] Implement `AilmentApplier::Apply` — respect max_stacks, refresh, overwrite policies.
- [x] Implement `AilmentTickDriver` — unified DoT tick with DamagePipeline integration.
- [x] Migrate EffectSystem §6 DoT logic to AilmentTickDriver.
- [x] Implement `AilmentAdapter` — map old BuffType to new AilmentContract.

## Phase 3: Testing

- [x] Unit: stack limit enforcement.
- [x] Unit: refresh vs extend vs independent policies.
- [x] Unit: overwrite logic (strongest/newest/additive).
- [x] Integration: multi-ailment scenario on single target.

## Phase 4: Final Gate

- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Deliverables

- AilmentEngine subsystem (AilmentRegistry, AilmentApplier, AilmentTickDriver, AilmentAdapter).
- Ailment contract definitions.
- Unit + integration tests.
