# Plan: GPU Flow Field Pathfinding Integration

## Phase 1: GPU Infrastructure & Cost Field
- [ ] Task: Create `GPUFlowFieldSystem` skeleton and define necessary `rlgl` buffers (Cost, Integration, Flow).
- [ ] Task: Implement "Rolling Grid" logic to transform world coordinates to grid coordinates based on player position.
- [ ] Task: Implement `flow_reset.compute` to clear buffers and `grid_count.compute` (or reuse) to populate Cost Field with enemy density.
- [ ] Task: Integrate static obstacle data (Tilemap) into the Cost Field update loop.
- [ ] Task: Write `FlowFieldBufferTest` to verify grid scrolling and cost populating accuracy.
- [ ] Task: Conductor - User Manual Verification 'Phase 1: GPU Infrastructure & Cost Field' (Protocol in workflow.md)

## Phase 2: Compute Shader Pathfinding Logic
- [ ] Task: Implement `flow_integration.compute` using an iterative Dijkstra-like approach or Jump Flood algorithm on the GPU.
- [ ] Task: Implement `flow_vector.compute` to generate 2D gradient vectors from the Integration Field.
- [ ] Task: Implement debug visualization in `RenderSystem` to overlay the Flow Field vectors on the screen.
- [ ] Task: Write `FlowFieldLogicTest` to ensure the generated vectors correctly point toward the player around a simple U-shaped obstacle.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Compute Shader Pathfinding Logic' (Protocol in workflow.md)

## Phase 3: AI System Integration & Performance Tuning
- [ ] Task: Update `AISystem` to sample the GPU Flow Field buffer and convert results to Steering Forces.
- [ ] Task: Implement blending logic between Flow Field steering and local Spatial Hash avoidance.
- [ ] Task: Optimize Compute Shader group sizes and memory layouts for maximum throughput.
- [ ] Task: Conduct a stress test with 5,000+ entities to verify stability and performance (Target: < 2ms GPU time for pathing).
- [ ] Task: Conductor - User Manual Verification 'Phase 3: AI System Integration & Performance Tuning' (Protocol in workflow.md)

## Phase 4: Final Polish & Stress Test
- [ ] Task: Refine "Crowd Density" weights to achieve optimal flanking behavior.
- [ ] Task: Ensure smooth transitions when entities enter/leave the local rolling grid.
- [ ] Task: Final code review, documentation update, and performance profiling.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Final Polish & Stress Test' (Protocol in workflow.md)
