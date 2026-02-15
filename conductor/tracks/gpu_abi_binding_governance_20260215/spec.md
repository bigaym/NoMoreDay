# GPU ABI & Binding Governance Spec

> **Track ID**: `gpu_abi_binding_governance_20260215`  
> **Type**: `refactor`  
> **Priority**: P1  
> **Compatibility Policy**: Strong compatibility, no behavioral change expected.

## 1. Goal

Enforce CPU/Shader ABI consistency and binding-point governance as first-class engineering contracts.

## 2. Scope

1. `src/engine/render/GPUData.hpp`
2. `src/engine/render/RenderConstants.hpp`
3. shader includes under `assets/shaders/generated`
4. new generator tooling under `tools/render_abi/`
5. ABI/binding tests under `tests/unit` and optional CI scripts under `scripts/`
6. schema checks for material/vfx/texture-array assets

## 3. Design Requirements

### 3.1 ABI Versioning

1. Introduce `GPU_ABI_VERSION` in both CPU and shader-generated include artifacts.
2. Require version match verification at startup in debug/dev builds.

### 3.2 Generated ABI Include

1. Create generator flow to emit canonical GLSL include from CPU data model.
2. Replace manually duplicated struct definitions where feasible.

### 3.3 Binding Registry Enforcement

1. Single authoritative binding registry in render constants.
2. Remove or gate raw literal `BindBase(N)` usage outside approved local compute scopes.
3. Add test/lint-like checks for binding collisions.

### 3.4 Schema Governance

1. Ensure schema-versioned assets have strict validation paths.
2. Standardize failure fallback behavior and structured error logs.

## 4. Non-Goals

1. No pass ordering redesign.
2. No major VFX algorithm changes.

## 5. Acceptance Criteria

1. ABI layout tests remain green and include version checks.
2. Generated shader ABI include is reproducible and used by runtime shaders.
3. Binding conflicts are detectable before runtime failures.
4. Schema validation behavior is consistent across material and vfx assets.
5. `build.bat` passes.

