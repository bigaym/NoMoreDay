# GPU Loot Rendering Implementation Plan

> Track ID: `v4_gpu_loot_rendering_20260219`  
> Spec: `spec.md`  
> Status: Completed

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| Phase 1 | GPU Data Layer | SSBO layout + ABI registration | [x] |
| Phase 2 | MDI Pipeline | FrustumCull + IndirectArgs + GPU loot draw | [x] |
| Phase 3 | Force-Directed Avoidance | GridHash + Repulsion + PositionUpdate | [x] |
| Phase 4 | Integration & Verification | RenderGraph + FeatureFlag + Tier + Benchmark | [x] |

---

## Phase 1: GPU Data Layer

### Tasks
- [x] Task 1.1: Define `GPULootInstance` in `GPUData.hpp`
- [x] Task 1.2: Add static_assert and ABI registration
- [x] Task 1.3: Build `GPULootSystem` skeleton (SSBO create + dropped item sync)
- [x] Task 1.4: Register loot bindings in `RenderConstants`

### Verification
- [x] ABI layout checks passed
- [x] SSBO payload consistent with CPU dropped-item data

---

## Phase 2: MDI Pipeline

### Tasks
- [x] Task 2.1: Implement `FrustumCullCS`
- [x] Task 2.2: Implement `IndirectArgsCS` (`instanceCount` update)
- [x] Task 2.3: Implement indirect draw path for loot quads
- [x] Task 2.4: Implement loot quad shader using `GPULootInstance`

### Verification
- [x] GPU route and CPU route render behavior aligned for in-view drops
- [x] Off-frustum drops are culled

---

## Phase 3: Force-Directed Avoidance

### Tasks
- [x] Task 3.1: Implement `GridHashCS`
- [x] Task 3.2: Implement `RepulsionCS`
- [x] Task 3.3: Implement `PositionUpdateCS`
- [x] Task 3.4: Tune stiffness/damping/minDistance/maxOffset
- [x] Task 3.5: Validate dense-drop stability path

### Verification
- [x] Label overlap reduced by compute avoidance path
- [x] Offset convergence bounded by damping/clamp policy

---

## Phase 4: Integration & Verification

### Tasks
- [x] Task 4.1: Integrate `GPULootPass` into RenderGraph contract sequence
- [x] Task 4.2: Implement feature flag `render.gpuLoot.enabled`
- [x] Task 4.3: Integrate tier policy (Low/Medium=CPU, High=GPU, Ultra=GPU+Glow)
- [x] Task 4.4: Add/update benchmark coverage
- [x] Task 4.5: Validate route switch stability (CPU/GPU fallback)

### Verification
- [x] `build.bat` passed
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` passed
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` passed
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` passed
- [x] `ctest --test-dir build -C Release -L performance --output-on-failure` passed

