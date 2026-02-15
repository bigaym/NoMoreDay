# VFX Material Pipeline Completion Spec

> **Track ID**: `vfx_material_pipeline_completion_20260215`  
> **Type**: `feature`  
> **Priority**: P2  
> **Compatibility Policy**: Strong compatibility, fill missing wiring and contract mismatches.

## 1. Goal

Close implementation gaps between current VFX/material pipeline and target design contracts, with minimal visual regression risk.

## 2. Key Gaps to Close

1. `MaterialSwap` event is declared but not wired in runtime sequencer.
2. Distortion runtime cap mismatch (`64` active runtimes vs pass upload cap `32`).
3. Lighting type contract is incomplete relative to declared point/spot/ambient model.
4. Missing explicit behavior policy for unsupported high-detail events on lower tiers.

## 3. Scope

1. `src/engine/vfx/VFXSequencerSystem.*`
2. `src/engine/vfx/VFXTypes.hpp`
3. `src/engine/vfx/VFXSequenceManager.*`
4. `src/engine/render/passes/DistortionPass.*`
5. `src/engine/render/lighting/LightManager.*`
6. `assets/vfx/*.json` (only if schema-compatible updates needed)
7. tests under `tests/unit` / `tests/integration`

## 4. Design Requirements

### 4.1 MaterialSwap Runtime Wiring

1. Define exact target scope (entity-local visual state vs global material remap).
2. Implement deterministic begin/end behavior with duration handling.
3. Ensure tier gating and fallback behavior are explicit.

### 4.2 Distortion Capacity Policy

1. Unify upstream runtime source cap and pass upload cap.
2. Add deterministic selection strategy when over cap.

### 4.3 Lighting Contract Completion

1. Either implement declared light type semantics or formally reduce contract and document.
2. Keep performance-aware constraints for high-count lighting scenarios.

## 5. Non-Goals

1. No rendergraph architectural redesign.
2. No global ABI generation redesign.

## 6. Acceptance Criteria

1. No runtime warning remains for `MaterialSwap` in nominal VFX paths.
2. Distortion source handling is deterministic and tested under overflow.
3. Lighting type behavior is consistent with documented contract.
4. Existing VFX sequences remain backward compatible.
5. `build.bat` passes.

