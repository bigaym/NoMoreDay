# Product Guidelines - NoMoreDay

## Narrative Tone
- **Atmosphere:** A synthesis of **Bleak & Oppressive** and **Gothic & Mysterious**. The world should feel heavy and hopeless, but layered with ancient secrets and eldritch horrors.
- **Theme:** **Otherworldly Fantasy (Eldritch Cultivation).** The environments and procedural generation should reflect a ruined world invaded by alien dimensions, where "Cultivation" is a desperate struggle against the void.

## Visual Identity
- **Style:** **High-Fidelity Pixel Art.** A retro-modern aesthetic that supports the dark fantasy theme.
- **VFX Language:** **Ink-Wash Cultivation Style (水墨修仙风).** Use fluid, high-performance GPU particles to create an ethereal yet visceral combat experience, blending traditional ink aesthetics with modern bloom and distortion.
- **Pipeline:** **AI-Driven Asset Generation.** Heavily utilize the `scripts/asset_gen.py` pipeline to generate the massive volume of 2D assets required for varied environments and enemies.

## Design Pillars
- **Gameplay First (Depth & Variety):**
    - **Deep Builds:** Prioritize complex interactions between Equipment Affixes, Monster Modifiers, and Passive Skills via the **Tag-Driven Engine**.
    - **User-Defined Endgames:** Move beyond static maps. Players build their own challenges through the **Dimensional Mosaic** system.
    - **Persistent Rivalry:** The **Nemesis System** ensures that failure has consequences and victories feel personal across multiple runs.
- **Technical Excellence (Performance & GPGPU):**
    - **Extreme Scalability:** Leverage ECS (EnTT) and Taskflow to simulate 10,000+ active entities.
    - **Compute-First Logic:** Offload heavy lifting (Physics, Particles, Pathfinding Flow-Fields) to **OpenGL 4.3 Compute Shaders** to free up CPU for complex logic.
    - **Memory Safety:** Strict resource management (RAII) and zero-allocation frame loops.

## User Experience (UX) Principles
- **Clarity in Chaos:** Even with 10,000 units, use color-coding (Tag-based) and distinct VFX silhouettes to maintain readability.
- **Seamless Flow:** Minimal loading times via async level preparation and bi-directional portals (Town Portal).
- **Responsive Controls:** Instant and precise Twin-Stick/WASD input to reward player skill.