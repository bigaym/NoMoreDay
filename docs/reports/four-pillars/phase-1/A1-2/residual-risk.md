# Phase 1 / A1-2 Residual Risks

## Open risks

1. This slice removes legacy material ABI structs from C++ runtime headers, but archived docs/plans still mention those schemas and may cause confusion if read as current implementation guidance.
2. Shader-side legacy compatibility assumptions were not broadened in this package; convergence is bounded to runtime C++ structure ownership.
3. Additional old-generation GPU payloads may remain in `GPUData.hpp` and require follow-up bounded slices for full Phase 1 convergence.

## Mitigation in next slices

- Update or annotate historical docs that still reference removed material schemas to clearly mark them as superseded by `GPUMaterialDataV3`.
- Continue package-by-package removal of unused legacy GPU payload definitions, each with focused ABI/render contract verification.
- Keep ABI governance tests centered on canonical structures and offsets used by active upload paths.
