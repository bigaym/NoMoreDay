# Specification: Legendary Merging (传奇融合)

## 1. Overview
The "Legendary Merging" system is a pinnacle endgame crafting mechanic where players fuse a **Unique** (Mythic) item and an **Exalted** (Uncommon/Purple) item. The goal is to produce an **Ancient** (Red) item that retains the Unique's identity while inheriting the Exalted item's powerful random affixes.

## 2. Technical Context & Constraints
*   **Affix Structure**: Each item can have up to 4 affixes (2 Prefixes + 2 Suffixes).
*   **Legendary Potential (LP)**: A variable (0-4) on Unique items determining how many affixes can be inherited.
*   **Data Integrity**: Merging must result in a consistent `ItemComponent` state, preserving original implicit and special effects.

## 3. Functional Requirements

### 3.1 Eligibility & Input
*   **Slot 1 (Base)**: Must be `Rarity::Mythic` with `legendaryPotential > 0`.
*   **Slot 2 (Fodder)**: Must be `Rarity::Uncommon` (Exalted) with exactly 4 affixes.
*   **Slot 3 (Catalyst)**: Must be a "Legendary Core" (Material item).
*   **Validation**: 
    - Base and Fodder must have the same `EquipmentSlot`.
    - Merging is blocked if any slot is empty or requirements aren't met.

### 3.2 The Merging Process (Algorithm)
1.  **Selection**: 
    - The player selects **one** affix from the Fodder item to be guaranteed.
    - If $LP > 1$, the system randomly selects $LP - 1$ additional affixes from the remaining 3.
2.  **Transformation**:
    - The Base item's `rarity` is updated to `Rarity::Ancient`.
    - The selected affixes are copied from the Fodder item to the Base item's `affixes` list.
    - **Tagging**: Inherited affixes should be internally distinguishable using the `isLegendary` flag in the `Affix` struct.
3.  **Consumption**:
    - The Fodder item entity is destroyed.
    - 1x Legendary Core is consumed from the player's inventory.

### 3.3 UI / UX Requirements
*   **Fusion Interface**: A dedicated tab in the Crafting Menu.
*   **Affix Selection UI**: When an Exalted item is placed, its 4 affixes become clickable for the "Guaranteed" selection.
*   **Preview**: Show the resulting item's tooltip, highlighting inherited affixes in **Red**.
*   **Animation**: A visual "fusion" effect (e.g., particle burst using `GPUParticleSystem`).

## 4. Implementation Details

### 4.1 Data Changes
*   **Rarity Enum**: Add `Ancient` to `NoMoreDay::Rarity`.
*   **Affix Tagging**: Add `bool isLegendary = false` to `Affix` struct in `ItemStats.hpp`.

### 4.2 Logic Flow (CraftingSystem)
*   Implement `CraftingSystem::fuseLegendary` to handle the logic.

##  acceptance Criteria
*   [ ] Items of different slots cannot be fused.
*   [ ] Fusing a 1LP Unique results in an Ancient item with exactly 1 extra (the selected) affix.
*   [ ] Fusing a 4LP Unique results in an Ancient item inheriting all 4 affixes.
*   [ ] UI correctly displays inherited affixes in Red.
*   [ ] Legendary Core is correctly deducted from the stack.
