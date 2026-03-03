# Phase 1 / A1-1 Residual Risks

## Open risks

1. Full end-to-end verification of the newly added contract test is blocked by an existing unrelated unity-build collision in the test target (`TalentModifierAdapterTests.cpp` vs `SkillSpecModifierAdapterTests.cpp`).
2. This slice only converges one legacy branch path (offscreen post-process fallback); other runtime toggles (for example V3 runtime toggle handling and GPU text/loot fallback logs) remain and may require later packages.
3. Integration suite currently passes against the last successfully built test binary when test compilation is blocked; this can mask new test registration until the compile issue is resolved.

## Mitigation in next slices

- Resolve/contain the unrelated unity collision (for example by isolating colliding TUs from unity) and re-run build + targeted contract case + integration suite.
- Continue Phase 1 A1 decomposition with similarly bounded removals for additional legacy/fallback runtime routes, each backed by focused render contracts.
- Keep offscreen-safe routing invariant enforced via contract test to prevent fallback branch reintroduction.
