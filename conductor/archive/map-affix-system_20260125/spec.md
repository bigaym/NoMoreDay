# Map Affix System & Dimensional Persistence

## 1. Overview
The Map Affix System governs the environmental modifiers, difficulty adjustments, and reward multipliers active in a Dimensional Rift (Mosaic Map). Currently, these modifiers are calculated transiently during map generation and lost upon scene transition.

This specification defines a robust **Map Affix System** and a **Dimensional Persistence Layer** to ensure that when a player opens a Rift, enters it, leaves (e.g., to Town), and returns, the Rift's properties (Affixes, Modifiers, Seed) remain identical.

## 2. Core Concepts

### 2.1 Map Affix (Definition)
A **Map Affix** is a discrete modifier applied to a game level. It can affect:
*   **Stats**: Enemy Density, Drop Rate, XP Gain.
*   **Gameplay**: Monster Damage, Player Resistances, Cooldown Recovery.
*   **Environment**: Periodic damage, weather effects, visual filters.

### 2.2 Active Dimensional State (Persistence)
To solve the "amnesia" bug where return portals lead to generic maps, we introduce a Global Singleton Component: `ActiveDimensionalState`. This component acts as the "Session State" for the current active Rift.

## 3. Data Structures

### 3.1 Map Affix Definition
```cpp
// src/game/data/MapAffix.hpp

enum class MapAffixCategory : uint8_t {
    Buff,       // Positive (Reward) - e.g., Drop Rate
    Debuff,     // Negative (Challenge) - e.g., Monster Strength, Player Weakness
    Environment // Environmental - Reserved for future use (not in regular pool)
};

enum class MapAffixType : uint8_t {
    // --- BUFFS (Rewards) ---
    DropRarity,          // +% Magic Find (The "Gilded" modifier)
    DropQuantity,        // +% Item Quantity (The "Opulent" modifier)

    // --- DEBUFFS (Challenges) ---
    // 1. Structural (Crowd)
    MonsterDensity,      // More monsters (Technically a challenge, though players love it)
    MonsterLevel,        // Higher Level scaling

    // 2. Enemy Defense (Tankiness)
    Enemy_ExtraHealth,   // +% HP Multiplier
    Enemy_Armor,         // + Flat Armor (10 - 3000)
    Enemy_Dodge,         // + Flat Dodge Rating (10 - 2500)
    Enemy_ResistAll,     // +% All Resistances (1-20%)
    Enemy_ResistPhys,    // +% Physical Resist (1-40%)
    Enemy_ResistFire,    // +% Fire Resist
    Enemy_ResistCold,    // +% Cold Resist
    Enemy_ResistLight,   // +% Lightning Resist
    Enemy_ResistPois,    // +% Poison Resist
    Enemy_ResistVoid,    // +% Void/Shadow Resist
    Enemy_CritResist,    // Reduced Bonus Damage from Crits

    // 3. Enemy Offense (Deadliness)
    Enemy_ExtraDamage,   // +% Global Damage
    Enemy_Fast,          // +% Attack/Cast/Move Speed
    Enemy_CritChance,    // +% Critical Strike Chance
    Enemy_ExtraBarrier,  // Gain Barrier/Shield (based on % HP)
    Enemy_BarrierRegen,  // Regenerate Barrier over time
    Enemy_ArmorShred,    // Enemies shred player armor on hit

    // 4. Player Penalties (Weakness)
    Player_ResistRedAll,      // -% Player All Resistances (1-20%)
    Player_ResistRedSpecific, // -% Specific Resistance (Fire/Cold/etc) (1-40%)
    Player_RedRecovery,       // -% Health/Mana Regen & Leech
    Player_Fragile,           // +% Damage Taken (Vulnerability)
    Player_DodgePenalty,      // -% Player Dodge Rating

    // --- ENVIRONMENT (Reserved) ---
    // Not in standard generation pool yet
    Env_Firestorm,       
    Env_Darkness,        
    Env_GroundIce,       
    Env_LightningStorm   
};

struct MapAffix {
    MapAffixType type;
    MapAffixCategory category; // New category tag
    float value;               // Magnitude
    std::string source;        // Description source
    
    // Persistence Logic
    int remainingLayers = -1; 
};

// Examples:
// - "Volatile" prefix: remainingLayers = 1 (High power, short lived)
// - "Stable" prefix: remainingLayers = 3 (Standard)
// - "Eternal" prefix: remainingLayers = -1 (Lasts entire dungeon)
```

