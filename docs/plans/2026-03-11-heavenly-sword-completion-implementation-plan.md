# Heavenly Sword Completion Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the missing Heavenly Sword mastery nodes and make attunement a stable, testable part of the specialization's field-control and tier-spend loop.

**Architecture:** Keep `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp` as the owning runtime for field/tier behavior, and only extend shared services/UI where attunement or cross-skill buffs genuinely need it. Reuse the current mastery persistence/HUD path instead of replacing it; this slice is about explicit selection semantics, missing node behavior, and targeted follow-through into the already-existing Blade Ascendant skills that consume Heavenly Sword state.

**Tech Stack:** C++20, EnTT ECS, JSON data tables, doctest/CTest, existing Blade mastery runtime (`BladeMasteryService`, `BladeResourceService`, `SkillSystem`, `UISkillHub`, `PlayerHUD`).

---

## Shared dependency note

If the Demon Blade plan has already introduced shared mastery cleanup helpers, reuse them here. This plan should only add Heavenly-Sword-specific shared work, mainly explicit attunement selection semantics.

### Task 1: Lock the missing-node and attunement gaps with failing tests

**Files:**
- Verify: `assets/data/mastery_skill_trees.json`
- Modify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Modify: `tests/unit/BladeMasteryTests.cpp`
- Modify: `tests/integration/SkillSystemTests.cpp`

**Step 1: Write the failing tests**

Add narrow tests for the currently-missing Heavenly Sword node groups and one explicit attunement-selection expectation, using the same setup style already present in `tests/unit/SkillBehaviorGuardTests.cpp`, `tests/unit/BladeMasteryTests.cpp`, and `tests/integration/SkillSystemTests.cpp`.

```cpp
TEST_CASE("[Unit] SkillBehaviorGuard - Heavenly Sword missing center-field nodes alter descent state") {
    // Use the existing skill-11 setup pattern from this file.
    // Allocate 1100/1104/1105 and compare against a baseline cast.
    // Assert against real fields already present, such as impact_damage_mult,
    // field_damage_mult, radius, duration, and any newly-added center-hit state.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*"`

Expected: FAIL.

**Step 3: Add one failing attunement-selection test**

Add a service- or UI-level test proving Heavenly Sword attunement can be explicitly set and survives the expected runtime path without direct component mutation in gameplay code.

**Step 4: Run the focused suite to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*"`

Expected: FAIL because the missing nodes and explicit attunement path are incomplete.

### Task 2: Add explicit attunement selection plumbing

**Files:**
- Modify: `src/game/systems/skill/BladeMasteryService.cpp`
- Modify: `src/game/systems/skill/BladeMasteryService.hpp`
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Modify: `src/game/systems/ui/UISkillHub.cpp`
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Test: `tests/unit/BladeMasteryTests.cpp`
- Test: `tests/tech/UITests.cpp`

**Step 1: Write the failing test**

Add one service test and one UI/tech test for explicit attunement selection.

```cpp
TEST_CASE("[Unit] Blade Mastery Service - Heavenly Sword attunement selection persists through refresh") {
    TestSetupScope testScope;
    LoadBladeMasteries();

    entt::registry registry;
    const entt::entity player = CreateBladeAscendant(registry, 50);
    BladeMasteryService::SetDebugUnlockOverrideEnabled(true);
    BladeMasteryService::RefreshPlayerState(registry, player);
    REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                               BladeMasteryId::HeavenlySword));

    REQUIRE(BladeMasteryService::SetHeavenlyAttunement(
        registry, player, BladeAttunement::Fire));
    BladeMasteryService::RefreshPlayerState(registry, player);
    CHECK(registry.get<BladeMasteryComponent>(player).heavenly_attunement ==
          BladeAttunement::Fire);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service - Heavenly Sword attunement selection persists through refresh"`

Expected: FAIL or compile failure because the explicit setter path is incomplete.

**Step 3: Write minimal implementation**

- add a narrow attunement setter/reset path in `BladeMasteryService` and declare it in `BladeMasteryService.hpp`;
- keep `BladeResourceService` as the read/translation layer for gameplay consumers;
- add a small existing-UI entry point in `UISkillHub.cpp` so attunement is not debug-only;
- keep `PlayerHUD.cpp` in sync so verification is visible during gameplay.

```cpp
static bool SetHeavenlyAttunement(entt::registry& registry,
                                  entt::entity entity,
                                  BladeAttunement attunement);
```

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Heavenly Sword*,[Tech] UI*Heavenly Sword*"`

Expected: PASS.

### Task 3: Implement impact-entry nodes and the delayed-scar follow-up

**Files:**
- Modify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Modify: `src/game/components/SkillDefs.hpp`
- Test: `tests/unit/SkillBehaviorGuardTests.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`

**Step 1: Write the failing test**

Cover the first missing gameplay slice:

- `1100`: initial impact/range stability
- `1104`: center-zone impact bonus
- `1105`: elite/boss and first-second field payoff
- `1106`: center-hit heavy slow
- `1108`: delayed sword-scar follow-up after the impact

```cpp
SUBCASE("Skill 11 impact-entry nodes strengthen center hit and immediate field follow-through") {
    // Compare a baseline cast against one with 1100/1104/1105/1106/1108.
    // Assert against impact_damage_mult, field_damage_mult, and any new center-hit
    // or delayed-follow-up state explicitly added to HeavenlySwordFieldComponent.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*"`

Expected: FAIL.

**Step 3: Write minimal implementation**

