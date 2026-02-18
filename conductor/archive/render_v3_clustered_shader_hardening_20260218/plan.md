# Render V3 Clustered Shader Hardening Plan

> **Track ID**: `render_v3_clustered_shader_hardening_20260218`  
> **TDD Policy**: integration-first for regression reproduction, then fix, then perf recheck.

## Phase 1: Reproduce and Pin

- [x] Create a minimal failing reproduction note from current integration failure output.
- [x] Pin exact shader lines and token-level root cause.
- [x] Add/adjust test assertion so failure reason is explicit and stable.

## Phase 2: Shader and Pass Fix

- [x] Patch `light_culling.comp` identifier/logic to remove compile error.
- [x] Run local shader compile path through integration test harness.
- [x] Verify `LightCullingPass::SucceededThisFrame()` transitions to true on valid setup.

## Phase 3: Contract and Fallback Safety

- [x] Confirm bindings still align with `BindingRegistry`.
- [x] Confirm compute->fragment barrier is preserved.
- [x] Confirm fallback behavior remains deterministic when shader load is forced to fail.

## Phase 4: Gate Recovery

- [x] Run `build.bat`.
- [x] Run `build.bat analyze`.
- [x] Run `build.bat perf`.
- [x] Store evidence in `validation.md`.

## DoD

- [x] Clustered integration suite passes.
- [x] No regression in shadow/material/vfx integration suites.
- [x] Track evidence is complete and reproducible.
