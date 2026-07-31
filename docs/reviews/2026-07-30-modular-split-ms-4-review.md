# MS-4 Persistence, Scene, and State Ownership Review

## Review Target

- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Evidence: `docs/reports/modular-split-exe-lib-dll/ms-4/evidence.md`
- Standards: `docs/workflows/review.md`, `conductor/code_standard.md`
- Protected exclusion: `docs/designs/modular-split-exe-lib-dll-design.md` is user-owned worktree state and was neither read for review nor staged.

## Round One

**Conclusion:** 提交

### Scope Alignment

- `src/engine/persistence/{GlobalSaveData.hpp, SaveManager.hpp/.cpp, SharedStash.hpp/.cpp}` moved to `src/game/persistence/`.
- `src/engine/scene/{SceneManager.hpp/.cpp, State.hpp, StateManager.hpp/.cpp}` moved to `src/game/scene/`.
- 39 include references updated across 34 files (App 3, Game 33, tests 3); no forwarding headers.
- Content preserved: the six moved files are byte-identical to their HEAD blobs under CRLF-normalized comparison; the four files with self-includes differ only in the expected `engine/` -> `game/` path updates (`SaveManager.cpp` lines 1/3/4, `SceneManager.cpp` line 1, `StateManager.hpp` line 6, `StateManager.cpp` line 1).
- SceneManager async-load gate and destructor `m_loadingFuture.wait()` preserved verbatim; save format, version semantics, and hardcoded `saves/` paths unchanged.
- Ledger reduces `111` to `85`: exactly 26 MS-4 rows removed (14 persistence + 12 scene, including the two `move_to_app` SharedContext rows); no other rows touched.
- No CMake, PCH, Types, `build.bat`, GPU, or P0 rendering work was modified.

### Verification

- `git grep -n "engine/persistence\|engine/scene" -- src tests`: no matches.
- `python scripts/check_module_boundaries.py`: PASS, `85/85` across 22 files.
- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat`: PASS; `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-4-build.log` contains `[Build] Build completed successfully.` and `[Build] All steps completed successfully`; zero error lines.
- Focused tests: `[Unit] SaveManager*` 8/8 (67 assertions), `[Unit] StashSystem*` 4/4 (10 assertions), `[Performance] SaveManager*` 2/2 (4 assertions).
- `git diff --check` excluding the protected design document: PASS; index empty.

### Findings

- **Low** `ms-4/evidence.md:7` wording "byte-for-byte" should be read as CRLF-normalized comparison; the evidence already carries that qualifier and the underlying claim is accurate.

### Accepted Risks

- `src/game/scene/State.hpp` and `StateManager.hpp` retain a Game -> App `SharedContext` dependency. The MS-0 ledger intent for those rows was `move_to_app`; the dependency is now untracked by the ledger (Engine/Core scan scope) and must be resolved before the MS-7 explicit target graph. Recorded as an MS-7 precondition.
- The old `src/engine/persistence/` and `src/engine/scene/` directories remain empty; harmless residue.

### Next Step

- Commit this reviewed MS-4 package; mark MS-4 complete in the plan.