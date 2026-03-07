# Blade Ascendant Mastery / Sword Saint Implementation Plan

> **For Implementers:** REQUIRED SUB-SKILL: Use `executing-plans` to execute this plan task-by-task.

**Goal:** Add a mastery layer for Blade Ascendant on top of the existing skill specialization stack, migrate the core combat resource from talent-gated `SwordIntent` ownership to profession/mastery-owned runtime state, and ship a first playable `Sword Saint` MVP with a formal unlock level of 50 and a debug override of 5 for testing.

**Architecture:** Reuse the current `SkillRegistry + SkillContract + SkillSystem + specialized_slots` pipeline and add a thin mastery/resource layer above it. Keep signature skills inside the normal skill data/runtime path, and treat mastery selection, unlock state, and resource mapping as profession state rather than astrolabe node state.

**Tech Stack:** C++20, CMake, MSVC multi-config, JSON-driven gameplay data, doctest/CTest, Raylib UI.

## Scope Freeze

### In Scope
- Shared mastery framework for Blade Ascendant only.
- `Sword Saint` as the only playable mastery in this pass.
- Profession-owned blade resource runtime with compatibility wrappers for existing `SwordIntent` code.
- Mastery unlock gate, mastery selection UI, persistence, and HUD updates.
- `Seven Star Slash` signature skill registration and a minimal `Flowing Thrust` / `Rending Wave` link loop.

### Out of Scope
- `Heavenly Sword` and `Demonic Blade` gameplay.
- Full mastery outer-ring tree.
- Equipment pivot routes, legendary coupling, and endgame balance.
- Large VFX/audio polish beyond what current runtime hooks already support.

## Locked Decisions

- Keep `Scheme A`: build mastery on top of the existing skill/spec architecture.
- Do **not** let a talent node create/destroy the core Blade Ascendant resource system.
- Formal `Sword Saint` unlock level remains `50`.
- Testing uses a debug unlock override level of `5`; do not use inflated starting XP as the primary test strategy.
- Keep compatibility shims for `GainSwordIntent` / `ConsumeSwordIntent` in the first migration pass.
- Keep mastery state separate from `AstrolabeComponent`.

## Deliverable Snapshot

At the end of this plan the repo should support:
- Selecting `Sword Saint` from the skill hub once the unlock gate passes.
- Switching Blade Ascendant profession resource semantics from default `SwordIntent` to `SwordFlow`.
- Casting `Seven Star Slash` through the normal skill runtime.
- Running a minimal combat loop: `Flowing Thrust -> gain sword flow -> Rending Wave responds/spends -> Seven Star Slash burst`.
- Saving and loading selected mastery, blade resource state, and signature skill state.

## Implementation Update (2026-03-07)

**Status:** Implemented in code; final verification now depends on the required build/CTest matrix.

### Completed outcomes

- Added mastery schema + data loading with `src/game/data/BladeMasteryData.hpp`, `src/game/data/BladeMasteryRegistry.hpp`, `src/game/data/BladeMasteryRegistry.cpp`, and `assets/data/blade_masteries.json`.
- Added mastery-owned runtime state in `src/game/components/SkillDefs.hpp` via `BladeMasteryComponent`, `BladeResourceComponent`, and `BladeSignatureSkillComponent`, while keeping `SwordIntentComponent` as a synchronized compatibility mirror.
- Added `src/game/systems/skill/BladeMasteryService.hpp/.cpp` and `src/game/systems/skill/BladeResourceService.hpp/.cpp` to own unlock, selection, resource mapping, debug override, and bridge behavior.
- Removed talent ownership of the core resource lifecycle in `src/game/systems/stats/AttributePipeline.cpp`; talent effects now modify an existing profession-owned runtime instead of creating/removing the resource.
- Updated `src/game/systems/skill/SkillSystem.cpp` so mastery-owned blade resources participate in on-hit gain, decay, spend, and pre-cast logic without breaking legacy `SwordIntent` fallback behavior.
- Added mastery UI + HUD support in `src/game/systems/ui/UISkillHub.cpp`, `src/game/systems/ui/PlayerHUD.cpp`, and `src/game/systems/ui/SwordIntentWidget.hpp/.cpp`.
- Added persistence in `src/game/data/SaveData.hpp` and `src/engine/persistence/SaveManager.cpp`, including the save-version fix that stops overwriting the snapshot header version.
- Added `Seven Star Slash` as skill `10` through normal skill data + contract + behavior registration using `assets/data/skills.json`, `assets/data/skill_contracts_compact.json`, `src/game/systems/skill/behaviors/SevenStarSlash.cpp`, and `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`.
- Wired the Sword Saint MVP loop so `Flowing Thrust` grants extra Sword Flow for Sword Saint, `Rending Wave` node `252` spends current Sword Flow instead of requiring legacy full `SwordIntent`, and `Seven Star Slash` consumes all remaining Sword Flow.

