# Architecture Audit: CPU-GPU Hybrid Rendering Pipeline

**Date**: 2026-01-26
**Auditor**: Architecture Auditor (AI)
**Target**: `RenderSystem.cpp`, `PopupRenderer`, `GPUParticleSystem`, `UIRenderer`
**Status**: 🟡 **MODERATE ARCHITECTURAL DEBT** (Overall Functional, but with Significant "Anti-Patterns")

## 1. Executive Summary

The rendering pipeline consists of a **hybrid CPU/GPU approach**:
- **Pure CPU**: UI overlays, Stash placeholders, damage text (legacy path), projectile submission.
- **Pure GPU (Instanced)**: Particles, Entity MDI, Skill Effects, Damage Popups (modern path).
- **Hybrid (CPU Collect -> GPU Render)**: Item Labels, Gold Labels, Beam Effects.

While individual subsystems are well-designed, the `RenderSystem::render()` function has become an **850-line monolith** that intermixes world rendering, VFX, UI elements, and debug visualization. This directly violates the **System Isolation** principle.

## 2. System Breakdown & Analysis

### 2.1. `RenderSystem::render()` - The "God Function"

| Line Range | Responsibility | Type | Issues |
| :--- | :--- | :--- | :--- |
| 151-159 | VFX Trails | GPU | ✅ Clean delegation to `TrailSystem` |
| 162-195 | Stash Placeholders | CPU | 🟡 Hardcoded `DrawText`, no batching |
| 200-277 | Sprite Rendering (Glow, Texture) | CPU | 🔴 `try_get<Rarity>` per entity; complex logic |
| 280-287 | GPU Systems Dispatch | GPU | ✅ Clean delegation |
| 291-345 | Pixel View & Molten Zones | CPU | 🟡 Functional, but dense |
| 351-510 | Visual Effects (Array, Pickup, etc.) | CPU (Shader) | 🔴 Massive switch statement |
| 512-561 | Legacy Damage Popups (`DamagePopup`) | CPU | ⚠️ **DEPRECATED PATH** - conflicts with new GPU `PopupRenderer` |
| 563-782 | Item Labels (Instanced) | Hybrid | ✅ Well-optimized |
| 784-820 | Debug Flow Field | CPU | ✅ Debug-only |
| 822-865 | Projectile Submission | Hybrid | 🟡 Functional |

### 2.2. Key Findings & Issues

#### 🔴 Issue 1: Dual Damage Popup Paths (Lines 512 vs 284)
```
Observation:
- Line 284: `NoMoreDay::render::PopupRenderer::Get().Update(dt); Render(viewProj);`
- Line 512: `popupView.each([&font, fontScale](...)) { DrawTextEx(...); }`

Impact:
The system has TWO active popup systems:
1. **New GPU-based `PopupRenderer`** (Instanced, efficient).
2. **Legacy ECS-based `DamagePopup` component** (Per-entity `DrawTextEx`).

Both paths run simultaneously. If a damage event creates both a `DamagePopup` ECS component AND calls `PopupRenderer::Emit()`, the text appears twice (double rendering).
```
**Recommended Action**: Remove the legacy `DamagePopup` view iteration (lines 512-561) and ensure `DamagePopupManager` or combat triggers only use `PopupRenderer::Emit()`.

---

#### 🔴 Issue 2: Visual Effects "Giant Switch" (Lines 399-509)
```cpp
switch (effect.type) {
  case VisualEffectType::AoeArray: { ... 40 lines ... }
  case VisualEffectType::Pickup: { ... }
  case VisualEffectType::DropPillar: { ... }
  case VisualEffectType::GoldSparkle: { ... }
  case VisualEffectType::LevelUp: { ... }
  case VisualEffectType::SwordIntentBurst: { ... 25 lines ... }
  default: break;
}
```
**Impact**:
This switch-case is the #1 source of complexity. Each new VFX type adds more code to an already bloated function. 

**Recommended Action**: Refactor to a **dispatch table** or **polymorphic VFX renderers**.
```cpp
// Proposed: VFXRendererRegistry
using VFXDrawFn = std::function<void(const Position&, const VisualEffect&, const SharedContext&)>;
static std::unordered_map<VisualEffectType, VFXDrawFn> s_vfxRenderers = {
    { VisualEffectType::Pickup, DrawPickupEffect },
    { VisualEffectType::LevelUp, DrawLevelUpEffect },
    // ...
};

// In render():
auto it = s_vfxRenderers.find(effect.type);
if (it != s_vfxRenderers.end()) it->second(pos, effect, context);
```

---

#### 🟡 Issue 3: Static Initialization in Hot Path (Lines 152, 402, 482)
```cpp
// Line 152
static Shader trailShader = {0};
if (trailShader.id == 0 && context.resources) {
    trailShader = context.resources->getShader(...);
}

// Line 402
static Shader arrayShader = {0};
if (arrayShader.id == 0 && context.resources) {
    arrayShader = context.resources->getShader(...);
}

// Line 482-486 (Texture Loading!)
static Texture2D inkTex = {0};
if (inkTex.id == 0 && FileExists("...")) {
    inkTex = LoadTexture("...");
}
```
**Impact**:
- These `static` variables are initialized **lazily on the render path**, which could cause micro-stalls on the first frame.
- More critically, **Line 482 calls `LoadTexture()` during render!** This is synchronous I/O in the main loop.

