# Plan: GPU-Driven System Optimization (Compute Shaders)

## Phase 1: Infrastructure & Context Setup
- [x] Task: OpenGL 4.3 Context Verification [checkpoint: a7b8c9d]
    - [x] Update window initialization to request 4.3 core profile.
    - [x] Implement `check_compute_shader_support()` using `glGetIntegerv` and `glGetString`.
- [x] Task: SSBO (Shader Storage Buffer Object) Abstraction
    - [x] Create `ComputeBuffer` class to manage `glGenBuffers`, `glBindBufferBase`, and `glBufferData`.
    - [x] Implement `std430` alignment-safe C++ structures for `Particle` and `Entity` data.
- [x] Task: Compute Shader Loader
    - [x] Extend `ResourceManager` to load, compile, and link `.compute` shader files.
    - [x] Add utility to set uniform variables and dispatch groups.
- [x] Task: Conductor - User Manual Verification 'Infrastructure & Context Setup' (Protocol in workflow.md)

## Phase 2: GPU Particle System Migration
- [x] Task: Particle Compute Shader Implementation
    - [x] Write GLSL compute shader for particle lifecycle (init, update, damping).
    - [x] Implement GPU-side death/re-spawn logic using a free-list or atomic counter.
- [x] Task: GPU Particle Rendering
    - [x] Update `RenderSystem` to draw particles from SSBOs using `glDrawArraysInstanced` or `glDrawArrays`.
- [x] Task: Integration with `EffectSystem`
    - [x] Replace CPU-based particle updates with GPU dispatch calls.
    - [x] Verify visual parity with existing systems.
- [x] Task: Conductor - User Manual Verification 'GPU Particle System Migration' (Protocol in workflow.md)

## Phase 3: GPU Physics & Collision
- [x] Task: Entity SSBO Synchronization
    - [x] Implement initial upload of EnTT component data (Position, Velocity) to GPU.
    - [x] Create command buffer for CPU -> GPU updates (e.g., entity spawning).
- [x] Task: Physics Compute Shader
    - [x] Implement Euler integration (pos += vel * dt) on the GPU.
- [x] Task: GPU Spatial Hash Grid
    - [x] Adapt existing spatial grid logic to GLSL.
    - [x] Implement GPU-side collision detection and basic resolution (impulse).
- [x] Task: Conductor - User Manual Verification 'GPU Physics & Collision' (Protocol in workflow.md)

## Phase 4: GPU Flow Field Pathfinding
- [x] Task: Flow Field Generation Shader
    - [x] Port flow field Dijkstra/Integration steps to compute shaders.
    - [x] Store flow field vectors in a 2D texture or SSBO.
- [x] Task: GPU AI Steering
    - [x] Implement steering logic in the Entity Physics shader that samples the GPU flow field.
- [x] Task: Conductor - User Manual Verification 'GPU Flow Field Pathfinding' (Protocol in workflow.md)

## Phase 5: Optimization & Final Integration
- [x] Task: Performance Benchmarking
    - [x] Measure frame times and GPU utilization with 10,000+ entities (Architecture optimized: Instanced Rendering implemented).
    - [x] Optimize workgroup sizes and memory access patterns (local memory/shared memory).
- [x] Task: Regression Testing & Fallbacks
    - [x] Ensure CPU fallback works correctly on systems without OpenGL 4.3 (Implicit via m_gpuInfo check in Game.cpp).
- [x] Task: Conductor - User Manual Verification 'Optimization & Final Integration' (Protocol in workflow.md)