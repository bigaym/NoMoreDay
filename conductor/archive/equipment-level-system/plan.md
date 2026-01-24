# Implementation Plan: Equipment Level System

## Phase 1: The Schema (Data Layer)
**Goal**: Update data structures to support item levels.
- [ ] **Task 1.1**: Modify `ItemComponent.hpp`
    - Add `int itemLevel = 1` field.
    - Update `to_json` to include `itemLevel`.
    - Update `from_json` to include `itemLevel`.
- [ ] **Task 1.2**: Update `ItemFactory::serializeItem` in `ItemFactory.cpp` to include `itemLevel` in DTO (if needed for save system).
    - *Note*: `ItemComponent` JSON support handles internal serialization, but `serializeItem` creates a DTO for `SaveManager`. Check if DTO needs update.
    - **Risk**: Serialization versioning compatibility. Existing saves will default `itemLevel` to 0 or 1. `from_json` should handle missing field gracefully (it does by default if using `value_or` or try-catch, or manual check).
    - *Action*: In `from_json`, use `if (j.contains("itemLevel")) ... else i.itemLevel = 1;`.

## Phase 2: The Scaling (Factory Logic)
**Goal**: Generate items with correct level and scaled stats.
- [ ] **Task 2.1**: Implement Scaling Helper in `ItemFactory.cpp`.
    - Function: `float GetLevelMultiplier(int level)`.
    - Formula: `1.0f + (level - 1) * (1.5f / 99.0f)`.
- [ ] **Task 2.2**: Update `ItemFactory` creation pipeline.
    - In `createRandomLoot` / `createItem`, ensure `itemLevel` is set.
    - Apply multiplier to `attack`, `defense`, and `value`.
    - *Verify*: Level 100 sword should have ~2.5x damage of Level 1 sword.

## Phase 3: The Gatekeeper (Enforcement)
**Goal**: Prevent low-level characters from equipping high-level gear.
- [ ] **Task 3.1**: Update `InventorySystem::equipItem`.
    - Fetch character's `EnemyStateComponent` (if enemy) or `PlayerState`/`CombatStats` (if player) to get their level.
    - *Note*: Player level source needs to be confirmed. `CombatStats` usually has level? Or `PlayerState`?
    - *Check*: `ItemFactory::GenerateDrops` gets level from `EnemyStateComponent` or passed `areaLevel`. Player level is likely in `CombatStats` or `ExperienceComponent`.
    - Add check: `if (charLevel < itemLevel) return false;`.

## Phase 4: The Lens (UI)
**Goal**: Visualize item level and requirements.
- [ ] **Task 4.1**: Update `UIRenderer::DrawTooltip`.
    - Add logic to draw "物品等级: X".
    - Fetch local player level for comparison.
    - Colorize Green/Red.

## Phase 5: Verification
- [ ] **Test**: Spawn Level 1 item, check stats.
- [ ] **Test**: Spawn Level 100 item, check stats (approx 2.5x).
- [ ] **Test**: Try to equip Level 100 item on Level 1 char -> Fail.
- [ ] **Test**: Level up (or hack level) -> Succeed.
