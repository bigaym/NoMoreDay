# Plan: Scene & Level Persistence Architecture

## Phase 1: Data-Driven Biomes & LevelManager Refactor
- [x] Task: Define `BiomeConfig` struct and `BiomeRegistry` to load environment data from `assets/data/biomes.json`. (e57d6b7)
- [x] Task: Update `LevelManager` to accept a `BiomeConfig` and generate levels based on biome-specific tiles and spawn rules. (b9d917d)
- [x] Task: Create `LocalLevelTag` and `PersistentTag` components to manage entity lifecycles. (b9d917d)
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Data-Driven Biomes' (Protocol in workflow.md)

## Phase 2: Portal System & Transition Logic
- [ ] Task: Implement `PortalComponent` (target biome, destination entrance ID).
- [ ] Task: Implement `PortalSystem` to handle interaction and trigger the transition.
- [ ] Task: Create `SceneManager` to orchestrate the "Save Current -> Clear Local -> Load New -> Spawn Player" flow.
- [ ] Task: Add a visual `FadeOverlay` to the `UISystem` for transitions.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Portal System & Transition Logic' (Protocol in workflow.md)

## Phase 3: Integration & Persistence Test
- [ ] Task: Implement a "Town" biome (safe zone, no monsters).
- [ ] Task: Create a test case: Enter Cave -> Gain Buff -> Return to Town -> Verify Buff still exists.
- [ ] Task: Create a test case: Kill Monsters in Cave -> Return to Town -> Enter Cave again -> Verify monsters are refreshed (or state restored depending on design).
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Integration & Persistence Test' (Protocol in workflow.md)
