# Performance Hardening Plan

## 1. Objectives
- Achieve stable 180 FPS with 10,000+ entities.
- Eliminate per-frame heap allocations in hot paths.
- Fix memory leaks in static caches.
- Optimize string-heavy logic to hash-based lookups.

## 2. Tasks
### Phase 1: Memory & Allocation [DONE]
- [x] Fix StatsCache memory leak (registry.on_destroy hook).
- [x] SkillSystem scratch buffer (s_entities_scratch).
- [x] RenderSystem FlowField sync (CPU shadow buffer).

### Phase 2: String Optimization [DONE]
- [x] Compile-time FNV-1a Hash implementation.
- [x] ItemComponent setNameHash integration.
- [x] StatsSystem SetBonus hash-based lookup.
- [x] Astrolabe structured conversions.

### Phase 3: Logic & Integration [DONE]
- [x] Sword Intent real-time attribute update (StatsDirty).
- [x] Verification and Build.

## 3. Results
- **Memory**: StatsCache now correctly erases entries upon entity destruction.
- **CPU**: SkillSystem update loop is allocation-free. RenderSystem no longer downloads vectors from GPU every frame.
- **DOD**: String comparisons in StatsSystem replaced with 32-bit integer comparisons.
