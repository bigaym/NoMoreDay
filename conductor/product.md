# Product Guide - NoMoreDay

## Initial Concept
NoMoreDay is a high-performance 2D Diablo-like Roguelite ARPG built with C++20 and ECS, designed to handle 10,000+ entities simultaneously in a dark fantasy apocalypse.

## Vision Statement
To create an immersive, performance-driven ARPG experience where players face overwhelming odds, utilizing deep character customization and visceral combat to survive an apocalyptic world.

## Target Audience
- **Hardcore ARPG Fans:** Players who crave complex build systems, itemization depth, and long-term character progression similar to Diablo or Path of Exile.

## Core Goals & Values
- **Deep Itemization:** Focus on a robust gear system where every drop matters, providing meaningful choices through prefixes, suffixes, and unique item identities.
- **Visceral Combat:** Ensure high-impact feedback for every action. Combat should feel weighted and satisfying, even when fighting thousands of enemies.
- **Extreme Scalability:** (Inferred) Leverage ECS and DOD to maintain technical excellence without sacrificing gameplay depth.

## Key Features

- **Complex Skill Trees:** Extensive and interlocking passive/active skill trees that allow players to fine-tune their combat style and complement their gear choices.

- **Active Skill Infrastructure:** A data-driven system supporting modular skill execution with a 4-state machine (Preparing, Casting, Settle, End). Features include skill registries, tag-based scaling, cooldown management, and resource (Mana) consumption.

- **Skill Specialization System:** A deep customization system allowing players to specialize in up to 5 active skills. Each specialized skill features its own unique talent tree, enabling players to drastically alter skill behavior, damage types, and mechanical properties through a dedicated "Skill Management" interface (Hotkey: S).

- **Concentric Astrolabe System:** A data-driven passive talent tree with a concentric layout, allowing for deep character specialization through Minor, Major, and Keystone nodes. Features an interactive, zoomable UI overlay with dynamic connection visuals, planning mode, and activation logic. Includes class-specific keystones like "Sword Heart" for Blade Ascendants.

- **Blade Ascendant Mechanics:** Specialized class logic for sword-wielding characters, including:

    - **Sword Intent:** A stackable buff system that rewards continuous melee aggression.

    - **Shadow Echo System:** A lightweight "Ghost" mechanism that creates ephemeral shadows to echo skill executions with snapshot-based attributes and independent timers. Includes mechanical safeguards such as tag-based exclusions (Movement, Buff, Channeled), resource costs (50% Mana), damage penalties (50% reduction), and internal cooldowns (3s) to ensure mechanical balance.

    - **Empowered Skill Hooks:** A robust pre/post-cast hook system that enables dynamic skill behavior, such as consuming 10 stacks of Sword Intent to trigger "Empowered" versions of skills with increased damage or modified properties.

    - **Movement Stances:** Implementation of continuous movement states like "Sword Riding," which grant significant mobility bonuses (e.g., +100% speed) after a 2-second wind-up period, cancelled by taking damage or stopping.

    - **Sword Heart Keystone:** A powerful passive that drastically increases weapon damage and grants block chance when wielding a sword single-handedly.

- **Multi-Biome Persistent World:** A data-driven level management system that allows seamless transitions between different environments (e.g., Safe Towns and Hostile Dungeons) via an interactive portal system. Features include biome-specific generation rules, entity lifecycle persistence, and async level preparation for smooth loading.

- **Procedural Map Generation:** A dynamic dungeon system utilizing cellular automata and Perlin noise to ensure that no two runs feel the same, promoting infinite replayability.

- **Forging & Crafting Interface:** A dedicated UI for modifying items using the "Forging Potential" system. Players can upgrade specific affixes to higher tiers, add new prefixes/suffixes to items with open slots, and use "Chaos" or "Refine" actions to reroll affix types or values, providing deep control over equipment progression.

- **Massive Scale Combat:** A high-performance, tag-driven damage engine supporting a 5-step calculation pipeline (Base, Conversion, Increased, More, Settle). It handles complex damage conversions (e.g., Physical to Fire) and applies multi-layered modifiers for Physical, Elemental, and Eldritch damage types, optimized for 10,000+ entities with zero-allocation update loops.

- **GPU-Driven AI Navigation:** A high-performance pathfinding system utilizing GPU flow fields to support 10,000+ entities. Features a 256x256 local "rolling grid" that calculates optimal steering vectors around static and dynamic obstacles using iterative compute shaders, enabling complex crowd behavior like flanking and surrounding with minimal CPU overhead.

- **Deep Build Economy:** A focus on crafting, rune words, and forging potential to allow for endgame "god-tier" character creation.

- **Dynamic UI & Inventory:** A highly responsive, zero-allocation UI system for managing complex equipment (11 slots) and unlimited materials, with full support for Chinese localization and dynamic text scaling.