### Realized test coverage

- New/updated coverage landed in `tests/unit/BladeMasteryTests.cpp`, `tests/unit/SystemMechanics.cpp`, `tests/integration/SkillSystemTests.cpp`, `tests/integration/SkillContractRegistryTests.cpp`, `tests/integration/GameplaySystems.cpp`, `tests/functional/SkillBehaviors.cpp`, and `tests/tech/UITests.cpp`.
- The implementation was done test-first in slices: registry/runtime foundation, persistence, skill-10 registration/gating, HUD/UI smoke, and the final combat-loop regression.

### Small deviations from the original file plan

- Task 2 components were implemented directly in `src/game/components/SkillDefs.hpp` instead of new standalone component headers to stay aligned with the existing shared runtime-component pattern.
- Task 1/6 testing landed in broader existing test files plus `tests/unit/BladeMasteryTests.cpp` instead of one new test file per subtask.
- `Seven Star Slash` contract integration was completed by adding skill `10` data and regenerating embedded contract blocks via `python scripts/gen_skill_contracts.py`; no `SkillRegistry` source changes were required.
- Formal unlock remains `50`; debug unlock remains `5`, but the runtime toggle is controlled by `BladeMasteryService::SetDebugUnlockOverrideEnabled(...)` rather than being baked into progression flow.

## Implementation Tasks

### Task 1: Add mastery data schema and debug override source

**Files to create**
- `src/game/data/BladeMasteryData.hpp`
- `src/game/data/BladeMasteryRegistry.hpp`
- `src/game/data/BladeMasteryRegistry.cpp`
- `assets/data/blade_masteries.json`
- `tests/unit/BladeMasteryRegistryTests.cpp`

**Files to modify**
- `src/game/data/SaveData.hpp`

**Implementation**
1. Define `BladeMasteryId` with `None`, `SwordSaint`, `HeavenlySword`, `DemonicBlade`.
2. Define `BladeResourceKind` with `SwordIntent`, `SwordFlow`, `SpiritBladeTier`, `Bloodthirst`.
3. Add `BladeMasteryProfile` with at least:
   - `mastery_id`
   - `unlock_level`
   - `resource_kind`
   - `signature_skill_id`
   - `default_linked_skill_ids`
4. Add an optional debug override value to the runtime-loadable mastery profile payload, e.g. `debug_unlock_level_override`.
5. Store `Sword Saint` with `unlock_level = 50` and `debug_unlock_level_override = 5` in dev/test data.
6. Add serialization fields in `src/game/data/SaveData.hpp` for future selected mastery state; use defaults so older saves continue loading.

**Test first**
- Add `tests/unit/BladeMasteryRegistryTests.cpp` to prove `Sword Saint` loads with `unlock_level = 50` and debug override `5`.

**Checkpoint**
- Optional checkpoint commit after the registry and JSON schema are stable.

### Task 2: Introduce mastery-owned runtime components

**Files to create**
- `src/game/components/BladeMasteryComponent.hpp`
- `src/game/components/BladeResourceComponent.hpp`
- `src/game/components/BladeSignatureSkillComponent.hpp`
- `tests/unit/BladeResourceComponentTests.cpp`

