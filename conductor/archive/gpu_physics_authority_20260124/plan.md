# Plan: GPU-Authority Physics Migration

## Phase 1: Core Architecture Refactor (The Authority Shift)
**Goal**: Establish GPU as the sole physics integrator for Enemies and fix the "Double Authority" conflict.

| ID | Task | Description | Est. Time | Priority |
| :--- | :--- | :--- | :--- | :--- |
| **1.1** | **Disable CPU Physics** | Modify `PhysicsSystem::updateAll` to strictly skip integration and collision for `EnemyTag`. | 0.5h | P0 |
| **1.2** | **Shader Friction Fix** | Remove the excessive `vel *= 0.90` damping in `physics.compute` for non-flow states. | 0.5h | P0 |
| **1.3** | **SyncBack Prediction** | Implement `P_now = P_old + V * 2dt` extrapolation in `GPUEntitySystem::SyncBack`. | 1.0h | P0 |
| **1.4** | **Teleport Protocol** | Finalize the `prevPos` reset logic and Velocity zeroing on spawn/teleport in `GPUEntitySystem::Update`. | 0.5h | P1 |

**Verification Target**: 
- No visual flickering for moving enemies.
- No "drag back" when AI sets velocity.

## Phase 2: Smoothness & Edge Cases (The Butter)
**Goal**: Polish the visual experience and handle high-speed/collision edge cases.

| ID | Task | Description | Est. Time | Priority |
| :--- | :--- | :--- | :--- | :--- |
| **2.1** | **Render Interpolation** | Verify `mix(prev, curr, alpha)` in `entity.vert`. Ensure `PrevPosition` is updated correctly in `SyncBack`. | 1.0h | P2 |
| **2.2** | **Boundary Clamping** | Ensure Extrapolation in `SyncBack` does not push entities outside map boundaries (0,0) -> (W,H). | 0.5h | P2 |
| **2.3** | **Kinematic Override** | Verify `GPU_ENTITY_FLAG_KINEMATIC` is correctly respected for Bosses or scripted events. | 1.0h | P3 |

## Risks
- **Overshoot**: Simple linear extrapolation might push entities into walls if they were about to collide in the GPU simulation.
    - *Mitigation*: Accept minor visual clipping for Phase 1. Add Raycast check in Phase 3 if needed.
- **Latency**: 32ms delay might be noticeable for "twitch" reaction (e.g., knockback).
    - *Mitigation*: Extrapolation hides the visual delay. Physics interactions (Hitboxes) still happen on CPU using the extrapolated position, so logic feels responsive.
