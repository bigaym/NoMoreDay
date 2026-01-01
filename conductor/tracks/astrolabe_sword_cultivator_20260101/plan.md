# Plan: Astrolabe Expansion - Sword Cultivator Foundation

## Phase 1: Mechanics - The Sword Heart (Basic)
- [x] Task: Create `SwordHeartComponent` to tag Sword Cultivators.
- [x] Task: Update `StatsSystem::Recalculate` to implement "Sword Heart" logic:
    - [x] Check if `SwordHeartComponent` exists.
    - [x] Check if Main Hand is Sword and Off Hand is Empty.
    - [x] If true: Apply 50% Weapon Damage Multiplier (More).
    - [x] If true: Add Block Chance (Base 20% + 0.5% per level?).
    - [x] If true: Add "Spell Damage equals 50% of Attack Damage" logic (Requires new StatType or conversion logic in `StatsSystem` or `DamagePipeline`).
- [~] Task: Create a test case in `StatsSystemTest` to verify "Sword Heart" bonuses are applied correctly.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Mechanics - The Sword Heart (Basic)' (Protocol in workflow.md)

## Phase 2: Data - Sword Cultivator Node Definition
- [ ] Task: Update `assets/data/astrolabe.json` to include the starting nodes for the Sword Cultivator path.
    - [ ] Node: "Sword Heart" (Keystone) - Grants the `SwordHeartComponent` (Need mechanism to grant components via Astrolabe).
    - [ ] Node: "Sword Training" (Minor) - +Physical Damage, +Attack Speed.
    - [ ] Node: "Flowing Qi" (Major) - +Movement Speed, +Mana Regen.
- [ ] Task: Update `AstrolabeSystem` to handle special node effects (like granting components).
    - [ ] Current system only applies `StatModifier`. Need a way to trigger logic/add components.
    - [ ] Proposal: Add `effects` list to `AstrolabeNode` (e.g., `{"type": "GrantComponent", "component": "SwordHeart"}`).
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Data - Sword Cultivator Node Definition' (Protocol in workflow.md)

## Phase 3: UI Polish & Planning Mode
- [ ] Task: Implement "Planning Mode" state in `AstrolabeUIComponent`.
    - [ ] Left-click on unallocated node adds to "Planned" set (Ghost visual).
    - [ ] Right-click on "Planned" node removes it.
- [ ] Task: Add "Confirm" and "Reset" buttons to the UI.
    - [ ] "Confirm" commits the planned nodes (calls `activate_node`).
    - [ ] "Reset" clears the planned nodes.
- [ ] Task: Implement "Refund" mode.
    - [ ] Toggle button to switch to Refund Mode.
    - [ ] Clicking allocated node refunds it (if it's a leaf node).
- [ ] Task: Conductor - User Manual Verification 'Phase 3: UI Polish & Planning Mode' (Protocol in workflow.md)
