# Blade Ascendant Heavenly Sword + Demon Blade Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ship the remaining two Blade Ascendant masteries, `Heavenly Sword` and `Demon Blade`, in one pass with full-delivery scope: mastery data, runtime resources, signature skills, complete mastery trees, HUD/UI feedback, persistence, and regression coverage.

**Architecture:** Reuse the existing Blade Ascendant mastery stack introduced for Sword Saint and extend it without reworking the core skill pipeline. Implement one shared preparation layer, then complete one vertical slice for Heavenly Sword and one vertical slice for Demon Blade, and finish with a single polish and verification pass.

**Tech Stack:** C++20, CMake, MSVC multi-config, JSON-driven gameplay data, doctest/CTest, Raylib UI.

---

## Scope Freeze

### In Scope
- Add `HeavenlySword` and `DemonBlade` mastery profiles to the existing Blade Ascendant mastery registry.
- Add `SpiritBladeTier` and `Bloodthirst` resource semantics to the existing runtime resource stack.
- Register skill `11` as `天剑降临` and skill `12` as `血海` through the normal skill registry/runtime path.
- Implement the full mastery-specific specialization trees for both signature skills inside `assets/data/mastery_skill_trees.json`.
- Update linked skill behaviors, HUD/UI, persistence, and regression tests.

### Out of Scope
- New base Blade Ascendant skills.
- Equipment pivot routes and unique-item couplings.
- Global UI redesigns outside the existing mastery/HUD surfaces.
- Audio overhaul or large VFX framework changes.

## Locked Decisions

- Keep the current mastery framework; do not replace `BladeMasteryService`, `BladeResourceService`, or the normal skill behavior registration path.
- Reserve skill ids `11` (`天剑降临`) and `12` (`血海`).
- Keep mastery-specific trees in `assets/data/mastery_skill_trees.json` and regenerate compact contracts after data changes.
- Do not create a separate post-MVP polish milestone; threshold telegraphing and readable resource feedback are required in this pass.
- Keep mastery-specific code localized to Blade Ascendant systems/behaviors; do not add generic cross-profession life-spend behavior.

## Deliverable Snapshot

At the end of this plan the repo should support:

- Selecting any of the three Blade Ascendant masteries in the skill hub.
- Running `Sword Saint`, `Heavenly Sword`, or `Demon Blade` on the same mastery framework.
- Displaying `Sword Flow`, `Spirit Blade Tier`, or `Bloodthirst` in the same HUD widget surface with mastery-specific labels and thresholds.
- Casting `天剑降临` and `血海` through the normal skill runtime.
- Loading full mastery-specific signature trees for all three Blade Ascendant masteries.
- Passing the required build + labeled CTest matrix.

## Task 1: Extend shared mastery data for all three Blade Ascendant masteries

**Files:**
- Modify: `src/game/data/BladeMasteryData.hpp`
- Modify: `src/game/data/BladeMasteryRegistry.hpp`
- Modify: `src/game/data/BladeMasteryRegistry.cpp`
- Modify: `assets/data/blade_masteries.json`
- Test: `tests/unit/BladeMasteryTests.cpp`

**Step 1: Write the failing test**

Add coverage in `tests/unit/BladeMasteryTests.cpp` that asserts:

```cpp
CHECK(registry.GetProfile(BladeMasteryId::HeavenlySword) != nullptr);
CHECK(registry.GetProfile(BladeMasteryId::DemonBlade) != nullptr);
CHECK(registry.GetProfile(BladeMasteryId::HeavenlySword)->resource_kind == BladeResourceKind::SpiritBladeTier);
CHECK(registry.GetProfile(BladeMasteryId::DemonBlade)->resource_kind == BladeResourceKind::Bloodthirst);
CHECK(registry.GetProfile(BladeMasteryId::HeavenlySword)->signature_skill_id == 11);
CHECK(registry.GetProfile(BladeMasteryId::DemonBlade)->signature_skill_id == 12);
```

**Step 2: Run the targeted test and confirm it fails**

Run: `./build.bat check`

Expected: unit test fails because the new mastery ids, resource kinds, and profiles do not exist yet.

**Step 3: Write the minimal implementation**

