# Combat Endgame Linker — Implementation Plan

> Track ID: `combat_endgame_linker_20260225` | Series: CS-M3-02  
> Depends on: CS-M2-01, CS-M2-02

## Phase 1: Contract Design
- [ ] Define `EndgameModifierContract` interface.
- [ ] Design modifier config schema.
- [ ] Map first 5 sample modifiers.

## Phase 2: Integration
- [ ] Implement modifier → DamagePipeline/AilmentEngine hooks.
- [ ] Implement modifier → DefenseContract hooks.
- [ ] Config loading infrastructure.

## Phase 3: Testing & Gate
- [ ] 5 modifier integration tests.
- [ ] Regression traceability test.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
