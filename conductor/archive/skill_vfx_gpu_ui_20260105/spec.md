# Specification: Skill System Optimization, UI Polish, and GPU VFX

## 1. Overview
This track focuses on a comprehensive enhancement of the combat experience. It aims to deepen skill mechanics (specifically Sword Intent), polish the user interface for better combat feedback, implement high-performance GPU-driven visual effects, improve skill assignment usability, and add on-screen monster health bars.

## 2. Functional Requirements

### 2.1 Skill Logic & Mechanics
*   **Interaction Logic:** Improve physical interactions such as knockback accuracy and piercing rules for projectiles.
*   **Sword Intent Depth:**
    *   Implement new triggers for gaining Sword Intent.
    *   Add decay rules (if not fully robust) and visual/UI feedback for current stacks.
    *   Ensure "Empowered" states significantly alter skill behavior (beyond simple damage numbers).

### 2.2 User Interface (UI)
*   **Skill Assignment:**
    *   **Skill Panel:** Accessible via the **'S'** key.
    *   **Drag-and-Drop:** Allow players to drag skills from the Skill Panel and drop them into the Skill Bar slots.
    *   **Right-Click Selection:** Right-clicking a slot on the Skill Bar opens a context menu or popup list of available/unlocked skills to assign to that specific slot (inspired by Last Epoch).
*   **Monster Health Bars (New):**
    *   Implement floating health bars above active enemies.
    *   Bars should clearly show current vs. max health.
    *   Integrate buff/debuff icons directly above the health bar.
*   **HUD & Skill Bar:**
    *   Visualize cooldowns with clearer overlay animations.
    *   Show "Charge" counts for multi-charge skills clearly.
    *   Highlight the active skill slot when pressing keys.
*   **Floating Combat Text (FCT):**
    *   Implement animated damage numbers (pop-up, float, fade).
    *   Style differentiation: Critical Hits (larger, shake, orange/gold), Status Effects (colored), Standard Hits (white).
*   **Tooltips & Tree:**
    *   Polish tooltip layout for readability (differentiate flavor text from stats).
    *   Improve the visual hierarchy of the Skill Tree UI.

### 2.3 Visual Effects (GPU Rendering)
*   **GPU Particle Systems:**
    *   Utilize Compute Shaders to render high-density particle effects for skills like "Infinite Blades" and "Sword Array".
    *   Minimize CPU-GPU bandwidth usage for particle updates.
*   **Empowered Visuals:**
    *   Add distinct shader effects (glow, trails, distortion) when a skill is cast with full Sword Intent.
*   **Status Feedback:**
    *   Visual indicators for elemental conversions (e.g., frost trails for Cold skills, sparks for Lightning).
*   **Screen-Space Effects:**
    *   Implement subtle screen shake on heavy hits or critical strikes.
    *   Add chromatic aberration or radial blur for "Ultimate" skill casts.

## 3. Non-Functional Requirements
*   **Performance:** Particle systems must run efficiently on the GPU without stalling the main game loop. Target 60 FPS minimum during heavy combat.
*   **Code Structure:** New rendering logic should be decoupled from core simulation logic where possible (e.g., via `EffectComponent` or `GPUData`).

## 4. Acceptance Criteria
*   [ ] Users can assign skills via Drag-and-Drop from the 'S' panel AND Right-Click Context Menu.
*   [ ] Monster health bars are visible and correctly display HP and status effects.
*   [ ] Sword Intent mechanics feel impactful and are clearly communicated via UI and VFX.
*   [ ] Damage numbers are dynamic, easy to read, and convey crit/status info clearly.
*   [ ] Skill bar provides instant, clear feedback on cooldowns and availability.
*   [ ] High-density particle effects render smoothly using Compute Shaders.
*   [ ] Screen shake and impact effects trigger correctly on appropriate events.
