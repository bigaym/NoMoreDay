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

## Follow-up Review: MS-3.4 SIMDSpatialGrid Decoupling

**Conclusion:** 提交

### Scope Alignment

- `src/engine/physics/SIMDSpatialGrid.hpp` no longer includes `game/components/Common.hpp`; `rebuild<PositionT, View>` and `query<CenterT, Func>` now duck-type the `x/y` coordinate access, with the unused `registry` parameter retained.
- Call sites updated with explicit template arguments: `ProjectileSystem.cpp:43`, `LootGridSystem.cpp:13`, `SIMDSpatialGridTest.cpp` (3 rebuild calls), `SpatialGridBenchmark.cpp:34`; the tests now include `game/components/Common.hpp` explicitly.
- A `query(const float (&)[2], ...)` overload plus private `queryImpl` kernel was required so that untouchable `RenderSystem.cpp:1141` and `ProjectileSystem.cpp:54` call sites keep compiling; center semantics and the SIMD kernel are unchanged.
- The ledger removes exactly one edge (`SIMDSpatialGrid.hpp` -> `game/components/Common.hpp`), reducing `113` to `112` across `32` scanned source files.
- No CMake, PCH, Types, `RenderSystem.cpp`, `SpatialGrid.hpp`, `build.bat`, GPU, or P0 rendering work was modified.

### Verification

- `python scripts/check_module_boundaries.py`: PASS, `112/112` across 32 files (re-run by reviewer).
- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests (re-run by reviewer).
- `cmd.exe /c build.bat`: PASS; `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-4-build.log` contains `[Build] Build completed successfully.` and `[Build] All steps completed successfully`.
- SIMDSpatialGrid focused tests: PASS, 4 cases / 10 assertions; SpatialGrid benchmark: PASS, 1 case.
- No `game/` include matches in `src/engine/physics/SIMDSpatialGrid.{hpp,cpp}`.
- `git diff --check` excluding the protected design document: PASS.

### Findings

- None.

### Accepted Risks

- `PositionT`/`CenterT` duck typing is documented and enforced at instantiation.
- The `const float (&)[2]` overload is a narrow compatibility surface for existing call sites.
- `SpatialHashGrid` (MS-3.5) remains deferred, blocked by the stale `RenderSystem` include.

### Next Step

- Commit this reviewed MS-3.4 package; keep MS-3 open for MS-3.5 spatial-grid migration under separate coordination.

## Follow-up Review: MS-3.5 SpatialHashGrid Migration

**Conclusion:** 修改 (Round One) -> 提交 (Re-verification)

### Round One Finding (High, process)

- `git diff --cached` was non-empty: the `git mv` used for the SpatialHashGrid move left the rename staged. This was a process violation, not a code defect.

### Resolved

- Parent ran `git reset -q`; re-verification confirms `git diff --cached --quiet` exits 0, `--cached --name-status` is empty, and the worktree boundary matches the MS-3.5 package exactly.

### Scope Alignment (Round One verified)

- `src/engine/physics/SpatialGrid.hpp` -> `src/game/systems/physics/SpatialGrid.hpp`, byte-identical (HEAD blob vs new file SHA-256 equal).
- Exactly one line deleted from `src/engine/render/RenderSystem.cpp` (stale `#include "engine/physics/SpatialGrid.hpp"`), user-authorized; no `SpatialHashGrid`/`GridEntry` usage existed in that file.
- 38 consumer includes updated (19 in `src/game`, 19 in `tests`); no old path references and no engine forwarding header remain.
- Ledger removes only the SpatialGrid edge; the 20 RenderSystem rows were renumbered with valid `source:line:include` ids.
- No CMake, PCH, Types, `build.bat`, GPU, or other render changes.

### Verification

- `python scripts/check_module_boundaries.py`: PASS, `111/111` across 31 files.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat`: PASS; `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-5-build.log` contains both success markers; zero error lines.
- Focused tests: `*Dash*` 1/2, `*Physics*` 4/4, `*Hazard*` 8/12, `*Skill*Matrix*` 10/821, `*SIMDSpatialGrid*` 4/10 — all pass.
- `git diff --check` excluding the protected design document: PASS.

### Findings

- None.

### Accepted Risks

- Ledger line numbers for RenderSystem.cpp are position-sensitive; any future single-line edit there requires a matching ledger fix (re-verified at MS-6).
- `SpatialGrid.hpp` now includes `game/components/Common.hpp` as an intra-Game edge, untracked by the reverse-dependency ledger; confirmed no Engine references remain.

### Next Step

- Commit this reviewed MS-3.5 package and close MS-3.
