# GPU Offloading Analysis Report
**Date:** 2026-01-25
**Agent:** Codebase Investigator

## Executive Summary
This report identifies key opportunities to offload CPU-bound rendering tasks to the GPU. The primary bottleneck is the lack of texture support in the Multi-Draw Indirect (MDI) pipeline, forcing most sprites to be rendered via CPU loops. Addressing this, along with unifying VFX and status indicators, will significantly improve performance.

## 1. Core Findings & Bottlenecks

### A. MDI Sprite Rendering (Critical)
*   **Current State:** `GPUEntitySystem` explicitly flags entities with `SpriteComponent` as `GPU_ENTITY_FLAG_NO_RENDER`. The `MDIRenderer` shader (`entity_mdi.frag`) only supports basic Circle SDF shapes.
*   **Impact:** The majority of game entities (monsters, items, projectiles) bypass the efficient MDI path and use slow CPU-side draw calls.
*   **Recommendation:** Implement Texture Arrays (`GL_TEXTURE_2D_ARRAY`) to allow the MDI shader to sample different sprites in a single draw call.

### B. Unified GPU VFX System
*   **Current State:** `RenderSystem` and `EffectSystem` handle many "common effects" (pickups, sparkles, hits) via CPU `switch-case` logic and individual draw calls.
*   **Impact:** High CPU overhead for strictly visual, high-frequency elements.
*   **Recommendation:** Create a `GPUCommonEffectSystem` using Compute Shaders to simulate and `glDrawArraysIndirect` to render these effects, similar to the existing skill effect system.

### C. Visual Status & Rarity
*   **Current State:** Rarity glows (e.g., Elite gold glow) and status effects (Frozen, Burning) are rendered as separate "Overlay" draw calls on top of entities.
*   **Impact:** Overdraw and increased draw call count.
*   **Recommendation:** Integrate status rendering into the main `entity_mdi.frag` shader using data synced via `GPUVisualStats` (SSBO).

### D. Legacy Cleanup
*   **Current State:** `DamagePopup` system is split between old CPU rendering and new GPU rendering.
*   **Recommendation:** Complete the migration and remove the legacy code.

## 2. Risk Analysis

### A. Texture Array Constraints
*   **Risk:** `GL_TEXTURE_2D_ARRAY` requires all layers to have identical dimensions.
*   **Mitigation:** Standardize on 128x128 for the primary gameplay layer. Irregular assets (UI, large bosses) should either use a separate "Large Atlas" or remain on the CPU/Instanced path if they are few in number.
*   **Context:** User confirmed most resources are 128x128.

### B. Bandwidth & Synchronization
*   **Risk:** Syncing more data (texture indices, visual stats) per entity increases PCI-e bandwidth usage.
*   **Mitigation:** Ensure `GPUInstanceData` and `GPUVisualStats` are tightly packed and aligned (16-byte alignment). Only sync necessary data.

### C. Transparency Sorting
*   **Risk:** MDI does not guarantee back-to-front sorting, which can cause artifacts for semi-transparent sprites.
*   **Mitigation:** Use alpha cutout (discard) for hard edges. For soft transparency, accept minor sorting artifacts or use a two-pass approach (Opaque MDI -> Sorted Transparent CPU/Instanced).

## 3. Implementation Roadmap

1.  **Phase 1: Foundation (Texture Arrays)** - Enable MDI for standard 128x128 sprites.
2.  **Phase 2: Status Integration** - Move rarity/status visual logic to Shader.
3.  **Phase 3: VFX Offload** - Migrate common effects to Compute Shaders.
4.  **Phase 4: Optimization** - Handle irregular assets and polish.
