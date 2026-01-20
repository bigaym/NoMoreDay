# Track Specification: String Dependency Elimination

## Goal
Eliminate runtime string dependencies and comparisons for core game systems (Biomes, Materials, Astrolabe Traits), replacing them with explicit enums and static map lookups for performance and safety.

## Key Changes
- **Enums**: Introduced `BiomeID`, `MaterialCategory`, and `TraitID`.
- **Logic**: Refactored `LevelManager`, `EnemySpawnSystem`, `RunewordSystem`, and `UIInventory` to use enums.
- **Data**: Updated JSON data files (`biomes.json`, `materials.json`, `astrolabe.json`) to include numeric IDs.
- **Optimization**: Replaced expensive `if-else` string chains with `static const` map lookups.
