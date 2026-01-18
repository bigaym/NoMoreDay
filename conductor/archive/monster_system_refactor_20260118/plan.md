# Monster System Refactor Plan

**Status:** Completed
**Completed:** 2026-01-18

## Phase 1: Data Definitions
- [x] **Refactor `EnemyRace` Enum**: Update `EnemyComponent.hpp` to align with the 9 new categories. Remove legacy/unused constants.
- [x] **Update `kRaceData`**: Ensure it has entries for all 9 races. Remove `texturePath` (as we will use dynamic registry lookups or formatted strings).

## Phase 2: System Logic
- [x] **Update `EnemySpawnSystem::initData`**:
    - Select 2 races based on Biome.
    - Store these active races.
- [x] **Update `EnemySpawnSystem::initTextures`**:
    - Load `_0` through `_4` textures for the active races.
    - Store them in a structure like `std::map<Race, std::array<Texture2D, 5>>`.
- [x] **Update `EnemySpawnSystem::initData` (Cluster Logic)**:
    - When generating spawn points, assign a random `VariantID` (0-4) based on weighted probabilities (providing a mix of archetypes).
- [x] **Update `EnemySpawnSystem::spawnEnemy`**:
    - Use `VariantID` to look up the correct texture.
    - Initialize `EnemyStateComponent` with the correct `EnemyArchetype` based on `VariantID`.
    - Apply archetype-specific stats (HP, Speed, Range) - *Refine existing logic*.

## Phase 3: Integration & Cleanup
- [x] **Verify `MonsterAssetRegistry.hpp` Usage**: Ensure we prefer the registry constants if possible, or consistent partial paths.
- [x] **Test**: Verify that a level spawns with ~10 distinct enemy visuals (2 races x 5 types).

## Phase 4: Biome Configuration
- [x] **Review `BiomeRegistry`**: Ensure all biomes have valid pools mapping to the new Enum names. (Town Safe Zone configured)
