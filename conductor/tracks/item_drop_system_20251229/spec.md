# Track: Item and Drop System - Specification

## 1. Overview
This track focuses on implementing a comprehensive item and equipment system along with a robust drop system. The primary goal is to enable players to significantly improve all combat and special attributes through equipment, offering deep customization and meaningful progression. This system will draw inspiration from ARPGs like Diablo 2 and Last Epoch, incorporating refinement, rune, and fusion mechanics.

## 2. Equipment System Functional Requirements

### 2.1 Equipment Slots
The player character shall have 11 distinct equipment slots, including two ring slots:
- Main Hand (Single-handed weapon, two-handed weapon occupying off-hand)
- Off Hand (Shield, orb, quiver, or secondary part of two-handed weapon)
- Head
- Shoulder
- Chest
- Hands
- Legs
- Feet
- Neck
- Ring (Left)
- Ring (Right)

### 2.2 Core Itemization Goals
The system shall aim to provide:
- **Deep Customization:** Allowing players to significantly customize their builds through diverse item choices, crafting, and modification systems.
- **Meaningful Loot Progression:** Ensuring that dropped items consistently offer opportunities for character improvement and exciting upgrades.
- **Build-Enabling Uniques:** Incorporating unique items that dramatically alter gameplay or enable specific build archetypes.
- **Strategic Trade-offs:** Forcing players to make interesting choices when selecting gear, often involving balancing offensive and defensive stats.

### 2.3 Item Attribute Systems
- **Base Stats + Affixes:** All equipment will have base stats and a system of affixes.
- **Affix System:**
    - **Prefix/Suffix:** Items will roll with up to 2 prefixes and 2 suffixes.
    - **Tiered Affixes:** Each affix will have multiple tiers, with higher tiers granting proportionally greater stat bonuses.
    - **Hybrid Affixes:** Support for affixes that grant multiple types of stats (e.g., "Strength and Fire Resistance").
    - **Conditional Affixes:** Support for affixes that grant bonuses under specific conditions (e.g., "Increased damage against slowed enemies").
    - **Special/Unique Affixes:** Inclusion of affixes that introduce unique gameplay mechanics or skill modifiers, beyond simple stat bonuses.
- **Set System:** Implementation of item sets that grant additional bonuses when multiple pieces of the set are equipped.

### 2.4 Item Modification Systems
- **Refinement System (洗练系统):** A system allowing players to reroll specific stats or affixes on an item.
- **Rune System (符文系统):** A system for socketing runes into items, providing additional modifiers or effects.
- **Fusion System (融合系统):** A mechanism for combining items to create more powerful or unique items, inspired by systems in Diablo 2 and Last Epoch.

## 3. Drop System Functional Requirements

### 3.1 Influences on Item Drops
Item drops and their quality shall be influenced by the following mechanisms:
- **Monster Level:** Higher monster levels will influence the tiers of affixes and the rarity of dropped items.
- **Monster Rarity/Type:** More challenging and rare monsters will have a higher drop rate and may drop items from specific loot tables not available in the global pool (potentially mapped by monster rarity levels).
- **Player Magic Find (MF):** A player stat that increases the chance of finding magic, rare, or unique items. The default value will be 0%.
- **Area Level/Difficulty:** Higher area levels and difficulties will increase the drop rate of rarer equipment.
- **Boss Drops:** Bosses will follow the "Monster Rarity/Type" logic for now, without exclusive boss-specific drop tables initially.

### 3.2 Loot Filter
- A comprehensive loot filter system shall be implemented, allowing players to filter out unwanted item drops (e.g., based on rarity, item type, or specific affixes), both visually and for automatic ignoring/deconstruction.

## 4. Non-Functional Requirements
- **Performance:** Item and drop systems must be highly performant, adhering to the game's ECS and DOD principles, ensuring minimal runtime allocations and efficient processing, especially for large numbers of dropped items and stat calculations.
- **Data-Driven:** The system should be highly data-driven, allowing for easy configuration and expansion of item bases, affixes, sets, and drop tables via external data files (e.g., JSON).

## 5. Acceptance Criteria
- All 11 defined equipment slots are functional and can equip items.
- Items can be generated with base stats and a combination of prefixes and suffixes (up to 2 each).
- Affixes are tiered and correctly apply their stat bonuses.
- Set items provide correct bonuses when equipped.
- Refinement, Rune, and Fusion systems are implemented and functional.
- Item drop rates and quality are correctly influenced by monster level, monster rarity, player Magic Find, and area level.
- The loot filter can effectively filter items based on configurable rules.

## 6. Out of Scope
- Exclusive boss-specific drop tables in the initial implementation. This will be considered for future iterations.

