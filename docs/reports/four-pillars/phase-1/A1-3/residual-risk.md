# Phase 1 / A1-3 Residual Risks

## Open risks

1. `RenderConfig::clusteredLightingV4Enabled` still exists in config serialization/tier wiring, so other code or tooling may still treat it as meaningful even though this render pass no longer uses it.
2. This bounded slice converges only `LightCullingPass`; other render passes still contain separate V2/V3 fallback and legacy gating routes.
3. The integration suite currently runs as one grouped CTest target, so failures are coarse-grained and can hide which specific render contract regressed without additional targeted doctest runs.

## Mitigation in next slices

- Remove or deprecate `clusteredLightingV4Enabled` from quality config once all dependent paths are converged.
- Continue Phase 1 pass-by-pass cleanup of remaining fallback routes (shadow and lighting fallback logs/routes) with one contract per removed branch.
- Keep using targeted doctest filters alongside integration CTest label runs to retain branch-level evidence for each convergence slice.
