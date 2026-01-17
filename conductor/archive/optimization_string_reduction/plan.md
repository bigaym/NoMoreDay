# Implementation Plan: String Optimization & Resistance Fix

## Phase 1: Static Enemy Data & Resistance Fix (High Priority)
Goal: Eliminate `EnemyRace` allocation in hot path and enable enemy resistances.

- [x] **Task 1.1**: Define `EnemyRaceData` struct and `kRaceData` conversion table in `src/game/components/EnemyComponent.hpp`.
    - Move logic from `EnemyRace` constructor to static table.
    - Use `Tag` combinations for resistances.
- [x] **Task 1.2**: Update `StatsSystem::Recalculate` to use `kRaceData`.
    - Remove `EnemyRace` instantiation.
    - Read base stats from static table.
- [x] **Task 1.3**: Implement Resistance Application Logic.
    - In `Recalculate`, iterate through the `raceData.resistances` Tag mask.
    - Apply flat resistance values (e.g., +25% or +50% base resistance) to `combat.resistances` for each matching Tag.
- [x] **Task 1.4**: Remove legacy `EnemyRace` struct definition once fully replaced.

## Phase 2: Affix String Removal
Goal: Reduce memory footprint of Item/Affix components.

- [x] **Task 2.1**: Update `Affix` struct in `src/game/components/ItemStats.hpp`.
    - Remove `std::string name`.
    - Update `to_json` / `from_json` to stop serializing the name (or write an empty string/computed value if needed for legacy compat, but preferably remove).
- [x] **Task 2.2**: Update `GetAffixDescription` / `GetAffixName`.
    - Ensure `GetAffixDescription` covers all cases that `.name` was used for.
    - If a specific "name" (e.g. "of the Bear") is needed, implement a `GetAffixName(AffixType, tier)` helper using a static lookup.
- [x] **Task 2.3**: Scan and fix all call sites.
    - Search for usages of `.name` on Affix objects.
    - Replace with `GetAffixDescription` or new helper.
    - Likely areas: `ItemTooltip`, `InventorySystem`, `LootSystem`.

## Phase 3: Validation
- [x] **Task 3.1**: Verify `StatsSystem` performance (profile if possible, or check heap alloc counts).
- [x] **Task 3.2**: Verify Enemy Resistances in-game (e.g. hitting a skeleton with Poison should deal less damage).
- [x] **Task 3.3**: Verify Item Tooltips display correctly without the cached string.
