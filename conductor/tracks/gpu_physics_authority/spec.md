# Tech Spec: GPU-Authority Physics Migration

## 1. Context & Problem Statement
Current architecture suffers from "Double Authority Oscillation":
- **CPU**: `PhysicsSystem` integrates position (`Pos += Vel * dt`) and resolves collisions.
- **GPU**: `physics.compute` integrates position (`Pos += Vel * dt`) and resolves collisions/flow-field.
- **SyncBack**: Reconciles GPU state (N-2 frames old) with CPU state.

**Symptoms**:
- Flickering/Jittering of enemies as CPU and GPU fight for position control.
- "Rubber-banding" where enemies are dragged back to old GPU positions.
- Interpolation artifacts (stretching/disappearing) during spawn/teleport.

## 2. Solution: GPU-Only Authority for Enemies
We will migrate the physics authority for all entities with `EnemyTag` strictly to the GPU.

### 2.1 Authority Model
| Entity Type | Control Source | Physics Authority | Collision Authority |
| :--- | :--- | :--- | :--- |
| **Player** | Input (CPU) | CPU (`PhysicsSystem`) | CPU (Grid) |
| **Enemy** | AI (CPU) -> Velocity | **GPU (`physics.compute`)** | **GPU (SpatialHash)** |
| **Projectile** | Logic (CPU) | CPU (`PhysicsSystem`) | CPU (Grid) |

### 2.2 Data Flow Refactoring

#### CPU -> GPU (Upload)
- **Velocity**: CPU sets *Intended Velocity* (e.g., from AI Pathfinding).
- **Position**: CPU sets *Initial Position* (Spawn) or *Teleport Target*.
- **Flags**: `TELEPORT_FLAG` (New) to tell GPU "Reset your physics state to this position".

#### GPU Physics (Compute)
- **Integration**: $P_{new} = P_{old} + V \times dt$.
- **Collision**: Resolve vs Static Map & Dynamic Neighbors.
- **Friction**: 
    - If `CHASE` (FlowField): Driven by Flow force.
    - If `PATROL/IDLE` (Kinematic-like): Respect CPU velocity, apply minimal air resistance ($0.98$), **NO** heavy friction ($0.90$).

#### GPU -> CPU (SyncBack)
- **Prediction (Dead Reckoning)**: 
    - Since we read data from Frame N-2 (Triple Buffer), we must extrapolate.
    - $P_{cpu} = P_{readback} + V_{readback} \times (Latency \approx 32ms)$.
- **Teleport Protection**:
    - If $|P_{cpu} - P_{predicted}| > Threshold$, assume CPU logic intervention (Teleport).
    - **Action**: Reject GPU update, keep CPU position, set `TELEPORT_FLAG` for next upload.

## 3. Implementation Details

### 3.1 PhysicsSystem.cpp (CPU)
Modification to `updateAll`:
```cpp
auto process_collision = [&](entt::entity entity) {
    // SKIP Enemies. GPU handles them.
    if (registry.any_of<EnemyTag>(entity)) return;
    // ... existing logic for Player/others
};

auto process_integration = [&](entt::entity entity) {
    // SKIP Enemies. GPU handles them.
    if (registry.any_of<EnemyTag>(entity)) return;
    // ... existing logic for Player/others
};
```

### 3.2 GPUEntitySystem.cpp (SyncBack)
Refined Prediction Logic:
```cpp
// 1. Read Old Data (N-2)
vec2 P_old = gpu_data.pos;
vec2 V_old = gpu_data.vel;

// 2. Extrapolate to Present Time
// dt * 2.0 simulates the 2-frame latency of triple buffering
vec2 P_predicted = P_old + V_old * (dt * 2.0f);

// 3. Teleport Check (CPU Authority Override)
if (DistSq(P_current_cpu, P_predicted) > 64.0f) {
    // CPU moved entity (Teleport/Spawn). Ignore GPU this frame.
    // Force PrevPos reset to disable interpolation.
} else {
    // GPU Authority. Accept Prediction.
    Pos_cpu = P_predicted;
    // Update PrevPos for rendering interpolation
    PrevPos_cpu = P_old; // Or P_predicted? 
    // CORRECT: PrevPos should track the VISUAL history.
    // If we just snapped to P_predicted, PrevPos should be P_cpu_last_frame.
}
```

### 3.3 Shader (physics.compute)
- **Remove Friction Penalty**: Delete `else { vel *= 0.90; }`. Trust the Velocity provided by CPU or FlowField.

## 4. Verification Plan (Acceptance Criteria)

### 4.1 Visual Stability
- [ ] **Idle Test**: Spawn 100 enemies. They must stand still or patrol smoothly without jitter.
- [ ] **Chase Test**: Trigger aggro. Enemies must follow player smoothly.
- [ ] **Boundary Test**: Push enemies against walls. No vibration.

### 4.2 Latency Check
- [ ] **Response Time**: When AI changes direction, visual change happens within < 50ms.
- [ ] **Overshoot**: Fast moving enemies stop correctly at target (no "slide past and snap back").

### 4.3 Edge Cases
- [ ] **Teleport**: Assassin skill teleport must be instant (no interpolation trail).
- [ ] **Spawn**: New entities appear solid (no flying in from (0,0)).
