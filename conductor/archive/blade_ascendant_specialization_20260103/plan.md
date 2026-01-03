# Plan: Blade Ascendant Skill Specialization (Remaining 7 Skills)

This plan outlines the monolithic implementation of the specialization talent trees and unique mechanics for the remaining 7 active skills of the Blade Ascendant class.

## Phase 1: Data Definition & Registry Setup [checkpoint: 8dc7e19]
Focus on populating the data-driven backbone for all 7 skills.

- [x] Task: Update `assets/data/skills.json` with full talent trees for Blade Formation, Blade Ward, Infinite Blades, Sword Array, Mind Blade, Blade Boomerang, and Phantom Flash. [b809b96]
- [x] Task: Ensure all node IDs are unique and prerequisites are correctly mapped according to the design draft. [b809b96]
- [x] Task: Verify that `AssetLoadingSystem` correctly parses the updated `skills.json` and populates the `SkillRegistry`. [b809b96]
- [x] Task: Conductor - User Manual Verification 'Phase 1: Data Definition' (Protocol in workflow.md)

## Phase 2: Logic Implementation - Utility, Defense & Counter [DONE]
Implementing the hooks and components for Blade Ward, Blade Boomerang, and Phantom Flash.

- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Blade Ward** (projectile interception, physical DR scaling).
- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Blade Boomerang** (returning projectile, magnetic pull/CC).
- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Phantom Flash** (riposte, stealth state, evasion reset).
- [x] Task: Write unit tests for Utility/Defense skills to verify stat scaling and hook triggers.
- [x] Task: Conductor - User Manual Verification 'Phase 2: Utility & Defense' (Protocol in workflow.md)

## Phase 3: Logic Implementation - Area & Automation [DONE]
Implementing the hooks and components for Blade Formation and Sword Array.

- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Blade Formation** (automated sentry blade logic, mode switching).
- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Sword Array** (area resonance, debuff/buff application).
- [x] Task: Update `SkillSystem` to handle persistent area entities and automated projectile spawners for these skills.
- [x] Task: Write unit tests for Area/Automation skills to verify entity lifecycle and multi-instance behavior.
- [x] Task: Conductor - User Manual Verification 'Phase 3: Area & Automation' (Protocol in workflow.md)

## Phase 4: Logic Implementation - Channeling & Ultimates [DONE]
Implementing the high-intensity logic for Mind Blade and the ultimate Infinite Blades.

- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Mind Blade** (channeling frequency, "Mind Flow" stacks, auto-lock).
- [x] Task: **Hook Protocol (Check -> Modify -> Create):** Implement/Refactor `RegisterEffect` and associated hooks for **Infinite Blades** (ultimate channeling, frequency ramping, full-screen auto-targeting).
- [x] Task: Ensure Sword Intent consumption logic correctly modifies these skills' performance.
- [x] Task: Write unit tests for Channeling/Ultimate skills to verify frequency scaling and targeting logic.
- [x] Task: Conductor - User Manual Verification 'Phase 4: Channeling & Ultimates' (Protocol in workflow.md)

## Phase 5: Final Integration & Performance Benchmarking [DONE]
System-wide verification and stress testing.

- [x] Task: Execute full suite of automated tests for all 9 Blade Ascendant skills (including existing 2).
- [x] Task: Run performance benchmarks in high-density combat (10k entities) to ensure no regressions from new skill hooks.
- [x] Task: Verify UI integration: ensure all 7 new trees are navigable and talent points can be allocated/unallocated correctly in the Skill Management UI.
- [x] Task: Conductor - User Manual Verification 'Phase 5: Final Integration' (Protocol in workflow.md)