**Files to modify**
- `src/game/components/SkillDefs.hpp`

**Implementation**
1. Add `BladeMasteryComponent` with `selected_mastery`, `unlocked_mask`, and `initialized`.
2. Add `BladeResourceComponent` with:
   - `kind`
   - `current`
   - `max`
   - `decay_timer`
   - `last_gain_source`
3. Add `BladeSignatureSkillComponent` with:
   - `skill_id`
   - `cooldown_remaining`
   - `is_available`
4. Keep the existing `SwordIntentComponent` in `src/game/components/SkillDefs.hpp` for compatibility, but mark it as a legacy/bridge representation whose data is sourced from the mastery resource service during migration.

**Test first**
- Add `tests/unit/BladeResourceComponentTests.cpp` for default construction and state transitions.

**Checkpoint**
- Optional checkpoint commit after components compile and unit tests pass.

### Task 3: Create services for mastery selection and blade resource access

**Files to create**
- `src/game/systems/skill/BladeMasteryService.hpp`
- `src/game/systems/skill/BladeMasteryService.cpp`
- `src/game/systems/skill/BladeResourceService.hpp`
- `src/game/systems/skill/BladeResourceService.cpp`
- `tests/unit/BladeMasteryServiceTests.cpp`
- `tests/unit/BladeResourceServiceTests.cpp`

**Files to modify**
- `src/game/systems/combat/ProgressionSystem.cpp`
- `src/game/systems/combat/ProgressionSystem.hpp`

**Implementation**
1. Add `BladeMasteryService::GetEffectiveUnlockLevel` that prefers debug override when enabled, otherwise returns the formal profile level.
2. Add `BladeMasteryService::CanUnlockMastery` and `CanSelectMastery`; key them off `PlayerStats.level` because `src/game/systems/combat/ProgressionSystem.cpp` already maintains it.
3. Add `BladeMasteryService::SelectMastery` to initialize `BladeMasteryComponent`, initialize `BladeResourceComponent`, and bind the signature skill state.
4. Add `BladeResourceService` methods for `Gain`, `Consume`, `CanSpend`, `GetCurrent`, `GetMax`, and `RemapResourceKind`.
5. Keep the service profession-scoped; do not pull astrolabe state into it.

**Test first**
- Extend `tests/unit/ProgressionSystemTests.cpp` or add `tests/unit/BladeMasteryServiceTests.cpp` to cover:
  - level 49 fails formal unlock
  - level 50 passes formal unlock
  - debug override allows unlock at level 5

**Checkpoint**
- Optional checkpoint commit after unlock logic and resource service tests pass.

### Task 4: Break talent ownership of Sword Intent and remap it to profession/mastery state

**Files to modify**
- `src/game/systems/stats/AttributePipeline.cpp`
- `src/game/systems/skill/SkillSystem.hpp`
- `src/game/systems/skill/SkillSystem.cpp`
- `src/game/components/SkillDefs.hpp`
- `tests/unit/SkillBehaviorGuardTests.cpp`
- `tests/integration/SkillSystemTests.cpp`
- `tests/integration/AstrolabeLogicTest.cpp`
- `tests/functional/SkillBehaviors.cpp`
- `tests/tech/UITests.cpp`

**Implementation**
1. Remove the rule in `src/game/systems/stats/AttributePipeline.cpp` that creates/removes the core resource purely from `TraitID::SwordIntentUnlock`.
2. Reframe the trait/node as a modifier of the profession resource rather than its owner.
3. Update `SkillSystem` to route all direct stack gain/spend logic through `BladeResourceService`.
4. Keep compatibility wrappers like `GainSwordIntent` and `ConsumeSwordIntent`; internally they should forward to the new resource service.
5. Ensure old code paths still work for non-mastery Blade Ascendant by mapping them to `BladeResourceKind::SwordIntent`.