- Add `HeavenlySword` and `DemonBlade` to `BladeMasteryId`.
- Add `SpiritBladeTier` and `Bloodthirst` to `BladeResourceKind`.
- Add mastery profiles to `assets/data/blade_masteries.json` with unlock/debug fields aligned to Sword Saint conventions.
- Keep max resource at `10` for both new masteries unless later tests prove a different cap is required.

**Step 4: Re-run the test**

Run: `./build.bat check`

Expected: registry tests pass and existing Sword Saint profile tests remain green.

**Step 5: Checkpoint**

Record the reserved ids and resource kinds in the plan notes if any values changed during implementation.

## Task 2: Extend runtime state and persistence for attunement and blood-oath semantics

**Files:**
- Modify: `src/game/components/SkillDefs.hpp`
- Modify: `src/game/data/SaveData.hpp`
- Modify: `src/engine/persistence/SaveManager.cpp`
- Modify: `src/game/systems/skill/BladeResourceService.hpp`
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Test: `tests/unit/BladeMasteryTests.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`

**Step 1: Write the failing tests**

Add tests for:

- current Heavenly Sword attunement survives save/load
- Demon Blade life-spend mode survives save/load
- changing mastery remaps the active resource kind without corrupting legacy `SwordIntentComponent`

Suggested assertions:

```cpp
CHECK(resource.kind == BladeResourceKind::SpiritBladeTier);
CHECK(resource.attunement == BladeAttunement::Lightning);
CHECK(resource.kind == BladeResourceKind::Bloodthirst);
CHECK(resource.life_spend_enabled);
```

**Step 2: Run targeted tests and confirm failure**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Blade mastery*"`

Expected: compilation or runtime failure because the new state fields are missing.

**Step 3: Write the minimal implementation**

- Extend `BladeResourceComponent` with only the shared state needed across saves/runtime, for example:

```cpp
enum class BladeAttunement : uint8_t { None, Lightning, Cold, Fire };

struct BladeResourceComponent {
    BladeResourceKind kind{BladeResourceKind::None};
    int current{0};
    int max{0};
    float decay_timer{0.0f};
    BladeAttunement attunement{BladeAttunement::None};
    bool life_spend_enabled{false};
};
```

- Add matching save fields with backward-compatible defaults.
- Keep mastery-specific transient windows in services/behaviors unless they are truly persistent.

**Step 4: Re-run the targeted tests**

Run: `./build.bat check`

Expected: persistence and remap coverage passes.

**Step 5: Sanity check existing saves**

Run the existing save/load regression suite if available through `./build.bat check` and make sure old saves still load with defaults.

## Task 3: Add shared mastery selection and HUD mode support for all three resource types

**Files:**
- Modify: `src/game/systems/skill/BladeMasteryService.cpp`
- Modify: `src/game/systems/skill/BladeMasteryService.hpp`
- Modify: `src/game/systems/ui/UISkillHub.cpp`
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Modify: `src/game/systems/ui/SwordIntentWidget.hpp`
- Modify: `src/game/systems/ui/SwordIntentWidget.cpp`
- Test: `tests/tech/UITests.cpp`

**Step 1: Write the failing UI tests**

Add smoke coverage that asserts:

- skill hub lists `Sword Saint`, `Heavenly Sword`, `Demon Blade`
- resource widget label and thresholds change with mastery
- Heavenly Sword shows current attunement state
- Demon Blade shows bloodthirst/danger state instead of sword-flow text

**Step 2: Run the targeted UI test**

Run: `./bin/NoMoreDayTests.exe --test-case="[Tech] UI*Blade*"`

Expected: UI assertions fail because only Sword Saint surfaces are wired.

**Step 3: Write the minimal implementation**

- Extend `BladeMasteryService::RefreshPlayerState(...)` so mastery selection initializes the correct resource presentation mode.
- Teach `SwordIntentWidget` to render one shared widget with resource-mode-specific strings, thresholds, and colors.
- Keep the widget generic; do not fork a new widget per mastery.

**Step 4: Re-run the UI tests**

Run: `./build.bat check`

Expected: UI smoke tests pass and Sword Saint display still works.

**Step 5: Manual smoke check**

Launch the game/editor build if part of your normal loop and verify mastery switching updates the same HUD region without overlap.

## Task 4: Register `天剑降临` as Blade Ascendant skill `11`