### 3.2 Dimensional State Component
This component will be attached to a global "World Entity" or the Registry Context.

```cpp
// src/game/components/WorldState.hpp

struct ActiveDimensionalState {
    // Identity
    bool isActive = false;
    uint32_t seed = 0;         // Map Generation Seed (Crucial for consistent terrain)
    
    // Configuration
    NoMoreDay::BiomeID biome;
    int depthLevel;
    
    // The Calculated Affixes
    NoMoreDay::ResonanceResult resonance; // Base Multipliers (Permanent)
    std::vector<MapAffix> explicitAffixes; // Specific Gameplay Modifiers (Decaying)
    
    // Dungeon Structure
    int maxDepth = 3;          // Standard Mosaic = 3 Levels
    int currentDepth = 1;

    // Source Data (for UI viewing)
    NoMoreDay::MosaicGrid sourceGrid;
    
    // State Tracking (Basic Persistence)
    bool isBossKilled = false;
    bool isCompleted = false;
};
```

## 4. System Flow

### 4.1 Activation (Mosaic Editor -> Gameplay)
1.  **Fragment Selection**: Player places Fragments into the Mosaic Grid.
    *   *Constraint Change*: Fragments now **ONLY** appear with Challenge Affixes (Debuffs, Monster Buffs, Density).
    *   **Reward Preview**: The UI displays a real-time "Affix Summary Panel" showing the calculated Difficulty Score and resulting Item Quantity/Rarity.
2.  `MapAffixCalculator` Processing:
    *   Scans all fragments for Challenge Affixes.
    *   Sums the **Difficulty Score (DS)**.
    *   **Calculates Rewards**: Generates system-level Buffs (Quantity/Rarity) based on the DS formulas.
3.  System populates `ActiveDimensionalState` in the Registry.
    *   Sets `isActive = true`.
    *   Generates a unique generation key.
    *   Save `ResonanceResult` and `MosaicGrid`.
    *   Stores the list of Challenge Affixes + Derived Reward Stats.
4.  `SceneManager` triggers transition.

### 4.2 Map Generation (Loading)
1.  `MapSystem` and `EnemySpawnSystem` read from `ActiveDimensionalState` (if active).
2.  **Terrain**: `MapSystem` uses `ActiveDimensionalState.seed` to generate the exact same cave layout.
3.  **Spawns**: `EnemySpawnSystem` applies `resonance` and `explicitAffixes`.

### 4.3 Leaving (Rift -> Town)
1.  Player uses Town Portal.
2.  `ActiveDimensionalState` **REMAINS UNTOUCHED** in the registry. It represents the "open" rift.
3.  Scene loads Town.

### 4.4 Returning (Town -> Rift)
1.  Player interacts with Return Portal.
2.  `PortalSystem` checks if `ActiveDimensionalState.isActive` is true.
3.  `SceneManager` transitions back to the Rift biome.
4.  **Crucial Step**: Instead of generating a new random seed/layout, `LevelManager` detects `ActiveDimensionalState` matches the target biome.
    *   Reuses `ActiveDimensionalState.seed` -> Same Terrain (for current depth).
    *   Reapplies `ActiveDimensionalState.affixes` -> Same Modifiers.

### 4.5 Descents (Level 1 -> Level 2)
1.  Player finds "Next Level" portal (not Return portal).
2.  `System` triggers transition to `currentDepth + 1`.
3.  **Affix Decay**:
    *   Iterate `explicitAffixes`.
    *   Decrement `remainingLayers`.
    *   Remove if `remainingLayers == 0`.
    *   Notify Player: "The influence of [Source] has faded."
4.  Update `ActiveDimensionalState.seed` (New seed for new level).
5.  Generate new map with *remaining* affixes.

