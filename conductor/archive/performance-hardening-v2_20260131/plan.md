# Implementation Plan: Performance Hardening V2

## Phase 1-3: AZDO Core & Sparse Sync [COMPLETED]
- [x] Task 1.1: Atomic Infrastructure (Persistent Mapping)
- [x] Task 1.2: Lock-Free Emit API
- [x] Task 1.3: Update Loop Cleanup
- [x] Task 2.1: Scatter Stats Shader
- [x] Task 2.2: MDI Sparse Staging API
- [x] Task 3.1: GPU Interpolation Shader Update
- [x] Task 3.2: Fixed-Step Physics Synchronization

## Phase 4: Loot Label Spatial Optimization [COMPLETED]

### [x] Task 4.1: Spatial Grid Manager
- **Goal**: Integrate `SIMDSpatialGrid` for loot entity management.
- **Status**: DONE

### [x] Task 4.2: Spatial Proximity Query
- **Goal**: Replace O(N) loop with spatial query.
- **Status**: DONE

## Phase 5: Particle System Auto-Scaling [COMPLETED]

### [x] Task 5.1: Adaptive Dispatch Range
- **Goal**: Dynamically shrink simulation workload.
- **Status**: DONE

### [x] Task 5.2: Dispatch Throttling
- **Goal**: Completely skip shader execution when idle.
- **Status**: DONE

## Phase 6: String & Layout Caching [COMPLETED]

### [x] Task 6.1: Gold Label Formatting
- **Goal**: Stop calling `snprintf` every frame.
- **Status**: DONE

### [x] Task 6.2: Layout Calculation Bypass
- **Goal**: Cache `MeasureTextEx` results.
- **Status**: DONE

## 4. Final Verification
- **Test**: Run `NoMoreDay.exe`.
- **Metric**: Check log for "Particle Update" time in static scene (target < 50us).
- **Metric**: Check "Render Entities" time with 500+ items on ground (target < 200us).