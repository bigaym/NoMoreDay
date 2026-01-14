# Implementation Plan: Legendary Merging (传奇融合)

This plan outlines the implementation of the Legendary Merging system, allowing players to fuse Unique items with Exalted items using a Legendary Core.

## Phase 1: Foundation & Data Structures
Preparation of the necessary data types and constants.

- [ ] **Task 1.1: Update Rarity Enum**
  - Add `Ancient` (Red) to `enum class Rarity` in `src/game/components/ItemComponent.hpp`.
  - Update `to_json`/`from_json` for `Rarity` if necessary.
- [ ] **Task 1.2: Update Affix Structure**
  - Add `bool isLegendary = false;` to `struct Affix` in `src/game/components/ItemStats.hpp`.
  - Update `to_json`/`from_json` to handle the new field.
- [ ] **Task 1.3: Define Legendary Core Material**
  - Add "Legendary Core" (传奇核心) to the item database (logic or config) to allow identification as a catalyst.
- [ ] **Task: Conductor - User Manual Verification 'Phase 1: Foundation' (Protocol in workflow.md)**

## Phase 2: Core Logic Implementation
Development of the backend fusion logic in `CraftingSystem`.

- [ ] **Task 2.1: Implement Fusion Validation**
  - In `CraftingSystem.hpp/cpp`, create a `fuseLegendary` method.
  - Implement checks: LP > 0, same Slot, Fodder is Exalted (4 affixes), Catalyst exists.
- [ ] **Task 2.2: Implement Affix Inheritance Algorithm**
  - Logic to take the `selectedIdx` and pick $LP-1$ random additional affixes.
  - Copy affixes and set `isLegendary = true`.
- [ ] **Task 2.3: Implement Item Transformation**
  - Change rarity to `Ancient`.
  - Update item name (e.g., prefix with "Ancient" or "Mythic").
  - Handle entity destruction of fodder and core consumption.
- [ ] **Task 2.4: Unit Tests for Fusion Logic**
  - Create a test case in `tests/ItemEquipmentTests.hpp` to verify LP inheritance (1LP, 2LP, 4LP scenarios).
- [ ] **Task: Conductor - User Manual Verification 'Phase 2: Core Logic' (Protocol in workflow.md)**

## Phase 3: UI & Visual Feedback
Updating the user interface to support the new feature.

- [ ] **Task 3.1: Update Tooltip Rendering**
  - Modify `ItemTooltip` logic (or wherever items are described) to render affixes with `isLegendary == true` in **Red**.
- [ ] **Task 3.2: Create Legendary Merging UI Tab**
  - Add a new tab in the `CraftingSystem` UI.
  - Implement 3 slots: Base, Fodder, Catalyst.
- [ ] **Task 3.3: Implement Affix Selection Interface**
  - Allow the user to click one of the fodder item's affixes to mark it as the guaranteed choice.
- [ ] **Task 3.4: Add Visual Effects (VFX)**
  - Trigger a particle burst when fusion completes using `GPUParticleSystem`.
- [ ] **Task: Conductor - User Manual Verification 'Phase 3: UI & Visuals' (Protocol in workflow.md)**

## Phase 4: Integration & Balancing
Ensuring the system works within the main game loop.

- [ ] **Task 4.1: Integrate with Game UI**
  - Ensure the Crafting Menu correctly opens the fusion tab.
- [ ] **Task 4.2: Final Integration Test**
  - Perform a full loop: Loot Unique/Exalted -> Obtain Core -> Fuse -> Verify Stats.
- [ ] **Task: Conductor - User Manual Verification 'Phase 4: Integration' (Protocol in workflow.md)**
