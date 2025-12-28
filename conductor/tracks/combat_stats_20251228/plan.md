# Track Plan: Refine Core Combat and Stats System

## Phase 1: Core Stats Architecture [checkpoint: 5474e52]
- [x] Task: Define `Stats` component and `StatModifier` structure in `src/components/Stats.hpp` (364bd5a)
- [x] Task: Create `StatsSystem` test suite (`tests/StatsSystemTest.cpp`) (364bd5a)
- [x] Task: Implement `StatsSystem::Recalculate` to derive secondary stats from primary attributes (364bd5a)
- [x] Task: Refactor `StatsSystem` to ensure SIMD compatibility (alignment) (3fb88aa)

## Phase 2: Damage Calculation Pipeline
- [x] Task: Define `DamageEvent` and `DamageType` enums in `src/components/Combat.hpp` (9c4f567)
- [x] Task: Create `CombatSystem` test suite covering mitigation and resistance formulas (f621b8b)
- [x] Task: Implement `CombatSystem::CalculateDamage` with support for Armor and Resistances (f621b8b)
- [x] Task: Implement `CombatSystem::ApplyDamage` to modify Health components (265e03d)

## Phase 2 Checkpoint: Damage Calculation Pipeline [checkpoint: 265e03d]

## Phase 3: Affix & Modifier Integration [checkpoint: ae8aaef]
- [x] Task: Define `ModifierSource` (Item, Skill, Buff)
- [x] Task: Test stacking rules (Additive vs Multiplicative) for Stat Modifiers
- [x] Task: Integrate `StatsSystem` with `InventoryComponent` to apply item stats

## Phase 4: Integration & Validation
- [~] Task: Benchmark `StatsSystem` update loop with 10,000 entities
- [ ] Task: Validate no memory allocations occur during `Update()`