**Files:**
- Modify: `assets/data/skills.json`
- Modify: `assets/data/skill_contracts_compact.json`
- Modify: `scripts/gen_skill_contracts.py`
- Modify: `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`
- Create: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Test: `tests/integration/SkillContractRegistryTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing tests**

Add coverage that asserts skill `11` exists, is mastery-gated to `HeavenlySword`, and has a registered behavior entry.

**Step 2: Run the targeted registration tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Integration] Skill contract*"`

Expected: failure because skill `11` and its behavior are missing.

**Step 3: Write the minimal implementation**

- Add skill `11` to `assets/data/skills.json` with tags matching the approved design:

```json
{
  "id": 11,
  "name_key": "天剑降临",
  "tags": ["Spell", "Area", "Elemental", "Mastery", "Field"],
  "mana_cost": 24.0,
  "cooldown": 12.0
}
```

- Register `HeavenlySwordDescent` in `SkillBehaviorRegistry.cpp`.
- Regenerate compact contracts after the JSON update.

**Step 4: Re-run the targeted tests**

Run: `python scripts/gen_skill_contracts.py && ./build.bat check`

Expected: skill registration tests pass and no compact-contract drift remains.

**Step 5: Guard the gating**

Confirm non-Heavenly-Sword Blade Ascendant masteries cannot cast skill `11` unless explicitly allowed by future design.

## Task 5: Implement Heavenly Sword resource spend, field behavior, and linked-skill loop

**Files:**
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Modify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Modify: `src/game/systems/skill/behaviors/BladeFormation.cpp`
- Modify: `src/game/systems/skill/behaviors/InfiniteBlades.cpp`
- Modify: `src/game/systems/skill/behaviors/SwordArray.cpp`
- Modify: `src/game/systems/skill/behaviors/RendingWave.cpp`
- Modify: `src/game/systems/skill/behaviors/MindBlade.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing gameplay tests**

Cover these behaviors:

- Heavenly Sword gains `SpiritBladeTier` from the intended linked sources.
- Casting `天剑降临` consumes up to the allowed tier cap and scales impact/field size.
- Field ticks recognize linked hits from `灵剑决` / `万剑归宗` / `剑阵·诛仙`.
- Current attunement determines element conversion and field effect branch.

**Step 2: Run the targeted gameplay tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Functional] Heavenly Sword*"`

Expected: failure because the field and spend logic is not implemented.

**Step 3: Write the minimal implementation**

- Add helper APIs in `BladeResourceService` for attunement lookup, capped spend, and tier gain from recognized Heavenly Sword sources.
- Implement `HeavenlySwordDescent.cpp` with a dual-phase cast model: initial impact plus timed field.
- Add a shared recognition path for “field-linkable sword hits” rather than duplicating per-skill checks.

Suggested helper shape:

```cpp
int ConsumeSpiritBladeTier(entt::registry& registry, entt::entity owner, int requested_max);
BladeAttunement GetCurrentAttunement(const entt::registry& registry, entt::entity owner);
bool IsHeavenlySwordLinkedSource(uint32_t skill_id);
```

**Step 4: Re-run the targeted tests**

Run: `./build.bat check`

Expected: Heavenly Sword loop tests pass without regressing Sword Saint behavior.

**Step 5: Add one boss-pressure regression**

Keep a focused test that proves the impact/field split still works against a single target and not only in AoE cases.

## Task 6: Add the full Heavenly Sword mastery tree to `mastery_skill_trees.json`

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`
- Modify: `assets/data/skill_contracts_compact.json`
- Modify: `scripts/gen_skill_contracts.py`
- Modify: `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Test: `tests/integration/SkillContractRegistryTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing contract test**

Add assertions for mastery `heavenly_sword` / skill `11`:

- node count `24-26`
- exactly the approved keystone / trigger / synergy roles
- no transmuter roles because element comes from attunement
- all trigger metadata populated

**Step 2: Run the contract test and confirm failure**

Run: `./bin/NoMoreDayTests.exe --test-case="[Integration] Skill contract registry*"`

Expected: the Heavenly Sword tree entry does not exist yet.

**Step 3: Write the minimal implementation**

- Add all approved nodes `1100-1124` for skill `11` in `assets/data/mastery_skill_trees.json`.
- Encode:
  - keystones: `天穹贯星`, `万象轮转`, `天相极化`
  - trigger: `剑雨回响`
  - synergy: `剑阵同调`
  - capped extra resist reduction from branch D
- Implement only the node hooks actually needed in `HeavenlySwordDescent.cpp` and linked behaviors; keep passive stat-only nodes data-driven where possible.

**Step 4: Re-run the contract and gameplay tests**

Run: `python scripts/gen_skill_contracts.py && ./build.bat check`

Expected: contract and gameplay tests pass together.

**Step 5: Verify exclusivity rules**

Add one regression that ensures the keystone branches do not stack in forbidden combinations if exclusion groups are configured.

## Task 7: Register `血海` as Blade Ascendant skill `12`

**Files:**
- Modify: `assets/data/skills.json`
- Modify: `assets/data/skill_contracts_compact.json`
- Modify: `scripts/gen_skill_contracts.py`
- Modify: `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`
- Create: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Test: `tests/integration/SkillContractRegistryTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing tests**

