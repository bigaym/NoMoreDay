# Track Specification: Scene & Level Persistence Architecture

## Overview
Currently, NoMoreDay handles levels in a monolithic way within `LevelManager`. This track aims to refactor the level management into a multi-biome, persistent scene system. This will allow players to move between different environments (e.g., Towns and Dungeons) while maintaining player state and correctly managing the lifecycle of world entities.

## Functional Requirements
- **Data-Driven Biomes**: Define environmental profiles (tilesets, monster pools, difficulty scaling) via JSON.
- **Scene Transition Logic**:
    - Smoothly transition between levels (fade out/in).
    - Clear non-persistent entities (monsters, projectiles) while preserving persistent ones (Player, UI components).
- **Portal/Gateway System**:
    - Interactive portals that trigger level loading.
    - Support for "Return to Town" or "Deepen into Dungeon" flows.
- **World State Registry**: A centralized way to track which levels have been visited and their current status (e.g., cleared, active).

## Technical Requirements
- **Entity Lifecycle Management**:
    - Tag entities as `PersistentTag` or `LocalLevelTag`.
    - `SceneManager` handles the destruction of `LocalLevelTag` entities during transition.
- **Async Preparation**: Utilize `Taskflow` to pre-generate the next level's spatial grid and flow field during the transition screen.
- **Coordinate Mapping**: Ensure the player appears at the correct "Entrance" point of the new level.

## Acceptance Criteria
- [ ] Successfully load a "Town" biome and a "Cave" biome from JSON configurations.
- [ ] Player can interact with a portal to switch between Biomes.
- [ ] Combat stats, buffs, and inventory are perfectly preserved after switching.
- [ ] Memory usage remains stable (no entity leaks across level switches).
- [ ] The transition includes a visual fade effect.
