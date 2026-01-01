# Specification: Astrolabe Expansion - Sword Cultivator Foundation

## Overview
This track focuses on transforming the Astrolabe system from a generic UI into a functional progression system that supports the "Sword Cultivator" (Blade Ascendant) class. It introduces the "Sword Heart" mechanic, defines the initial talent tree nodes, and adds essential UI features for planning and refunding points.

## Functional Requirements

### 1. Sword Heart Mechanic (Class Passive)
- **Concept:** A unique trait for Sword Cultivators that rewards the "One-Handed Sword, No Offhand" playstyle.
- **Logic:**
    - Trigger Condition: Player has `SwordHeartComponent` AND Main Hand is `Sword` AND Off Hand is `Empty`.
    - Effects:
        - **Weapon Damage:** +50% (Multiplicative) to Base Weapon Damage.
        - **Block Chance:** +20% Base Block Chance (simulating a "Parry").
        - **Spell Conversion:** 50% of Attack Damage bonuses are applied to Spell Damage (allowing Hybrid builds).

### 2. Astrolabe Data & Logic
- **Special Effects:** Nodes must be able to do more than just add stats. They need to be able to grant Components (e.g., granting `SwordHeartComponent` to unlock the class trait).
- **Data Structure:** Extend `AstrolabeNode` JSON definition to support an `effects` array.
- **Initial Nodes:**
    - **Sword Heart (Keystone):** The class starter node.
    - **Sword Training (Minor):** Basic damage nodes.
    - **Flowing Qi (Major):** Utility nodes.

### 3. UI Planning Mode
- **Safety:** Players should not accidentally spend points.
- **Planning:** Clicking a node should tentatively allocate it ("Ghost" state).
- **Commit:** A "Confirm" button finalizes the allocation.
- **Reset:** A "Reset" button clears pending allocations.
- **Refund:** A specific mode to unlearn skills (one by one, respecting dependencies).

## Technical Implementation
- **StatsSystem:** Needs to be updated to check for `SwordHeartComponent` and equipment state during recalculation.
- **AstrolabeSystem:** Needs to parse and apply special effects (Component granting).
- **UI:** Needs visual distinction for "Planned" vs "Allocated" nodes.

## Acceptance Criteria
- [ ] "Sword Heart" mechanic works: equipping a sword with no offhand drastically increases damage if the trait is active.
- [ ] `astrolabe.json` contains the Sword Cultivator starting nodes.
- [ ] Activating the "Sword Heart" node in the UI correctly grants the `SwordHeartComponent`.
- [ ] Players can plan a path of nodes and confirm them in batch.
- [ ] Players can refund points (if they have points to refund and connectivity is maintained).
