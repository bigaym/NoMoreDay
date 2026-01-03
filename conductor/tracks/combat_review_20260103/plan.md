# Plan: Combat System Logic Integrity Review

This plan outlines a comprehensive review and verification of the combat system's logic, focusing on the damage pipeline, buff systems, and skill specializations via unit testing and static analysis.

## Phase 1: Preparation & Baseline Testing
Set up the environment and ensure existing combat tests are stable.

- [ ] Task: Audit existing combat-related test files (`CombatSystemTest.hpp`, `DamagePipelineTest.hpp`, `BuffComponentTest.hpp`) to identify coverage gaps based on recent skill/buff updates.
- [ ] Task: Execute current combat test suite and document baseline results/failures.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Preparation & Baseline Testing' (Protocol in workflow.md)

## Phase 2: Damage Pipeline & Snapshotting Review
Deep dive into the sequence of damage calculations and the accuracy of property snapshotting.

- [ ] Task: **Static Review:** Audit `src/systems/DamagePipeline.cpp` against `设计文档/战斗系统与属性设计.md`. Verify the order of Base -> Increased -> More -> Armor/Resist.
- [ ] Task: **TDD:** Write unit tests in `DamagePipelineTest.hpp` to verify edge cases: 100% resistance, overflow damage, and zero-damage scenarios.
- [ ] Task: **Logic Review:** Audit `src/systems/ProjectileSystem.cpp` (or equivalent) to ensure `CombatStats` are captured *at the moment of release* and not updated mid-flight.
- [ ] Task: **TDD:** Write a dedicated test case for Projectile Snapshotting to verify stats don't change if the player's attributes change while the projectile is in flight.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Damage Pipeline & Snapshotting Review' (Protocol in workflow.md)

## Phase 3: Buffs & Specialization Logic Review
Verify the dynamic behavior of status effects and the complex triggers of the Blade Ascendant class.

- [ ] Task: **Static Review:** Audit `src/systems/BuffSystem.cpp` and `BuffRegistry.cpp` for stacking logic and expiration cleanup.
- [ ] Task: **TDD:** Write tests to verify Buff stacking limits and duration refreshing (e.g., does a new application of a stackable buff refresh all stacks?).
- [ ] Task: **Logic Review:** Audit `src/systems/SkillSystem.cpp` hooks for Blade Ascendant. Specifically check "Shadow Casting" trigger rates and "Sword Intent" consumption.
- [ ] Task: **TDD:** Update `SkillSystemTest.hpp` to include assertions for specific talent node modifiers (e.g., +20% damage per Sword Intent consumed).
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Buffs & Specialization Logic Review' (Protocol in workflow.md)

## Phase 4: Final Integration & Report
Consolidate findings and ensure system-wide consistency.

- [ ] Task: Run full regression suite including `FinalIntegrationTest.hpp` to ensure no side effects from potential fixes during the review.
- [ ] Task: Generate a brief "Combat System Review Report" (as a markdown file in the track directory) summarizing any logic bugs found and fixed.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Final Integration & Report' (Protocol in workflow.md)
