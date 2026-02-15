# RenderGraph Contract Hardening Plan

> **Track ID**: `rendergraph_contract_hardening_20260215`

## Phase 1: Foundation

- [x] Define resource ownership enums/tags for scene HDR, lit HDR, post LDR, final output.
- [x] Extend `RenderGraph` node metadata for validation diagnostics.
- [x] Add debug-only graph validation pass at `Build()`.

## Phase 2: Logic

- [x] Implement hazard checks: read-before-write, duplicate writers, undeclared resources.
- [x] Decouple HDR activation from bloom-only gate while preserving compatibility.
- [x] Introduce explicit ownership transitions in `RenderSystem::render`.

## Phase 3: Integration

- [x] Route postprocess/distortion transient intermediates via pool-backed allocation path.
- [x] Standardize pass state boundary helpers and remove ad-hoc state leakage points.
- [x] Add integration logging for graph ownership transitions.

## Phase 4: Polish & Tests

- [x] Add unit tests for graph validation behavior.
- [x] Add integration test for framebuffer ownership across default/offscreen paths.
- [x] Run `build.bat`, fix regressions, document migration notes.

## Acceptance Gates (DoD)

- [x] Quantified thresholds: compare against current baseline and keep `RenderSystem::render` P95 frame time regression <= 5% in `bench_rendering_system`; graph validation adds <= 0.2 ms/frame in debug mode.
- [x] Cross-tier regression matrix passes on `Low/Medium/High/Ultra` including startup, resize rebuild, Alt+Tab/context restore, and default/offscreen framebuffer ownership path checks.
- [x] ABI migration policy documented and enforced: any RenderGraph-related shader/resource contract change updates ABI/version notes and emits clear compatibility error text instead of silent fallback.
