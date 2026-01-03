# Track: Crafting System UI Implementation

## Goal
Implement the user interface for the Crafting System, allowing players to upgrade, add, chaos, and refine affixes on items using the existing backend logic.

## Context
- **Backend**: `CraftingSystem.cpp` implements logic for `upgradeAffix`, `addAffix`, `chaosAffix`, `refineAffixValues`.
- **UI System**: `UIRenderer` and `UISystem` handle rendering. We need a new `UICrafting` module or integration into `UIInventory`.
- **Goal**: A dedicated crafting panel or a context-menu-driven approach (e.g., right-click item -> "Craft"). Most ARPGs use a dedicated panel where you place an item.

## Plan

### Phase 1: Infrastructure & Serialization Fixes
- [x] Task: Add `ActiveSkillsComponent` and `ActiveEffectsComponent` to `SerializationSystem` to ensure game state is fully saveable before adding more state.
- [x] Task: Create `UICrafting.hpp` and `UICrafting.cpp` structure.
- [x] Task: Register `UICrafting` in `UISystem`.

### Phase 2: UI Layout
- [x] Task: Implement `UICrafting::Draw` with a slot for the target item.
- [x] Task: Display item details (affixes, tiers, ranges) in the crafting panel.
- [x] Task: Display "Forging Potential" prominently.

### Phase 3: Crafting Interactions
- [x] Task: Implement buttons for each affix to "Upgrade" (if potential allows).
- [x] Task: Implement "Add Affix" functionality (Prefix/Suffix slot selection).
- [x] Task: Implement "Chaos" (Reroll) and "Refine" (Values) actions.
- [x] Task: Connect UI actions to `CraftingSystem` logic.

### Phase 4: Feedback & Polish
- [x] Task: Add sound effects or visual feedback on crafting success/failure.
- [x] Task: Verify persistence of crafted items.

## Knowledge
- **Forging Potential**: Items have `forgingPotential`. Crafting costs potential.
- **Affix Tiers**: Max tier is 5.
- **Slots**: Items have up to 2 Prefixes and 2 Suffixes.
