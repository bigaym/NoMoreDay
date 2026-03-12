# Demon Blade Completion Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the missing Demon Blade mastery nodes so `BloodSea` delivers the low-life, sustain, pursuit, and corruption fantasy promised by the current mastery tree data.

**Architecture:** Keep `src/game/systems/skill/behaviors/BloodSea.cpp` as the owning runtime for Demon Blade signature behavior and reuse existing ECS/test patterns instead of inventing a parallel mastery subsystem. Extend `BloodSeaFieldComponent` only when state must survive across updates or linked-hit callbacks, and treat any missing global support (for example healing-cap semantics) as an explicit decision point rather than silently widening scope.

**Tech Stack:** C++20, EnTT ECS, JSON data tables, doctest/CTest, existing Blade mastery runtime (`BladeMasteryService`, `BladeResourceService`, `SkillSystem`, `PlayerHUD`).

---

## Shared dependency note

This plan owns the first shared-support slice for the urgent mastery work. If it adds a cleanup helper or lifecycle contract in `BladeMasteryService`, the Heavenly Sword plan should reuse that behavior rather than redefining it.

### Task 1: Lock the missing-node contract with failing guard tests

**Files:**
- Verify: `assets/data/mastery_skill_trees.json`
- Modify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Modify: `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`

**Step 1: Write the failing test**

Add narrow tests that prove the currently-missing Demon Blade promises do not exist yet, using the same registry setup patterns already present in `tests/unit/SkillBehaviorGuardTests.cpp` and `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`. Compare the missing-node cast against a real Blood Sea baseline; do not use `bonus_damage_mult > 1.0f` as the signal because the existing Bloodthirst baseline already pushes that value above `1.0f`.

```cpp
TEST_CASE("[Unit] SkillBehaviorGuard - Demon Blade low-life nodes alter BloodSea state") {
    entt::registry registry;
    CombatEventDispatcher::Init();
    SkillBehaviorRegistry::Initialize();
    SkillSystem::ShutdownHooks();
    SkillSystem::InitHooks();

    const auto player = test::skill_keynode_matrix::CreateCaster(registry, 400.0f);
    auto& stats = registry.get<CombatStats>(player);
    stats.max_health = 200.0f;
    stats.health = 60.0f;

    auto& mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto& resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 6;
    resource.max = 10;

    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    active.specialized_slots[0].skill_id = 12;
    active.specialized_slots[0].allocated_points = {{1201, 2}, {1204, 2}, {1206, 2}};

    SkillExecution exec{.skill_id = 12, .owner = player, .target_pos = {18.0f, 0.0f}};
    auto cast = SkillBehaviorRegistry::GetCast(12);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    const auto& field = view.get<BloodSeaFieldComponent>(*view.begin());
    CHECK(field.bonus_damage_mult > baselineField.bonus_damage_mult);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*"`

Expected: FAIL because the new node effects are not implemented yet.

**Step 3: Add one integration-level failure for a linked/field lifecycle case**

Add one integration-level assertion in `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` or `tests/integration/SkillSystemTests.cpp` that covers a missing runtime promise without inventing new helper APIs. Good candidates are:

- `1203`: follow-speed delta should change field movement on update;
- `1219`: duration extension should survive one or two `SkillSystem::Update` ticks;
- `1223`: void branch should lengthen or strengthen the applied debuff path once `1220` is active.

**Step 4: Run the integration slice to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Integration] SkillSystem - Blood Sea*,[Integration] SkillKeyNodeMatrix*12*"`

Expected: FAIL because the linked or lifecycle behavior does not exist yet.

### Task 2: Implement the missing baseline field-shaping nodes

**Files:**
- Modify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Modify: `src/game/components/SkillDefs.hpp`
- Test: `tests/unit/SkillBehaviorGuardTests.cpp`
- Test: `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`

**Step 1: Write the failing test**

Cover the first missing slice using only existing, concrete field knobs:

- `1200`: range stability / better initial field footprint
- `1201`: higher field pulse damage
- `1202`: Bloodthirst amplifies field damage harder than the current baseline
- `1203`: higher follow speed