**Test first**
- Update existing tests that emplace/assert `SwordIntentComponent` so they either use the bridge path or explicitly assert the compatibility contract.

**Checkpoint**
- Optional checkpoint commit after all existing skill tests still pass under the compatibility layer.

### Task 5: Add mastery UI entry points and HUD resource presentation

**Files to modify**
- `src/game/systems/ui/UISkillHub.cpp`
- `src/game/systems/ui/UISkillHub.hpp`
- `src/game/systems/ui/PlayerHUD.cpp`
- `src/game/systems/ui/SwordIntentWidget.cpp`
- `src/game/systems/ui/SwordIntentWidget.hpp`
- `tests/tech/UITests.cpp`

**Implementation**
1. Add a `Mastery` section to `UISkillHub` above or beside the current specialization slots.
2. Show for each mastery: name, lock state, unlock requirement, and current selection state.
3. Surface debug/test unlock state clearly enough that QA can tell whether the 5-level override is active.
4. Update the HUD path in `src/game/systems/ui/PlayerHUD.cpp` to render the active blade resource through a generalized widget API.
5. Either rename `SwordIntentWidget` later or keep it as a wrapper around a generalized blade-resource drawing function in the first pass.

**Test first**
- Extend `tests/tech/UITests.cpp` with a smoke test that opens the skill hub for a Blade Ascendant with `Sword Saint` unlocked and verifies the draw path does not assert.

**Checkpoint**
- Optional checkpoint commit after UI smoke tests pass.

### Task 6: Persist mastery state, resource state, and signature-skill state

**Files to modify**
- `src/game/data/SaveData.hpp`
- `tests/tech/UITests.cpp`

**Files to create**
- `tests/unit/SaveDataMasterySerializationTests.cpp`

**Implementation**
1. Extend `CharacterSaveData` with:
   - `selected_mastery`
   - blade resource snapshot
   - signature skill snapshot
2. Add version-tolerant JSON defaults so existing saves without mastery data still load.
3. Keep save compatibility focused on presence/absence of new fields; do not force a destructive save upgrade.

**Test first**
- Add `tests/unit/SaveDataMasterySerializationTests.cpp` for round-tripping the new mastery/resource fields.

**Checkpoint**
- Optional checkpoint commit after round-trip serialization passes.

### Task 7: Register `Seven Star Slash` as a normal skill and contract participant

**Files to modify**
- `assets/data/skills.json`
- `assets/data/skill_contracts_compact.json`
- `scripts/gen_skill_contracts.py`
- `src/game/data/SkillRegistry.cpp`
- `src/game/data/SkillRegistry.hpp`
- `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`
- `tests/integration/SkillContractRegistryTests.cpp`
- `tests/fixtures/skill_specialization_keynodes.json`

**Files to create**
- `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- `tests/unit/SevenStarSlashTests.cpp`

**Implementation**
1. Add `Seven Star Slash` to `assets/data/skills.json` as a normal skill entry with its own ID, tags, costs, cooldown, and icon.
2. Add a compact contract entry only if the skill needs a first-pass mini tree or key-node audit; otherwise explicitly document that the signature skill ships without a specialization tree in MVP.
3. Register the behavior in `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`.
4. Keep the implementation on the standard `SkillExecution` path so signature skills do not become a parallel runtime system.

**Test first**
- Add `tests/unit/SevenStarSlashTests.cpp` for cast success, cooldown enforcement, and damage/event emission.

**Checkpoint**
- Optional checkpoint commit after the skill is castable through the normal registry/runtime path.

### Task 8: Wire Sword Saint MVP combat links

**Files to modify**
- `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- `src/game/systems/skill/behaviors/RendingWave.cpp`
- `src/game/systems/skill/SkillSystem.cpp`
- `tests/unit/SkillBehaviorGuardTests.cpp`
- `tests/integration/SkillSystemTests.cpp`
- `tests/functional/SkillBehaviors.cpp`

