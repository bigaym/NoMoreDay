# Comprehensive Code Audit Report (2026-01-24)

> **Auditor**: Gemini Code Risk Analyzer
> **Focus**: Physics (Thread Safety), Rendering (GPU/SSBO), Architecture (DOD)
> **Date**: 2026-01-24

## 1. Executive Summary

本次审计对 `PhysicsSystem`、`GPUEntitySystem` 及 `MDIRenderer` 进行了深度代码审查。
**主要发现**：
1.  **逻辑缺失**：物理系统在并行化重构后，丢失了 `ForceField` (力场/黑洞) 的计算逻辑。
2.  **维护风险**：`MDIRenderer` 与 `GPUData` 中的 GPU 数据结构定义存在 **尺寸不一致 (64B vs 48B)**，极易导致 Shader 读取错位或数据破坏。
3.  **性能瓶颈**：`GPUEntitySystem` 目前每一帧都进行全量数据拷贝 (Full State Copy)，随着实体数量增加 (Target: 200k)，主线程开销将显著增加。

---

## 2. 详细审查结果 (Detailed Findings)

### 2.1 Physics & Thread Safety (CPU)

|ID|Severity|Component|Issue|
|---|---|---|---|
|**P-01**|🔴 **Critical** |`GameplayState`|**Force Fields Logic Missing**.<br>The Taskflow implementation in `UpdatePhysics` (Phase 1/2) completely omits `PhysicsSystem::applyForceFields`. Skills relying on force fields (Vortex, Gravity) are currently broken.|
|**P-02**|🟡 Medium|`PhysicsSystem`|**Magic Numbers**.<br>Hardcoded constants found: `WALL_REPULSION_FACTOR = 20.0f`, `Damping = 0.92f`, `STEP_SIZE = 10.0f`. Should be moved to `Common.hpp` or `PhysicsConstants` namespace.|
|**P-03**|🟢 Safe|`Taskflow`|Thread safety verified. `for_each` partitions work by Entity ID. Phase 1 (Resolve) writes `Velocity`, Phase 2 (Integration) writes `Position`. `SpatialGrid` access is read-only. No race conditions detected.|

**Evidential Snippet (Missing Logic):**
```cpp
// src/game/states/GameplayState.cpp (L662)
void GameplayState::UpdatePhysics(float dt) {
  // ... initialization ...
  // Phase 1: Resolve Collisions (Taskflow)
  // Phase 2: Update Positions (Taskflow)
  // MISSING: PhysicsSystem::applyForceFields(registry, dt, m_spatialGrid);
}
```

### 2.2 Rendering & GPU Architecture

|ID|Severity|Component|Issue|
|---|---|---|---|
|**R-01**|🟠 **High**|`MDIRenderer`|**SSBO Struct Mismatch**.<br>`GPUData.hpp` defines `GPUEntity` as **64 bytes**. `MDIRenderer.hpp` defines `GPUInstanceData` as **48 bytes**. Although `MDIRenderer` currently relies on `GPUEntitySystem`'s buffer, this divergence in C++ definitions creates a latent crash risk if `MDIRenderer` logic is extended or if Shaders rely on `GPUInstanceData` layout.|
|**R-02**|🟡 Medium|`GPUEntitySystem`|**Full Buffer Copy**.<br>`Update` performs `memcpy` of the entire `m_shadowBuffer` (12.8MB for 200k entities) every frame. `m_blockDirty` logic is declared but unused.|
|**R-03**|🟢 Safe|`GPUEntitySystem`|**Double Free Protection**.<br>`OnGPUIndexDestroyed` and `Update` (KilledTag check) both safely handle slot release without conflict due to `gpuIdx.index != -1` check.|

**Evidential Snippet (Struct Mismatch):**
```cpp
// GPUData.hpp
struct GPUEntity { ... float padding[6]; }; // Size: 64 Bytes ✅

// MDIRenderer.hpp
struct GPUInstanceData { ... float padding[3]; }; // Size: 48 Bytes ❌
// Comment claims: "Matches GPUEntity in GPUData.hpp" (FALSE)
```

---

## 3. Optimization & Remediation Plan

### 3.1 Immediate Fixes (Hotfixes)

1.  **Restore Force Fields**:
    *   In `GameplayState::UpdatePhysics`, insert `PhysicsSystem::applyForceFields(registry, dt, m_spatialGrid);` **before** the Taskflow execution.
    *   Assuming Force Field application is logic-heavy, consider parallelizing it as `Phase 0` in Taskflow (modifying Velocity).

2.  **Align SSBO Structs**:
    *   Padding `GPUInstanceData` in `MDIRenderer.hpp` to **64 bytes**.
    *   Ensure `cull.compute` and `entity_mdi.vert` use consistent strides (64 bytes).

### 3.2 Strategic Refactoring (Phase 2)

1.  **Implement Dirty Blocks**:
    *   Activate `m_blockDirty` logic in `GPUEntitySystem`.
    *   Only `memcpy` blocks (e.g., 1024 entities) that have changed or moved.
    *   Estimates: 50%+ reduction in bandwidth for non-combat scenes.

2.  **Configuration Extraction**:
    *   Move Physics magic numbers to `src/game/components/PhysicsConstants.hpp`.

---

## 4. Conclusion

The system is architecturally sound regarding thread safety, which was the primary concern of the audit. However, the functional regression (Force Fields) and the data structure mismatch in rendering represent significant quality risks. Prioritize **P-01** and **R-01** for immediate resolution.
