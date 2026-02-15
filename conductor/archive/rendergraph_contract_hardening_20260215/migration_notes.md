# RenderGraph Contract Migration Notes

## Contract Version
- `RENDERGRAPH_CONTRACT_VERSION = 1` in `src/engine/render/graph/RenderGraph.hpp`.

## Version Bump Rules
1. Bump the version when resource tags, owner rules, or pass ownership ordering semantics change.
2. Keep N/N-1 compatibility at most; older contracts are unsupported.
3. Any pass introducing known resources must use tag+owner declarations, not legacy string-only declarations.

## Runtime Enforcement
1. Build-time validation emits explicit `RenderGraph[vX]` error text.
2. Debug builds fail fast (`std::logic_error`) on contract violations.
3. Release builds keep diagnostics in logs and prevent silent fallback behavior.

## Compatibility Impact in This Track
1. Known resource names are now contract-bound:
   - `SceneColor`, `SceneDepth`, `PostProcessColor`, `DistortionColor`, `BackBuffer`.
2. Ownership transitions are explicit in `RenderSystem::render` and `CompositePass` input selection.
3. Legacy passes using string-only known-resource writes will fail validation in debug.
