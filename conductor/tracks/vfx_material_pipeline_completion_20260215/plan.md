# VFX Material Pipeline Completion Plan

> **Track ID**: `vfx_material_pipeline_completion_20260215`

## Phase 1: Foundation

- [x] Finalize MaterialSwap runtime semantics and data path.
- [x] Define distortion overflow behavior contract.
- [x] Confirm lighting-type contract decisions (implement vs narrow).

## Phase 2: Logic

- [x] Implement `MaterialSwap` execution path in `VFXSequencerSystem`.
- [x] Add state lifecycle handling for swap duration/end reset.
- [x] Unify distortion caps and deterministic eviction/priority logic.

## Phase 3: Integration

- [x] Integrate behavior with quality-tier detail filtering.
- [x] Update vfx sequence loading/validation messages for new behavior.
- [x] Add or update sequences to exercise MaterialSwap and distortion stress paths.

## Phase 4: Polish & Tests

- [x] Unit tests for MaterialSwap dispatch and lifetime behavior.
- [x] Unit/integration tests for distortion overflow determinism.
- [x] Run `build.bat`, fix regressions, and document final behavior.

## Acceptance Gates (DoD)

- [x] Quantified thresholds: `MaterialSwap` and distortion handling add <= 0.3 ms/frame P95 overhead in VFX stress scene; distortion overflow drop/evict count is logged and bounded by configured cap every frame.
- [x] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` including sequence load, hot reload, resize rebuild, and Alt+Tab/context restore while preserving deterministic swap/lifetime behavior.
- [x] ABI migration policy documented and enforced: sequence/material schema versioning defines backward compatibility and fallback; unsupported versions fail validation with explicit error text.
