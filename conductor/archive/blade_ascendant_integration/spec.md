# Track Spec: Blade Ascendant System Integration

## 1. Overview
This track handles the **integration of VFX assets into the ECS architecture**. It connects the gameplay logic (Skills, Sword Intent) with the visual representation (Particles, Shaders, Trails).

## 2. User Stories
*   **As a Player**, I want to see my sword glow brighter as I build "Sword Intent" stacks, so I know when I'm at full power.
*   **As a Player**, I want to see a cool light trail when I use "Flowing Thrust", emphasizing speed.
*   **As a Player**, I want "Blade Formation" swords to physically hover behind me and shoot out, not just spawn projectiles from nowhere.

## 3. ECS Architecture

### 3.1 Components
*   **`SwordIntentVisualComponent`**:
    *   `int currentLevel`: To smooth transition between levels.
    *   `float intensity`: Driving the shader glow parameter.
*   **`MotionTrailComponent`**:
    *   `std::deque<Vector2> history`: Position history for trail mesh generation.
    *   `float width`: Width of the trail.
    *   `Color color`: Tint.
    *   `float decayTime`: How fast it fades.
*   **`HoloBladeComponent`**:
    *   Marker for entities that should use the `HoloBladeShader`.

### 3.2 Systems
*   **`VFXSystem` (New or logic in RenderSystem)**:
    *   Updates `MotionTrailComponent`: Extrudes mesh from history points.
    *   Updates `SwordIntentVisualComponent`: Syncs with gameplay `SwordIntentComponent`.
    *   Handles `EventVFXTrigger`: Spawns one-shot particles (Shockwaves, Hits).

### 3.3 Skill Integrations
*   **Flowing Thrust**: Add `MotionTrailComponent` during dash state.
*   **Blade Formation**: Create Child Entities for the hovering swords with `HoloBladeComponent`.
*   **Infinite Blades**: Trigger GPU Particle Emitter with high rate.

## 4. Integration Logic
1.  **Decoupling**: Gameplay logic sends Events (`EntityHit`, `SkillCast`). VFX logic listens or polls state.
2.  **Performance**: Trail mesh generation should be efficient (std::vector reuse).

## 5. Dependencies
*   Completion of `blade_ascendant_vfx` track (Shaders/Assets must exist).
*   Existing `SkillSystem` and `ProjectileSystem`.
