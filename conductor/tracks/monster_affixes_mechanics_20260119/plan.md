# Track: Advanced Combat Mechanics (Monster Affix V2.0 - Part 3)

**Goal**: Implement complex combat interactions, entity cloning, and resource denial mechanics. This brings "tactical depth" to the game.

## 1. Core Mechanisms
- [ ] **Entity Cloning**:
    - Implement `EntityFactory::CloneEntity(source, stats_multiplier)`.
    - Used for `Mirror Image`. Clones should look identical but have lower HP/Damage.
- [ ] **Damage Pipeline Interception**:
    - Add hooks in `DamagePipeline` for "Proximity Check" (Suppressor) and "Invulnerability" (Shielding).
- [ ] **Link System**:
    - Create `LinkComponent` to visualize connections between entities (e.g., Shielding source -> target).
    - Implement beam rendering in `VisualFXSystem`.

## 2. Affix Implementation
- [ ] **Mirror Image (镜像)**:
    - Mechanic: OnHit/HP Threshold -> Spawn 2 clones.
    - Behavior: Clones use basic AI, no affixes.
- [ ] **Shielding (护盾)**:
    - Mechanic: Find nearby allies -> Apply `Invulnerable` buff.
    - Visual: Gold beam connecting Source to Target.
- [ ] **Soul Eater (噬魂)**:
    - Mechanic: Listen to global `EntityDeath` event. If in range -> Add `SoulStack`.
    - Effect: Each stack grants +5% Size, +5% Dmg, +5% AtkSpd. Cap at 50 stacks.
- [ ] **Suppressor (压制)**:
    - Mechanic: "Proximity Shield". Take 90% less damage from sources > 300px away.
    - Visual: Red bubble shield.
- [ ] **Mana Siphon (虹吸)**:
    - Mechanic: Aura that drains Player Mana (Resource).
    - Implementation: `AuraComponent` + `ResourceDrain` effect. Safety zone inside the ring.

## 3. Integration & Polish
- [ ] **Visual Clarity**: Ensure Shielding beams and Suppressor bubbles are distinct.
- [ ] **Balance**: Tune `Soul Eater` scaling to prevent infinite scaling scenarios.
