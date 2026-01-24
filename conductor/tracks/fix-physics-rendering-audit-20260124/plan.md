# Implementation Plan: Physics & Rendering Fix (Audit 2026-01-24)

## Phase 1: The Integrity (Critical Fixes)
**Objective**: Fix the Logic Regression (ForceFields) and Data Corruption Risk (SSBO Mismatch).
**Estimated Time**: 1.5 Hours

### Task 1.1: Fix SSBO Struct Mismatch
- **Action**: Modify `MDIRenderer.hpp` to pad `GPUInstanceData` to 64 bytes.
- **Validation**: `static_assert(sizeof(GPUInstanceData) == 64)` passes.
- **Risk**: If shaders used 48 bytes explicitly (unlikely for `std430` usually 16-byte aligned), they might need update. Current `physics.compute` uses 64 bytes. `cull.compute` needs verification but usually operates on binding bases.

### Task 1.2: Restore ForceForce Logic
- **Action**: In `GameplayState::UpdatePhysics`, insert `PhysicsSystem::applyForceFields(registry, dt, m_spatialGrid)` before the Taskflow execution.
- **Detail**: This function modifies `Velocity` (accumulator). The subsequent `resolveCollisions` also modifies `Velocity`. Since `applyForceFields` runs **serially** before Taskflow, or as a **predecessor logic**, it is thread-safe (Write Velocity -> Barrier -> Read/Write Velocity).
- **Validation**: Spawn a Vortex (Skill ID associated with Blackhole) and verify enemies are pulled.

## Phase 2: The Cleanup (Code Hygiene)
**Objective**: Remove magic numbers identified in audit.
**Estimated Time**: 0.5 Hours

### Task 2.1: Extract Physics Constants
- **Action**: Define constants in `PhysicsSystem.hpp` or `Common.hpp`.
    - `WALL_REPULSION_FACTOR` (20.0f)
    - `CCD_STEP_SIZE` (10.0f)
    - `ENTITY_DAMPING_FACTOR` (0.92f)
- **Refactor**: Update `PhysicsSystem.cpp` to use these constants.

## Validation Matrix
| Step | Check | Expectation |
|---|---|---|
| Build | Compile Project | No Errors, Static Assert Passes |
| Runtime | Enter Gameplay | No Crash on MDI Render |
| Logic | Use Vortex Skill | Enemies are pulled towards center |
| Audit | Code Review | No magic numbers in `PhysicsSystem.cpp` |
