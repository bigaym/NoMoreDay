# Plan: Item and Drop System

## Phase 1: Core Item Data Structure & Equipment Slots

- [x] Task: Define basic `Item` component and `Equipment` component structure.
    - [x] Sub-task: Create `ItemComponent.hpp` and define essential item properties (ID, name, rarity, etc.).
    - [x] Sub-task: Create `EquipmentComponent.hpp` to manage equipped items and their slots.
- [x] Task: Implement `EquipmentSlot` enum and mapping.
    - [x] Sub-task: Define `EquipmentSlot` enum in `EquipmentComponent.hpp` with 11 slots.
    - [x] Sub-task: Implement helper functions for adding/removing items from slots.
- [x] Task: Integrate `EquipmentComponent` with player entity.
    - [x] Sub-task: Add `EquipmentComponent` to player creation logic.
    - [x] Sub-task: Implement basic equipping/unequipping logic (without stat changes yet).
- [x] Task: Write Unit Tests for Core Item & Equipment Components.
    - [x] Sub-task: Create `ItemSystemTest.cpp` and `EquipmentSystemTest.cpp`.
    - [x] Sub-task: Test item component initialization and data integrity.
    - [x] Sub-task: Test equipment slot management (equipping, unequipping, slot validation).
    - [x] Task: Conductor - User Manual Verification 'Core Item Data Structure & Equipment Slots' (Protocol in workflow.md)

## Phase 2: Affix System Implementation

- [ ] Task: Define `Affix` data structures.
    - [ ] Sub-task: Create `AffixComponent.hpp` to represent prefixes, suffixes, and their tiers/values.
    - [ ] Sub-task: Implement `Affix` data structure including `StatModifier` (from `Stats.hpp`) and tier information.
    - [ ] Sub-task: Design and implement data loading for affixes (e.g., from JSON files).
- [ ] Task: Implement random affix generation logic.
    - [ ] Sub-task: Develop a system to randomly select prefixes and suffixes based on item type and level.
    - [ ] Sub-task: Ensure correct tier generation for affixes.
- [ ] Task: Integrate affixes with item generation.
    - [ ] Sub-task: Modify `ItemFactory` to attach randomly generated affixes to new items.
    - [ ] Sub-task: Update `ItemComponent` to store affix data.
- [ ] Task: Implement `StatsSystem` integration for affix effects.
    - [ ] Sub-task: Modify `StatsSystem` to read and apply `StatModifier` from equipped item affixes to `CombatStats`.
- [ ] Task: Write Unit Tests for Affix System.
    - [ ] Sub-task: Test affix data loading and parsing.
    - [ ] Sub-task: Test random affix generation (distribution, tiering).
    - [ ] Sub-task: Test `StatsSystem` correctly applies affix modifiers.
- [ ] Task: Conductor - User Manual Verification 'Affix System Implementation' (Protocol in workflow.md)

## Phase 3: Item Modification Systems

- [ ] Task: Implement Refinement System.
    - [ ] Sub-task: Define refinement recipe data structure.
    - [ ] Sub-task: Implement UI interaction and logic for rerolling affixes (e.g., consuming currency).
- [ ] Task: Implement Rune System (Sockets).
    - [ ] Sub-task: Add socket component to items.
    - [ ] Sub-task: Implement logic for inserting/removing runes from sockets.
    - [ ] Sub-task: Define `Rune` data structure and its effects.
- [ ] Task: Implement Fusion System (Placeholder).
    - [ ] Sub-task: Create a placeholder structure or system for item fusion mechanics. (Detailed implementation deferred for future tracks).
- [ ] Task: Write Unit Tests for Item Modification Systems.
    - [ ] Sub-task: Test refinement logic and stat rerolling.
    - [ ] Sub-task: Test socketing and rune application/removal.
- [ ] Task: Conductor - User Manual Verification 'Item Modification Systems' (Protocol in workflow.md)

## Phase 4: Drop System Core

- [ ] Task: Design and implement loot table data structures.
    - [ ] Sub-task: Create `LootTable.hpp` to define possible drops, rarity, and weights.
    - [ ] Sub-task: Implement data loading for loot tables (e.g., from JSON files).
- [ ] Task: Integrate `XPAwardingSystem` with loot generation.
    - [ ] Sub-task: Modify `XPAwardingSystem` or create a new `LootGenerationSystem` to trigger loot drops upon enemy death.
- [ ] Task: Implement monster level and rarity influence on drops.
    - [ ] Sub-task: Adjust drop quality (affix tiers, item rarity) based on monster level.
    - [ ] Sub-task: Implement logic for rare monsters to drop from specific loot tables.
- [ ] Task: Implement Player Magic Find (MF) influence.
    - [ ] Sub-task: Add a `MagicFindComponent` to the player.
    - [ ] Sub-task: Modify drop logic to increase rarity chances based on player MF.
- [ ] Task: Implement Area Level influence.
    - [ ] Sub-task: Implement a mechanism to pass current area level to drop system.
    - [ ] Sub-task: Adjust drop quality based on area level.
- [ ] Task: Write Unit Tests for Drop System.
    - [ ] Sub-task: Test loot table parsing and item selection.
    - [ ] Sub-task: Test monster level, MF, and area level influence on drop quality and rarity distribution.
- [ ] Task: Conductor - User Manual Verification 'Drop System Core' (Protocol in workflow.md)

## Phase 5: Loot Filter

- [ ] Task: Design and implement loot filter data structure and configuration.
    - [ ] Sub-task: Define rules for filtering (e.g., by rarity, item type, specific affixes).
    - [ ] Sub-task: Implement persistence for player-defined loot filter rules.
- [ ] Task: Implement visual filtering and automatic ignoring/deconstruction.
    - [ ] Sub-task: Develop logic to hide or visually highlight items based on filter rules.
    - [ ] Sub-task: Implement automatic ignoring or deconstruction of filtered items.
- [ ] Task: Integrate Loot Filter with `RenderSystem` and `DropSystem`.
    - [ ] Sub-task: Modify `RenderSystem` to apply visual filter rules.
    - [ ] Sub-task: Modify `DropSystem` to apply automatic ignoring/deconstruction.
- [ ] Task: Write Unit Tests for Loot Filter.
    - [ ] Sub-task: Test loot filter rule parsing and application.
    - [ ] Sub-task: Test visual filtering and automatic item handling.
- [ ] Task: Conductor - User Manual Verification 'Loot Filter' (Protocol in workflow.md)
