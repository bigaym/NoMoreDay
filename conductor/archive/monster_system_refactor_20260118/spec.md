# Monster System Refactor Spec

## Goal
Refactor the monster spawn system to utilize the rich variety of generated assets (5 variants per race) and align the monster classification with the new artistic direction.

## Core Changes

### 1. Enemy Taxonomy (Enumeration)
Redefine `EnemyRace::Type` to strictly match the 9 available asset categories:
1.  **Undead** (Skeleton)
2.  **Demon** (Demon)
3.  **Corrupted** (Warcraft/Abomination)
4.  **Cultist** (Cultist)
5.  **Elves** (Fallen Elf)
6.  **Beast** (Beastman)
7.  **Goblin** (Goblin/Yecha)
8.  **Machine** (Mechanism)
9.  **Elemental** (Elemental)

*Remove legacy types: Dragonkin, Slime, Animal unless we have assets for them.*

### 2. Archetype Mapping
Map the 5 asset variants (`_0` to `_4`) to specific `EnemyArchetype` roles:
- **Variant 0**: `WARRIOR` (Balanced Melee)
- **Variant 1**: `RANGER` (Physical Ranged)
- **Variant 2**: `TANK` (High HP/Defense)
- **Variant 3**: `ASSASSIN` (High Speed/Burst)
- **Variant 4**: `MAGE` (Magic Ranged/Support)

### 3. Map Monster Pool
- **Constraint**: Each map generation selects **2 Races** (e.g., Undead + Cultist).
- **Expansion**: Instead of 2 types of enemies, the map will feature **10 distinct enemy types** (2 Races × 5 Archetypes).
- **Spawn Logic**:
    - During cluster generation, decide the Race (one of the 2 selected).
    - Inside the cluster, spawn a mix of Archetypes (Variants 0-4).
    - Typical composition: Many Warriors/Archers, fewer Tanks/Assassins, rare Mages.

### 4. Technical Implementation
- **`EnemySpawnSystem::initTextures`**: Must load all 5 textures for the 2 active races in the current biome.
- **`EnemySpawnSystem::spawnEnemy`**: 
    - Determine `VariantID` (0-4) based on `EnemyArchetype`.
    - Assign the correct texture from the loaded pool.
    - Set stats based on Archetype (e.g., Tank has more HP, Assassin moves faster).

## Data Structures

### Updated `EnemyRace::Type`
```cpp
enum Type : uint8_t {
    UNDEAD, DEMON, CORRUPTED, CULTIST, ELVES, 
    BEAST, GOBLIN, MACHINE, ELEMENTAL,
    COUNT
};
```

### Updated `EnemySpawnData`
Needs to carry `VariantID` or `Archetype` info so the spawner knows what to create.
```cpp
struct EnemySpawnData {
    // ... existing ...
    int enemyRace;      // e.g., UNDEAD
    int enemyVariant;   // 0-4 (Archetype)
};
```
