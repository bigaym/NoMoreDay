# Validation - v4_gpu_loot_rendering_20260219

## 2026-02-19 Phase 1 (Tasks 1.1-1.4)

### Implemented
- Added `GPULootInstance` (32B, standard layout) in `src/engine/render/GPUData.hpp`.
- Added `GPULootSystem` skeleton:
  - `src/engine/render/GPULootSystem.hpp`
  - `src/engine/render/GPULootSystem.cpp`
  - Supports SSBO allocation and per-frame sync from ECS loot entities (`LootTag + Position + ItemComponent/GoldComponent`).
- Registered loot SSBO binding in `src/engine/render/RenderConstants.hpp`:
  - `Binding::SSBO_LOOT_INSTANCE`
  - `LootPassBinding::INSTANCE_SSBO`
- Integrated lifecycle/sync hooks in `src/engine/render/RenderSystem.cpp`:
  - `GPULootSystem::Init()` in `RenderSystem::Initialize()`
  - `GPULootSystem::SyncDroppedItems(registry)` in `RenderSystem::render(...)`
  - `GPULootSystem::Shutdown()` in `RenderSystem::Shutdown()`
- Updated binding governance registry:
  - `src/engine/render/core/BindingRegistry.cpp`
- Updated GPU ABI governance:
  - `tools/render_abi/abi_manifest.json` adds `GPULootInstance`
  - Regenerated `assets/shaders/generated/gpu_abi.glslinc`
  - Added layout assertions in `tests/unit/GPUABIGovernanceTest.cpp`

### Verification Evidence
- `python tools/render_abi/generate_gpu_abi.py` PASS (`gpu_abi_changed=True`).
- `.\build.bat` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS (1/1).
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS (1/1).

## 2026-02-19 Phase 2-4 (Tasks 2.1-4.5)

### Implemented
- Implemented full GPU loot pipeline in `src/engine/render/GPULootSystem.hpp` and `src/engine/render/GPULootSystem.cpp`:
  - frustum cull compute dispatch
  - indirect args compute dispatch
  - grid hash + repulsion + position update compute chain
  - indirect loot quad rendering (multi-draw path with fallback)
- Added loot shaders:
  - `assets/shaders/loot/loot_frustum_cull.compute`
  - `assets/shaders/loot/loot_indirect_args.compute`
  - `assets/shaders/loot/loot_grid_hash.compute`
  - `assets/shaders/loot/loot_repulsion.compute`
  - `assets/shaders/loot/loot_position_update.compute`
  - `assets/shaders/loot/loot_quad.vert`
  - `assets/shaders/loot/loot_quad.frag`
- Added RenderGraph pass type:
  - `src/engine/render/passes/GPULootPass.hpp`
  - `src/engine/render/passes/GPULootPass.cpp`
- Integrated `GPULootPass` into `src/engine/render/RenderSystem.cpp` pass chain and added CPU/GPU loot route switch.
- Extended RenderGraph pass-order contract with `GPULootPass` in `src/engine/render/graph/RenderGraph.cpp`.
- Extended profiler contracts with `RenderPassId::GPULoot` and budget `0.20ms`:
  - `src/engine/render/debug/RenderProfiler.hpp`
  - `src/engine/render/debug/RenderProfiler.cpp`
- Added feature/tier routing fields in render config:
  - `gpuLootEnabled`
  - `gpuLootGlowEnabled`
  - File: `src/engine/render/core/RenderConstants.hpp`
- Added settings override support and tier policy wiring for `render.gpuLoot.enabled`:
  - `src/engine/render/core/QualityTierManager.hpp`
  - `src/engine/render/core/QualityTierManager.cpp`
- Added tests:
  - `tests/unit/QualityTierManagerTest.cpp` (GPULoot tier matrix + feature flag switch)
  - `tests/performance/RenderingBenchmark.cpp` (profiler pass list includes GPULoot)

### Verification Evidence
- `.\build.bat` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS (1/1).
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS (1/1).
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS (1/1).
- `ctest --test-dir build -C Release -L performance --output-on-failure` PASS (1/1).
