# Track: Advanced Combat Mechanics (Monster Affix V2.0 - Part 3)

**Goal**: Implement complex combat interactions, entity cloning, and resource denial mechanics. This brings "tactical depth" to the game.

**Status**: ✅ Core Implementation Complete

## 1. Core Mechanisms
- [x] **Entity Cloning**:
    - Implemented `EntityUtils::CloneEntity(source, stats_multiplier)`.
    - Used for `Mirror Image`. Clones look identical but have lower HP/Damage.
    - Location: `src/game/utils/EntityUtils.hpp`
- [x] **Damage Pipeline Interception**:
    - Added hooks in `DamagePipeline` for "Invulnerable Check" and "Proximity Check" (Suppressor).
    - Location: `src/game/systems/combat/DamagePipeline.cpp`
- [x] **Link System**:
    - Created `LinkComponent` to visualize connections between entities.
    - Location: `src/game/components/AdvancedAffixComponents.hpp`
    - VFX rendering: TODO (needs visual beam implementation)

## 2. Affix Implementation
- [x] **Mirror Image (镜像)**:
    - Mechanic: OnHit/HP Threshold -> Spawn 2 clones.
    - Behavior: Clones use basic AI, no affixes, 10% HP, 50% damage.
    - Location: `MonsterAffixSystem::OnEnemyTakeDamage`
- [x] **Shielding (护盾)**:
    - Mechanic: Find nearby allies -> Apply `Invulnerable` buff.
    - Location: `MonsterAffixSystem::ProcessShielding`
    - Gold beam visual: TODO
- [x] **Soul Eater (噬魂)**:
    - Mechanic: Listen to global `EntityDeath` event. If in range -> Add `SoulStack`.
    - Effect: Each stack grants +5% Size, +5% Dmg, +5% AtkSpd. Cap at 50 stacks.
    - Location: `MonsterAffixSystem::OnEnemyDeath`, `ProcessSoulEater`
- [x] **Suppressor (压制)**:
    - Mechanic: "Proximity Shield". Take 90% less damage from sources > 300px away.
    - Location: `DamagePipeline::Calculate`
    - Red bubble shield visual: TODO
- [x] **Mana Siphon (虹吸)**:
    - Mechanic: Aura that drains Player Mana (Resource).
    - Implementation: `ResourceDrainComponent` + donut-shaped safe zone.
    - Location: `MonsterAffixSystem::ProcessManaSiphon`

## 3. New Components Created
- `LinkComponent` - Entity connection visualization
- `CloneComponent` - Clone entity marker and lifecycle
- `ResourceDrainComponent` - Resource drain aura config
- `InvulnerableComponent` - Invulnerability state
- `SoulEaterComponent` - Soul stack tracking
- `SuppressorComponent` - Distance-based damage reduction

## 4. Integration & Polish
- [x] **MonsterAffixRegistry**: Added 4 new affix types (MirrorImage, SoulEater, Suppressor, ManaSiphon)
- [ ] **Visual Clarity**: Beam rendering for Shielding, bubble for Suppressor
- [ ] **Balance Tuning**: SoulEater scaling values may need adjustment
- [ ] **Unit Tests**: Add tests for new affix behaviors

## 5. Files Modified
- `src/game/components/AdvancedAffixComponents.hpp` (NEW)
- `src/game/utils/EntityUtils.hpp` (NEW)
- `src/game/data/MonsterAffixRegistry.hpp` (MODIFIED)
- `src/game/systems/combat/MonsterAffixSystem.hpp` (MODIFIED)
- `src/game/systems/combat/DamagePipeline.cpp` (MODIFIED)
