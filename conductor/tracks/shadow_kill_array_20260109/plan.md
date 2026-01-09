# Implementation Plan: `影杀阵` (Shadow Kill Array) Safeguards

## Phase 1: Data & Infrastructure Preparation
- [ ] **Task: Update Skill Registry Metadata**
    - [ ] Add `Movement`, `Buff`, `Aura`, and `Channeled` tags to relevant skills in `assets/data/skills.json` if missing.
    - [ ] Update the description of `影杀阵` (ID 124) in `assets/data/skills.json` to reflect the 50% damage and mana cost.
- [ ] **Task: Define Shadow Component**
    - [ ] Create/Update `src/components/SkillSystem.hpp` to include `ShadowCloneComponent` for identifying temporary duplication entities.
    - [ ] Add `LastShadowTriggerTime` to `PlayerState` for the 3s Internal Cooldown (ICD).
- [ ] **Task: Conductor - User Manual Verification 'Phase 1: Data & Infrastructure' (Protocol in workflow.md)**

## Phase 2: Logic Implementation & Tag Filtering
- [ ] **Task: Create Shadow Duplication Test**
    - [ ] Create `tests/ShadowKillArrayTest.hpp` to verify basic duplication logic and tag exclusion.
- [ ] **Task: Implement Tag Exclusion & ICD**
    - [ ] Modify `SkillSystem.cpp` to check for `Movement`, `Buff`, `Aura`, and `Channeled` tags before triggering duplication.
    - [ ] Implement the 3-second check against `LastShadowTriggerTime`.
- [ ] **Task: Implement Resource Consumption**
    - [ ] Add logic in `SkillSystem.cpp` to deduct 50% Mana when a shadow is spawned.
    - [ ] Ensure the shadow only spawns if the player has sufficient Mana.
- [ ] **Task: Conductor - User Manual Verification 'Phase 2: Logic & Filtering' (Protocol in workflow.md)**

## Phase 3: Combat Integration & Shadow Scaling
- [ ] **Task: Create Combat Scaling Test**
    - [ ] Add test cases to `tests/ShadowKillArrayTest.hpp` to verify the 50% damage reduction for shadow-cast skills.
- [ ] **Task: Implement Shadow Damage Multiplier**
    - [ ] Update `DamagePipeline.cpp` or `StatsSystem.cpp` to detect if the source entity has `ShadowCloneComponent`.
    - [ ] Apply a final `0.5x` multiplier to damage instances originating from shadows.
- [ ] **Task: Implement Shadow Lifecycle**
    - [ ] Ensure the shadow entity is marked for destruction immediately after its `SkillComponent` reaches the `End` state.
- [ ] **Task: Conductor - User Manual Verification 'Phase 3: Combat & Scaling' (Protocol in workflow.md)**

## Phase 4: Final Integration & Verification
- [ ] **Task: Full Integration Testing**
    - [ ] Run all tests in `tests/ShadowKillArrayTest.hpp`.
    - [ ] Verify no regressions in `CombatSystemTest.hpp`.
- [ ] **Task: Visual Polish (Optional/Check)**
    - [ ] Ensure the `RenderSystem` handles the shadow's transparency if a tint/alpha component is available.
- [ ] **Task: Conductor - User Manual Verification 'Phase 4: Final Verification' (Protocol in workflow.md)**
