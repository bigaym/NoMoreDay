# Render V3 Material Phase Shift GPU Sync Validation

## 1. Required Commands

1. `build.bat`
2. `build.bat analyze`
3. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
4. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## 2. Mandatory Evidence

- [x] Unit evidence for active-phase payload change.
- [x] Unit evidence for reset-to-baseline payload change.
- [x] Integration evidence for phase-shift lifecycle.
- [x] No schema regression in material/vfx loaders.

### Evidence Log (2026-02-18)

1. `build.bat` passed.
2. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` passed (`nmd.tests.unit`).
3. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` passed (`nmd.tests.integration`).
4. `build.bat analyze` passed (existing historical analyzer warnings remain; no new failure introduced by this track).
5. Runtime payload checks:
   - `tests/unit/VFXSequencerTest.cpp`: active phase shift changes `pbrLite.x/y` and `emissiveAndIntensity.w`; expiry restores baseline.
   - `tests/integration/VFXLightingIntegrationTest.cpp`: tick-based transition confirms active mutation on next frame and post-duration restoration.

## 3. Risk Regression Checklist

- [x] No new allocations introduced in render hot path.
- [x] No UB/UAF findings introduced in static analysis.
- [x] No degradation in non-phase-shift material rendering path.

## 4. Final Verdict

- Status: `PASS`
- Reviewer: `Codex`
- Date: `2026-02-18`
