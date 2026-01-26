# Architecture Audit: GPU Rendering Pipeline

**Date**: 2026-01-26
**Auditor**: Architecture Auditor (AI)
**Target**: `src/engine/render/*` (MDIRenderer, GPUEntitySystem)
**Status**: 🔴 **CRITICAL ARCHITECTURAL DEBT**

## 1. Executive Summary

Based on the `architecture-auditor` skill analysis, the current GPU rendering pipeline exhibits significant violations of Data-Oriented Design (DOD) principles, redundant platform abstraction layers, and poor system isolation. 

While the "feature" works (entities are rendered), the underlying implementation is fragile, cache-unfriendly, and notoriously difficult to test or extend. The `GPUEntitySystem` has become a "God Class" managing physics, rendering, logic sync, and resource management.

## 2. Critical Violations (Must Fix)

| Category | Issue | Impact | Location |
| :--- | :--- | :--- | :--- |
| **DOD / Perf** | **"try_get Soup" in Hot Loop** | `GPUEntitySystem::Update` calls `try_get` 5-6 times *per entity* for unrelated components (Stats, AI, Effects). **Destroys CPU Cache coherence.** | `GPUEntitySystem.cpp:186` |
| **Architecture** | **Wheel Reinvention** | `MDIRenderer` manually loads OpenGL function pointers (`glfwGetProcAddress`), ignoring existing `GPUUtils`. Creates platform fragility. | `MDIRenderer.cpp:58` |
| **Safety** | **Hardcoded Binding Indices** | Magic numbers (0, 1, 2, 3) are scattered across files for SSBO binding. Mismatches will cause silent corruption. | `MDIRenderer.cpp`, `GPUEntitySystem.cpp` |
| **Isolation** | **Singleton Abuse** | `MDIRenderer::Get()` makes renderer a global singleton. Tightly couples Systems to this instance, making unit testing impossible. | `MDIRenderer.hpp:39` |
| **Logic** | **God Method** | `GPUEntitySystem::Update` mixes Physics Integration, Texture Selection, AI Flag Packing, and Stat Synchronization. | `GPUEntitySystem.cpp:166` |

## 3. "Chaos Points" Analysis

1.  **Split Personality**: 
    `GPUEntitySystem` is trying to be a Physics Engine, a Render Proxy, and a Logic Sync all at once. This violation of the Single Responsibility Principle makes the code dense and hard to follow.

2.  **Platform Abstraction Leaks**: 
    Raw OpenGL calls (`glDrawArraysIndirect`, `glMemoryBarrier`) are mixed with Raylib (`rlgl`) code. The definition of "who owns the GL context" is blurry.

3.  **Inefficient Data Flow**: 
    Data flows `Registry -> std::vector (Shadow) -> memcpy -> Mapped GPU Buffer`. For many components (like Stats), we copy data even when it hasn't changed.

## 4. Proposed Code Standard: GPU Rendering

To regulate the chaos, the following standards are proposed for adoption.

### 4.1. Binding & Location Constants

**Rule**: NEVER use raw integers for bindings. Define them in a central namespace.

```cpp
// src/engine/render/RenderConstants.hpp
namespace NoMoreDay::RenderConstants {
    enum Binding : uint32_t {
        SSBO_ENTITY_DATA  = 0, // Physics/Transform data (Binding 0)
        SSBO_VISIBLE_ID   = 1, // Culled indices (Binding 1)
        SSBO_COMMAND      = 2, // Indirect Draw Commands (Binding 2)
        SSBO_VISUAL_STATS = 3, // Glow, colors, effects (Binding 3)
    };
}
```

### 4.2. GPU Abstraction Strictness

**Rule**: **No raw OpenGL calls in Systems.**
*   **Allowed**: `GPUUtils::DispatchCompute`, `GPUUtils::MemoryBarrier`.
*   **Allowed**: `ResourceManager::loadComputeShader`.
*   **Banned**: `glfwGetProcAddress`, `glDrawArrays...` (unless strictly inside `MDIRenderer`).
*   **Requirement**: `MDIRenderer` must use `GPUUtils` for extension loading.

### 4.3. The "Update Group" Pattern (DOD Compliance)

**Rule**: Do not mix Logic updates with Transform updates. Split loops.

**❌ Bad (Current):**
```cpp
auto view = registry.view<Position, GPUIndex>(); 
for(auto e : view) {
    // STALL: Random access to unrelated memory
    if (auto* stats = registry.try_get<CombatStats>(e)) { ... } 
}
```

**✅ Good (Proposed):**
```cpp
// Pass 1: Transforms (Linear memory access, prefetch friendly)
auto groupTr = registry.group<GPUIndex>(entt::get<Position, Radius>); 
for(auto e : groupTr) {
    // fast, cache-friendly updates of Position -> GPUBuffer
}

// Pass 2: Visuals (Sparse update, only for specific entities)
auto viewStats = registry.view<GPUIndex, CombatStats, ActiveEffectsComponent>(); 
for(auto e : viewStats) {
    // specific update for visual stats
}
```

### 4.4. Logic & Render Separation

**Rule**: `GPUEntitySystem` should only handle **Transforms and Physics**.
*   **New System**: `GPUVisualSyncSystem` (or a distinctive function/phase) should handle syncing `CombatStats`, `Buffs`, and `TextureIndex`.
*   **Benefit**: Physics runs fast every frame. Visual sync runs less frequently (e.g., every 5 frames or on dirty flag) without stalling the physics loop.

## 5. Implementation Roadmap

1.  **Phase 1: Standardization**
    *   Create `src/engine/render/RenderConstants.hpp`.
    *   Refactor `GPUUtils` to expose all needed GL wrappers.
    *   Clean up `MDIRenderer` to use `GPUUtils` and remove manual loading.

2.  **Phase 2: Decoupling**
    *   Refactor `GPUEntitySystem::Update`: Split into `UpdatePhysicsData` and `UpdateVisualData`.
    *   Implement EnTT Groups for the `GPUIndex` + `Position` hot path.

3.  **Phase 3: Optimization**
    *   Revisit `GPUVisualStats` sync frequency (dirty flags).