```cpp
SUBCASE("Skill 12 baseline field nodes change radius damage and follow speed") {
    // Build from the existing Blood Sea setup pattern in this file.
    // Compare a base cast against a cast with 1200/1201/1202/1203 allocated.
    // Assert against real fields: radius, bonus_damage_mult, move_follow_speed.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*"`

Expected: FAIL because the baseline node group is incomplete.

**Step 3: Write minimal implementation**

Implement `1200-1203` in `BloodSea.cpp`, reusing existing fields where possible:

- `1200` should influence initial `field.radius` and/or reduce the variance between small and large consumes;
- `1201` should scale `field.bonus_damage_mult` or the pulse base damage path;
- `1202` should add a stronger consumed-`Bloodthirst` multiplier than the current baseline formula;
- `1203` should increase `field.move_follow_speed`.

Only add new `BloodSeaFieldComponent` fields if a value must persist after cast; otherwise keep the math local to cast/update.

```cpp
struct BloodSeaFieldComponent {
    float bonus_damage_mult = 1.0f;
    float leech_ratio = 0.12f;
    float move_follow_speed = 10.0f;
};
```

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*"`

Expected: PASS.

### Task 3: Implement the low-life and sustain branch with an explicit scope decision

**Files:**
- Modify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Possibly modify: `assets/data/mastery_skill_trees.json`
- Test: `tests/unit/SkillBehaviorGuardTests.cpp`
- Test: `tests/unit/BladeMasteryTests.cpp`

**Step 1: Write the failing test**

Add narrow tests for the sustain branch:

- `1204`: low-health damage increase
- `1206`: stronger danger/deadliness scaling at low health
- `1209`: Bloodthirst increases leech efficiency
- `1210`: low-health leech gets stronger
- `1212`: Blood Sea raises healing ceiling or an equivalent local sustain cap
- `1213`: overflow sustain feeds new Bloodthirst

```cpp
SUBCASE("Skill 12 sustain nodes change leech and low-life scaling") {
    // Use the existing field-cast pattern.
    // Assert against real values already present or explicitly added:
    // bonus_damage_mult, leech_ratio, and any newly-added local sustain-cap field.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*,[Unit] Blade Mastery Service - Heavenly Sword and Demon Blade shared state"`

Expected: FAIL.

**Step 3: Make the scope decision before writing code**

Verify whether the current repo already has a reusable healing-cap primitive. If it does not, do **not** add a broad global subsystem in this slice.

Use this rule:

- if there is an existing healing-cap path, wire `1212` into it;
- if not, keep the sustain work local to `BloodSea` and `BladeResourceService`, and rewrite `1212` in `assets/data/mastery_skill_trees.json` to the narrower behavior that is actually implemented.

**Step 4: Write minimal implementation**

Implement the branch after the scope choice is clear:

- low-life modifiers in `BloodSea::UpdateField` / `DealPulse` path (`1204`, `1206`);
- stronger leech curve from current `leech_ratio` and consumed Bloodthirst (`1209`, `1210`);
- overflow-heal-to-Bloodthirst behavior through the existing recovery keystone path (`1213`);
- a minimal local sustain-cap field only if required for `1212`.

**Step 5: Run focused tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*,[Unit] Blade Mastery Service - Heavenly Sword and Demon Blade shared state"`

Expected: PASS.

### Task 4: Implement pursuit, linked-pressure, and void-extension nodes

**Files:**
- Modify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Modify: `src/game/components/SkillDefs.hpp`
- Test: `tests/unit/SkillBehaviorGuardTests.cpp`
- Test: `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`

**Step 1: Write the failing test**

Cover the remaining missing runtime promises and group them by the mechanisms that already exist in `BloodSea.cpp`:

- `1205`: hunted-target / pursued-target pressure
- `1208`: pulse aftershock or short trailing damage window
- `1214`: Bloodthirst gained from recovery briefly empowers the intended linked Blade Ascendant skill
- `1215`: stronger close-range suppression
- `1216`: faster pulse cadence
- `1218`: linked pulse carries extra suppression
- `1219`: longer duration
- `1223`: stronger / longer void miasma damage when `1220` is active

