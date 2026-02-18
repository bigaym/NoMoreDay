# Render V3 Clustered Shader Hardening Plan

> **Track ID**: `render_v3_clustered_shader_hardening_20260218`  
> **TDD Policy**: integration-first for regression reproduction, then fix, then perf recheck.

## Phase 1: Reproduce and Pin

- [ ] Create a minimal failing reproduction note from current integration failure output.
- [ ] Pin exact shader lines and token-level root cause.
- [ ] Add/adjust test assertion so failure reason is explicit and stable.

## Phase 2: Shader and Pass Fix

- [ ] Patch `light_culling.comp` identifier/logic to remove compile error.
- [ ] Run local shader compile path through integration test harness.
- [ ] Verify `LightCullingPass::SucceededThisFrame()` transitions to true on valid setup.

## Phase 3: Contract and Fallback Safety

- [ ] Confirm bindings still align with `BindingRegistry`.
- [ ] Confirm compute->fragment barrier is preserved.
- [ ] Confirm fallback behavior remains deterministic when shader load is forced to fail.

## Phase 4: Gate Recovery

- [ ] Run `build.bat`.
- [ ] Run `build.bat analyze`.
- [ ] Run `build.bat perf`.
- [ ] Store evidence in `validation.md`.

## DoD

- [ ] Clustered integration suite passes.
- [ ] No regression in shadow/material/vfx integration suites.
- [ ] Track evidence is complete and reproducible.
