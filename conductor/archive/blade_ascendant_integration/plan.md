# Plan: Blade Ascendant System Integration

## Phase 1: Infrastructure
- [x] **Task 1: Components**
    - Create `src/game/components/vfx/MotionTrailComponent.hpp`.
    - Create `src/game/components/vfx/SwordIntentVisualComponent.hpp`.
    - Create `src/game/components/vfx/HoloBladeComponent.hpp`.
- [x] **Task 2: Trail Rendering System**
    - Implement mesh generation logic for trails (Triangle Strip).
    - Integrate into `RenderSystem` or create `TrailRenderSystem`.

## Phase 2: Core State Visuals
- [x] **Task 3: Sword Intent Visuals**
    - Create `SwordIntentVisualSystem`.
    - Logic: Read `SwordIntentComponent` stack count -> Update `SwordIntentVisualComponent` intensity/particles.
    - Add "Burst" effect on max stacks.

## Phase 3: Skill Integration
- [x] **Task 4: Flowing Thrust**
    - Modify `FlowingThrust.cpp` (or `SkillSystem`) to attach `MotionTrailComponent` during dash.
    - Remove component after dash.
- [x] **Task 5: Blade Formation (Holo Swords)**
    - Modify `BladeFormation.cpp`.
    - When active, spawn N child entities with `HoloBladeComponent` and `OrbitComponent`.
    - Sync positions with player.
- [x] **Task 6: Infinite Blades**
    - Modify `InfiniteBlades.cpp`.
    - Link to `GPUParticleSystem` emitter.
- [x] **Task 7: Blade Ward**
    - Add persistent orbiting sword visuals (similar to Blade Formation but distinct).

## Phase 4: Polish (Legacy/Optional)
- [x] **Task 8: Audio & Shake**
    - Add `CameraShake` events to critical hits/finishers.