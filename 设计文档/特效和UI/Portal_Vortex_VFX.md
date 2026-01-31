# Portal Vortex VFX Design

## 1. Overview
Replace the primitive `DrawRing` based portal rendering with a high-fidelity, shader-driven Vortex Effect. This effect serves as the visual anchor for all inter-scene transitions (Town, Dungeon, Dimensional, Boss).

## 2. Visual Specification
- **Shape**: Vertical Oval (Aspect Ratio 3:5).
- **Motion**: Inward spiraling vortex (Black Hole accretion disk style).
- **Core**: Bright, singularity-like center.
- **Edge**: Soft fade-out with energy tendrils.
- **Distortion**: UV warping based on polar coordinates and noise texture.

## 3. Implementation Details

### 3.1 Shader: `portal_vortex.fs`
**Inputs (Uniforms):**
- `sampler2D uTexture`: Base noise texture (`vfx_energy_noise.png`).
- `vec4 uColor`: Tint color (HDR enabled).
- `float uTime`: Global time for animation.
- `float uSwirlStrength`: Intensity of the spiral distortion (Default: 3.0).
- `float uCoreSize`: Radius of the dark/bright center (Default: 0.15).

**Algorithm:**
1. **UV Mapping**: Convert UV (0..1) to Centered (-1..1).
2. **Aspect Correction**: Scale Y by 0.6 (3:5 ratio) to work in circular logic, then stretch back for rendering.
3. **Polar Conversion**: Calculate `radius` (r) and `angle` (theta).
4. **Swirl Math**: `theta += uSwirlStrength / (r + 0.1) * sin(uTime)`; // Non-linear twist
5. **Texture Sampling**: Sample noise texture with `vec2(theta / TWO_PI, r - uTime * speed)`.
6. **Masking**:
   - **Outer Mask**: Smoothstep falloff at r=0.5.
   - **Core Mask**: Inverse falloff at r=uCoreSize.
7. **Color Grading**: `finalColor = texture * uColor * (1.0 + r * 2.0)` (Brighter edges).

### 3.2 Colors (Tint)
- **Town Portal**: `GOLD` (R=1.0, G=0.8, B=0.2)
- **Next Level**: `CYAN` (R=0.2, G=0.8, B=1.0)
- **Dimensional Gate**: `VOID_PURPLE` (R=0.6, G=0.0, B=1.0)
- **Boss Portal**: `BLOOD_RED` (R=1.0, G=0.1, B=0.1)

## 4. Integration Plan
1. **Resources**: Load `portal_vortex.fs` and `vfx_energy_noise.png` in `LevelManager` or `PortalSystem`.
2. **Rendering**:
   - In `PortalSystem::Render`, replace `DrawRing` loop.
   - Use `BeginShaderMode(vortexShader)`.
   - Draw a simple `DrawTexturePro` (using a blank white texture or the noise texture itself) on a Quad.
   - `EndShaderMode()`.
3. **Optimizations**:
   - Batch rendering if possible (though portals are few).
   - Use `uTime` from `GetTime()`.

## 5. Assets
- **Shader**: `assets/shaders/vfx/portal_vortex.fs`
- **Texture**: `assets/textures/vfx/vfx_energy_noise.png` (Existing)
