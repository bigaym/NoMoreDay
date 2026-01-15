# Legislative Affix System Support Plan

**Objective**: Extend the current game engine (ECS, Events, Stats) to support the complex mechanisms required by the [Legendary Affixes Design Draft](../../设计文档/legendary_affixes.md). This track focuses on infrastructure, not the implementation of individual items.

## Phase 1: Tag & Event Registry Expansion
- [x] **Extend TagRegistry**: Add necessary tags for logic filtering.
    - [x] `Tag::Potion` (For "Second Wind", "Lifestone")
    - [x] `Tag::Dash` (For "Shadow Clone", "Phase Walk")
    - [x] `Tag::SwordSkill` (Explicitly mark sword skills for "Grandmaster" or specific scalings)
    - [x] `Tag::Elite`, `Tag::Boss` (For "Giant Slayer")
    - [x] `Tag::Gold` (For economic affixes)
- [x] **Expand CombatEventType**: Add new event triggers.
    - [x] `OnPotionUse` (Implemented as `OnUsePotion`)
    - [x] `OnDash` (Existing)
    - [x] `OnKill` (Existing, verified context)
    - [x] `OnResourceConsumed` (Specifically for Sword Intent consumption events)
    - [x] `OnMoveDistance` (For "Momentum" and "Static Charge")

## Phase 2: Dynamic Stat Conversion Framework
- [x] **Generalize Stat Conversion**: Refactor the ad-hoc conversion logic (e.g., `IntToArmor` in `AstrolabeSystem`) into a generic `StatConversion` system within `StatsSystem`.
    - [x] Allow items/affixes to register conversions via `StatConversionComponent` or `ItemComponent::conversions`.
    - [x] Implemented `apply_conversions` lambda in `StatsSystem::Recalculate`.
- [x] **Support "Dependent Stats"**: Implement logic for stats that depend on other dynamic values.
    - [x] Initial implementation: Attack Speed based on Sword Intent stacks.

## Phase 3: Logic Hooks & System Integration
- [x] **Sword Intent Hooks**: 
    - [x] Modified `RendingWave` (Talent 252) to trigger `OnResourceConsumed` when Sword Intent is spent.
- [x] **Movement Tracking**:
    - [x] Updated `MovementStanceSystem` to track distance traveled via `MovementAccumulator` and dispatch `OnMoveDistance` every 100 units.
- [x] **Potion System Hooks**:
    - [x] Integrated `OnUsePotion` event dispatch into `InventorySystem::useItem`.
- [x] **Player Initialization**:
    - [x] Updated `GameplayState` to emplace `MovementAccumulator` and `CombatEventDispatcher` to the player.

## Phase 4: Unique Mechanic Support (High Risk)
- [x] **Titan's Grip Support**: 
    - [x] Implemented `TitanGripTrait` in `Stats.hpp`.
    - [x] Updated `InventorySystem::equipItem` to allow dual-wielding two-handed weapons for entities with this trait.
    - [x] Added `AffixType::TitanGrip` to allow items to provide this trait dynamically.
- [x] **Data-Driven Damage Type Conversion**:
    - [x] Ensured `DamagePipeline` can pull conversions directly from global modifiers or item components.
    - [x] Added `damage_modifiers` to `ItemComponent`.

## Phase 5: Testing & Validation
- [x] **Infrastructure Tests**: Write unit tests to verify:
    - [x] Events fire correctly (Dash, Potion, Intent Consume).
    - [x] Stat conversions apply correctly (e.g., set Int, check Armor).
    - [x] Tags are correctly propagated.
- [x] **Sample Legendary Item Implementation**: Define 1-2 legendary items in JSON (or via code injection) to verify the whole pipeline.
    - [x] Verified Titan's Grip via Legendary Gloves.
    - [x] Verified Damage Conversion via Legendary Ring.
