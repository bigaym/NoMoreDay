# RenderGraph Contract Hardening Plan

> **Track ID**: `rendergraph_contract_hardening_20260215`

## Phase 1: Foundation

- [ ] Define resource ownership enums/tags for scene HDR, lit HDR, post LDR, final output.
- [ ] Extend `RenderGraph` node metadata for validation diagnostics.
- [ ] Add debug-only graph validation pass at `Build()`.

## Phase 2: Logic

- [ ] Implement hazard checks: read-before-write, duplicate writers, undeclared resources.
- [ ] Decouple HDR activation from bloom-only gate while preserving compatibility.
- [ ] Introduce explicit ownership transitions in `RenderSystem::render`.

## Phase 3: Integration

- [ ] Route postprocess/distortion transient intermediates via pool-backed allocation path.
- [ ] Standardize pass state boundary helpers and remove ad-hoc state leakage points.
- [ ] Add integration logging for graph ownership transitions.

## Phase 4: Polish & Tests

- [ ] Add unit tests for graph validation behavior.
- [ ] Add integration test for framebuffer ownership across default/offscreen paths.
- [ ] Run `build.bat`, fix regressions, document migration notes.

## Acceptance Gates (DoD)

- [ ] Quantified thresholds: compare against current baseline and keep `RenderSystem::render` P95 frame time regression <= 5% in `bench_rendering_system`; graph validation adds <= 0.2 ms/frame in debug mode.
- [ ] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` including startup, resize rebuild, Alt+Tab/context restore, and default/offscreen framebuffer ownership path checks.
- [ ] ABI migration policy documented and enforced: any RenderGraph-related shader/resource contract change updates ABI/version notes and emits clear compatibility error text instead of silent fallback.
