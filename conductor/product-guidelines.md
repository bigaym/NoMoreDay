# Product Guidelines - NoMoreDay

## Narrative Tone
- **Atmosphere:** A synthesis of **Bleak & Oppressive** and **Gothic & Mysterious**. The world should feel heavy and hopeless, but layered with ancient secrets and eldritch horrors.
- **Theme:** **Otherworldly Fantasy (Eldritch Cultivation).** The environments and procedural generation should reflect a ruined world invaded by alien dimensions, where "Cultivation" is a desperate struggle against the void.

## Visual Identity
- **Style:** **High-Fidelity Pixel Art.** A retro-modern aesthetic that supports the dark fantasy theme.
- **VFX Language:** **Ink-Wash Cultivation Style (水墨修仙风).** Use fluid, high-performance GPU particles to create an ethereal yet visceral combat experience. The implementation uses **Indirect Drawing** and **Triple Buffering** to ensure zero-stutter rendering of 200k+ particles.
- **Pipeline:** **AI-Driven Asset Generation.** Successfully utilizing the `scripts/asset_gen.py` pipeline (ComfyUI/SDXL based) to generate consistent 2D assets, enabling rapid iteration of monster variants and environment props.

## Design Pillars
- **Gameplay First (Depth & Variety):**
    - **Deep Builds:** Complex interactions between Equipment Affixes (T1-T7), Monster Modifiers, and the **5-Step Damage Pipeline**.
    - **User-Defined Endgames:** Dimensional Mosaic system is functional, allowing tactical map construction.
    - **Persistent Rivalry:** Nemesis System with **build-adaptive evolution** logic is implemented.
- **Technical Excellence (Performance & GPGPU):**
    - **Extreme Scalability:** ECS (EnTT) + Taskflow DAG scheduler simulating 10,000+ active entities with SIMD optimization.
    - **Compute-First Logic:** Physics, GPU Flow Fields, and Fog of War are fully offloaded to **OpenGL 4.3 Compute Shaders**.
    - **Rendering Mastery:** Using **Multi-Draw Indirect (MDI)** to minimize Draw Calls, keeping the main thread focused on gameplay logic.
    - **Memory Safety:** Strict RAII and zero-allocation frame loops verified.

## User Experience (UX) Principles
- **Clarity in Chaos:** Even with 10,000 units, use color-coding (Tag-based) and distinct VFX silhouettes to maintain readability.
- **Seamless Flow:** Minimal loading times via async level preparation and bi-directional portals (Town Portal).
- **Responsive Controls:** Instant and precise Twin-Stick/WASD input to reward player skill.