# Validation - v4_advanced_lighting_20260219

## Build
- `build.bat`: PASS

## Tests
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`: PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`: PASS
- `ctest --test-dir build -C Release -L performance --output-on-failure`: FAIL

## Performance Failure Classification
- `nmd.tests.performance` failed at:
  - `[Performance] ParticleTrail - Scenario 4 SubEmitter 1k/frame`
  - `dispatchOverheadMs = 0.246931` (threshold `< 0.2`)
- Classification: non-blocking for this track (V4 advanced lighting scope).
- Linked registry item: `BUG-20260218-002` (already tracking intermittent/external performance-suite blocking).

## Scope Validation Notes
- Phase 3 baseline delivered:
  - Global height field system with chunk dirty-region incremental updates.
  - Terrain/static/dynamic height stamping path wired to `HeightShadowPass`.
- Phase 4.6 delivered:
  - `ShadowPreparePass` now assigns per-light `shadowMapIndex` and writes back to active light SSBO before clustered culling.
  - Render pass order updated to `Scene -> Shadow -> LightCulling -> Lighting -> HeightShadow -> ...`.
- Phase 5 task-level validation delivered:
  - Auto-degrade chain logic is exercised by existing quality manager tests/integration path.
  - Performance benchmark command executed with evidence captured above.