Add coverage that asserts skill `12` exists, is mastery-gated to `DemonBlade`, and has a registered behavior.

**Step 2: Run the targeted registration tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Integration] Skill contract*"`

Expected: failure because skill `12` and its behavior are missing.

**Step 3: Write the minimal implementation**

- Add skill `12` with the approved shape:

```json
{
  "id": 12,
  "name_key": "血海",
  "tags": ["Spell", "Area", "Physical", "DoT", "Void", "Mastery"],
  "mana_cost": 0.0,
  "cooldown": 14.0
}
```

- Treat life payment in behavior/service logic, not as a negative mana hack in JSON.
- Register `BloodSea` in `SkillBehaviorRegistry.cpp`.

**Step 4: Re-run the targeted tests**

Run: `python scripts/gen_skill_contracts.py && ./build.bat check`

Expected: skill registration tests pass for skill `12`.

**Step 5: Guard the gating**

Confirm non-Demon-Blade masteries cannot cast skill `12`.

## Task 8: Implement Demon Blade life-spend, bloodthirst generation, and linked-skill loop

**Files:**
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Modify: `src/game/systems/skill/BladeResourceService.hpp`
- Modify: `src/game/systems/skill/SkillSystem.cpp`
- Modify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Modify: `src/game/systems/skill/behaviors/PhantomFlash.cpp`
- Modify: `src/game/systems/skill/behaviors/BladeBoomerang.cpp`
- Modify: `src/game/systems/skill/behaviors/BladeWard.cpp`
- Modify: `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- Modify: `src/game/systems/skill/behaviors/MindBlade.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing gameplay tests**

Cover these behaviors:

- Demon Blade mana-cost skills spend life instead of mana.
- low-life melee hits and overflow leech grant `Bloodthirst`.
- each `Bloodthirst` stack increases damage taken and outgoing damage as specified.
- `血海` consumes all current `Bloodthirst` and creates a moving field around the player.
- `绝影绝剑` danger window boosts `血海` when the synergy node is active.

**Step 2: Run the targeted gameplay tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Functional] Demon Blade*"`

Expected: failure because life-spend and bloodthirst logic is missing.

**Step 3: Write the minimal implementation**

- Add `BladeResourceService` helpers for Demon Blade:

```cpp
bool ShouldSpendLifeForSkill(const entt::registry& registry, entt::entity owner, uint32_t skill_id);
float ComputeDemonBladeLifeCost(const SkillData& skill);
void GainBloodthirst(entt::registry& registry, entt::entity owner, int amount, BloodthirstSource source);
int ConsumeAllBloodthirst(entt::registry& registry, entt::entity owner);
```

- Route life payment through `SkillSystem.cpp` pre-cast checks so normal mana code stays intact for other professions/masteries.
- Implement `BloodSea.cpp` as a moving field that tracks the owner and stores the consumed-stack snapshot for damage/healing calculations.

**Step 4: Re-run the targeted tests**

Run: `./build.bat check`

Expected: Demon Blade loop passes without changing non-Demon-Blade mana behavior.

**Step 5: Add one survivability guard test**

Keep a regression that proves life-spend cannot bypass death-prevention/health-floor rules incorrectly.

## Task 9: Add the full Demon Blade mastery tree to `mastery_skill_trees.json`

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`
- Modify: `assets/data/skill_contracts_compact.json`
- Modify: `scripts/gen_skill_contracts.py`
- Modify: `src/game/systems/skill/behaviors/BloodSea.cpp`
- Modify: `src/game/systems/skill/behaviors/PhantomFlash.cpp`
- Test: `tests/integration/SkillContractRegistryTests.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1: Write the failing contract test**

