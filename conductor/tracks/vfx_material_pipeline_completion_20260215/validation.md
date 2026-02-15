# VFX Material Pipeline Completion Validation

## Sequence & Schema Contract

Policy source:
- `src/engine/vfx/VFXSequenceManager.hpp` (`VFX_SCHEMA_VERSION`)
- `src/engine/vfx/VFXSequenceManager.cpp` (missing/unsupported schema hard rejection with explicit error text)

Compatibility tests:
- `tests/unit/VFXSequencerTest.cpp` (`Invalid Schema Fallback`)
- `tests/unit/VFXSequencerTest.cpp` (`Missing Schema Version Is Rejected`)
- `tests/unit/MaterialTest.cpp` (`Invalid Json Does Not Override Existing State`)
- `tests/unit/MaterialTest.cpp` (`Missing Schema Version Is Rejected`)

## Lighting Type Contract (Point/Spot/Ambient)

Implemented contract:
- `src/engine/render/GPUData.hpp` (`GPULight` now carries direction/cone/type payload)
- `src/engine/render/lighting/LightManager.cpp` (CPU mapping from `LightComponent` to GPU light type semantics)
- `assets/shaders/lighting/light_accumulation.frag` (Spot cone + AmbientZone accumulation logic)
- `assets/shaders/lighting/volumetric_light.frag` (Spot cone-aware volumetric contribution)

ABI governance:
- `GPU_ABI_VERSION` bumped to `2` in `src/engine/render/GPUData.hpp`
- regenerated include: `assets/shaders/generated/gpu_abi.glslinc`

Coverage:
- `tests/unit/LightingTest.cpp` (`GPULight ABI Layout`)
- `tests/unit/LightingTest.cpp` (`LightType Mapping Spot Ambient Point`)

## Added Stress Sequences

Assets:
- `assets/vfx/material_swap_combo.json`
- `assets/vfx/distortion_overflow_stress.json`

Coverage:
- `tests/unit/VFXSequencerTest.cpp` (`Asset Stress Sequences Available`)

## Cross-Tier Regression Evidence

- Matrix coverage (Low/Medium/High/Ultra) including sequence load, hot reload, resize/context restore:
  - `tests/integration/VFXTierMatrixIntegrationTest.cpp`
- Deterministic swap lifetime:
  - `tests/unit/VFXSequencerTest.cpp` (`MaterialSwap Runtime Lifetime`)
- Low-tier fallback:
  - `tests/unit/VFXSequencerTest.cpp` (`MaterialSwap Fallback On Low Detail`)
- Distortion overflow determinism and cap:
  - `tests/unit/VFXSequencerTest.cpp` (`Distortion Overflow Deterministic Cap`)

## Performance Evidence (2026-02-15)

Command:
- `build.bat ninja perf`

Relevant outputs:
- `VFXSequencer MaterialSwap+Distortion Stress: Mean=0.005ms, P99=0.008ms`  
  (test enforces P95 <= 0.3ms)
- `DistortionPass::Execute (2K@8): Mean=0.008ms, P99=0.008ms`

Performance test implementation:
- `tests/performance/MaterialVFXBenchmark.cpp`
