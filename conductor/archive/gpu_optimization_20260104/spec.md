# Specification: GPU-Driven System Optimization (Compute Shaders)

## Overview
This track aims to significantly boost the game's performance and visual scalability by offloading heavy computational loads—specifically particles, entity physics, collisions, and pathfinding—to the GPU using OpenGL 4.3 Compute Shaders. By adopting a "GPU-Driven" approach, we minimize CPU-GPU data transfer and leverage parallel processing for 10,000+ entities.

## Functional Requirements

### 1. GPU Compute Infrastructure
- **OpenGL 4.3+ Integration:** Configure the rendering context to support Compute Shaders. 
- **Hardware Fallback Mechanism:** Implement a check to detect support for OpenGL 4.3. If unavailable, provide a graceful fallback (likely to existing CPU systems).
- **Buffer Management:** Develop an abstraction for Shader Storage Buffer Objects (SSBOs) to store entity and particle data.

### 2. Offloaded Systems (GPU Implementation)
- **Particle System:** Migrate particle lifecycle (spawning, velocity updates, damping, and death) to compute shaders.
- **Entity Physics & Movement:** Handle position and velocity updates for thousands of active entities on the GPU.
- **Collision Detection:** Implement GPU-side collision checks (likely utilizing the existing Spatial Hash Grid logic adapted for shaders).
- **Flow Field Pathfinding:** Calculate vector fields for AI navigation directly on the GPU.

### 3. GPU-Driven Architecture
- **State Ownership:** The primary simulation state for the offloaded systems will reside in GPU memory.
- **CPU Control:** The CPU (EnTT ECS) will send high-level commands (e.g., "Spawn 500 enemies at X", "Set target for Flow Field Y") to the GPU.
- **Minimal Readback:** Synchronization from GPU to CPU will be minimized to prevent PCIe bottlenecks, used only for critical logic (e.g., player-enemy interaction triggers).

## Non-Functional Requirements
- **Performance:** Target a stable 60+ FPS while simulating 10,000+ entities and 50,000+ particles.
- **Data Alignment:** Ensure C++ structures match the `std430` layout used in GLSL for SSBOs.
- **Compatibility:** Strictly adhere to the C++20 standard and existing project patterns.

## Acceptance Criteria
- [ ] Successfully initializes an OpenGL 4.3 context within the Raylib environment.
- [ ] Particles are updated and rendered entirely through GPU compute/draw calls.
- [ ] Entity movement and basic collision resolution are performed on the GPU without per-frame CPU intervention.
- [ ] Flow field vectors are generated and accessible to shaders for entity movement.
- [ ] No regression in performance on hardware supporting OpenGL 4.3.

## Out of Scope
- Rewriting the entire rendering pipeline (only specific high-count systems).
- GPU-side AI decision-making (logical state machines remain on CPU).
- Support for OpenGL versions below 3.3.
