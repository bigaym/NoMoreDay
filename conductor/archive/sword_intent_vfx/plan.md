# Track: Sword Intent System Visual Implementation

**Goal**: Implement comprehensive visual feedback for the Blade Ascendant's "Sword Intent" (剑意) system, transforming it from a numerical stat into a visceral, screen-shaking visual experience as per the updated [BladeAscendant_VFX_Design](../../设计文档/特效和UI/BladeAscendant_VFX_Design.md).

## Context
The core Blade Ascendant skills are integrated, but the "Sword Intent" mechanic lacks visual weight. We need to implement the detailed 0-10 stack progression, the "Human-Sword Unity" (人剑合一) overload state, and the empowered skill feedback.

## Plan

### Phase 1: Asset Generation & Preparation
- [x] **Texture Generation**:
    - [x] `ui_sword_icon.png`: Create sprite for the 10-stack UI indicator.
    - [x] `vfx_ink_splatter.png`: Create alpha mask for burst impacts.
    - [x] `vfx_aura_noise.png`: Noise texture for the character aura.
- [x] **Shader Implementation**:
    - [x] `vfx_aura.frag`: Implement rim lighting and scrolling noise for the character aura.
    - [x] Update `vfx_distortion.frag` to support a fullscreen vignette/distortion effect driven by uniform.

### Phase 2: UI Implementation
- [x] **HUD Update**:
    - [x] Create `SwordIntentWidget` class.
    - [x] Render 10 micro-sword icons.
    - [x] Implement logic to light up icons based on current `SwordIntentComponent` stacks.
    - [x] Add animation (scale/glow) when a stack is gained.

### Phase 3: Visual Feedback System (The "Juice")
- [x] **Aura System**:
    - [x] Integrate `vfx_aura.frag` into the entity rendering pipeline (via `SwordIntentVisualSystem`).
    - [x] Link aura intensity/color to Sword Intent stacks (1-3: faint ink, 4-7: rising particles, 8-9: unstable glitch).
- [x] **Overload State (10 Stacks)**:
    - [x] Implement `SwordPressureSystem` (Visual part integrated in `SwordIntentVisualSystem`).
    - [x] Trigger `vfx_circle_shockwave` on reaching 10 stacks (handled via ParticleSystem).
    - [x] Apply fullscreen vignette and slight distortion (vignette shader support added, applied via Aura intensity boost).
    - [x] Enable "Rim Light + Bloom" on the character sprite (Aura shader).

### Phase 4: Empowered Skill Impact
- [x] **Event Hook**:
    - [x] Listen for `SkillCastEvent` where `consumed_sword_intent` is true (handled in `SkillSystem`).
- [x] **Visuals**:
    - [x] Trigger "Reverse Shockwave" (suction effect) on cast.
    - [x] Spawn `vfx_ink_splatter` particles on impact.
    - [x] Apply a brief "Inverted Color" post-processing effect for 0.1s on impact (optional, replaced with high contrast ink burst).

## References
- Design Doc: `设计文档/特效和UI/BladeAscendant_VFX_Design.md`
- Codebase: `src/game/components/Combat.hpp` (SwordIntentComponent)
