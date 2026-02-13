# Dynamic 2D Lighting System (Execution Plan)

> **Track ID**: `dynamic_lighting_system_20260212`  
> **Spec**: `spec.md` (V1.0)  
> **Status**: `IN PROGRESS`  
> **Last Updated**: `2026-02-12`

---

## Overview

| Phase | Topic | Main Deliverable | Estimate | Status |
|------|------|------------------|----------|--------|
| 1 | Data and Config | GPULight, LightComponent, RenderConfig/QualityTier extension | 0.5d | Done |
| 2 | LightManager | ECS collect, cull/sort, SSBO upload, transient lights | 1.0d | Done |
| 3 | LightingPass + Shader | Fullscreen light accumulation pass and shader | 1.5d | Done |
| 4 | RenderGraph Integration | Graph insertion, lifecycle, low-tier fallback validation | 0.5d | Partial |
| 5 | Gameplay Integration | Player aura, drop lights, skill hit flashes | 0.5d | Done |
| 6 | Test and Validation | Unit tests, perf benchmark, stability validation | 0.5d | In Progress |

---

## Phase 1: Data and Config (Done)

### Task 1.1: GPULight data structure
- [x] Add `components::GPULight` (32 bytes) in `src/engine/render/GPUData.hpp`.
- [x] Add `components::LightType` enum.
- [x] Add ABI assertion: `static_assert(sizeof(GPULight) == 32)`.
- [x] Add `NoMoreDay::Constants::Lighting` presets.

### Task 1.2: ECS light component
- [x] Create `src/game/components/LightComponent.hpp`.
- [x] Define light params (radius, intensity, color, priority, flicker).

### Task 1.3: Render config and quality tiers
- [x] Add `maxLights`, `ambientIntensity`, `ambientColorR/G/B` in `src/engine/render/core/RenderConstants.hpp`.
- [x] Configure tier defaults in `src/engine/render/core/QualityTierManager.cpp`:
  - Low: `maxLights=0`, `ambientIntensity=0.5`
  - Medium: `maxLights=32`, `ambientIntensity=0.3`
  - High: `maxLights=128`, `ambientIntensity=0.25`
  - Ultra: `maxLights=256`, `ambientIntensity=0.2`

### Task 1.4: SSBO binding contract
- [x] Add/rename binding 9 as `SSBO_LIGHT_DATA` in `src/engine/render/RenderConstants.hpp`.

---

## Phase 2: LightManager (Done)

### Task 2.1: Core skeleton
- [x] Create `src/engine/render/lighting/LightManager.hpp`.
- [x] Create `src/engine/render/lighting/LightManager.cpp`.
- [x] Implement `Initialize()`, `Shutdown()`, singleton `Get()`.

### Task 2.2: ECS collection and culling
- [x] Collect lights via `registry.view<Position, LightComponent>()`.
- [x] World-space view culling.
- [x] Priority + distance sorting.
- [x] Clamp by `maxLights`.

### Task 2.3: GPU upload and bind
- [x] Bind SSBO with `SSBO_LIGHT_DATA`.
- [x] Upload with `OrphanAndUpload()` per frame.
- [x] Implement one-frame transient lights via `AddTransientLight()`.

### Task 2.4: Flicker behavior
- [x] Apply time-based intensity modulation for `flicker=true` lights.

---

## Phase 3: LightingPass + Shader (Done)

### Task 3.1: Shader
- [x] Add `assets/shaders/lighting/light_accumulation.frag`.
- [x] Reuse `assets/shaders/postprocess/fullscreen.vert`.
- [x] Use attenuation model `(1 - d^2)^2`.

### Task 3.2: Pass skeleton
- [x] Add `src/engine/render/passes/LightingPass.hpp`.
- [x] Add `src/engine/render/passes/LightingPass.cpp`.
- [x] Implement `Initialize()`, `Shutdown()`, `OnResize()`.

### Task 3.3: Execute pipeline
- [x] Respect `dynamicLightingEnabled` gate.
- [x] Bind HDR input, set uniforms, run fullscreen draw.
- [x] Blit `m_litBuffer` back to `hdrSceneBuffer`.

---

## Phase 4: RenderGraph Integration (Partial)

