# Product Guide - NoMoreDay

## Initial Concept
NoMoreDay is a high-performance 2D Diablo-like Roguelite ARPG built with C++20 and ECS. It challenges players to survive a dark fantasy apocalypse where 10,000+ entities clash, featuring user-constructed endgame dimensions and persistent monster evolution.

## Vision Statement: The Infinite Cycle
To create an immersive ARPG experience where the **Roguelite run loop** meets **persistent ARPG accumulation**. Players don't just "beat" the game; they navigate an infinite cycle of rebirth, tailoring their own dungeons, forging heirlooms across lives, and hunting nemeses that learn from their builds.

## Target Audience
- **Hardcore ARPG Fans:** Players who crave complex build systems (Diablo/PoE/Last Epoch) and itemization depth.
- **Roguelite Enthusiasts:** Players who enjoy high-stakes gameplay and procedural unpredictability.

## Core Goals & Values
- **Tag-Driven Ecology:** Everything (skills, items, monsters) is a container of tags. This allows for emergent synergies and streamlined damage calculation.
- **VISCERAL Performance:** Technical excellence is a gameplay feature. High entity counts create "Massive Scale Combat" that remains fluid and responsive.
- **Player-Centric Endgame:** Shifting the power of map generation to the player through the Dimensional Mosaic.

## Key Features

### 1. The Dimensional Mosaic (Endgame Construction)
- **Map Fragments:** Monsters drop Tetris-shaped fragments (Terrain, Affix, Unique).
- **The Loom UI:** A 3x3 grid where players stitch together the next floor. 
- **Resonance:** Placing matching elements (e.g., Fire next to Fire) triggers resonance, multiplying rewards and difficulty.
- **Strategic Risk:** Curse fragments can be used to unlock Tier 7 (God-tier) affixes on connected tiles.

### 2. Persistent Nemesis System
- **Faction Aggro:** Killing monsters of a specific faction (Undead, Void, etc.) increases their hatred.
- **Dynamic Evolution:** At 100% aggro, a Nemesis spawns. It is dynamically generated to **counter the player's build** (e.g., gaining Ice Resist if the player uses Ice skills).
- **Hunter AI:** Nemeses are not static bosses; they actively hunt the player across the floor with dedicated "Red Alert" BGM.

### 3. Infinite Progression: Eternal Nightmare
- **Corruption System:** As players descend deeper into the nightmare, Corruption increases monster stats exponentially while boosting the drop rate of T7 affixes.
- **Heirloom Vault:** Rare Mythic items can be marked as "Heirlooms," allowing them to be inherited by a new character.
- **Dynamic Scaling:** Heirlooms have their stats compressed at low levels, scaling back to full power as the character grows.

### 4. Tag-Driven Combat Engine
- **5-Step Pipeline:** (Base -> Conversion -> Increased -> More -> Settle). Optimized via SIMD (xsimd).
- **Ailment Integration:** Status effects like Bleed, Ignite, and Shock are handled natively by the pipeline based on damage tags.
- **Shadow Echoes:** Blade Ascendants can create "Shadows" that mimic skill execution, supporting complex trigger builds without recursive performance hits.

### 5. Advanced Skill Specialization
- **Concentric Astrolabe:** A massive passive tree for global character stats.
- **Skill Masteries:** 5 active skills can be specialized with their own unique 20-node talent trees, allowing form changes (e.g., Melee to Projectile).

### 6. GPGPU Optimized AI & VFX
- **GPU Flow Fields:** 10,000+ monsters navigate using iterative compute shaders, enabling complex flocking and surrounding behaviors.
- **Ink-Wash Particles:** 200,000+ particles rendered using Indirect Drawing, themed around traditional ink-wash cultivation aesthetics.

### 7. Expanded Monster Archetypes
- **Synergetic AI:** Support (buffing), Assassin (stealth/backstab), and Tank (blocking LOS) archetypes work together.
- **Elite Modifiers:** Components like `SoulLink` (shared health) and `Avenger` (stat gain on ally death) create challenging encounters.

## Build Economy & Crafting
- **Forging Potential:** A high-stakes crafting system where players upgrade, reroll (Chaos), or refine specific affixes until the item's potential is exhausted.
- **Loot Filters:** Built-in support for filtering loot by Tiers, LP (Legendary Potential), and Base types.