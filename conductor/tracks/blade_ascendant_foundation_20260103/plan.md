# Implementation Plan: Sword Cultivator Foundation Expansion

This plan outlines the architectural changes needed to support advanced Blade Ascendant mechanics, focusing on extensibility, performance, and modularity.

## Phase 1: Skill Infrastructure (Hooks & Modifiers)
**Goal:** Implement the "hooks" and component-driven modifier system to allow skills to be dynamic and specialized.

- [ ] **Task 1: Implement Skill Logic Hooks**
    - [ ] Add `PreCastHook` and `PostCastHook` function pointers or event handlers to the `SkillSystem`.
    - [ ] Update `SkillSystem.cpp` to trigger `PreCast` during the `Preparing` state and `PostCast` during the `Settle` or `End` state.
    - [ ] Create `SkillHookTest.hpp` to verify hooks are called at the correct state transitions.
- [ ] **Task 2: Component-Based Modifier System**
    - [ ] Define a generic `SkillModifierComponent` that can hold a collection of modifiers (Damage Inc/More, Conversion, etc.).
    - [ ] Update `DamagePipeline` to iterate through any `SkillModifierComponent` attached to the `DamageEvent` or `Projectile`.
    - [ ] Write `SkillModifierTest.hpp` to verify that attached modifiers correctly change damage output.
- [ ] **Task 3: Conductor - User Manual Verification 'Skill Infrastructure' (Protocol in workflow.md)**

## Phase 2: Lightweight Shadow System
**Goal:** Create the "Ghost" echo mechanism for skill repetition without the overhead of full combat entities.

- [ ] **Task 1: Define Shadow Architecture**
    - [ ] Implement `ShadowComponent` and a `SkillSnapshot` struct (storing SkillID, position, direction, and base power).
    - [ ] Create the `ShadowSystem` that processes entities with `ShadowComponent`, handling their lifetime and execution.
- [ ] **Task 2: Implement Shadow Execution Logic**
    - [ ] Ensure `ShadowSystem` can trigger visual effects and create "one-off" `DamageEvent` entities based on the snapshot.
    - [ ] Create `ShadowSystemTest.hpp` to verify shadows spawn, execute their snapshot, and clean themselves up.
- [ ] **Task 3: Conductor - User Manual Verification 'Shadow System' (Protocol in workflow.md)**

## Phase 3: Sword Intent & Empowered Logic
**Goal:** Integrate the class-specific resource with the new hook system.

- [ ] **Task 1: Empowered Cast Hook**
    - [ ] Implement a specific `SwordIntentHook` that checks `SwordIntentComponent` for 10 stacks.
    - [ ] Logic: If stacks == 10, set `isEmpowered = true` on `SkillExecution` and reset stacks to 0.
    - [ ] Register this hook in the `SkillSystem` during player initialization.
- [ ] **Task 2: Verification**
    - [ ] Write a test case in `ProgressionSystemTest.hpp` (or a new file) to verify stack consumption and the "Empowered" flag state.
- [ ] **Task 3: Conductor - User Manual Verification 'Empowered Logic' (Protocol in workflow.md)**

## Phase 4: Movement Stance (Sword Riding)
**Goal:** Implement the foundation for enhanced movement and "Stance" transitions.

- [ ] **Task 1: Movement Stance Component & Logic**
    - [ ] Implement `MovementStanceComponent` and `MovementStanceSystem`.
    - [ ] Logic: Track continuous movement time; if > 2s, transition to `SwordRiding` state (add speed buff).
- [ ] **Task 2: Stance Interruption**
    - [ ] Update the damage handling logic to check for `MovementStanceComponent` and reset/interrupt it upon taking damage.
    - [ ] Create `MovementStanceTest.hpp` to verify transition and interruption triggers.
- [ ] **Task 3: Conductor - User Manual Verification 'Movement Stance' (Protocol in workflow.md)**

## Phase 5: Final Integration
**Goal:** Ensure all foundational pieces work together seamlessly.

- [ ] **Task 1: Integration Test**
    - [ ] Create a complex test scenario: Player uses an empowered skill -> triggers a shadow echo -> moves to enter sword riding stance.
- [ ] **Task 2: Performance Benchmarking**
    - [ ] Verify that spawning 50+ shadows simultaneously does not drop FPS (use a stress test component).
- [ ] **Task 3: Conductor - User Manual Verification 'Final Integration' (Protocol in workflow.md)**