**Recommended Action**:
Move all static resource loads to `RenderSystem::Initialize()`.

---

#### 🟡 Issue 4: Unsafe Text Pointer in `TextRenderCmd` (Lines 44-49)
```cpp
struct TextRenderCmd {
    Vector2 position;
    const char* text; // Pointer to existing component string (unsafe if component deleted)
    float fontSize;
    Color color;
    bool centered;
};
```
The comment explicitly acknowledges the risk. While safe "within frame," this pattern is fragile. If `ItemComponent.name` or `LabelCacheComponent.cachedText` is modified or entity is destroyed before the text draw pass, this becomes a UAF (Use-After-Free).

**Recommended Action**:
Consider using `std::string_view` with a clear ownership contract, or a small string buffer (`char[64]`) for short labels.

---

### 2.3. Good Patterns to Preserve

| Component | Pattern | Why it's Good |
| :--- | :--- | :--- |
| Item Label Instancing (Lines 563-782) | CPU Collects -> SSBO Upload -> GPU Instanced Draw | Modern, scalable, batches all labels in 1 draw call |
| `PopupRenderer` | `PersistentBuffer` + Instanced Draw | Zero-copy, handles thousands of popups efficiently |
| `GPUParticleSystem` | Compute Shader + Indirect Draw | True fire-and-forget, ~0.3ms for 10k particles |
| `VisibleItemCache` (Phase 1) | Shared data between render passes | Avoids recomputing visibility for hit-testing |

## 3. `PopupRenderer` Specific Audit

| Category | Finding | Severity |
| :--- | :--- | :--- |
| Allocation | `std::to_string` in `Emit()` | 🟡 Minor (per-popup heap alloc) |
| Logic | Hardcoded Chinese glyph map | 🟢 Acceptable for current scope |
| Sync | Uses `PersistentBuffer` correctly | ✅ |
| Singleton | `Get()` returns static instance | 🟡 Same testability concern as `MDIRenderer` |

## 4. `UIRenderer` Audit (Brief)

`UIRenderer` is a 1477-line file handling all UI drawing. It is CPU-only and uses Raylib's immediate-mode API.

| Finding | Details |
| :--- | :--- |
| **Tooltips are batched** | `DrawTooltip`, `DrawSkillTooltip` use `DrawTextEx` but measure text once. |
| **Slot drawing is complex** | `DrawSlot` (lines 159-358) has 200 lines for a single slot. Could be simplified. |
| **Theme system exists** | `UITheme` struct and `s_theme` allow customization. Good. |
| **No GPU instancing for UI** | All UI is CPU-drawn. This is acceptable for static UI but could be optimized for inventory grids (hundreds of slots). |

## 5. Proposed Code Standard: CPU/GPU Hybrid Rendering

### 5.1. The "Render Dispatcher" Pattern

`RenderSystem::render()` should become a **thin dispatcher**:
```cpp
void RenderSystem::render(...) {
    // 1. World Layer
    WorldRenderer::RenderTrails(registry, context);
    WorldRenderer::RenderStashes(registry, context);
    WorldRenderer::RenderSprites(registry, context, camera);

    // 2. GPU Systems
    GPUParticleSystem::Get().Render(camera);
    GPUEntitySystem::Get().Render(context, camera);
    PopupRenderer::Get().Render(viewProj);

    // 3. VFX Layer
    VFXRenderer::RenderAll(registry, context);

    // 4. Labels (Hybrid)
    LabelRenderer::RenderLabels(registry, context, camera);

    // 5. Projectiles
    ProjectileRenderer::Render(registry, context, camera);
}
```
Each sub-renderer is a simple class or namespace with a single responsibility.

### 5.2. Mandatory Resource Preloading

**Rule**: No `LoadTexture`, `LoadShader`, or `getShader` calls in `render()`.
All resources must be acquired in `Initialize()` or via `AssetLoadingSystem`.

### 5.3. Deprecation of Legacy `DamagePopup`

**Rule**: The `DamagePopup` ECS component should be considered **DEPRECATED**.
- Remove the ECS view iteration in `RenderSystem::render()`.
- `DamagePopupManager` must call `PopupRenderer::Emit()` exclusively.
- Remove `DamagePopup` component in a future cleanup task.

---

## 6. Implementation Roadmap

1.  **Phase 1: Cleanup (Low Risk)**
    *   Remove legacy `DamagePopup` render path.
    *   Move static shader/texture loads to `Initialize()`.

2.  **Phase 2: Refactoring (Medium Risk)**
    *   Extract `VFXRenderer` from `RenderSystem`.
    *   Replace giant `switch` with dispatch map.

3.  **Phase 3: Optimization (Optional)**
    *   Investigate GPU instancing for `UIRenderer::DrawSlot` (inventory grids).
