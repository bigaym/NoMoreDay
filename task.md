# Skill System Implementation: Sword Cultivator

This task tracks the phased implementation of the Sword Cultivator's full skill set, moving from technical foundations to complex gameplay mechanics and finally visual polish.

- [x] **Phase 1: Shadow Entity System (Technical Foundation)**
    - [x] Define `ShadowComponent` & `ShadowSystem` structure <!-- id: 0 -->
    - [x] Implement `ShadowSystem` lifecycle & behavior loop <!-- id: 1 -->
    - [x] Implement `ShadowBehavior` for skill mimicry <!-- id: 2 -->
    - [x] Integration: `Flowing Thrust` (Branch B: Shadow Kill) <!-- id: 3 -->
- [x] **Phase 2: Blade Formation (Ling Jian Jue) Deep Dive**
    - [x] Refactor `BladeFormation` for state-driven behavior <!-- id: 4 -->
    - [x] Implement "Sword Rain" branch logic (Multi-projectile tracking) <!-- id: 5 -->
    - [x] Implement "Heavy Sword" branch logic (Entity merging) <!-- id: 6 -->
- [ ] **Phase 3: Defensive & Counter Mechanics**
    - [ ] `Blade Ward`: Projectile interception system <!-- id: 7 -->
    - [ ] `Phantom Flash`: Counter-attack state watcher <!-- id: 8 -->
- [ ] **Phase 4: Visual Polish (GPU VFX)**
    - [ ] Bind `GPUParticleSystem` to `OnSkillHit`/`OnCast` events <!-- id: 9 -->
    - [ ] Implement visual feedback for Sword Intent stacks <!-- id: 10 -->
