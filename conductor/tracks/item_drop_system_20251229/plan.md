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

- [x] Task: Define `Affix` data structures.
  - [x] Sub-task: Create `AffixComponent.hpp` to represent prefixes, suffixes, and their tiers/values.
  - [x] Sub-task: Implement `Affix` data structure including tier information.
- [x] Task: Implement Affix Data Loading.
  - [x] Sub-task: Add `loadAffixDefinitions` to `ItemFactory` to load from `affixes.json`.
  - [x] Sub-task: Implement JSON parsing for `AffixDefinition` and `AffixTier`.
- [x] Task: Implement random affix generation logic using loaded data.
  - [x] Sub-task: Update `generateRandomAffix` to select affixes based on `allowedTags` and item level from loaded definitions.
  - [x] Sub-task: Ensure correct tier selection and value rolling from `AffixTier` data.
- [x] Task: Integrate affixes with item generation.
  - [x] Sub-task: Modify `ItemFactory` to attach randomly generated affixes to new items.
  - [x] Sub-task: Update `ItemComponent` to store affix data.
- [x] Task: Implement `StatsSystem` integration for affix effects.
  - [x] Sub-task: Modify `StatsSystem` to read and apply modifiers from equipped item affixes to `CombatStats`.
- [x] Task: Write and Fix Unit Tests for Affix System.
  - [x] Sub-task: Fix `AffixSystemTest.cpp` to work with the new `ItemFactory` methods.
  - [x] Sub-task: Test affix data loading and parsing.
  - [x] Sub-task: Test random affix generation (distribution, tiering).

- [x] Task: Conductor - User Manual Verification 'Affix System Implementation' (Protocol in workflow.md)

## Phase 3: Item Modification Systems

- [x] Task: Implement Refinement System.
  - [x] Sub-task: Define refinement recipe data structure.
  - [x] Sub-task: Implement UI interaction and logic for rerolling affixes (e.g., consuming currency).
- [x] Task: Implement Rune System (Sockets).
  - [x] Sub-task: Add socket component to items.
  - [x] Sub-task: Implement logic for inserting/removing runes from sockets.
  - [x] Sub-task: Define `Rune` data structure and its effects.
- [x] Task: Implement Fusion System (Placeholder).
  - [x] Sub-task: Create a placeholder structure or system for item fusion mechanics. (Detailed implementation deferred for future tracks).
- [x] Task: Write Unit Tests for Item Modification Systems.
  - [x] Sub-task: Test refinement logic and stat rerolling.
  - [x] Sub-task: Test socketing and rune application/removal.
- [x] Task: Conductor - User Manual Verification 'Item Modification Systems' (Protocol in workflow.md) [checkpoint: 6343cb4]

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
