# Product Guidelines - NoMoreDay

## Narrative Tone
- **Atmosphere:** A synthesis of **Bleak & Oppressive** and **Gothic & Mysterious**. The world should feel heavy and hopeless, but layered with ancient secrets and eldritch horrors.
- **Theme:** **Otherworldly Fantasy (Eldritch).** The environments and procedural generation should reflect not just a ruined world, but one invaded by alien, incomprehensible dimensions.

## Visual Identity
- **Style:** **High-Fidelity Pixel Art.** A retro-modern aesthetic that supports the dark fantasy theme.
- **Pipeline:** **AI-Driven Asset Generation.** Heavily utilize the `scripts/asset_gen.py` pipeline to generate the massive volume of 2D assets required for varied environments and enemies.

## Design Pillars
- **Gameplay First (Depth & Variety):**
    - **Deep Builds:** Prioritize complex interactions between Equipment Affixes, Monster Modifiers, and Passive Skills.
    - **Variety:** Ensure a vast pool of affixes (prefixes/suffixes) to drive the loot hunt and build diversity.
- **Technical Excellence (Performance & Safety):**
    - **Compile-Time Optimization:** Leverage C++20 features (Concepts, Templates, `constexpr`) to shift logic to compile time wherever possible.
    - **Zero-Cost Abstractions:** Ensure runtime performance is not compromised by architectural choices.
    - **Memory Safety:** Strict resource management (RAII) to guarantee zero memory leaks, crucial for long play sessions and high entity counts.

## User Experience (UX) Principles
- **Clarity in Chaos:** Visual effects must be distinct enough to be readable when 10,000 units are on screen.
- **Responsive Controls:** Input must be instant and precise (Twin-Stick) to reward player skill.
