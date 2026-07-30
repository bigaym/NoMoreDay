# MS-3 Input and ECS Physics Ownership Review

## Review Target

- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Evidence: `docs/reports/modular-split-exe-lib-dll/ms-3/evidence.md`
- Standards: `docs/workflows/review.md`, `conductor/code_standard.md`
- Protected exclusion: `docs/designs/modular-split-exe-lib-dll-design.md` is user-owned worktree state and was neither read for review nor staged.

## Round One

**Conclusion:** 修改

### Finding

- **Medium** `tests/unit/InputSystemTests.cpp`: the new regression test restored only `UISystem::State.isTyping`. Existing skill-tree, quantity-popup, or Astrolabe state could enter the same input-blocking branch and make the typing assertion pass without proving the typing gate.

### Required Action

- Isolate and restore every UI gate evaluated before `isTyping`, including exact Astrolabe visibility/fade state.

## Round Two

**Conclusion:** 修改

### Findings

- **Medium** `UIAstrolabe::Hide()` retains the fade alpha, so saving only a visible boolean did not guarantee a closed gate or exact state restoration.
- **Low** the evidence referenced an earlier successful build log rather than the latest review run.

### Required Action

- Add a minimal state snapshot/restore seam for `s_visible` and `s_alpha`; run the complete build again and reference its final log.

## Final Acceptance Review

**Conclusion:** 提交

### Scope Alignment

- `InputSystem` moved from `src/engine/input/` to `src/game/systems/input/` without input mapping changes.
- `PhysicsSystem` moved from `src/engine/physics/` to `src/game/systems/physics/` without collision, dash, Taskflow, or public API changes.
- Consumers and focused tests use only the new Game-owned paths. No Engine-to-Game forwarding headers remain.
- The ledger removes exactly 15 resolved reverse edges: six InputSystem plus nine PhysicsSystem entries, reducing `128` to `113` across `33` scanned source files.
- The Astrolabe `VisibilityState` helper saves/restores only the existing visible and fade-alpha fields, so the test can isolate UI gates without changing runtime fade behavior.
- No CMake, PCH, Types, RenderSystem, GPU, RenderGraph, ResourceManager, or other P0 rendering work was modified.

### Verification

- `python scripts/check_module_boundaries.py`: PASS, `113/113` across 33 files.
- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat`: PASS; `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-build-final.log` contains both required build-success markers.
- Input typing regression: PASS, 1 case and 7 assertions.
- Dash tunneling regression: PASS, 1 case and 2 assertions.
- Physics performance coverage: PASS, 3 cases and 3 assertions.
- `git diff --check` excluding the protected design document: PASS.

### Findings

- None.

### Accepted Risks

- `SpatialGrid` remains deferred because its active migration requires handling the stale `RenderSystem` include outside this package.
- `SIMDSpatialGrid` remains deferred until its Game `Position` dependency is separately decoupled while retaining real Engine consumers.
- The ledger intentionally covers direct quoted includes only; transitive, generated, and angle-bracket dependencies are reserved for later audit.

### Next Step

- Commit this reviewed Input/ECS Physics ownership package. Keep MS-3 open for the separately reviewed spatial-grid packages.