## 5. Risk-Reward Mathematical Model

To ensure a fair correlation between **Difficulty** (Challenge) and **Loot** (Reward), we implement a **Difficulty Score (DS)** system. This system dynamically calculates the Drop Rarity and Quantity bonuses based on the active debuffs.

### 5.1 Difficulty Weights ($W_{type}$)
All affixes are weighted by their impact on survival and clear speed.

| Category | Affix Examples | Weight ($W$) | Rationale |
| :--- | :--- | :--- | :--- |
| **Statistical** | `MonsterDensity`, `MonsterLevel` | **0.5** | Increases chaos but is generally desirable for farming. |
| **Defense** | `HP`, `Barrier`, `Resist`, `Armor`, `Dodge` | **1.0** | Increases "Time to Kill" (TTK). Pure efficiency penalty. |
| **Offense** | `Damage`, `Speed`, `Crit`, `Shred` | **2.0** | Increases risk of sudden death (One-shot potential). |
| **Player Debuff** | `ResistRed`, `Fragile`, `DodgePenalty`, `Recovery` | **3.0** | Highest risk. fundamentally alters character survivability. |

### 5.2 Core Formulas

1.  **Individual Affix Score ($S_i$)**:
    The score is normalized based on the Affix Tier (T1 to T10, roughly corresponding to 10% steps).
    $$ S_i = \text{TierValue} \times W_{type} $$
    *(e.g., T1 = 10 points, T2 = 20 points)*

2.  **Total Difficulty Score ($DS$)**:
    $$ DS = \sum S_i $$

3.  **Reward Scaling (Calculated)**:
    Rewards are no longer random rolls; they are a deterministic result of the chosen difficulty.

    *   **Bonus Item Rarity (Magic Find)**:
        *   **Definition**: Multiplies the weight/probability of rolling higher rarity Tiers (Magic -> Rare -> Legendary) AND high-end sub-stats (LP, Sockets).
        *   **Linear Scale**: $$ \Delta MF = DS \times 1.5\% $$
    
    *   **Bonus Item Quantity (The Juice)**:
        *   **Definition**: Multiplies the number of drop rolls per kill.
        *   **Logarithmic Growth**: $$ \Delta Quant = 50\% \times \log_2(1 + \frac{DS}{40}) $$
        
        *Table Examples:*
        *   DS = 40  -> **+50%** Quantity
        *   DS = 120 -> **+100%** Quantity (2x Items)
        *   DS = 600 -> **+200%** Quantity (3x Items)

### 5.3 Calculation Example (The "Mosaic Panel" View)

**Active De-Buffs (From Fragments):**

| Affix Name | Tier | Category | Weight | Score ($S_i$) |
| :--- | :--- | :--- | :--- | :--- |
| **Swarming** (+40% Density) | T4 (40) | Statistical | 0.5 | **20** |
| **of Iron** (+Armor) | T2 (20) | Defense | 1.0 | **20** |
| **of Violence** (+Damage) | T3 (30) | Offense | 2.0 | **60** |
| **of Exposure** (-Resist) | T1 (10) | Player Debuff | 3.0 | **30** |

**Total Difficulty Score ($DS$): 130**

**Derived Rewards:**
* **Item Rarity**: 
  $$
  $130 \times 1.5\% = $ **+195%**
  $$
  
* **Item Quantity**: 
  $$
  $50\% \times \log_2(1 + 130/40) \approx $ **+104%**
  $$
  

### 5.4 Advanced Probability Scaling (Legendary Potential)
To ensure high difficulty drives the chase for "God Rolls", **Drop Rarity** directly acts as a multiplier for **Legendary Potential (LP)** and **Socket** probabilities.

$$
$$ P_{final} = P_{base} \times (1 + \frac{\text{DropRarity}\%}{10}) $$
$$
**Example: Rolling LP 4 (The Grail)**

*   **Base Probability**: $1 \times 10^{-6}$ (1 in 1,000,000)
*   **With +0% Rarity**: 1 in 1,000,000.
*   **With +195% Rarity (DS 130)**:
    $$ 10^{-6} \times (1 + 1.95) \approx 3 \times 10^{-6} $$ (Chance tripled)