### Task 4.1: Graph insertion
- [x] Create and hold `g_lightingPass` in `src/engine/render/RenderSystem.cpp`.
- [x] Insert order: `Scene -> Lighting -> VFX`.
- [x] Gate on `dynamicLightingEnabled && useHdrSceneBuffer`.
- [x] Execute `LightManager::Update()` before graph execution.

### Task 4.2: Lifecycle and resize
- [x] Call `LightManager::Initialize()` in render system init.
- [x] Call `LightManager::Shutdown()` in render system shutdown.
- [x] Route resize events to `g_lightingPass->OnResize()`.

### Task 4.3: Low-tier fallback acceptance
- [x] Automated check: Low tier bypasses LightingPass (unit test).
- [x] Manual check: screenshot comparison against Phase 1 baseline.
- [x] Manual check: recorded evidence for Low/Medium switching.

---

## Phase 5: Gameplay Integration (Done)

### Task 5.1: Player light
- [x] Add persistent player aura in `src/game/states/GameplayState.cpp`.

### Task 5.2: Drop lights
- [x] Add Rare+ drop lights in `src/game/systems/item/DropSystem.cpp`.
- [x] Map color/intensity by rarity.

### Task 5.3: Skill-hit flash lights
- [x] Add transient hit flashes in `src/game/systems/skill/ProjectileSystem.cpp`.
- [x] Map parameters by skill type.

---

## Phase 6: Testing and Validation (In Progress)

### Task 6.1: Unit tests (Done)
- [x] Add `tests/unit/LightingTest.cpp`.
- [x] Cover ABI layout, quality config, collect/cull/sort, transient behavior.
- [x] Cover low-tier bypass behavior.

### Task 6.2: Performance benchmark (Done)
- [x] Add `tests/performance/LightingBenchmark.cpp`.
- [x] Use GPU timer query (`GL_TIME_ELAPSED`).
- [x] Cover budgets:
  - 64 lights `<= 0.5ms`
  - 128 lights `<= 0.8ms`
  - 256 lights `<= 1.0ms`

### Task 6.3: Stability validation (Partial)
- [x] Add `tests/integration/LightingStabilityTest.cpp`.
- [x] Automated: repeated resize (>=20) passes.
- [x] Automated: repeated quality tier switching passes.
- [ ] Manual: 30-minute combat stress run.
- [ ] Manual: GPU memory growth observation over time.
- [x] Runtime diagnostics added for long-run validation:
  - periodic lighting cull/budget stats log
  - HDR/Lighting buffer create/resize footprint log

### Task 6.4: Observability Logs for Validation (Done)
- [x] Add non-hot-path logs for quality tier changes (`QualityTierManager::ForceTier`).
- [x] Add settings lifecycle logs (`SettingsState::OnEnter` / `OnExit`).
- [x] Add settings interaction logs (tab switch, quality switch).
- [x] Add startup settings snapshot log in `Game::Game`.

---

## Implemented Files

### New files
- `src/game/components/LightComponent.hpp`
- `src/engine/render/lighting/LightManager.hpp`
- `src/engine/render/lighting/LightManager.cpp`
- `src/engine/render/passes/LightingPass.hpp`
- `src/engine/render/passes/LightingPass.cpp`
- `assets/shaders/lighting/light_accumulation.frag`
- `tests/unit/LightingTest.cpp`
- `tests/performance/LightingBenchmark.cpp`
- `tests/integration/LightingStabilityTest.cpp`

### Updated files
- `src/engine/render/GPUData.hpp`
- `src/engine/render/RenderConstants.hpp`
- `src/engine/render/core/RenderConstants.hpp`
- `src/engine/render/core/QualityTierManager.cpp`
- `src/engine/render/RenderSystem.cpp`
- `src/app/Game.cpp`
- `src/game/states/GameplayState.cpp`
- `src/game/states/SettingsState.cpp`
- `src/game/systems/item/DropSystem.cpp`
- `src/game/systems/skill/ProjectileSystem.cpp`

---

## Definition of Done (Current)

- [x] Implementation completed and `build.bat` passes.
- [x] Lighting unit tests pass.
- [x] Lighting performance benchmark passes.
- [x] Resize/tier-switch automated stability checks pass.
- [x] Manual low/medium visual fallback acceptance recorded.
- [ ] 30-minute in-game stress validation.
- [ ] GPU memory stability validation.

---

*Version: 1.2 (rewritten to fix encoding corruption)*  
*Updated: 2026-02-12*
