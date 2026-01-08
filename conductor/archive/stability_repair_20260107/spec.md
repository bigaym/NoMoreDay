# Specification: Stability & Test Suite Repair

## 1. Overview
This track focuses on resolving critical stability issues and test failures identified during the "Skill System Optimization" phase. The primary goal is to ensure a green test suite, fix logic errors in damage scaling, and prevent crashes in headless testing environments.

## 2. Objectives
- **Fix Logic Error:** Resolve the value mismatch in `SkillSystemTest` related to "Tag Scaling & Conversion" (Expected: 125.0f, Actual: 137.5f).
- **Fix Crash:** Resolve the `BiomeRegistryTest` segmentation fault that occurs in headless/windowless environments.
- **Investigate Side Effects:** Analyze and fix potential race conditions or state pollution in `SkillSystemTest` involving `SwordIntent` decay.
- **Enhance Coverage:** Add regression tests to cover the identified edge cases.

## 3. Functional Requirements
- **Skill System:**
    - `GetStatWithTags` must correctly calculate damage multipliers when tags overlap or convert (e.g., Physical -> Cold).
    - Ensure `SwordIntent` stack logic (gain/decay) is deterministic in tests.
- **Biome Registry:**
    - `BiomeRegistry` loading must not crash when Raylib graphics context is missing or minimal.
    - Ensure asset loading fails gracefully or uses placeholders in headless mode.

## 4. Non-Functional Requirements
- **Headless Support:** All unit tests must pass without an active GPU/Window context (CI/CD compatible).
- **Behavior Preservation:** Fixes must not alter the intended gameplay balance or mechanics defined in previous tracks.
- **Code Quality:** No "hacky" workarounds; solutions should address root causes (e.g., proper dependency injection or mocking for graphics).

## 5. Acceptance Criteria
- [ ] `SkillSystemTest` passes with correct values for all Tag Scaling assertions.
- [ ] `BiomeRegistryTest` runs successfully without segmentation faults in headless mode.
- [ ] `SwordIntent` logic is verified to be stable across multiple test runs.
- [ ] All existing tests in `tests_runner` pass.
