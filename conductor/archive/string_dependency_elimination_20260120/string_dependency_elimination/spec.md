# Spec: String Dependency Elimination

## 1. Introduction
This specification entails the removal of runtime string comparisons and allocations in critical game loops ("hot paths") and core logic systems. It also includes updating JSON data sources to explicitly provide integer IDs for properties previously identified only by strings, ensuring robust data-driven logic.

## 2. Core Definitions

### 2.1. Biome Identification
**Current State**: Biomes are identified by `std::string id` (e.g., "town").
**New State**: `BiomeID` Enum + JSON Update.

```cpp
enum class BiomeID : uint8_t {
    None = 0, // Fallback
    Town = 1,
    Cave = 2, 
    // ... maps to numeric_id in JSON
};
```

**JSON Schema Change (`biomes.json`)**:
Add `numeric_id` field.
```json
{
    "id": "town",
    "numeric_id": 1, 
    "name": "平安镇 (Town)",
    ...
}
```

**Runtime**:
*   `BiomeRegistry` loads `numeric_id` -> `BiomeID` mapping.
*   `PortalComponent` stores `BiomeID targetBiome`.
*   Systems use `if (currentBiome == BiomeID::Town)`.

### 2.2. Material Categorization
**Current State**: `category` string (e.g., "Mineral", "Rune").
**New State**: `MaterialCategory` Enum + JSON Update.

```cpp
enum class MaterialCategory : uint8_t {
    Misc = 0,
    Mineral = 1,
    Fragment = 2,
    Rune = 3,
    AffixShard = 4,
    // ...
    Count
};
```

**JSON Schema Change (`materials.json`)**:
Add `category_id` field.
```json
{
    "id": 1001,
    "category": "Mineral",
    "category_id": 1,
    ...
}
```

**Runtime**:
*   `MaterialDefinition` stores `MaterialCategory categoryEnum`.
*   `UIInventory` uses `MaterialCategory` for filtering.

### 2.3. Astrolabe Effects (Pure Logic)
**Current State**: Effect Value is String.
**New State**: `TraitID` Enum.

```cpp
enum class TraitID : uint16_t {
    None = 0,
    SwordHeart = 100,
    SwordIntentUnlock = 101,
    // ...
};
```

**JSON Schema Change (`astrolabe.json` - if applicable)**:
*   Add `trait_id` to effect objects if possible.
*   *Alternatively*, if many effects are unique/script-like, maintain a load-time Registry that strictly maps Strings to IDs, but adding explicit IDs is preferred if editing the JSON is feasible.
*   **Decision**: For Astrolabe, we will stick to **Runtime Parsing w/ Registry** for now unless the User explicitly demands JSON edits there too, as Astrolabe effects are often "Params" rather than just IDs. But for known Traits, we can define `trait_id`.

## 3. Implementation Steps
1.  **JSON Updates**: Scripts/Manual edits to add `numeric_id` / `category_id`.
2.  **Enum Definitions**: Define Enums in C++.
3.  **Loading Logic**: Update `BiomeRegistry::Load` and `MaterialRegistry::Load` to parse the new integer fields.
4.  **Runtime Refactor**: Update `PortalSystem`, `UIInventory` to use Enums.

## 4. Verification
*   **Data Integrity**: Ensure every JSON entry has a valid, unique ID where required.
*   **Logic Check**: Verify Game Logic (Town Portal, Inventory Filter) works identical to before.
