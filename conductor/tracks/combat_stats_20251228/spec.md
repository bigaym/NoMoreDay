# Track Specification: Refine Core Combat and Stats System

## Goal
To implement a robust, high-performance combat and statistics system that supports deep itemization and character builds, capable of handling 10,000+ entities.

## Requirements

### 1. Stats Component (`Stats.hpp`)
- **Core Attributes:** Strength, Dexterity, Intelligence, Vitality.
- **Derived Stats:** Max HP, Max Mana, Crit Chance, Crit Damage, Armor, Evasion, etc.
- **Implementation:** POD struct optimized for EnTT.
- **Logic:** System to recalculate derived stats when core attributes or equipment change.

### 2. Damage Pipeline (`CombatSystem.cpp`)
- **Damage Types:** Physical, Fire, Cold, Lightning, Poison, Void (Eldritch).
- **Calculation Step:** `(Base Damage * (1 + %Increased) * (1 + %More)) + Flat Adds`.
- **Mitigation:** Armor reduction, Resistance caps, Dodge mechanics.
- **Event Handling:** Hook for "On Hit", "On Kill", "On Crit" effects.

### 3. Affix System Integration
- **Modifier Application:** Efficient way to apply `StatModifier` structs to the `Stats` component.
- **Stacking Rules:** Handle Additive vs. Multiplicative modifiers correctly.

### 4. Performance Constraints
- **Zero Allocations:** No memory allocation during the combat update loop.
- **Batch Processing:** Use EnTT views to process combat in tight loops, friendly to CPU cache.

## Out of Scope
- Visual Effects (VFX) implementation.
- UI display of damage numbers (Damage Floaters).
- Network replication.
