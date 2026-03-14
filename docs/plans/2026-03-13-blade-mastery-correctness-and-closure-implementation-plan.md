# Blade Mastery Correctness And Closure Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix the highest-risk Blade Ascendant correctness bugs first, then close the approved Heavenly Sword productization gaps, and finally re-review design completeness, UI quality, and code quality.

**Architecture:** Keep the existing Blade mastery runtime structure intact. Repair the narrow correctness faults in `SevenStarSlash`, `BladeResourceService`, and `SaveManager` with regression tests first; then extend the existing `BladeMasteryService` and `UISkillHub` paths to productize Heavenly Sword attunement and missing closure work without introducing a parallel state machine.

**Tech Stack:** C++20, EnTT, raylib UI, existing Blade mastery services/behaviors, doctest/CTest.

---

### Task 1: Fix `Seven Star Slash` target filtering

**Files:**
- Modify: `tests/functional/SkillBehaviors.cpp`
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Verify context: `src/game/components/Common.hpp`

**Step 1: Write the failing regression test**

Extend the Seven Star Slash functional coverage with a case that places a valid enemy target and a nearby non-enemy combat entity in range, then assert only the enemy loses health.

**Step 2: Run the narrow test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Functional] Skill - Seven Star Slash*"`

Expected: the new case fails because the non-enemy entity is currently treated as a slash target.

**Step 3: Write the minimal implementation**

Tighten `GatherTargets()` in `src/game/systems/skill/behaviors/SevenStarSlash.cpp` so it only returns valid enemy recipients for this behavior.

**Step 4: Run the narrow test to verify it passes**

Run: `./bin/NoMoreDayTests.exe --test-case="[Functional] Skill - Seven Star Slash*"`

Expected: the enemy-only regression now passes.

### Task 2: Fix Blade resource hit-tracking cleanup timebase

**Files:**
- Modify: `tests/unit/BladeMasteryTests.cpp`
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Verify context: `src/game/components/SkillDefs.hpp`

**Step 1: Write the failing unit test**

Add a unit test that seeds `BladeResourceComponent.hit_tracking` with an old absolute timestamp, advances `BladeResourceService::Update()`, and asserts expired tracking entries are removed.

**Step 2: Run the narrow test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Resource Service - expires stale hit tracking"`

Expected: the new cleanup test fails on the current mixed-timebase logic.

**Step 3: Write the minimal implementation**

Unify cleanup and tracking writes onto one consistent time base in `BladeResourceService.cpp`.

**Step 4: Run the narrow test to verify it passes**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Resource Service - expires stale hit tracking"`

Expected: stale entries are removed and the test passes.

### Task 3: Normalize Blade mastery state after save restore

**Files:**
- Modify: `tests/unit/SystemMechanics.cpp`
- Modify: `src/engine/persistence/SaveManager.cpp`
- Verify context: `src/game/systems/skill/BladeMasteryService.cpp`

**Step 1: Write the failing restore regression**

Add a save-restore unit test that persists an incompatible mastery/resource/signature combination and asserts restore normalizes it back to the selected mastery's correct runtime state.

**Step 2: Run the narrow test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SaveManager - Blade mastery restore normalizes runtime state"`

Expected: the new restore-normalization expectation fails on current restore logic.

**Step 3: Write the minimal implementation**

Adjust `SaveManager::restoreFromSnapshot()` so Blade mastery restore reuses shared normalization and only preserves compatible persisted counters/state.

**Step 4: Run the narrow test to verify it passes**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SaveManager - Blade mastery restore normalizes runtime state"`

Expected: restored runtime state is normalized and the test passes.

### Task 4: Close Heavenly Sword attunement productization

**Files:**
- Modify: `tests/unit/BladeMasteryTests.cpp`
- Modify: `tests/tech/UITests.cpp`
- Modify: `src/game/systems/skill/BladeMasteryService.hpp`
- Modify: `src/game/systems/skill/BladeMasteryService.cpp`
- Modify: `src/game/systems/ui/UISkillHub.cpp`

**Step 1: Write the failing service/UI tests**

Add a unit test for a supported Heavenly Sword attunement setter path and a tech/UI guard for a visible attunement selection affordance in the mastery hub.

**Step 2: Run the narrow tests to verify they fail**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service - Heavenly Sword attunement*,[Tech] SkillUI - mastery hub exposes Heavenly Sword attunement controls"`

Expected: both fail because attunement is not yet productized through service/UI flow.

**Step 3: Write the minimal implementation**

Expose a shared attunement setter in `BladeMasteryService` and add the minimum `UISkillHub` controls needed to drive and surface it.

**Step 4: Run the narrow tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service - Heavenly Sword attunement*,[Tech] SkillUI - mastery hub exposes Heavenly Sword attunement controls"`

Expected: service and hub attunement coverage pass.

### Task 5: Close the remaining Heavenly Sword runtime gaps

**Files:**
- Modify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Modify: `tests/integration/SkillSystemTests.cpp`
- Modify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Verify context: `assets/data/mastery_skill_trees.json`

**Step 1: Write failing node-closure tests**

Add narrow guard/integration coverage for the specific Heavenly Sword nodes approved as still missing.

**Step 2: Run the narrow tests to verify they fail**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*"`

Expected: the newly added node-closure cases fail on the current runtime.

**Step 3: Write the minimal implementation**

Implement only the missing documented Heavenly Sword node behaviors in `HeavenlySwordDescent.cpp`, keeping field-local logic in the behavior file.

**Step 4: Run the narrow tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*"`

Expected: the new Heavenly Sword closure cases pass.

### Task 6: Update bug registry, run verification, and re-review

**Files:**
- Modify: `conductor/bug_registry.md`

**Step 1: Update bug entries after verification**

Record root cause, fix summary, verification evidence, and regression coverage for the new Blade mastery bug entries.

**Step 2: Run focused verification**

Run: `./bin/NoMoreDayTests.exe --test-case="[Functional] Skill - Seven Star Slash*,[Unit] Blade Resource Service - expires stale hit tracking,[Unit] SaveManager - Blade mastery restore normalizes runtime state,[Unit] Blade Mastery Service - Heavenly Sword attunement*,[Tech] SkillUI - mastery hub exposes Heavenly Sword attunement controls,[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*"`

Expected: all touched focused tests pass.

**Step 3: Run repo-required broader verification**

Run: `./build.bat debug` and then `ctest --test-dir build -C Debug -L unit --output-on-failure`

Expected: build succeeds and the debug unit suite remains green.

**Step 4: Re-review the repaired work**

Dispatch a second 3-axis review covering design completeness, UI quality, and code quality against the repaired branch.
