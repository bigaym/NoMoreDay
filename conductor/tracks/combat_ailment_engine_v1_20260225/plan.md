# Combat Ailment Engine V1 — Implementation Plan

> Track ID: `combat_ailment_engine_v1_20260225` | Series: CS-M2-01  
> Depends on: CS-M1-01, CS-M1-02

---

## Phase 1: Contract Design & Data Model

- [ ] Define `AilmentContract` struct with all parameters from spec.
- [ ] Define `AilmentRegistry` to load contracts from config.
- [ ] Define enum types: `RefreshPolicy`, `OverwritePolicy`, `DamagePoolPolicy`.
- [ ] Write contract definition data for at least: Poison, Ignite, Bleed.

## Phase 2: Engine Core

- [ ] Implement `AilmentApplier::Apply` — respect max_stacks, refresh, overwrite policies.
- [ ] Implement `AilmentTickDriver` — unified DoT tick with DamagePipeline integration.
- [ ] Migrate EffectSystem §6 DoT logic to AilmentTickDriver.
- [ ] Implement `AilmentAdapter` — map old BuffType to new AilmentContract.

## Phase 3: Testing

- [ ] Unit: stack limit enforcement.
- [ ] Unit: refresh vs extend vs independent policies.
- [ ] Unit: overwrite logic (strongest/newest/additive).
- [ ] Integration: multi-ailment scenario on single target.

## Phase 4: Final Gate

- [ ] `build.bat` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Deliverables

- AilmentEngine subsystem (AilmentRegistry, AilmentApplier, AilmentTickDriver, AilmentAdapter).
- Ailment contract definitions.
- Unit + integration tests.
