# Specification: Astrolabe (星盘) System - Phase 1 Foundation

## Overview
The Astrolabe system is a core progression feature in NoMoreDay, providing a concentric-layout passive talent tree. Players spend points earned during leveling to activate nodes that provide stat modifiers, mechanic changes, and "Keystones" that alter gameplay logic. Phase 1 focuses on the data infrastructure, logic validation, and integration with the existing Stats system.

## Functional Requirements
- **Data-Driven Architecture**: Use a hybrid approach where `astrolabe.json` defines layout and metadata, mapped to C++ enums/registries for logic.
- **Node Types**:
    - **Minor**: Small stat boosts.
    - **Major**: Significant stat boosts or secondary effects.
    - **Keystone**: Game-altering mechanics (e.g., Blood Magic).
- **Activation Logic**:
    - Verify player has available talent points.
    - Verify all prerequisite nodes are activated.
    - Validate that the node ID is valid.
- **Component Storage**: Store activated nodes in an `AstrolabeComponent` on the player entity.
- **Stat Integration**: Activated nodes must contribute to the `StatsSystem` recalculation pipeline.
- **Dirty Flagging**: Activating a node marks the player's combat stats as "dirty" to trigger a refresh.

## Non-Functional Requirements
- **Performance**: Zero allocation during combat. Node lookups should be $O(1)$ or $O(\log n)$ using IDs.
- **Extensibility**: The JSON format must support adding new nodes and connections without breaking existing logic.
- **Localization**: Use keys in JSON for names/descriptions to support the existing Chinese localization system.

## Acceptance Criteria
- [ ] `AstrolabeRegistry` can successfully load and parse `assets/data/astrolabe.json`.
- [ ] `AstrolabeComponent` correctly tracks activated node IDs.
- [ ] `ProgressionSystem` (or a dedicated `AstrolabeSystem`) correctly validates and activates nodes.
- [ ] Activating a node that grants `+5 Strength` results in the player's `CombatStats` reflecting the change after the next refresh.
- [ ] Unit tests verify: loading, prerequisite validation, and stat application.

## Out of Scope (For Phase 1)
- Complex UI rendering and interactive menu.
- "Star Bridges" (cross-sector connections).
- Skill-specific specialization trees (these are separate from the character Astrolabe).