**Implementation**
1. In `src/game/systems/skill/behaviors/FlowingThrust.cpp`, add the minimal `Sword Saint` resource gain hook.
2. In `src/game/systems/skill/behaviors/RendingWave.cpp`, add the minimal resource response/spend hook.
3. Gate both behaviors on selected mastery so non-`Sword Saint` Blade Ascendant behavior remains unchanged.
4. Expose `Seven Star Slash` availability from `BladeSignatureSkillComponent` when the configured threshold is met.
5. Keep the loop intentionally small for MVP: no full outer-ring passive tree, no equipment pivot logic.

**Test first**
- Add or extend integration coverage to prove:
  - `Flowing Thrust` gains sword flow
  - `Rending Wave` consumes or amplifies from sword flow
  - `Seven Star Slash` becomes available after threshold and spends the expected resource

**Checkpoint**
- Optional checkpoint commit after the full MVP combat loop passes tests.

## Verification Commands

Run from repo root `F:\NoMoreDay`.

### Narrow checks while developing
- `./build.bat debug`
- `./bin/NoMoreDayTests.exe --test-case="[Unit] BladeMasteryRegistry"`
- `./bin/NoMoreDayTests.exe --test-case="[Unit] BladeMasteryService"`
- `./bin/NoMoreDayTests.exe --test-case="[Unit] BladeResourceService"`
- `./bin/NoMoreDayTests.exe --test-case="[Unit] SevenStarSlash"`

### Regression checks before marking complete
- `./build.bat check`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

## Manual QA Checklist

1. Create or load a Blade Ascendant below level 5 and confirm `Sword Saint` is locked.
2. Raise the same character to level 5 in debug/test mode and confirm `Sword Saint` can be selected.
3. Disable debug override and confirm level 5 is no longer enough.
4. Raise or mock the character to level 50 and confirm formal unlock succeeds.
5. Select `Sword Saint`, cast `Flowing Thrust`, and verify blade resource gain appears in the HUD.
6. Cast `Rending Wave` and verify the configured sword-flow response.
7. Trigger `Seven Star Slash` and verify cooldown/resource consumption.
8. Save and reload; verify mastery selection, resource state, and signature skill state persist.
9. Load a non-mastery Blade Ascendant save and verify legacy skill specialization still works.

## Risks and Mitigations

### Risk 1: Hidden `SwordIntentComponent` coupling
- **Why it matters:** Existing tests, HUD, VFX, and behavior files directly touch `SwordIntentComponent`.
- **Mitigation:** Keep a compatibility bridge in the first pass and migrate call sites incrementally.

### Risk 2: Signature skill becomes a special-case runtime
- **Why it matters:** That will make `Heavenly Sword` and `Demonic Blade` much harder later.
- **Mitigation:** Force `Seven Star Slash` through `SkillRegistry`, `SkillExecution`, and `SkillBehaviorRegistry`.

### Risk 3: Mastery state leaks into astrolabe semantics
- **Why it matters:** It blurs profession branch choice with profession passive investment.
- **Mitigation:** Keep mastery data in dedicated components/services and only let astrolabe nodes modify the active resource.

### Risk 4: Debug unlock logic ships as production behavior
- **Why it matters:** It invalidates progression pacing.
- **Mitigation:** Centralize unlock resolution in `BladeMasteryService::GetEffectiveUnlockLevel` and gate override loading behind dev/test config.

## Suggested Execution Order

1. Task 1
2. Task 2
3. Task 3
4. Task 4
5. Task 5
6. Task 6
7. Task 7
8. Task 8

Do not start `Seven Star Slash` or the combat links before the resource ownership migration is stable.

## Definition of Done

- `Sword Saint` mastery data is loaded from its own mastery registry/config source.
- Unlock gate is formally level 50 and testable with a debug override at level 5.
- Core blade resource ownership no longer depends on a talent node existing.
- The HUD and skill hub display mastery/resource state without breaking existing specialization UI.
- `Seven Star Slash` is a standard skill runtime participant.
- The MVP combat loop is test-covered and manually verifiable.
- Existing skill specialization regression suites still pass.