Add assertions for mastery `demon_blade` / skill `12`:

- node count `24-26`
- approved keystones / trigger / synergy / transmuters exist
- trigger metadata populated
- transmuters are mutually exclusive

**Step 2: Run the contract test and confirm failure**

Run: `./bin/NoMoreDayTests.exe --test-case="[Integration] Skill contract registry*"`

Expected: the Demon Blade tree entry does not exist yet.

**Step 3: Write the minimal implementation**

- Add nodes `1200-1224` for skill `12` in `assets/data/mastery_skill_trees.json`.
- Encode:
  - keystones: `无间血狱`, `饮海而生`, `虚蚀瘴幕`
  - trigger: `鲜血回灌`
  - synergy: `绝影共噬`
  - transmuters: `血潮奔流`, `血环噬身`
- Keep passive numerical nodes data-driven; add code hooks only for moving-form rewrites, refund logic, and synergy windows.

**Step 4: Re-run the contract and gameplay tests**

Run: `python scripts/gen_skill_contracts.py && ./build.bat check`

Expected: Demon Blade contract and runtime tests pass together.

**Step 5: Verify mutual exclusivity**

Add a regression that ensures only one form transmuter can drive the field shape at a time.

## Task 10: Finish readability, feedback, and full regression sync

**Files:**
- Modify: `src/game/systems/ui/SwordIntentWidget.cpp`
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Modify: `src/game/systems/vfx/SwordIntentVisualSystem.cpp`
- Modify: `tests/unit/BladeMasteryTests.cpp`
- Modify: `tests/integration/SkillSystemTests.cpp`
- Modify: `tests/functional/SkillBehaviors.cpp`
- Modify: `tests/tech/UITests.cpp`
- Modify: `设计文档/职业设计草案_剑修.md`

**Step 1: Write the failing readability tests**

Add focused coverage for:

- Heavenly Sword threshold hints at `5+`, `7+`, `10`
- Demon Blade low-life warning and bloodthirst label
- post-cast state resets after `天剑降临` and `血海`

**Step 2: Run the targeted tests**

Run: `./bin/NoMoreDayTests.exe --test-case="[Tech] UI*|[Functional] Blade Ascendant*"`

Expected: feedback tests fail until the widget and visual system are updated.

**Step 3: Write the minimal implementation**

- Extend HUD strings and threshold cues without splitting the widget into multiple implementations.
- Add only the minimal VFX state needed for readability: attunement color cue, spirit-blade threshold emphasis, bloodthirst danger emphasis, field-active cue.
- Update the design doc status notes in `设计文档/职业设计草案_剑修.md` to mark the two masteries as implemented once code and tests are done.

**Step 4: Run the required verification matrix**

Run:

```bash
./build.bat
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
```

Expected: build succeeds, labeled suites pass, and no Blade Ascendant regression remains.

**Step 5: Final checkpoint**

- Record any balance-risk deviations that were required during implementation.
- Do not create a commit unless explicitly requested by the user.

## Notes for the Implementer

- Prefer adding helpers to `BladeResourceService` over scattering mastery-specific checks across unrelated systems.
- Use the Heavenly Sword field-link logic and Demon Blade life-spend routing as the only new cross-skill abstractions in this pass.
- If a node can stay purely data-driven, keep it out of C++.
- If a behavior branch needs transient state that is not persisted, keep it local to the behavior/component instead of bloating the shared save schema.
- Re-run `python scripts/gen_skill_contracts.py` immediately after editing `assets/data/skills.json` or `assets/data/mastery_skill_trees.json`.

## Recommended Execution Order

1. Task 1 -> Task 3 to stabilize shared foundation.
2. Task 4 -> Task 6 to finish Heavenly Sword end-to-end.
3. Task 7 -> Task 9 to finish Demon Blade end-to-end.
4. Task 10 for final feedback and full regression.

Plan complete and saved to `conductor/docs/plans/2026-03-07-blade-ascendant-heavenly-sword-demon-blade-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration.

**2. Parallel Session (separate)** - Open a new session with `executing-plans`, batch execution with checkpoints.
