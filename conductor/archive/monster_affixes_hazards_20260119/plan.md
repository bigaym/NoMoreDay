# Track: Environmental & Hazard Affixes (Monster Affix V2.0 - Part 1)

**Goal**: Implement high-impact environmental hazard affixes that force player movement. Focus on generic `HazardSystem` and visual feedback using GPU particles.

## 1. Core Systems
- [ ] **HazardSystem Infrastructure**:
    - Create `HazardComponent` (radius, damage, tick_interval, duration, damage_type).
    - Implement `HazardSystem` to handle tick timers, spatial queries, and damage application via `DamagePipeline`.
    - Support "delayed activation" (e.g., for `Volatile` explosions).
- [ ] **Visuals Integration**:
    - Integrate with `GPUParticleSystem` to spawn particle emitters for hazards.
    - Implement `HazardVisualComponent` to link ECS entities to particle emitters.

## 2. Affix Implementation
- [ ] **Frozen (极寒)**:
    - Mechanic: Spawn `FrozenOrb` entity that follows player, stops, and explodes after delay.
    - Effect: Deals Cold damage and applies `Chill`/`Freeze` debuff.
- [ ] **Toxic (剧毒)**:
    - Mechanic: OnDeath, spawn 3 `VolatileOrb` projectiles that seek player.
    - Effect: Impact creates a `ToxicPool` (Hazard) dealing Poison DoT.
- [ ] **Void Zone (虚空)**:
    - Mechanic: Periodically spawn static `VoidZone` under player.
    - Effect: Deals True Damage (bypass armor/resist). High visual distortion.
- [ ] **Storm Strider (雷行)**:
    - Mechanic: OnHit, spawn a static "Lightning Ghost" at monster position.
    - Effect: Ghost explodes after 1.5s dealing Lightning damage.

## 3. Integration & Polish
- [ ] Update `MonsterAffixRegistry` with new affix definitions.
- [ ] Add unit tests for `HazardSystem`.
- [ ] Verify performance with 50+ concurrent hazards.
