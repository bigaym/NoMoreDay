# Specification: GPU Flow Field Pathfinding Integration

## Overview
Implement a high-performance GPU-driven flow field pathfinding system to support seamless navigation for 10,000+ entities. This system will offload the expensive Dijkstra/A* pathing and vector field generation to Compute Shaders, providing AI entities with optimal steering vectors towards the player while handling dynamic obstacles and crowd congestion.

## Functional Requirements

### 1. GPU Grid Management
- **Rolling Grid:** Implement a 256x256 local grid that centers and follows the player's position (Snapped to tile size for stability).
- **Cell Specification:** Each cell represents a 10x10 pixel area (matching MapSystem tiles).
- **Data Buffers:** 
    - **Cost Buffer:** Stores static traversal costs (SSBO).
    - **Density Buffer:** Stores dynamic crowd density (SSBO, updated via grid_count).
    - **Integration Field:** Stores cumulative costs (SSBO).
    - **Flow Field:** Stores final 2D direction vectors (SSBO).

### 2. Cost Calculation Logic
- **Static Terrain:** Read from the existing map collision data to mark walls and pits as non-traversable (infinite cost).
- **Dynamic Objects:** Update the cost field when environmental objects (e.g., barrels, gates) are created or destroyed.
- **Crowd Density:** Dynamically increase cell costs based on the number of enemies present in that cell to encourage flanking/surrounding behavior rather than clumping.

### 3. GPU Compute Pipeline
- **Step 1 (Cost Update):** A compute shader to populate the Cost Field from CPU-provided obstacle data and GPU-side entity positions.
- **Step 2 (Integration):** A wavefront-based or iterative jump-flooding compute shader to calculate the distance field (cumulative cost) from the player.
- **Step 3 (Flow Generation):** A compute shader to calculate gradients from the integration field and output a 2D vector field.

### 4. CPU-GPU Integration (Steering)
- **Vector Sampling:** AI entities in the `AISystem` will sample the flow field texture/buffer based on their world position.
- **Steering Blending:** The sampled vector will be applied as a "Steering Force" (Desired Velocity), allowing the existing CPU-side collision avoidance and physics to refine the final movement.

## Non-Functional Requirements
- **Performance:** Compute shader execution for a 256x256 grid must complete within 1-2ms on mid-range GPUs.
- **Memory Efficiency:** Use compact data formats (e.g., `half` or `uint8` for costs where applicable) to minimize VRAM bandwidth.
- **Consistency:** Grid updates must handle sub-pixel movements smoothly as the player moves (rolling grid interpolation).

## Acceptance Criteria
- [ ] Compute shaders correctly generate a flow field towards the player in a test scene with obstacles.
- [ ] AI entities successfully navigate around static and dynamic obstacles using the GPU field.
- [ ] Enemies show basic flanking behavior (avoiding high-density clusters) instead of moving in a single file line.
- [ ] System remains stable and maintains 60+ FPS with 5,000+ active AI entities.

## Out of Scope
- Multi-target pathfinding (e.g., enemies pathing to different locations simultaneously).
- Long-distance global pathfinding (outside the 16,384px local grid).
