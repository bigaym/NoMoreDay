# Combat Endgame Linker — Implementation Plan

> Track ID: `combat_endgame_linker_20260225` | Series: CS-M3-02  
> Depends on: CS-M2-01, CS-M2-02

## Phase 1: Contract Design
- [x] Define `EndgameModifierContract` interface.
- [x] Design modifier config schema.
- [x] Map first 5 sample modifiers.

## Phase 2: Integration
- [x] Implement modifier → DamagePipeline/AilmentEngine hooks.
- [x] Implement modifier → DefenseContract hooks.
- [x] Config loading infrastructure.

## Phase 3: Testing & Gate
- [x] 5 modifier integration tests.
- [x] Regression traceability test.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
