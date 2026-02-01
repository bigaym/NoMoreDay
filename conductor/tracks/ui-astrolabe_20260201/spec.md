# Specification: Void Astrolabe (Global Passive System)

**Track ID:** ui-astrolabe_20260201
**Type:** Feature
**Created:** 2026-02-01
**Status:** Draft

## Summary
Implement a Grim Dawn-inspired "Void Astrolabe" UI for the global passive talent system. This involves an infinite canvas, star-map aesthetics, and a graph-based data structure that supports arbitrary positioning and constellation groupings.

## Context
The current passive system is a placeholder grid. We need a visual overhaul to match the "Xianxia/Void" theme. The system must support high-performance rendering of hundreds of star nodes with shader-based backgrounds.

## User Story
As a player, I want to explore a vast, mysterious star map to allocate my passive points, so that I feel a sense of progression and discovery in the cultivation path.

## Acceptance Criteria
- [ ] **Data Structure**: `ConstellationData` and `StarNode` support float positions and grouping.
- [ ] **Visuals**: Shader-based "Void" background with parallax stars and nebulae.
- [ ] **Interaction**: Infinite panning and smooth zooming (0.5x to 2.0x).
- [ ] **Rendering**: GPU-accelerated batch rendering for star nodes (avoiding thousands of `DrawCircle` calls).
- [ ] **Integration**: Connects to the existing `StatsSystem` to apply modifiers.

## Dependencies
- `UIRenderer` (for batching primitives)
- `AssetLoadingSystem` (for star textures/shaders)

## Out of Scope
- New gameplay mechanics (this track is UI/UX and Data Structure only).
- Sound effects (separate track).

## Technical Notes
- Use `rlgl` for batching star quads.
- Use a dedicated `Camera2D` for the UI canvas logic.
