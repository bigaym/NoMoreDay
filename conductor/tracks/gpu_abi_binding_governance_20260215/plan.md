# GPU ABI & Binding Governance Plan

> **Track ID**: `gpu_abi_binding_governance_20260215`

## Phase 1: Foundation

- [ ] Add explicit ABI version constants and runtime validation hooks.
- [ ] Define generator input format for GPU structs/bindings.
- [ ] Set up `tools/render_abi/` skeleton and invocation path.

## Phase 2: Logic

- [ ] Implement ABI include generator for GLSL (`gpu_abi.glslinc` style).
- [ ] Align shader-side struct usage with generated include integration points.
- [ ] Add baseline tests for generated output stability.

## Phase 3: Integration

- [ ] Enforce central binding registry usage in render systems.
- [ ] Replace unsafe literal binding usage where outside allowed local scopes.
- [ ] Add binding-collision detection checks.

## Phase 4: Polish & Tests

- [ ] Extend ABI unit tests with version mismatch coverage.
- [ ] Add schema validation tests for material/vfx version policies.
- [ ] Run `build.bat` and full test pass; document governance rules in conductor docs.

## Acceptance Gates (DoD)

- [ ] Quantified thresholds: generated ABI artifacts are byte-stable across repeated runs (no diff when inputs unchanged) and generator runtime stays <= 2 s on current repo baseline.
- [ ] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` with shader compile/link, binding registry validation, and runtime resource binding checks enabled.
- [ ] ABI migration policy documented and enforced: `GPU_ABI_VERSION` bump rules, supported compatibility window (N/N-1), and hard-fail behavior for unsupported version mismatch are implemented and tested.