Reuse existing fields first:

- `impact_damage_mult`
- `field_damage_mult`
- `duration`
- `radius`

Only add truly new state where existing fields are insufficient, for example a center-hit slow payload or delayed-scar follow-up marker.

```cpp
struct HeavenlySwordFieldComponent {
    float impact_damage_mult = 1.0f;
    float field_damage_mult = 1.0f;
    // Add only new fields that are actually required by 1106/1108.
};
```

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*"`

Expected: PASS.

### Task 4: Implement the cycle/control branch and follow-through into active sword skills

**Files:**
- Modify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Modify: `src/game/components/SkillDefs.hpp`
- Possibly modify: `src/game/systems/skill/behaviors\MindBlade.cpp`
- Possibly modify: `src/game/systems/skill/behaviors\SwordArray.cpp`
- Possibly modify: `src/game/systems/skill/behaviors\BladeFormation.cpp`
- Test: `tests/unit/SkillBehaviorGuardTests.cpp`
- Test: `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`

**Step 1: Write the failing test**

Cover the remaining missing nodes:

- `1112`: active sword attack frequency while the field exists
- `1114`: refunded tiers briefly strengthen the intended non-signature Blade Ascendant skills
- `1118`: field gains extra pressure against afflicted targets and refreshes duration

```cpp
SUBCASE("Skill 11 cycle and affliction nodes refresh pressure and empower follow-up skills") {
    // Use the existing cycle-refund and linked-hit setup already present in this file.
    // For 1114, inspect the actual target skills in the current Blade Ascendant mapping
    // before choosing the assertion site; do not guess the skill ids.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillKeyNodeMatrix*11*"`

Expected: FAIL.

**Step 3: Write minimal implementation**

Implement the missing control branch with the smallest cross-file footprint:

- keep `1112` in Heavenly Sword state if it only affects field-side cadence;
- if `1114` must strengthen `灵剑决` / `万剑归宗`, verify the exact skills in current data and wire the temporary buff into the owning behavior files rather than adding a generic global modifier;
- implement `1118` through the existing linked-hit / affliction path so afflicted targets take extra pressure and have durations refreshed.

Keep the attunement branch explicit so Lightning/Frost/Fire still differ in tests.

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Integration] SkillKeyNodeMatrix*11*"`

Expected: PASS.

### Task 5: Add blocking shared support for cleanup, persistence, and HUD clarity

**Files:**
- Modify: `src/game/systems/skill/BladeMasteryService.cpp`
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Verify: `src/game/data/SaveData.hpp`
- Test: `tests/unit/BladeMasteryTests.cpp`
- Test: `tests/tech/UITests.cpp`

**Step 1: Write the failing test**

Add one persistence/refresh test and one UI/tech test covering attunement visibility after mastery transitions, using existing `CreateBladeAscendant(...)`, `BladeMasteryService::RefreshPlayerState(...)`, and `UISkillHub::Draw(...)` patterns.

```cpp
TEST_CASE("[Unit] Blade Mastery Service - Heavenly Sword attunement resets only when the mastery becomes invalid") {
    // Create a Blade Ascendant, select Heavenly Sword, set attunement via the new API,
    // call RefreshPlayerState, and verify the attunement persists.
    // Then switch to Sword Saint and verify it resets to None.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Heavenly Sword*,[Tech] UI*Heavenly Sword*"`

Expected: FAIL.

**Step 3: Write minimal implementation**

- ensure refresh and mastery switching preserve valid attunement state but clear invalid state;
- surface the attunement and active field pressure clearly in HUD text;
- keep save/load compatible by relying on the existing `BladeMasteryComponent` unless a truly new persisted field is required;
- if the Demon Blade shared cleanup helper already exists, reuse it for Heavenly Sword field cleanup instead of creating a second policy path.

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Heavenly Sword*,[Tech] UI*Heavenly Sword*"`

Expected: PASS.

### Task 6: Full verification

**Files:**
- Verify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Verify: `src/game/systems/skill/BladeMasteryService.cpp`
- Verify: `src/game/systems/skill/BladeResourceService.cpp`
- Verify: `src/game/components/SkillDefs.hpp`
- Verify: `src/game/systems/ui/UISkillHub.cpp`
- Verify: `src/game/systems/ui/PlayerHUD.cpp`
- Verify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Verify: `tests/unit/BladeMasteryTests.cpp`
- Verify: `tests/integration/SkillSystemTests.cpp`
- Verify: `tests/tech/UITests.cpp`

**Step 1: Run the focused Heavenly Sword doctest cases**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Heavenly Sword*,[Unit] Blade Mastery Service*Heavenly Sword*,[Integration] SkillSystem - Heavenly Sword*,[Integration] SkillKeyNodeMatrix*11*,[Tech] UI*Heavenly Sword*"`

Expected: PASS.

**Step 2: Build the project**

Run: `./build.bat`

Expected: Build completes successfully.

**Step 3: Run CI-labelled verification**

Run: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

Expected: PASS.

**Step 4: Perform one short manual sanity check**

Verify in-game that:

- attunement can be selected and is visible before and after casting;
- Heavenly Sword fields feel different across attunements;
- switching away from Heavenly Sword clears invalid attunement-dependent state cleanly.

## Data-rescope fallback

If a node promise turns out to require a new global subsystem that is out of scope for this slice, narrow the data description in `assets/data/mastery_skill_trees.json` to the behavior actually implemented. Prefer an honest smaller node over an unbounded system expansion.
