# Performance Analysis Report - 2026-01-31

## 1. Analysis Context
- **Target Log**: `build/bin/logs/NoMoreDay.log`
- **Timestamp**: ~13:06:20
- **Hardware Environment**: Intel Iris Xe (Integrated GPU)
- **Baseline**: MSVC 2022 Build, VSync disabled.

## 2. Key Performance Indicators (KPI)

| Metric | Avg Duration (us) | Avg Duration (ms) | Peak Duration (ms) | Severity |
| :--- | :--- | :--- | :--- | :--- |
| **Particle Update** | 2,150 | 2.15 | **35.33** | 🔴 High |
| **Render Entities (MDI)** | 2,200 | 2.20 | ~2.50 | 🟡 Medium |
| **MDI Cull (Compute)** | 75 | 0.07 | 0.12 | 🟢 Low |
| **Level Update** | 900 | 0.90 | 1.10 | 🟢 Low |
| **Frame Render (Total)** | 3,100 | 3.10 | 4.50 | 🟢 Low |

*Note: Total frame time suggests a potential 322 FPS, but periodic spikes in Particle Update cause visible micro-stutter.*

## 3. Identified Bottlenecks & Root Causes

### 3.1 [CPU] Particle System Lock Contention & Memory Copies
- **Issue**: `GPUParticleSystem::Update` takes 2.2ms every frame.
- **Root Cause**: 
    1. **Mutex Contention**: Multiple systems (Skill, Item, Hazard) emit particles. Thread-local buffers are used, but they are aggregated in the main loop using a `std::mutex` lock.
    2. **Redundant Copies**: The current flow follows: `Sub-thread Vector -> Main Loop Aggregate -> Upload to GPU`. This creates O(N) memory traffic on the CPU caches.
    3. **Spike Hazard**: A 35ms spike was detected, likely caused by a massive amount of particles (e.g., massive mob death) forcing the vector to resize or the CPU to stall on aggregate memory operations.

### 3.2 [IO/GPU] MDI Full Buffer Synchronization
- **Issue**: Syncing 30,000 entities to GPU takes 2.2ms.
- **Root Cause**: 
    1. **Full Memcpy**: Every frame, `GPUVisualStats` for all entities (even stationary ones) is copied to the mapped buffer.
    2. **PCIe Pressure**: Constant 2MB+ transfer every frame contributes to CPU stall while waiting for the GPU driver to manage memory visibility.

### 3.3 [CPU] Logging System Clock Queries (REMEDIATED)
- **Issue**: `LOG_LIMITED_INFO` macros were querying `std::chrono::steady_clock::now()` on Every Frame per call site.
- **Root Cause**: Cumulative cost of `QueryPerformanceCounter` syscalls.
- **Status**: **FIXED** on 2026-01-31 by implementing global frame-time caching via `NoMoreDay::utils::Time`.

## 4. Modern Architecture Recommendations (AZDO)

### 4.1 Lock-free Virtual Ring Buffer (Particles)
- **Concept**: Abandon the "Aggregate" phase.
- **Implementation**: 
    - Use `std::atomic<uint32_t>` for a global "TicketCounter".
    - Use `Persistent Mapped Buffers` (GL_ARB_buffer_storage).
    - Threads directly write to the assigned GPU memory slot.
- **Goal**: Zero CPU aggregation cost.

### 4.2 Sparse Buffer Synchronization (MDI)
- **Concept**: Only sync what changed.
- **Implementation**: 
    - Implement a `DirtyBitset` tracking modified entities.
    - Use `glBufferSubData` or a secondary `Staging Buffer + Compute Shader` to scatter updates to the main SSBO.
- **Goal**: Reduce PCIe traffic by >90%.

### 4.3 GPU-Side Interpolation
- **Concept**: Decouple logic tick (60Hz) from render (Uncapped).
- **Implementation**: 
    - Store `prevPosition` and `targetPosition` in the SSBO.
    - Move interpolation `pos = mix(prev, target, alpha)` to the Vertex Shader.
- **Goal**: Remove 30,000+ floating point calculations from the CPU main loop.

## 5. Summary Recommendation
Prioritize **GPUParticleSystem** refactor using Atomic Counters and Persistent Mapping. This will eliminate the 35ms "lag spikes" that occur during combat density peaks.
