# Plan: Skill System Optimization, UI Polish, and GPU VFX

## Phase 1: Skill System Logic & Sword Intent Depth [checkpoint: 7abe196]
- [x] Task: Refactor Sword Intent mechanics in `SkillSystem.cpp` to include more robust triggers and decay rules. (ff9ad70)
- [x] Task: Implement advanced "Empowered" behaviors for core skills (e.g., increased radius, extra projectiles, or unique mechanics). (43a1b02)
- [x] Task: Improve physical interaction logic (knockback consistency and piercing rules). (6930030)
- [x] Task: Write unit tests to verify Sword Intent calculations and skill-specific logic. (6930030)
- [x] Task: Conductor - User Manual Verification 'Phase 1: Skill System Logic & Sword Intent Depth' (Protocol in workflow.md)

## Phase 2: Monster Health Bars & Status UI [checkpoint: 4b9310e]
- [x] Task: Implement `MonsterHealthBarSystem` to render floating HP bars above enemies.
- [x] Task: Integrate buff/debuff icon rendering above monster health bars.
- [x] Task: Add "Sword Intent" stack display above the player's health bar.
- [x] Task: Write tests for health bar visibility and status icon synchronization.
- [x] Task: Conductor - User Manual Verification 'Phase 2: Monster Health Bars & Status UI' (Protocol in workflow.md)

## Phase 3: Skill Assignment UI (Drag-and-Drop & Context Menu) [checkpoint: 1159cb3]
- [x] Task: Update the Skill Panel (Key: 'S') to support draggable skill icons. (2d747eb)
- [x] Task: Implement Drag-and-Drop handling between Skill Panel and Skill Bar. (2d747eb)
- [x] Task: Implement Right-Click Context Menu on Skill Bar slots for quick skill selection (Last Epoch style). (2d747eb)
- [x] Task: Write tests for skill assignment persistence and UI interaction events. (2d747eb)
- [x] Task: Conductor - User Manual Verification 'Phase 3: Skill Assignment UI (Drag-and-Drop & Context Menu)' (Protocol in workflow.md)

## Phase 4: HUD, Tooltips, and Floating Combat Text (FCT) [checkpoint: d1e2f3a]
- [x] Task: Enhance the Skill Bar HUD with clearer cooldown overlays and active/press highlights. (8e7f1a2)
- [x] Task: Implement an animated Floating Combat Text system with distinct styles for Crits and Status Effects. (8e7f1a2)
- [x] Task: Polish Tooltip layout and Skill Tree visual hierarchy. (8e7f1a2)
- [x] Task: Write tests for FCT lifecycle and HUD state updates. (d1e2f3a)
- [x] Task: Conductor - User Manual Verification 'Phase 4: HUD, Tooltips, and Floating Combat Text (FCT)' (Protocol in workflow.md)

## Phase 5: GPU Visual Effects (Compute Shaders & Screen Space)
- [x] Task: Implement GPU-driven particle effects for "Infinite Blades" and "Sword Array" using Compute Shaders. (Ref: SkillSystem.cpp, GPUParticleSystem.cpp, particle.compute)
- [x] Task: Add empowered shader effects (glow/trails) for Sword Intent bursts. (Ref: SkillSystem.cpp UpdateSwordIntent)
- [x] Task: Implement elemental status visual feedback (frost trails, lightning sparks). (Ref: StatsSystem.cpp UpdateBuffs)
- [x] Task: Add Screen Shake and subtle Screen-Space effects for heavy impacts. (Ref: RenderSystem.cpp, CombatSystem.cpp)
- [ ] Task: Verify performance benchmarks (60+ FPS during VFX-heavy combat).
- [ ] Task: Conductor - User Manual Verification 'Phase 5: GPU Visual Effects (Compute Shaders & Screen Space)' (Protocol in workflow.md)
