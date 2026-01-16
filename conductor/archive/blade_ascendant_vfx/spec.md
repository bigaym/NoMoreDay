# Track Spec: Blade Ascendant VFX Asset Pipeline

## 1. Overview
This track focuses on the **technical implementation of visual assets** required for the Blade Ascendant class. It involves writing specialized shaders, defining particle system configurations, and creating procedural texture assets (if needed).

**Goal**: Provide the rendering building blocks (Shaders + Assets) for the integration track.

## 2. User Stories
*   **As a Rendering Engineer**, I want a `SwordTrailShader` that supports UV scrolling and alpha fade so that sword slashes look dynamic.
*   **As a Level Designer**, I want a `DistortionShader` to simulate heat waves or shockwaves without complex geometry.
*   **As a Class Designer**, I want "Holographic" sword materials (`HoloBladeShader`) to differentiate "Spirit Swords" from physical weapons.

## 3. Technical Components

### 3.1 Shaders
*   **`assets/shaders/vfx/sword_trail.glsl`**:
    *   **VS**: Expands a line strip into a triangle strip based on width.
    *   **FS**: Scrolling UV texture opacity * Vertex Color opacity.
*   **`assets/shaders/vfx/distortion.glsl`**:
    *   Screen-space refraction shader. Uses a normal map to offset screen UVs.
*   **`assets/shaders/vfx/holo_blade.glsl`**:
    *   Rim lighting (Fresnel effect).
    *   Scrolling noise for "energy flow".
    *   Additive blending.

### 3.2 Particle Configurations (`assets/data/particles/`)
*   **`blade_ascendant_particles.json`**:
    *   `SwordIntent_Aura`: 1-10 layer logic.
    *   `FlowingThrust_Spark`: Impact sparks.
    *   `InfiniteBlades_Stream`: High density GPU particle stream.
    *   `BladeFormation_Hover`: Floating idle particles.

### 3.3 Textures
*   Use procedural generation (python script) or existing assets for:
    *   `trail_mask.png`: For the sword slash gradient.
    *   `noise_flow.png`: For energy flowing on holo blades.

## 4. Implementation Logic
1.  **Shader Authoring**: Write GLSL files.
2.  **Asset Registration**: Add to `ResourceManager`.
3.  **Visual Verification**: Create a test scene/state (`TestVFXState`) to preview these effects in isolation.

## 5. Dependencies
*   Existing `RenderSystem` and `ShaderManager`.
*   OpenGL 4.3 Compute Shader support (already verified).
