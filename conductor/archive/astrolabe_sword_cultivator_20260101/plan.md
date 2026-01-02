# Plan: Astrolabe Expansion - Sword Cultivator Foundation

## Phase 1: Mechanics - The Sword Heart (Basic) [checkpoint: 7167cf8]
- [x] Task: Create `SwordHeartComponent` to tag Sword Cultivators.
- [x] Task: Update `StatsSystem::Recalculate` to implement "Sword Heart" logic:
    - [x] Check if `SwordHeartComponent` exists.
    - [x] Check if Main Hand is Sword and Off Hand is Empty.
    - [x] If true: Apply 50% Weapon Damage Multiplier (More).
    - [x] If true: Add Block Chance (Base 20% + 0.5% per level?).
    - [x] If true: Add "Spell Damage equals 50% of Attack Damage" logic (Requires new StatType or conversion logic in `StatsSystem` or `DamagePipeline`).
- [x] Task: Create a test case in StatsSystemTest to verify "Sword Heart" bonuses are applied correctly.
- [x] Task: Conductor - User Manual Verification 'Phase 1: Mechanics - The Sword Heart (Basic)' (Protocol in workflow.md)

## Phase 2: Data - Sword Cultivator Node Definition
- [ ] Task: Update `assets/data/astrolabe.json` to include the starting nodes for the Sword Cultivator path.
    - [ ] Node: "Sword Heart" (Keystone) - Grants the `SwordHeartComponent` (Need mechanism to grant components via Astrolabe).
    - [ ] Node: "Sword Training" (Minor) - +Physical Damage, +Attack Speed.
    - [ ] Node: "Flowing Qi" (Major) - +Movement Speed, +Mana Regen.
- [ ] Task: Update `AstrolabeSystem` to handle special node effects (like granting components).
    - [ ] Current system only applies `StatModifier`. Need a way to trigger logic/add components.
    - [ ] Proposal: Add `effects` list to `AstrolabeNode` (e.g., `{"type": "GrantComponent", "component": "SwordHeart"}`).
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Data - Sword Cultivator Node Definition' (Protocol in workflow.md)

## Phase 3: UI Polish & Planning Mode [checkpoint: 832cf10]
- [x] Task: Implement "Planning Mode" state in `AstrolabeUIComponent`.
    - [x] Left-click on unallocated node adds to "Planned" set (Ghost visual).
    - [x] Right-click on "Planned" node removes it.
- [x] Task: Add "Confirm" and "Reset" buttons to the UI.
    - [x] "Confirm" commits the planned nodes (calls `activate_node`).
    - [x] "Reset" clears the planned nodes.
- [x] Task: Implement "Refund" mode.
    - [x] Toggle button to switch to Refund Mode.
    - [x] Clicking allocated node refunds it (if it's a leaf node).
- [x] Task: Conductor - User Manual Verification 'Phase 3: UI Polish & Planning Mode' (Protocol in workflow.md)
