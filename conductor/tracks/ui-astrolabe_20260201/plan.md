# Implementation Plan: Void Astrolabe

**Track ID:** ui-astrolabe_20260201
**Spec:** [spec.md](./spec.md)
**Created:** 2026-02-01
**Status:** [ ] Not Started

## Overview
We will implement the Astrolabe in three layers: Data (Model), View (Renderer), and Interaction (Controller).

## Phase 1: Data Structure & Layout
Define the schema for constellations and stars, decoupling them from the old grid logic.

### Tasks
- [ ] Task 1.1: Define `Constellation` and `StarNode` structs in `src/game/data/TalentData.hpp`.
- [ ] Task 1.2: Implement `TalentLoader` to load constellation data from JSON (or hardcoded builder for now).
- [ ] Task 1.3: Create a default "Void Map" data set with at least 3 constellations.

### Verification
- [ ] `NoMoreDayTests` passes data loading checks.

## Phase 2: Rendering System
Implement the visual layer using shaders and instancing.

### Tasks
- [ ] Task 2.1: Write `assets/shaders/void_nebula.fs` for the background.
- [ ] Task 2.2: Implement `AstrolabeRenderer::DrawBackground` using the shader.
- [ ] Task 2.3: Implement `AstrolabeRenderer::DrawStars` using `rlgl` batching.
- [ ] Task 2.4: Implement `AstrolabeRenderer::DrawConnections` using thick, glowing lines.

### Verification
- [ ] Visual inspection: Background flows, stars render at correct positions.

## Phase 3: Interaction & Integration
Connect input to the camera and clicks to the logic.

### Tasks
- [ ] Task 3.1: Implement Pan/Zoom logic using Raylib's `Camera2D` adapted for UI.
- [ ] Task 3.2: Implement `HitTest` logic for transforming mouse screen coords to world coords.
- [ ] Task 3.3: Hook up `StatsSystem` to the new node IDs.

### Verification
- [ ] Can pan/zoom smoothly.
- [ ] Clicking a star allocates a point and updates stats.

## Final Verification
- [ ] All acceptance criteria met.
