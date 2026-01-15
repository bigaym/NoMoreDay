# Technical Specification: Legendary Affix System Support

## 1. Overview
This document details the technical implementation for extending the game engine to support complex legendary affixes. The goal is to provide a comprehensive event and hook system that allows data-driven definition of item effects without hardcoding logic for each item.

## 2. Tag Registry Expansion (`src/game/data/TagRegistry.hpp|cpp`)

### 2.1 New Tags
We need to introduce functional tags to identify skill types, item types, and action contexts.

| Tag ID | Name | Description | Rationale |
| :--- | :--- | :--- | :--- |
| `Tag::SwordSkill` | "Sword Skill" | Marks skills belonging to the Blade Ascendant path. | Used by `leg_grandmaster`, `leg_spirit_sword`. |
| `Tag::Potion` | "Potion" | Context tag for potion usage events. | Used by `leg_lifestone`, `leg_second_wind`. |
| `Tag::Dash` | "Dash" | Context tag for dash/movement skills. | Used by `leg_shadow_clone`, `leg_phase_walk`. |
| `Tag::Elite` | "Elite" | Target tag for elite enemies. | Used by `leg_giant_slayer`. |
| `Tag::Boss` | "Boss" | Target tag for boss enemies. | Used by `leg_giant_slayer`. |
| `Tag::Gold` | "Gold" | Context tag for gold pickup/possession. | Used by `leg_greed`, `leg_goldskin`. |

### 2.2 Implementation Details
*   Add enum values to `Tag` enum class.
*   Update `kTagInfoTable` in `TagRegistry.cpp` to include string IDs and display names.

## 3. Combat & Game Events (`src/game/systems/combat/CombatEvents.hpp`)

### 3.1 New Event Types
Expand `CombatEventType` to cover non-combat actions pivotal to legendary effects.

```cpp
enum class CombatEventType {
    // ... existing ...
    OnPotionUse,        // Triggered when a potion is consumed
    OnDash,             // Triggered when a dash skill is used
    OnKill,             // Triggered when an enemy is killed (ensure source/target context)
    OnResourceConsumed, // Generic resource consumption (Intent, Mana, HP)
    OnMoveDistance,     // Triggered periodically (e.g. every 1m moved)
    OnGoldPickup        // Triggered when gold is picked up
};
```

### 3.2 Event Payload Extensions (`CombatEvent` struct)
*   **`OnResourceConsumed`**: Needs `float amount` and `ResourceType type` (Enum: Mana, Health, SwordIntent).
*   **`OnMoveDistance`**: Needs `float distance` (accumulated).
*   **`OnKill`**: Needs `entt::entity victim` and `entt::entity killer`.

## 4. Stat Conversion Framework (`src/game/systems/combat/StatsSystem.hpp`)

### 4.1 Concept
Replace hardcoded "Int to Armor" or "Dex to Evasion" logic with a generic list of `StatConverter` structs stored in `CombatStats`.

### 4.2 Data Structures
```cpp
struct StatConverter {
    StatType source_stat;
    StatType target_stat;
    float ratio;        // target = source * ratio
    // Optional: Thresholds or curves? Start with linear.
};

// Add to CombatStats component:
std::vector<StatConverter> stat_conversions;
```

### 4.3 Logic Flow
In `StatsSystem::Recalculate`:
1.  Calculate Base Attributes (Str, Dex, Int, Spirit, Con).
2.  Iterate `stat_conversions`.
3.  Calculate added values and apply to `target_stat`.
4.  *Note*: This requires a multi-pass approach or strict ordering (Attributes first, then Derived Stats).

## 5. Hook Integration Plan

### 5.1 Skill System (`SkillSystem.cpp`)
*   **Intent Consumption**: In `TryCast` or wherever Sword Intent is used/cleared, `CombatEventDispatcher::Dispatch(OnResourceConsumed, ...)` with type `SwordIntent`.
*   **Dash Detection**: If a skill has `Tag::Dash`, dispatch `OnDash`.

### 5.2 Movement System (`MovementSystem.cpp`) (or PlayerSystem)
*   **Distance Tracking**:
    *   Add `float distance_accumulator` to `PlayerState`.
    *   In Update, `accumulator += Vector2Distance(pos, last_pos)`.
    *   If `accumulator >= 1.0f` (1 meter), dispatch `OnMoveDistance` and decrement.
    *   Handle "Stationary" logic: If `Vector2Distance < epsilon` for X seconds, trigger `OnStationary`.

### 5.3 Inventory/Potion System
*   **Potion Use**: Dispatch `OnPotionUse` when a potion is successfully used.

## 6. Legendary Effect Implementation Pattern

All legendary effects will be implemented as **Listeners** registered to `CombatEventDispatcher` or specific system hooks.

**Example: `leg_blade_resonance` (Blade Resonance)**
*   **Trigger**: `OnResourceConsumed` (Type: SwordIntent)
*   **Logic**:
    *   Check `ctx.amount` (stacks consumed).
    *   Perform AoE damage: `DamagePipeline::ApplyValues(..., amount * 100%, ...)`.
    *   Spawn Visual Effect: Shockwave.

**Example: `leg_sword_heart` (Sword Heart)**
*   **Mechanism**: Add a `StatConverter` { Source: Int, Target: SwordIntentMax, Ratio: 0.01 } (1 per 100).
*   **Note**: "Max Sword Intent" is a Component member, not a `CombatStat`. We might need to map it to a generic "Secondary Stat" or handle it via a specific listener that updates the component when Stats change.
    *   *Alternative*: `StatsSystem` update could invoke a `UpdateComponentStats` helper.

## 7. Migration of Hardcoded Logic
*   Move current `AstrolabeSystem` effects (`IntToArmor`, `IntToCritMult`) to use the new `StatConverter` system.

## 8. Definition of Success
1.  Compiles without errors.
2.  Can defined a test case where consuming a potion triggers a log/counter (via event).
3.  Can define a test case where moving 10 meters triggers 10 events.
4.  Can define a 'StatConverter' that increases Armor based on Int, verified in `StatsSystemTests`.
