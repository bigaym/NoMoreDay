# Implementation Plan: Performance Hardening V2

## Phase 1-3: AZDO Core & Sparse Sync [COMPLETED]
- [x] Task 1.1: Atomic Infrastructure (Persistent Mapping)
- [x] Task 1.2: Lock-Free Emit API
- [x] Task 1.3: Update Loop Cleanup
- [x] Task 2.1: Scatter Stats Shader
- [x] Task 2.2: MDI Sparse Staging API
- [x] Task 3.1: GPU Interpolation Shader Update
- [x] Task 3.2: Fixed-Step Physics Synchronization

## Phase 4: Loot Label Spatial Optimization

### [ ] Task 4.1: Spatial Grid Manager
- **Goal**: Integrate `SIMDSpatialGrid` for loot entity management.
- **Files**: `src/engine/render/RenderSystem.cpp`, `src/engine/render/RenderSystem.hpp`
- **Steps**:
    1.  Add `std::unique_ptr<systems::SIMDSpatialGrid> m_itemGrid` to `RenderSystem`.
    2.  In `RenderSystem::Init`, initialize grid with 128 cell size.
    3.  Create `UpdateItemGrid()` helper: use `registry.view<ItemComponent, Position>()` to rebuild grid.
    4.  Add `bool m_itemGridDirty` flag to `RenderSystem`. Set true when items are added/removed.

### [ ] Task 4.2: Spatial Proximity Query
- **Goal**: Replace O(N) loop with spatial query.
- **Files**: `src/engine/render/RenderSystem.cpp`
- **Steps**:
    1.  In `RenderSystem::Render` (Item Section), call `m_itemGrid->query(camera.target, 1500.0f, callback)`.
    2.  Move instance generation logic into the query callback.
    3.  Verify `MAX_RENDER_LABELS` is still respected.

## Phase 5: Particle System Auto-Scaling

### [ ] Task 5.1: Adaptive Dispatch Range
- **Goal**: Dynamically shrink simulation workload.
- **Files**: `src/engine/render/GPUParticleSystem.cpp`
- **Steps**:
    1.  Add `uint32_t m_targetDispatchCount` to class.
    2.  In `Update()`, if `m_lastKnownAliveCount == 0` and `m_emitHead == 0`, set `m_targetDispatchCount = 0`.
    3.  Else, set `m_targetDispatchCount = max(m_lastKnownAliveCount + 2048, emitHead)`.
    4.  Apply `m_targetDispatchCount` to compute shader `workGroups` calculation.

### [ ] Task 5.2: Dispatch Throttling
- **Goal**: Completely skip shader execution when idle.
- **Files**: `src/engine/render/GPUParticleSystem.cpp`
- **Steps**:
    1.  In `Update()`, wrap `rlComputeShaderDispatch` in `if (workGroups > 0)`.
    2.  Reset `m_currentParticleCount` to 0 when idle to avoid stale simulations.

## Phase 6: String & Layout Caching

### [ ] Task 6.1: Gold Label Formatting
- **Goal**: Stop calling `snprintf` every frame.
- **Files**: `src/engine/render/RenderSystem.cpp`
- **Steps**:
    1.  Modify Gold collection pass: only update `cachedText` if `LabelCacheComponent::isValid` is false.
    2.  Set `isValid = false` only when Gold entity is created.

### [ ] Task 6.2: Layout Calculation Bypass
- **Goal**: Cache `MeasureTextEx` results.
- **Files**: `src/engine/render/RenderSystem.cpp`
- **Steps**:
    1.  Update `LabelCacheComponent` logic: check `fontScale` and `rarity` against cached values.
    2.  If match, use `labelCache.cachedSize` immediately.

## 4. Final Verification
- **Test**: Run `NoMoreDay.exe`.
- **Metric**: Check log for "Particle Update" time in static scene (target < 50us).
- **Metric**: Check "Render Entities" time with 500+ items on ground (target < 200us).