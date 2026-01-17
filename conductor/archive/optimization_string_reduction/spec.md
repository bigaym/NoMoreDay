# String Usage Optimization & Resistance Logic Fix

## Core Concept
Optimize the codebase by removing heap-allocated strings from hot paths (specifically `StatsSystem`) and high-frequency components (`ItemComponent`, `Affix`). Replace string-based logic with Data-Oriented Design (DOD) principles using static lookup tables and `Tag` bitmasks.

## Motivation
1.  **Performance**: `StatsSystem::Recalculate` is called frequently. The current implementation allocates `std::vector<std::string>` for `EnemyRace` on every call, causing massive memory churn.
2.  **Correctness**: The string-based resistance logic in `EnemyRace` is currently ignored by `StatsSystem`, meaning enemies have no functional resistances.
3.  **Memory Footprint**: `Affix` structs contain `std::string` for names. With 10,000+ items/entities, this adds significant memory overhead.

## Data Structure Changes

### 1. Enemy Race Data (Static Lookup)
Replace the dynamic `EnemyRace` class with a static POD struct and a global `constexpr` lookup table.

```cpp
// In EnemyComponent.hpp or new EnemyData.hpp
struct EnemyRaceData {
    floatbaseHP;
    float baseDamage;
    float baseSpeed;
    float baseXP;
    float baseArmor;
    Tag resistances; // Bitmask instead of vector<string>
    std::string_view texturePath; // Shared view instead of string copy
};

static constexpr std::array<EnemyRaceData, 12> kRaceData = {{
    // UNDEAD
    {30.0f, 15.0f, 40.0f, 10.0f, 100.0f, Tag::Bleeding | Tag::Poison, "assets/textures/monster/skeleton_0.png"},
    // ... mapped to EnemyRace::Type index
}};
```

### 2. Affix Optimization
Remove the cached `name` string from the `Affix` instance.

```cpp
// In ItemStats.hpp
struct Affix {
    AffixType type = AffixType::Count;
    float value = 0.0f;
    int tier = 0;
    bool isPrefix = true;
    // std::string name;  <-- REMOVE THIS
    Tag required_tags = Tag::None;
    bool isLegendary = false;
};
```

## Logic Updates

### 1. StatsSystem
*   **Recalculate**: Remove `EnemyRace` instantiation. Use `enemy->raceType` to index `kRaceData`.
*   **Resistance Application**:
    ```cpp
    const auto& raceData = kRaceData[static_cast<size_t>(enemy->raceType)];
    // Apply base stats
    calcs[...].base = raceData.baseHP;
    
    // Apply resistances via Tag mask (requires combat.resistances support or loop)
    // Since StatsSystem uses array<float> for resistances, we map Tag bits to indices.
    // Optimization: Pre-calculate race resistance array values if possible, 
    // or efficiently loop through set bits in Tag.
    ```

### 2. UI / Tooltip Generation
*   Create a helper function `GetAffixName(const Affix&)` that generates the name on-demand logic (already partially exists as `GetAffixDescription`).
*   Update `ItemTooltip` and `Inventory` to use this helper instead of accessing `.name`.

## JSON Contract Changes
*   `AffixDefinition`: `requiredSkillTags` is already `std::vector<std::string>`, which is fine for *definition* loading, as long as it converts to `Tag` at runtime (which it does).
*   **No breaking changes** to save data format expected, as `Affix` name was likely transient or computable. *Check serialization: `to_json` for `Affix` currently writes `name`. We should remove it or write a computed values.*

## Impact Analysis
*   **Memory**: Significant reduction in heap allocations per frame and per item.
*   **CPU**: Reduced `malloc`/`free` overhead in combat loop.
*   **Risk**: Low. Primary risk is UI text displaying incorrect names if dynamic generation logic has gaps.

