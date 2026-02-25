# Combat Boss Framework — Implementation Plan

> Track ID: `combat_boss_framework_20260225` | Series: CS-M3-03  
> Depends on: CS-M2-01, CS-M2-02, CS-M2-03

## Phase 1: Phase System
- [ ] Define `BossPhaseConfig` struct.
- [ ] Implement HP-threshold phase transitions.
- [ ] Phase-specific behavior/immunity switching.

## Phase 2: Counter Windows
- [ ] Implement `CounterWindow` timer with frame-precision.
- [ ] Counter success/failure event hooks.

## Phase 3: Boss Prototype
- [ ] Create 1 multi-phase Boss prototype.
- [ ] Wire ailment interactions through AilmentEngine.
- [ ] Configure failure penalties.

## Phase 4: Testing & Gate
- [ ] Full Boss flow integration test.
- [ ] Counter window precision test.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