```cpp
SUBCASE("Skill 12 pursuit and void nodes preserve torrent and ring identity") {
    // Compare 1221 and 1222 with shared support nodes allocated.
    // Assert against real fields: duration, damage_interval, move_follow_speed,
    // linked_pulse_cooldown, resist_shred, and any new hunted-target state.
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*,[Integration] SkillKeyNodeMatrix*12*"`

Expected: FAIL.

**Step 3: Write minimal implementation**

Implement the remaining branch with the smallest new state model that fits current code:

- add one tracked target field only if `1205` cannot be expressed from `exec.target_pos` or linked-hit state alone;
- keep linked-skill whitelisting explicit in `HandleLinkedHit`;
- implement `1216`, `1218`, and `1219` through `damage_interval`, linked pulse timing, and `duration`;
- implement `1208` and `1223` through the pulse/debuff path already used by `1220`/`1224`.

Make sure the new values compose cleanly with `1221`, `1222`, `1224`, and `1220` without collapsing the distinction between torrent and ring forms.

**Step 4: Run the focused guard suite**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*,[Integration] SkillKeyNodeMatrix*12*"`

Expected: PASS.

### Task 5: Add blocking shared support for cleanup, HUD, and persistence

**Files:**
- Modify: `src/game/systems/skill/BladeMasteryService.cpp`
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Verify: `src/game/data/SaveData.hpp`
- Test: `tests/unit/BladeMasteryTests.cpp`
- Test: `tests/tech/UITests.cpp`

**Step 1: Write the failing test**

Add one service-level test for mastery-switch cleanup expectations and one HUD/tech test for the new Demon Blade runtime states, using existing service names and raw ECS inspection.

```cpp
TEST_CASE("[Unit] Blade Mastery Service - switching away from Demon Blade clears BloodSea fields") {
    // Build from CreateBladeAscendant(registry, 50).
    // Spawn a real Blood Sea field by casting skill 12.
    // Call BladeMasteryService::SelectMastery(registry, player, BladeMasteryId::SwordSaint).
    // Assert registry.view<BloodSeaFieldComponent>().begin() == end().
}
```

**Step 2: Run tests to verify they fail**

Run: `./build.bat`
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Demon Blade*,[Tech] UI*Demon Blade*"`

Expected: FAIL.

**Step 3: Write minimal implementation**

- define the cleanup contract once: when Blade Ascendant profession is lost or selected mastery changes away from Demon Blade, owned `BloodSeaFieldComponent` entities are destroyed;
- expose only the most valuable new HUD text, such as active form, low-life danger, or stronger miasma pressure;
- keep persistence changes minimal and compatible with existing save data;
- if no new persisted state is added, leave `SaveData.hpp` unchanged and note that explicitly in the implementation changelog.

**Step 4: Run focused tests to verify they pass**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade Mastery Service*Demon Blade*,[Tech] UI*Demon Blade*"`

Expected: PASS.

### Task 6: Full verification

**Files:**
- Verify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Verify: `src/game/components/SkillDefs.hpp`
- Verify: `src/game/systems/skill/BladeMasteryService.cpp`
- Verify: `src/game/systems/ui/PlayerHUD.cpp`
- Verify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Verify: `tests/unit/BladeMasteryTests.cpp`
- Verify: `tests/integration/SkillSystemTests.cpp`
- Verify: `tests/tech/UITests.cpp`

**Step 1: Run the focused Demon Blade doctest cases**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillBehaviorGuard - Demon Blade*,[Unit] Blade Mastery Service*Demon Blade*,[Integration] SkillSystem - Blood Sea*,[Integration] SkillKeyNodeMatrix*12*,[Tech] UI*Demon Blade*"`

Expected: PASS.

**Step 2: Build the project**

Run: `./build.bat`

Expected: Build completes successfully.

**Step 3: Run CI-labelled verification**

Run: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

Expected: PASS.

**Step 4: Perform one short manual sanity check**

Verify in-game that:

- low-life Demon Blade play visibly changes pressure and sustain behavior;
- torrent and ring transmuters still read differently;
- switching out of Demon Blade does not leave stale field behavior behind.

## Data-rescope fallback

If a node promise cannot be implemented sanely without creating a broad new subsystem, update `assets/data/mastery_skill_trees.json` so the node description matches the narrower behavior actually implemented in this slice. Do not hide a scope explosion inside the combat plan.
