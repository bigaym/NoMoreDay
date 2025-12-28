# Track Specification: Loot, Experience, and Leveling Systems

## Overview
Implement the core progression and reward loops for NoMoreDay. This includes a robust XP/Leveling system and a scalable Loot/Drop system capable of handling high-density entity counts.

## Functional Requirements

### 1. Experience & Leveling System
- **XP Scaling:** Implement an exponential curve for XP requirements per level.
- **Level-Difference Penalty:** XP gain is reduced when the player defeats enemies significantly lower than their level.
- **Stat Integration:** Add an `experience_gain_mult` attribute to `CombatStats`.
- **Level-Up Rewards:**
    - Automatic baseline growth of primary stats (Strength, Dexterity, Intelligence, Vitality).
    - Grant **5 Attribute Points** for manual distribution.
    - Grant **1 Skill Point** for the skill tree.
- **Persistence:** Track `current_level`, `current_xp`, `available_attribute_points`, and `available_skill_points` in a `PlayerState` component.

### 2. Loot & Drop System
- **Categories:**
    - **Equipment:** Weapons/Armor with random affixes (Rarities: Common, Magic, Rare, Unique).
    - **Consumables:** Potions and Scrolls.
    - **Currencies:** Gold and Crafting Materials.
- **Drop Logic:**
    - **Global Pool:** Used by ordinary enemies.
    - **Weighted Tables:** Used by Elites and Bosses (combined with Global Pool).
- **Magic Find (MF):** A calculation function that uses the player's MF stat to modify:
    - **Quantity:** Number of items dropped.
    - **Rarity:** Probability of rolling higher rarity tiers.
- **Performance:** Drop calculations must be efficient and avoid allocations in the main combat loop (triggered on enemy death).

## Acceptance Criteria
1. Killing an enemy grants XP based on level difference and the player's XP modifier.
2. Reaching the XP threshold triggers a level-up event, incrementing level and points.
3. Enemies drop items/gold upon death according to their assigned pools/tables.
4. Magic Find correctly shifts the rarity distribution of dropped items.
5. All new data is stored in EnTT components.

## Out of Scope
- UI implementation for the Level-up screen or Inventory.
- Actual Skill Tree logic (only the "Skill Point" counter is implemented).
- Complex item-flipping or physics-based loot scattering (visuals).
