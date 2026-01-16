# Plan: Blade Ascendant VFX Asset Pipeline

## Phase 1: Shader Implementation (Core)
- [x] **Task 1: Sword Trail Shader**
    - Create `assets/shaders/vfx/sword_trail.vs` and `.fs`.
    - Implement UV scrolling logic.
    - Register in `ShaderManager`.
- [x] **Task 2: Holo Blade Shader**
    - Create `assets/shaders/vfx/holo_blade.vs` and `.fs`.
    - Implement Fresnel rim lighting + Noise scroll.
    - Register in `ShaderManager`.
- [x] **Task 3: Distortion Shader**
    - Create `assets/shaders/vfx/distortion.fs`.
    - Implement screen-space refraction math.

## Phase 2: Asset Generation
- [x] **Task 4: Texture Generation**
    - Create `scripts/gen_vfx_textures.py`.
    - Generate `trail_mask.png` (linear gradient with noise).
    - Generate `energy_noise.png` (perlin/cellular noise).
    - Run script to populate `assets/textures/vfx/`.

## Phase 3: Particle Configuration
- [x] **Task 5: Particle Definitions**
    - Create `assets/data/particles/blade_ascendant.json`.
    - Define `SwordIntent_Lv1` to `SwordIntent_Lv10`.
    - Define `InfiniteBlades_Emitter`.
    - Define `BladeWard_Orbit`.

## Phase 4: Verification
- [x] **Task 6: Visual Testbed**
    - Create `src/game/states/TestVFXState.hpp/.cpp`.
    - Render a dummy sword with Holo shader.
    - Render a trail following the mouse.
    - Verify GPU particle emission performance.
