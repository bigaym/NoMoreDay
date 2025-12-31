# Implementation Plan - Equipment Asset Automation and Random Icon Matching

This plan outlines the steps to automate equipment asset registration and implement random icon assignment for dropped items.

## Phase 1: Asset Registry Automation
- [x] Task: Create Python script `scripts/gen_equipment_registry.py` to scan `assets/textures/equipment`
- [x] Task: Implement C++ header generation for `src/core/EquipmentAssetRegistry.hpp` with `constexpr` mapping
- [x] Task: Integrate script into build process or document manual execution steps
- [x] Task: Conductor - User Manual Verification 'Asset Registry Automation' (Protocol in workflow.md)

## Phase 2: Data Model and Factory Integration
- [x] Task: Add `textureId` to `ItemComponent` and update JSON serialization
- [x] Task: Write unit tests in `tests/ItemSystemTest.hpp` to verify `textureId` persistence
- [x] Task: Implement `getRandomTextureForType(ItemType type, EquipmentSlot slot)` helper in `ItemFactory`
- [x] Task: Update `ItemFactory` methods (`createWeapon`, `createArmor`, etc.) to assign random `textureId`
- [x] Task: Verify random assignment logic via unit tests
- [x] Task: Conductor - User Manual Verification 'Data Model and Factory Integration' (Protocol in workflow.md)

## Phase 3: UI and Loading Integration
- [x] Task: Update `AssetLoadingSystem` to load all textures from `EquipmentAssetRegistry`
- [x] Task: Update `UIInventory` to use `ItemComponent::textureId` for rendering item icons
- [x] Task: Update `DropSystem` if necessary to ensure items spawned in-world show correct icons
- [x] Task: Run integration tests for Inventory UI rendering

## Phase 4: Final Verification and Cleanup
- [x] Task: Perform a full build and run all test suites
- [x] Task: Verify asset mapping efficiency and memory usage (basic check)
- [x] Task: Document the workflow for adding new equipment assets
- [x] Task: Conductor - User Manual Verification 'Final Verification and Cleanup' (Protocol in workflow.md)

## Documentation - Adding New Equipment Assets
1. Add new PNG files to `assets/textures/equipment/<category>/`.
2. Run `python scripts/gen_equipment_registry.py` to regenerate the C++ header.
3. Rebuild the project. The new assets will be automatically loaded and available for random assignment.