*   **With +1000% Rarity (God Slayer Run)**:
    $$ 10^{-6} \times (1 + 10) \approx 1.1 \times 10^{-5} $$ (~1 in 90,000)

*Note: This applies to LP 1-4 and Socket Counts (1-4).*


## 6. Implementation Stages (See plan.md)
*   **Phase 1**: Data Structures & Calculator (The Foundation)
*   **Phase 2**: Persistence Wiring (The Fix)
*   **Phase 3**: Affix Logic Integration & Decay (The Features)

## Appendix A: Map Affix Registry (v2.0)

This registry separates affixes into **Buffs** (Rewards), **Debuffs** (Challenges), and **Environment** (Special).

### A.1 Buffs (Rewards)
The only positive modifiers that increase the "value" of a run.

| Enum | Name | Effect | Tier Values (T1 - T10) |
| :--- | :--- | :--- | :--- |
| `DropRarity` | **Gilded** | +% Item Rarity (Magic Find) | 20% - 150% |
| `DropQuantity` | **Opulent** | +% Item Quantity | 10% - 100% |

> *Note: XP Gain is handled by Monster Level/Density, not a separate multiplier.*

### A.2 Debuffs (Challenges)
These affixes build the Difficulty Score of the Rift.

#### General & Crowd
| Enum | Name | Effect | Range / Note |
| :--- | :--- | :--- | :--- |
| `MonsterDensity` | **Swarming** | +% Pack Size | +10% (Low Weight Difficulty) |
| `MonsterLevel` | **Nightmare** | + Level Offset | +1 to +5 Levels |

#### Enemy Durability (Tank)
| Enum | Name | Effect | Range |
| :--- | :--- | :--- | :--- |
| `Enemy_ExtraHealth` | **of the Colossus** | +% Health | 20% - 100% |
| `Enemy_ExtraBarrier`| **of the Aegis** | Gain Barrier (% of Max HP) | 20% - 80% |
| `Enemy_BarrierRegen`| **of Restoration** | Barrier Regen per Sec (% Max) | 1% - 5% |
| `Enemy_Armor` | **of Iron** | + Flat Armor | 500 - 3000 |
| `Enemy_Dodge` | **of Mist** | + Flat Dodge Rating | 500 - 2500 |
| `Enemy_ResistAll` | **of Prism** | +% All Resistances | 5% - 20% |
| `Enemy_Resist<Type>`| **of <Element>** | +% Specific Resist | 25% - 60% |
| `Enemy_CritResist` | **of Adamant** | -% Crit Dmg Taken | 30% - 60% |

#### Enemy Offense (Damage)
| Enum | Name | Effect | Range |
| :--- | :--- | :--- | :--- |
| `Enemy_ExtraDamage` | **of Violence** | +% Damage | 15% - 50% |
| `Enemy_Fast` | **of Frenzy** | +% Speed | 10% - 35% |
| `Enemy_CritChance` | **of Precision** | +% Crit Chance | 20% - 100% |
| `Enemy_ArmorShred` | **of Sundering** | Chance to shred armor | 5% - 15% chance |

#### Player Impairment (Debuffs)
| Enum | Name | Effect | Range |
| :--- | :--- | :--- | :--- |
| `Player_ResistRedAll`| **of Exposure** | -% All Resistance | -5% to -20% |
| `Player_ResistRed<T>`| **of Vulnerability**| -% Specific Resistance | -15% to -40% |
| `Player_RedRecovery` | **of Atrophy** | -% Regen & Leech Rate | -30% to -60% |
| `Player_Fragile` | **of Glass** | +% Damage Taken | +10% to +25% |
| `Player_DodgePenalty`| **of Unwavering** | -% Dodge Chance | -15% to -30% |

### A.3 Environmental (Special)
*Reserved for future unique map mechanics. Not currently in the random pool.*

- `Env_Firestorm`: Meteor showers.
- `Env_Darkness`: Reduced light radius.
- `Env_GroundIce`: Reduced friction.
- `Env_LightningStorm`: Targeted lightning strikes.
