# Plan: Stability & Test Suite Repair

## Phase 1: Skill System Logic & Sword Intent Stability
- [x] Task: Root cause analysis of `SkillSystemTest` value mismatch. (Expected 125.0f vs Actual 137.5f) [checkpoint: 78a1b2c]
    - *Note: Resolved. Discrepancy was due to a missing 10% bonus in the test expectation for Projectile logic or a misunderstanding of base values. Correct values are 165.0f (Melee) and 125.0f (Projectile).*
- [x] Task: Fix `GetStatWithTags` to handle overlapping damage tags and conversion correctly.
    - *Note: Implemented `IsDamageStat` and inheritance logic. Verified with `Conversion Inheritance` test case (165.0f).*
- [x] Task: Refactor `SwordIntent` gain/decay logic for better determinism in test environments.
    - *Note: Fixed decay logic to handle large dt and guarded visuals with `IsWindowReady()`.*
- [x] Task: Write regression tests in `SkillSystemTest.hpp` to verify the fix and prevent future regressions.
    - *Note: Added `Conversion Inheritance` and `Decay Robustness` test cases.*
- [x] Task: Conductor - User Manual Verification 'Phase 1: Skill System Logic & Sword Intent Stability' (Protocol in workflow.md) [checkpoint: 91eafa9]

## Phase 2: Headless Test Stability & Biome Registry
- [ ] Task: Investigate `BiomeRegistryTest` segmentation fault in headless mode.
- [ ] Task: Refactor `BiomeRegistry` and `AssetLoadingSystem` to decouple asset loading from Raylib's OpenGL context during unit testing.
- [ ] Task: Implement a "Headless Mode" flag or mock for Raylib functions that require a window.
- [ ] Task: Write tests in `BiomeRegistryTest.hpp` to ensure loading works without a graphics context.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Headless Test Stability & Biome Registry' (Protocol in workflow.md)

## Phase 3: Final Verification & Integration
- [ ] Task: Run the full `tests_runner.exe` and ensure all suites (including `SkillUITest`) pass.
- [ ] Task: Perform a final build in `Release` and `Debug` configurations to ensure no cross-platform/config regressions.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Final Verification & Integration' (Protocol in workflow.md)
