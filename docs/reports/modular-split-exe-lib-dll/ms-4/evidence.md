# MS-4 Persistence, Scene, and State Ownership Evidence

## Changes

- Moved five persistence files from `src/engine/persistence/` to `src/game/persistence/`: `GlobalSaveData.hpp`, `SaveManager.hpp`, `SaveManager.cpp`, `SharedStash.hpp`, `SharedStash.cpp`.
- Moved five scene/state files from `src/engine/scene/` to `src/game/scene/`: `SceneManager.hpp`, `SceneManager.cpp`, `State.hpp`, `StateManager.hpp`, `StateManager.cpp`.
- Content preserved byte-for-byte except the moved files' self-includes: `SaveManager.cpp` lines 1/3/4, `SceneManager.cpp` line 1, `StateManager.hpp` line 6, `StateManager.cpp` line 1 (`engine/persistence/` -> `game/persistence/`, `engine/scene/` -> `game/scene/`). The other six files are byte-identical to their `HEAD` blobs (CRLF-normalized comparison). SceneManager async-load gate (`State::WAIT_FOR_FUTURE`, `std::async` call sites) and destructor `m_loadingFuture.wait()` preserved verbatim.
- Updated 39 include references across 34 files (`src/app` 3, `src/game` 33, `tests` 3): App (`Game.cpp`, `Game.hpp`), all 11 state file pairs, `CombatSystem.cpp`, `StashSystem.cpp`, `UIStash.cpp`, `PortalSystem.{hpp,cpp}`, `SaveManagerBenchmark.cpp`, `StashSystemTest.cpp`, `SystemMechanics.cpp`, plus the self-includes listed above. `SharedStash.cpp` keeps its relative `#include "SharedStash.hpp"` (unchanged). `UIStash.cpp` trailing `// ADDED` comment preserved.
- No forwarding headers, no Types-layer abstractions, no CMake/PCH/`build.bat` changes (CMake globs `src/*.cpp`, so no manifest edits). `saves/` paths and save format untouched.
- MS-0 ledger: removed the 26 entries owned by the moved files (14 persistence + 12 scene rows, including the two `move_to_app` SharedContext rows for `State.hpp`/`StateManager.hpp` and the `SceneManager.cpp` rows); `111` -> `85` entries.

## Boundary Ledger

- `python scripts/check_module_boundaries.py`: PASS, `85/85` observed/ledger direct quoted edges (was `111/111`).
- The checker reports 22 source files: the prior 31-file inventory lost the ten moved files that carried the removed edges.

## Verification

- `git grep -n "engine/persistence\|engine/scene" -- src tests`: no matches (exit 1). Verified additionally with a UTF-8-aware scan of the moved files under `src/game/persistence/` and `src/game/scene/`: no matches.
- `python scripts/check_module_boundaries.py`: PASS `85/85`.
- `python scripts/check_core_candidate_contract.py`: PASS (`[MS-1 Contract] PASS: Core manifest and Types CMake boundary match.`).
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests (`Ran 25 tests`, `OK`).
- `cmd.exe /c build.bat check`: PASS (`Check mode: Skipping compilation.`, exit 0).
- `cmd.exe /c build.bat` log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-4-build.log`; contains both `[Build] Build completed successfully.` (line 33) and `[Build] All steps completed successfully` (line 41); no `error C`/`error LNK`/`fatal error`/`FAILED` lines.
- Focused tests (`.\bin\NoMoreDayTests.exe`, case names confirmed via `--list-test-cases`):
  - `--test-case="[Unit] SaveManager*"`: PASS, 8 cases and 67 assertions.
  - `--test-case="[Unit] StashSystem*"`: PASS, 4 cases and 10 assertions.
  - `--test-case="[Performance] SaveManager*"`: PASS, 2 cases and 4 assertions.
- `git diff --check`: PASS (CRLF warnings only).
- `git status --short`: only the expected MS-4 file set (10 deleted `src/engine/` files, 30 modified consumer files, untracked `src/game/persistence/` and `src/game/scene/` directories, ledger and plan updates) plus the protected design document `docs/designs/modular-split-exe-lib-dll-design.md` that was already modified before this package; it was not read, edited, or staged. Nothing staged or committed.

## Deferred Scope and Risks

- **SharedContext follow-up precondition for MS-7:** the now Game-owned `src/game/scene/State.hpp` and `StateManager.hpp` still include `app/SharedContext.hpp` (Game -> App edge). The MS-0 ledger intent was `move_to_app`, but after this migration the edge is a same-monolith Game -> App include; it must be resolved before MS-7 builds the explicit target graph (Game must not depend on App). It is not visible to the reverse-dependency ledger because that ledger scans Engine/Core candidates only.
- `src/engine/persistence/` and `src/engine/scene/` directories are now empty and contain no files.
- No P0 rendering, GPU, RenderGraph, ResourceManager, `src/pch.hpp`, CMake, or `build.bat` changes.
- No behavior, save format, `saves/` path, namespace, or public API changes.
