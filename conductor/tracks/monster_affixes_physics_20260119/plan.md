# Track: Physics & Crowd Control Affixes (Monster Affix V2.0 - Part 2)

**Goal**: Implement physics-based mechanics and crowd control effects. Requires extension of `PhysicsSystem` and `GridMap`.

## 1. Physics Extensions
- [ ] **Force Field Support**:
    - Extend `PhysicsSystem` to support radial force fields (Attract/Repel).
    - Create `ForceFieldComponent` (strength, radius, falloff_type).
- [ ] **Dynamic Terrain Modification**:
    - Implement API in `MapSystem` to temporarily spawn "Blocking Entities" (Walls).
    - Ensure AI navigation (FlowField) updates or handles these dynamic obstacles.

## 2. Affix Implementation
- [ ] **Vortex (漩涡)**:
    - Mechanic: Periodic "Pull" effect.
    - Implementation: Attach `ForceFieldComponent` (Attract) to monster. Visual cue: Distortion shader.
- [ ] **Waller (筑墙)**:
    - Mechanic: Spawn U-shaped wall around player.
    - Implementation: Create static entities with `ColliderComponent` (Static). Auto-destroy after 5s.
- [ ] **Entangler (纠缠)**:
    - Mechanic: OnHit chance to apply `Rooted` state to player.
    - Implementation: Add `Rooted` flag in `PlayerState`. Disable movement input, allow skill usage.
- [ ] **Teleporter (闪烁) - V2**:
    - Mechanic: Improved logic. Teleport behind player -> Attack immediately.
    - Visuals: Add "fade out/fade in" tweening instead of instant snap for better game feel.

## 3. Integration & Polish
- [ ] **Counter-Play Tuning**: ensure walls don't trap player permanently (destructible or timed).
- [ ] **Physics Stability**: Verify `Vortex` doesn't pull players into walls/out of bounds.
