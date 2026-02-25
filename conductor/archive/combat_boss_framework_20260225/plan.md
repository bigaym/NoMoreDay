# Combat Boss Framework — Implementation Plan

> Track ID: `combat_boss_framework_20260225` | Series: CS-M3-03  
> Depends on: CS-M2-01, CS-M2-02, CS-M2-03

## Phase 1: Phase System
- [x] Define `BossPhaseConfig` struct.
- [x] Implement HP-threshold phase transitions.
- [x] Phase-specific behavior/immunity switching.

## Phase 2: Counter Windows
- [x] Implement `CounterWindow` timer with frame-precision.
- [x] Counter success/failure event hooks.

## Phase 3: Boss Prototype
- [x] Create 1 multi-phase Boss prototype.
- [x] Wire ailment interactions through AilmentEngine.
- [x] Configure failure penalties.

## Phase 4: Testing & Gate
- [x] Full Boss flow integration test.
- [x] Counter window precision test.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
