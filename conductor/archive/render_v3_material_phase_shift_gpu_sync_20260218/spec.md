# Render V3 Material Phase Shift GPU Sync Spec

> **Track ID**: `render_v3_material_phase_shift_gpu_sync_20260218`  
> **Type**: `bugfix`  
> **Priority**: P0  
> **Depends On**: `v3_material_lighting_depth_20260215`, `v3_vfx_lighting_integration_20260215`

## 1. Goal

Ensure `MaterialPhaseShift` runtime events reliably change GPU material shading output each frame while active, and cleanly restore baseline values on expiry.

## 2. Problem Statement

Current flow sets runtime phase-shift state and marks `m_dirty`, but GPU upload source data can remain stale when no material slot write occurs in the same frame.

## 3. Scope

1. `src/engine/render/MaterialManager.cpp/.hpp`
2. `src/engine/vfx/VFXSequencerSystem.cpp`
3. `tests/unit/VFXSequencerTest.cpp`
4. `tests/integration/VFXLightingIntegrationTest.cpp`

## 4. Data and ECS Model

### 4.1 Runtime state
Material runtime scaling is singleton-like state in `MaterialManager`:
1. `m_runtimePhaseShiftActive`
2. `m_runtimeRoughnessScale`
3. `m_runtimeSpecularScale`
4. `m_runtimeEmissiveScale`

### 4.2 Contract
When runtime phase shift changes:
1. GPU material buffer for active upload range must be rebuilt from canonical CPU material state.
2. Rebuild must be deterministic and O(n) over uploaded material count.
3. No heap allocation in hot render path.

## 5. Implementation Strategy

1. Add an explicit "rebuild GPU cache from material slots" path that applies runtime multipliers.
2. Invoke rebuild when runtime shift state changes (set/reset), before SSBO upload.
3. Keep phase shift reset idempotent and safe.

## 6. Risk Controls

1. Preserve existing schema compatibility behavior.
2. Avoid pointer lifetime risks across EnTT operations.
3. Keep per-frame cost bounded; no dynamic allocation in update hot path.

## 7. Acceptance Criteria

1. Unit test proves phase shift actually mutates GPU upload payload values.
2. Unit/integration test proves payload values return to baseline after runtime expiry.
3. Existing VFX schema/tierPolicy tests remain green.
4. No regression in material schema v1/v2 compatibility behavior.
