# Technical Specification: Performance Hardening V2 (AZDO & Spatial Optimization)

## 1. Overview
The V2 performance hardening track aims to resolve the significant FPS drop observed after high-kill/high-loot events. Current analysis identifies three primary culprits:
1. **O(N) Traversal of Loot**: `RenderSystem` iterates thousands of ground items every frame on the CPU.
2. **Particle Over-Simulation**: `GPUParticleSystem` continues to simulate dead particles because its dispatch range never shrinks.
3. **Redundant UI Overhead**: Ground item labels perform expensive string formatting and text measurement every frame.

## 2. Technical Goals
- **Spatial Loot Collection**: Reduce CPU collection time from $O(N_{total})$ to $O(N_{visible})$.
- **Adaptive Particle Dispatch**: Reduce `Particle Update` GPU time by shrinking simulation range based on actual alive count.
- **Zero-Alloc UI Labels**: Eliminate per-frame heap allocations and string formatting for static loot labels.

## 3. Data Model Design

### 3.1 Item Spatial Grid
Reuses `SIMDSpatialGrid` for high-performance proximity queries.
- **Grid Configuration**: 
    - `cellSize`: 128.0 units (matched to average loot spread).
    - `boundary`: ±5000.0 units.
- **Indexing Strategy**: 
    - Only rebuild when the number of items changes (dirty flag on item creation/destruction).

### 3.2 Label Cache Component
Standardizes the caching of text and layout for ground items.
```cpp
struct LabelCacheComponent {
    char cachedText[64];      // Pre-formatted string (e.g., "100 Gold")
    Vector2 cachedSize;       // Result of MeasureTextEx
    int lastFontSize = 0;     // Detect font scale changes
    uint32_t lastRarityHash = 0; // Detect rarity/name changes
    bool isValid = false;     // Dirty flag
};
```

### 3.3 Particle System Metrics
```cpp
struct ParticleSystemMetrics {
    uint32_t aliveCount;      // Read back from GPU atomic
    uint32_t currentDispatch; // Current range being simulated
    float idleTimer;          // Time since last active particle
};
```

## 4. System Logic Flow

### 4.1 Phase 4: Loot Label Spatial Optimization
1.  **System**: `LootGridSystem`
    - Monitors `entt::observer` for `ItemComponent` or `GoldComponent` creation/destruction.
    - Triggers `ItemSpatialGrid::rebuild` only when the item count changes.
2.  **System**: `RenderSystem::DrawLootLabels`
    - Replaces `registry.view<ItemComponent>().each(...)` with:
      ```cpp
      m_itemGrid.query(camera.target, 1500.0f, [&](entt::entity e, const Vector2& pos) {
          // Perform culling and instance generation only for nearby items
      });
      ```

### 4.2 Phase 5: Particle System Auto-Scaling
1.  **Metric Collection**:
    - Every frame, read `m_lastKnownAliveCount` (populated via `async readback` in Phase 1).
2.  **Adaptive Dispatch**:
    - If `aliveCount < currentDispatch * 0.5` for 60 consecutive frames:
      - `currentDispatch = max(aliveCount + 1024, 0)`.
    - If `Emit` is called:
      - Immediately expand `currentDispatch` to handle new emission.
3.  **Optimization**: Skip `rlComputeShaderDispatch` entirely if `currentDispatch == 0`.

### 4.3 Phase 6: String & Layout Caching
1.  **Gold Optimization**: 
    - Format `"%d Gold"` only once in `GoldComponent` creation or value change.
2.  **Layout Bypass**:
    - If `LabelCacheComponent::isValid` is true AND `fontScale` hasn't changed, skip `MeasureTextEx`.

## 5. Shader Logic Changes

### 5.1 `scatter_stats.compute` (Completed in Phase 2)
- Uses `std430` layout for efficient random-access writes to `GPUVisualStats` buffer.

### 5.2 `entity_mdi.vert` (Verified in Phase 3)
- Utilizes `uTime` and `interpolationFactor` for smooth visuals without CPU intervention.

## 6. Acceptance Criteria (AC)
- [ ] **CPU Scalability**: 2000+ items on map result in < 0.2ms CPU time for `RenderSystem` collection pass.
- [ ] **GPU Efficiency**: `Particle Update` time drops to < 0.05ms when no particles are active.
- [ ] **UI Performance**: `Render UISystem` time (excluding HUD) stays < 0.1ms in static loot scenes.
- [ ] **Visual Integrity**: All item labels remain correctly centered and colored.

## 7. Risks & Mitigations
- **Grid Rebuild Cost**: Rebuilding the grid for 5000 items might cause a frame spike.
    - *Mitigation*: Rebuild only when count changes, or use an incremental grid update if necessary.
- **SIMD Alignment**: `SIMDSpatialGrid` requires 32-byte alignment.
    - *Mitigation*: Ensure `xsimd::aligned_allocator` is used for all internal SOA vectors.
