# Spec: Comprehensive String Dependency Refactor

## 1. Introduction
This specification outlines the comprehensive removal of runtime string dependencies across multiple game systems (`LootFilter`, `FragmentDropSystem`, `MaterialRegistry`, `RunewordSystem`, `UIMinimap`, `EnemySpawnSystem`). This is a follow-up to the initial String Dependency Elimination track (which focused on Biome and Category Enums) to cover remaining structural leaks.

## 2. Core Definitions

### 2.1. LootFilter Optimization
**Current State**: `LootFilter::evaluate` iterates rules and performs string comparisons for Rarity (`"Legendary"`) and ItemType (`"Weapon"`).
**New State**:
*   JSON parsing converts these to `Rarity` and `ItemType` Enums at load time (already partially done, but needs strict enforcement).
*   **Affix Mathing**: Currently does `item.name.find(condition.baseName)`.
    *   **Optimization**: If `baseName` is a known static ID (e.g. "Iron Sword"), pre-hash or map it to `ItemID`.
    *   **Fallback**: Keep string compare for loose matching but optimize with `std::string_view` or `KMP` if performance critical (low priority for UI logic, but `LootFilter` runs on Drop generation).

### 2.2. FragmentDropSystem & Logic
**Current State**: `RollFragmentElement` checks `areaElement == "fire"`.
**New State**: `FragmentElement` Enum.
```cpp
enum class FragmentElement : uint8_t { None, Fire, Cold, Lightning, Shadow, Chaos, Count };
```
*   `LevelManager` or `ZoneData` should store `FragmentElement dominantElement` instead of string.

### 2.3. Runeword System
**Current State**: `parseItemType("Sword")` checks strings. `stringToAffixType("strength")` is a massive if-else chain.
**New State**:
*   **Static Map**: Replace `stringToAffixType` if-else chain with `static const std::unordered_map<std::string, AffixType>`.
*   **Load-Time Resolution**: Do not store check strings like `"Sword"` in `RunewordDefinition` at runtime if possible; map them to `ItemSubType` enum or `ItemType` flags.

### 2.4. UIMinimap
**Current State**: `levelManager.getCurrentBiome() == "town"` in Draw loop.
**New State**: Use `BiomeID` (from Track 1) here as well. `levelManager.getCurrentBiomeID()`.

### 2.5. EnemySpawnSystem (Complete Refactor)
**Current State**: `raceName == "undead"` checks in `initData`.
**New State**:
*   **Race Registry**: `static const std::unordered_map<std::string, EnemyRace>` for parsing JSON/Config.
*   **Runtime**: Store `EnemyRace` enum in `SpawnData`.

## 3. Implementation Plan

### Phase 1: Item & Loot Systems
*   **LootFilter**: Ensure all condition checks (Rarity, Type) use Enums. Optimize `baseName` check if possible.
*   **FragmentDropSystem**: Switch `areaElement` string to `FragmentElement` Enum.
*   **RunewordSystem**: Replace `stringToAffixType` huge if-else with `static map` lookup.

### Phase 2: World & Enemy Systems
*   **EnemySpawnSystem**: Implement `kRaceStringMap` to replace multiple if-else blocks.
*   **UIMinimap**: Replace `getCurrentBiome() == "town"` with `getCurrentBiomeID() == BiomeID::Town`.

### Phase 3: JSON Integrity
*   Ensure `materials.json` and other data files support these Enum mappings (implicitly or explicitly).

## 4. Verification
*   **Drop Test**: Kill enemies, verify Loot Filter and Fragment Drops still work.
*   **Runeword Test**: Verify Runewords still activate (Stats parsing).
*   **Minimap**: Verify Zone Name and markers display correctly in Town vs Dungeon.
