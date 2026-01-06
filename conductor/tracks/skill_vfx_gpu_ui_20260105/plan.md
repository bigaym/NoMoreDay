# Plan: Skill System Optimization, UI Polish, and GPU VFX

## Phase 1: Skill System Logic & Sword Intent Depth
- [x] Task: Refactor Sword Intent mechanics in `SkillSystem.cpp` to include more robust triggers and decay rules. (ff9ad70)
- [x] Task: Implement advanced "Empowered" behaviors for core skills (e.g., increased radius, extra projectiles, or unique mechanics). (43a1b02)
- [ ] Task: Improve physical interaction logic (knockback consistency and piercing rules).
- [ ] Task: Write unit tests to verify Sword Intent calculations and skill-specific logic.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Skill System Logic & Sword Intent Depth' (Protocol in workflow.md)

## Phase 2: Monster Health Bars & Status UI
- [ ] Task: Implement `MonsterHealthBarSystem` to render floating HP bars above enemies.
- [ ] Task: Integrate buff/debuff icon rendering above monster health bars.
- [ ] Task: Add "Sword Intent" stack display above the player's health bar.
- [ ] Task: Write tests for health bar visibility and status icon synchronization.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Monster Health Bars & Status UI' (Protocol in workflow.md)

## Phase 3: Skill Assignment UI (Drag-and-Drop & Context Menu)
- [ ] Task: Update the Skill Panel (Key: 'S') to support draggable skill icons.
- [ ] Task: Implement Drag-and-Drop handling between Skill Panel and Skill Bar.
- [ ] Task: Implement Right-Click Context Menu on Skill Bar slots for quick skill selection (Last Epoch style).
- [ ] Task: Write tests for skill assignment persistence and UI interaction events.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Skill Assignment UI (Drag-and-Drop & Context Menu)' (Protocol in workflow.md)

## Phase 4: HUD, Tooltips, and Floating Combat Text (FCT)
- [ ] Task: Enhance the Skill Bar HUD with clearer cooldown overlays and active/press highlights.
- [ ] Task: Implement an animated Floating Combat Text system with distinct styles for Crits and Status Effects.
- [ ] Task: Polish Tooltip layout and Skill Tree visual hierarchy.
- [ ] Task: Write tests for FCT lifecycle and HUD state updates.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: HUD, Tooltips, and Floating Combat Text (FCT)' (Protocol in workflow.md)

## Phase 5: GPU Visual Effects (Compute Shaders & Screen Space)
- [ ] Task: Implement GPU-driven particle effects for "Infinite Blades" and "Sword Array" using Compute Shaders.
- [ ] Task: Add empowered shader effects (glow/trails) for Sword Intent bursts.
- [ ] Task: Implement elemental status visual feedback (frost trails, lightning sparks).
- [ ] Task: Add Screen Shake and subtle Screen-Space effects for heavy impacts.
- [ ] Task: Verify performance benchmarks (60+ FPS during VFX-heavy combat).
- [ ] Task: Conductor - User Manual Verification 'Phase 5: GPU Visual Effects (Compute Shaders & Screen Space)' (Protocol in workflow.md)
