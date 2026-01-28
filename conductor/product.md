# Product Guide - NoMoreDay

## Initial Concept
NoMoreDay is a high-performance 2D Diablo-like Roguelite ARPG built with C++20 and ECS. It challenges players to survive a dark fantasy apocalypse where 10,000+ entities clash, featuring user-constructed endgame dimensions and persistent monster evolution.

## Vision Statement: The Infinite Cycle
To create an immersive ARPG experience where the **Roguelite run loop** meets **persistent ARPG accumulation**. The project has moved past its architectural foundation, achieving **Protocol-Compliant Integrity (DOD/ECS)**, and is now in the **Content & Polish Phase (V1.0 Readiness)**, with a fully functional high-performance engine supporting massive-scale combat and complex build depth.

## Target Audience
- **Hardcore ARPG Fans:** Players who crave complex build systems (Diablo/PoE/Last Epoch) and itemization depth.
- **Roguelite Enthusiasts:** Players who enjoy high-stakes gameplay and procedural unpredictability.

## Core Goals & Values
- **Tag-Driven Ecology:** Everything (skills, items, monsters) is a container of tags. This allows for emergent synergies and streamlined damage calculation. [FULLY IMPLEMENTED]
- **VISCERAL Performance:** Technical excellence is a gameplay feature. Leveraging **Multi-Draw Indirect (MDI)**, **GPU Compute**, and **Lockless Staging**, the engine maintains fluid 60+ FPS even with 10,000+ units. [PROVEN]
- **Scalable Concurrency:** The engine is built for multi-core scaling. Featuring **thread-local RNG**, **Shared-Mutex resource caching**, and **Zero-Lock action merging**, ensuring thread safety without sacrificing frame-time stability. [HARDENED 2026-01-28]
- **Player-Centric Endgame:** Shifting the power of map generation to the player through the Dimensional Mosaic. [FUNCTIONAL]

## Key Features

### 1. The Dimensional Mosaic (Implemented)
- **Map Fragments:** Monsters drop Tetris-shaped fragments (Terrain, Affix, Unique).
- **The Loom UI:** A 3x3 grid where players stitch together the next floor. 
- **Resonance:** Placing matching elements (e.g., Fire next to Fire) triggers resonance, multiplying rewards and difficulty.
- **Strategic Risk:** Curse fragments can be used to unlock Tier 7 (God-tier) affixes on connected tiles.

### 2. Persistent Nemesis System (Implemented)
- **Faction Aggro:** Killing monsters of a specific faction (Undead, Void, etc.) increases their hatred.
- **Dynamic Evolution:** At 100% aggro, a Nemesis spawns. It is dynamically generated to **counter the player's build** (e.g., gaining Ice Resist if the player uses Ice skills).
- **Hunter AI:** Nemeses are not static bosses; they actively hunt the player across the floor with dedicated "Red Alert" BGM.

### 3. Infinite Progression: Eternal Nightmare
- **Corruption System:** As players descend deeper into the nightmare, Corruption increases monster stats exponentially while boosting the drop rate of T7 affixes.
- **Heirloom Vault:** Rare Mythic items can be marked as "Heirlooms," allowing them to be inherited by a new character via the account-wide persistence layer.
- **Dynamic Scaling:** Heirlooms have their stats compressed at low levels, scaling back to full power as the character grows.

### 4. Tag-Driven Combat Engine (Implemented)
- **5-Step Pipeline:** (Base -> Conversion -> Increased -> More -> Settle). Optimized via SIMD (xsimd) and branchless logic.
- **Concurrent Simulation:** Physics and projectiles utilize a **Chunk-based Zero-Lock Merge** pattern, enabling linear performance scaling with thread count.
- **Ailment Integration:** Status effects like Bleed, Ignite, and Shock are handled natively by the pipeline based on damage tags.
- **Shadow Echoes:** Support for complex trigger builds (e.g., shadows mimicking skills) without recursive performance hits.

### 5. Advanced Skill Specialization
- **Concentric Astrolabe:** A massive passive tree for global character stats with 500+ nodes.
- **Skill Masteries:** Active skills have unique 20-node talent trees. Blade Ascendant (剑修) is fully implemented; further classes in development.

### 6. GPGPU Optimized AI & VFX (Implemented)
- **GPU Flow Fields:** 10,000+ monsters navigate using iterative compute shaders, enabling complex flocking behaviors.
- **Ink-Wash Particles:** 200,000+ particles rendered using Indirect Drawing and Triple Buffering. Powered by a **Lockless Thread-Local Staging** architecture, allowing high-frequency emission across all CPU cores.

### 7. Expanded Monster Archetypes
- **Synergetic AI:** Support (buffing), Assassin (stealth/backstab), and Tank (blocking LOS) archetypes work together.
- **Elite Modifiers:** 50+ unique modifiers including `SoulLink` and `Avenger`.

## Build Economy & Crafting (Implemented)
- **Forging Potential:** A high-stakes crafting system where players upgrade or refine specific affixes until potential is exhausted.
- **Legendary Merging:** Fuse Mythic items with Exalted fodder via the **Legendary Potential (LP)** system.
- **Runewords:** 33 unique runes corresponding to the "Thirty-Three Heavens," supporting D2-style sequencial socketing.
- **Loot Filters:** Built-in XML/JSON based filtering for Tiers, LP, and Base types.