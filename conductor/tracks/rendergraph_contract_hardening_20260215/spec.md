# RenderGraph Contract Hardening Spec

> **Track ID**: `rendergraph_contract_hardening_20260215`  
> **Type**: `refactor`  
> **Priority**: P0 (stability first)  
> **Compatibility Policy**: Strong compatibility, visual output must stay functionally equivalent.

## 1. Goal

Upgrade the current RenderGraph pipeline from a pass executor to a contract-enforced render pipeline that guarantees:

1. deterministic pass order validity,
2. explicit frame ownership for render targets,
3. stable GL state boundaries,
4. safe transient render target lifecycle.

## 2. Current Gaps

1. RenderGraph records read/write declarations but does not validate hazards or ownership.
2. HDR chain activation is coupled to bloom/default framebuffer conditions.
3. `TransientResourcePool` exists but is not the primary allocator for transient pass targets.
4. Pass boundaries rely on conventions more than formal contract checks.

## 3. Scope

1. `src/engine/render/graph/RenderGraph.hpp`
2. `src/engine/render/graph/RenderGraph.cpp`
3. `src/engine/render/graph/RenderContext.hpp`
4. `src/engine/render/RenderSystem.cpp`
5. `src/engine/render/resources/TransientResourcePool.hpp`
6. `src/engine/render/resources/TransientResourcePool.cpp`
7. `src/engine/render/passes/*.hpp`
8. `src/engine/render/passes/*.cpp`
9. new tests under `tests/unit` and `tests/integration`

## 4. Design Requirements

### 4.1 Frame Ownership Contract

Single-frame ownership must be formalized:

1. `ScenePass` writes scene HDR target.
2. `Lighting/Volumetric/VFX/UIWorld` only operate on declared ownership transitions.
3. `PostProcess` reads HDR and writes LDR chain.
4. `Composite` is the only stage writing to final output target.

### 4.2 RenderGraph Validation

Build-time graph checks must detect:

1. read-before-write resources,
2. conflicting writes in same stage ordering,
3. undeclared resource usage from pass setup.

### 4.3 Transient Resource Policy

Transient targets used by postprocess/distortion sub-steps must come from the pool layer (or a pool-backed adapter), not ad-hoc persistent allocations.

### 4.4 GL State Contract

All passes must conform to a baseline exit state contract and keep raylib interop deterministic.

## 5. Non-Goals

1. No major shader algorithm changes in this track.
2. No visual redesign.
3. No ABI/schema versioning work (covered in separate track).

## 6. Acceptance Criteria

1. Graph validation fails fast in debug when contracts are violated.
2. Resize/offscreen/default framebuffer paths are stable with no black-screen regressions.
3. Existing visual behavior for baseline scenes remains equivalent.
4. Unit/integration tests are added and passing.
5. `build.bat` passes.

