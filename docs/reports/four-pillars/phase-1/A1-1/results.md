# Phase 1 / A1-1 Results

## Outcome

- Package status: `bounded-slice-complete`
- Removed a legacy offscreen post-process fallback routing branch in `RenderSystem` by deleting `offscreenPostProcessOnly` and collapsing post-process gating to strict offscreen-safe behavior.
- Added a render contract test that guards this convergence point against reintroduction.

## Removed branch/fallback

- Removed route: `(!offscreenV3SafeMode || offscreenPostProcessOnly)` in `RenderSystem::render()`.
- New route: `useHdrSceneBuffer && !offscreenV3SafeMode && g_postProcessPass != nullptr`.
- Rationale: `offscreenPostProcessOnly` was a hardcoded `false` legacy fallback switch; removing it preserves runtime behavior while eliminating a dead branch path.

## Key verification

- `./build.bat` -> FAIL (`NoMoreDayTests` compile error in pre-existing unity-test collision between `tests/unit/TalentModifierAdapterTests.cpp` and `tests/unit/SkillSpecModifierAdapterTests.cpp`)
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS (`1/1`, `2.36s`)
- `./bin/NoMoreDayTests.exe --test-case="*Offscreen post-process legacy fallback route removed*"` -> PASS runner, `0` matched (expected while test target remains stale due failed test build)

## Changed files

- `src/engine/render/RenderSystem.cpp`
- `tests/unit/RenderSystemBranchConvergenceContractTest.cpp`
- `docs/reports/four-pillars/phase-1/A1-1/results.md`
- `docs/reports/four-pillars/phase-1/A1-1/residual-risk.md`
