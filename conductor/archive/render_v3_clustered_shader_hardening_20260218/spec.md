# Render V3 Clustered Shader Hardening Spec

> **Track ID**: `render_v3_clustered_shader_hardening_20260218`  
> **Type**: `bugfix`  
> **Priority**: P0  
> **Depends On**: `v3_clustered_lighting_20260215`, `v3_validation_and_release_gate_20260215`  
> **Target**: recover Step C runtime correctness and Step F integration gate stability.

## 1. Goal

Restore Clustered Lighting from "fallback only" to "compiled + executed + validated" state.

This track addresses:
1. Compute shader compile failure in `assets/shaders/lighting/light_culling.comp`.
2. Missing gate coverage for shader compile regression.
3. Runtime diagnostics needed for deterministic fallback and fast root-cause triage.

## 2. Scope

1. `assets/shaders/lighting/light_culling.comp`
2. `src/engine/render/passes/LightCullingPass.cpp`
3. `tests/integration/ClusteredLightingIntegrationTest.cpp`
4. `tests/performance/ClusteredLightingBenchmark.cpp`
5. release gate integration inputs (if needed for compile-check evidence)

## 3. Risk Model

### 3.1 Failure mode
When the compute shader does not compile, `LightCullingPass` always falls back and the V3 clustered path never executes.

### 3.2 Impact
1. Functional regression against design section 6.
2. Performance data becomes non-representative.
3. Step F gate confidence is degraded.

## 4. Design and Data Contract

### 4.1 Shader safety constraints
1. Avoid reserved keyword collisions in GLSL identifiers.
2. Keep `std430` field order unchanged for:
   - `LightData`
   - `ClusterHeaderData`
   - `ClusterLightIndexData`
   - `ClusterPackedLightData`
3. Keep binding numbers compatible with `BindingRegistry` contract.

### 4.2 Runtime behavior
1. On compile/init failure: deterministic fallback to V2 lighting and structured warning.
2. On success: clustered path enabled when `v3Enabled && clusteredLightingEnabled`.
3. Preserve explicit compute->fragment barrier path.

## 5. Test Strategy

1. Integration: all clustered integration tests must pass in `RelWithDebInfo`.
2. Performance: clustered benchmark must run and emit metric lines.
3. Negative test: forced shader load failure still falls back without crash.

## 6. Acceptance Criteria

1. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` passes.
2. `ClusteredLightingIntegrationTest` no longer fails on shader compile.
3. No new ABI/binding conflict failures.
4. Logs include one clear fallback reason when compile failure is injected.
5. Build + analyze + perf remain green after fix.
