# Plan: String Dependency Elimination

**Track ID**: `string_dependency_elimination`
**Objective**: Eliminate runtime string comparisons using explicit integer IDs in JSON configurations.

## Phase 1: World Module (Critical Hot Path)
**Goal**: Use explicit `numeric_id` in `biomes.json` to drive `PortalSystem` logic.

- [x] **Step 1.1**: Update `assets/data/biomes.json`.
- [x] **Step 1.2**: Define `BiomeID` Enum in `components/MapComponent.hpp`.
- [x] **Step 1.3**: Update `BiomeRegistry.hpp/.cpp`.
- [x] **Step 1.4**: Update `PortalComponent` and `PortalSystem.cpp`.

## Phase 2: UI Module (Allocation Hot Path)
**Goal**: Use explicit `category_id` in `materials.json` to filter Inventory.

- [x] **Step 2.1**: Define `MaterialCategory` Enum in `systems/item/MaterialRegistry.hpp`.
- [x] **Step 2.2**: Update `assets/data/materials.json`.
- [x] **Step 2.3**: Update `MaterialRegistry` Loader.
- [x] **Step 2.4**: Refactor `UIInventory`.

## Phase 3: Verification
- [x] **Step 3.1**: Run Game.
- [x] **Step 3.2**: Check `PortalSystem` transitions (Town <-> Dungeon).
- [x] **Step 3.3**: Check Inventory Filters.
- [x] **Step 3.4**: Ensure no regressions in JSON loading (no crashes).

**Completed**: 2026-01-20