# Tier Detection & Auto-Degrade Spec

> **Track ID**: `tier_detection_autodegrade_20260215`  
> **Type**: `feature`  
> **Priority**: P1  
> **Compatibility Policy**: Strong compatibility, quality reduction only when budget pressure is detected.

## 1. Goal

Implement hardware capability detection and runtime auto-degrade policies aligned with GPU rendering system goals:

1. robust initial tier classification,
2. measurable pass-budget enforcement,
3. deterministic degrade sequence under sustained overload.

## 2. Scope

1. `src/engine/render/core/QualityTierManager.*`
2. `src/engine/render/debug/RenderProfiler.*`
3. `src/engine/render/RenderSystem.cpp`
4. `tests/performance/RenderingBenchmark.cpp`
5. `tests/performance/*` (supporting benchmark profiles)
6. optional settings serialization touchpoints in `src/app/Settings.hpp`

## 3. Design Requirements

### 3.1 Capability Probe

Probe and record:

1. `GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS`,
2. `GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS`,
3. `GL_MAX_COMPUTE_WORK_GROUP_SIZE`,
4. `GL_MAX_TEXTURE_SIZE`,
5. `GL_MAX_ARRAY_TEXTURE_LAYERS`,
6. `GL_MAX_IMAGE_UNITS`.

### 3.2 Tier Classification

Tier assignment must combine:

1. hard capability floor,
2. short synthetic baseline benchmark,
3. persisted user override precedence.

### 3.3 Auto-Degrade Sequence

Under sustained budget overflow:

1. reduce bloom mips,
2. disable distortion,
3. reduce max lights,
4. reduce particle/sub-emitter budgets,
5. drop optional volumetric features.

### 3.4 Observability

All tier and degrade decisions must be logged with reason codes and current pass timings.

## 4. Non-Goals

1. No shader ABI refactor.
2. No rendergraph ownership redesign.

## 5. Acceptance Criteria

1. Startup logs include capability snapshot + selected tier source.
2. Auto-degrade can be reproduced in pressure benchmark scenarios.
3. Degrade actions are reversible when frame budget recovers.
4. Existing manual quality override path still works.
5. `build.bat` and performance tests pass.

