# GPU ABI & Binding Governance Notes

## 1) ABI Versioning Rules
- CPU source of truth: `src/engine/render/GPUData.hpp`
- Shader source of truth: `assets/shaders/generated/gpu_abi.glslinc`
- Current contract macros:
  - `GPU_ABI_VERSION`
  - `GPU_ABI_COMPAT_MIN_VERSION`

### Bump policy
- Any incompatible CPU/GLSL layout or packing change must bump `GPU_ABI_VERSION`.
- Compatibility window is `N/N-1`:
  - current runtime accepts shader ABI versions in `[N-1, N]`.
  - older than `N-1` is unsupported and considered incompatible.

### Runtime behavior
- Debug/dev (`!NDEBUG`): hard-fail on unsupported ABI mismatch.
- Release (`NDEBUG`): log explicit compatibility error and continue only if compatible.

## 2) Generated Artifact Governance
- Generator: `tools/render_abi/generate_gpu_abi.py`
- Manifest: `tools/render_abi/abi_manifest.json`
- Outputs:
  - `assets/shaders/generated/gpu_abi.glslinc`
  - `assets/shaders/generated/material_abi.glslinc`

### Reproducibility
- Generator output must be byte-stable when input files are unchanged.
- `--check` mode is used for no-diff validation.

## 3) Binding Governance
- Global shared bindings are defined in `RenderConstants::Binding`.
- Do not use raw `BindBase(<literal>)` in runtime source files.
- Local compute-only bindings must use named namespaces in `RenderConstants.hpp`
  - `ParticleCS`, `FlowFieldCS`, `StatsScatterCS`, `FogOfWarCS`, etc.

