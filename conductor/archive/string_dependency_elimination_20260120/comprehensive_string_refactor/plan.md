# Plan: Comprehensive String Dependency Refactor

**Track ID**: `comprehensive_string_refactor`
**Objective**: Eliminate runtime string usage in LootFilter, FragmentDrop, Runeword, Minimap, and EnemySpawn systems.

## Phase 1: Item & Loot Logic
- [x] **Step 1.1**: Refactor `RunewordSystem.cpp`.
- [x] **Step 1.2**: Refactor `FragmentDropSystem.cpp`.
- [x] **Step 1.3**: Refactor `LootFilter.cpp`.

## Phase 2: World & Enemy Logic
- [x] **Step 2.1**: Refactor `EnemySpawnSystem.cpp`.
- [x] **Step 2.2**: Refactor `UIMinimap.cpp`.

## Phase 3: Registry Cleanups
- [x] **Step 3.1**: Check `MaterialRegistry.cpp` logic.

## Phase 4: Integration Verification
- [x] **Step 4.1**: Compile and Fix errors.
- [x] **Step 4.2**: Verify `RunewordSystem` loads attributes correctly (log check).
- [x] **Step 4.3**: Verify `EnemySpawnSystem` spawns correct races (visual check).

**Completed**: 2026-01-20