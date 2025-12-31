# Implementation Plan - Equipment Visual Enhancement and Randomization

This plan outlines the steps to integrate the equipment asset registry into the factory and enhance rarity-based visuals in the UI.

## Phase 1: Data Structure and Factory Logic
- [ ] Task: Update `BaseItemDef` struct in `ItemFactory.cpp` to include `std::string assetCategory`.
- [ ] Task: Update `WEAPON_BASES`, `ARMOR_BASES`, and other base item collections with appropriate categories.
- [ ] Task: Refactor `getRandomTextureForType` in `ItemFactory.cpp` to use the `assetCategory` from the selected `BaseItemDef`.
- [ ] Task: Ensure `ItemFactory::createWeapon` and `ItemFactory::createArmor` retrieve the category from the base definition.
- [ ] Task: Verify category-to-asset mapping via unit tests in `tests/ItemSystemTest.hpp`.
- [ ] Task: Conductor - User Manual Verification 'Data Structure and Factory Logic' (Protocol in workflow.md)

## Phase 2: UI Visual Enhancements
- [ ] Task: Modify `UIRenderer::DrawSlot` to accept an optional tint color (derived from rarity).
- [ ] Task: Implement dynamic pulse/glow animation logic in `UIRenderer` using `GetTime()`.
- [ ] Task: Update `UIRenderer::DrawSlot` to render an animated border for `Rarity::Legendary` and `Rarity::Mythic`.
- [ ] Task: Integrate subtle icon tinting for higher rarities to distinguish variants.
- [ ] Task: Verify UI animations and tinting in the inventory screen.
- [ ] Task: Conductor - User Manual Verification 'UI Visual Enhancements' (Protocol in workflow.md)

## Phase 3: Final Integration and Verification
- [ ] Task: Perform a full build and run the `tests_runner` suite.
- [ ] Task: Manually verify that identical items (e.g., two "Iron Longswords") can have different icons.
- [ ] Task: Document the mapping between `BaseItemDef` categories and the `EquipmentAssetRegistry`.
- [ ] Task: Conductor - User Manual Verification 'Final Integration and Verification' (Protocol in workflow.md)
