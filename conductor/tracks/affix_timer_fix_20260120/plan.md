# Plan: Monster Affix Timer Fix

Addressing timer conflicts when multiple affixes are active on the same entity.

## Phase 1: Component Refactoring
*   [ ] **Task 1.1: Refactor `MonsterAffixComponent`**
    *   File: `src/game/data/MonsterAffixRegistry.hpp`
    *   Replace `timer1`, `timer2` with `std::array<float, 4> timers`.
    *   Initialize timers to 0.0f.

## Phase 2: System Logic Update
*   [ ] **Task 2.1: Update `MonsterAffixSystem::Update`**
    *   File: `src/game/systems/combat/MonsterAffixSystem.hpp` (and .cpp if exists, currently header-only mostly)
    *   Iterate through affixes with index.
    *   Pass index to all `Process...` functions.
*   [ ] **Task 2.2: Update Process Signatures**
    *   File: `src/game/systems/combat/MonsterAffixSystem.hpp`
    *   Update `ProcessMolten`, `ProcessTeleporter`, `ProcessFrozen`, `ProcessVoidZone`, `ProcessShielding`, `ProcessVortex`, `ProcessWaller`.
    *   Ensure each uses `affix.timers[idx]` instead of shared timers.

## Phase 3: Verification
*   [ ] **Task 3.1: Update Integration Test**
    *   File: `tests/integration/NemesisScalingTest.hpp`
    *   Update the "Molten" test to verify it doesn't break "Shielding".
    *   Explicitly check that `Shielding` triggers even when `Molten` is active.
*   [ ] **Task 3.2: Regression Run**
    *   Run `tests_runner.exe` to ensure no other systems are affected.
