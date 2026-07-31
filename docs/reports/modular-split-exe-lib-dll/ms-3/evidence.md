# MS-3 Input and ECS Physics Ownership Evidence

## Changes

- Moved `InputSystem` from `src/engine/input/` to `src/game/systems/input/`.
- Moved `PhysicsSystem` from `src/engine/physics/` to `src/game/systems/physics/`.
- Updated active Game and test consumers to the Game-owned headers.
- Added `tests/unit/InputSystemTests.cpp` to verify typing clears movement, attack, and dash input after forcing every other UI gate closed; its scope guard restores typing, skill-tree, quantity-popup, and the exact Astrolabe visibility/fade state.
- Removed the six InputSystem and nine PhysicsSystem stale reverse edges from the MS-0 ledger.

## Boundary Ledger

- `python scripts/check_module_boundaries.py`: PASS, `113/113` observed/ledger direct quoted edges.
- The checker reports 33 source files: the prior 36-file inventory lost the InputSystem implementation and both PhysicsSystem files that carried the removed edges.

## Verification

- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat check`: PASS.
- `cmd.exe /c build.bat` log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-build-final.log`; final run contains both `Build completed successfully` and `All steps completed successfully`.
- `NoMoreDayTests.exe --test-case="[Unit] InputSystem - Typing clears gameplay actions"`: PASS, 1 case and 7 assertions.
- `NoMoreDayTests.exe --test-case="[Bugfix] Dash - Dash Tunneling Reproduction"`: PASS, 1 case and 2 assertions.
- `NoMoreDayTests.exe --test-case="[Performance] PhysicsSystem*"`: PASS, 3 cases and 3 assertions.
- `git diff --check` (excluding the protected design document): PASS.

## Deferred Scope and Risks

- `SpatialGrid` remains deferred because of the stale `RenderSystem` include.
- `SIMDSpatialGrid` remains deferred because it requires separate primitive decoupling.
- No P0 rendering, GPU, RenderGraph, or ResourceManager files were changed.
- The first full-build attempt exposed only the new test's incorrect namespace qualification for global ECS/UI types; it was corrected before the passing final build.

## MS-3.4: SIMDSpatialGrid Primitive Decoupling

## Changes

- `src/engine/physics/SIMDSpatialGrid.hpp`: removed `#include "game/components/Common.hpp"`; templated `rebuild` on `PositionT` (`view.template get<PositionT>(entity)`) and `query` on `CenterT` (duck-typed `.x`/`.y`). Retained the currently unused `registry` parameter. Callback shape `callback(entity, {x, y})` unchanged.
- Added a `query(const float (&)[2], ...)` brace-init overload because existing call sites pass `{x, y}` braced lists (e.g. `RenderSystem.cpp:1141`, `ProjectileSystem.cpp:54`, unit tests); a deduced `CenterT` cannot bind a braced-init-list, and `RenderSystem.cpp` is out of scope. Shared SIMD kernel moved to private `queryImpl(float cx, float cy, ...)`.
- `ProjectileSystem.cpp`: `s_enemyGrid.rebuild<Position>(registry.view<EnemyTag, Position>(), registry);`.
- `LootGridSystem.cpp`: `RenderSystem::s_itemGrid->rebuild<Position>(lootView, registry);`.
- `tests/unit/SIMDSpatialGridTest.cpp` and `tests/performance/SpatialGridBenchmark.cpp`: explicit `<Position>` on `rebuild`; explicit `#include "game/components/Common.hpp"` (previously transitively included).
- Removed the single `src/engine/physics/SIMDSpatialGrid.hpp -> game/components/Common.hpp` edge from the MS-0 ledger (113 -> 112 entries).
- `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`: MS-3.4 marked `[x]`; MS-3.5 (`SpatialHashGrid`) added as deferred; MS-3 milestone remains `[~]`.

## Boundary Ledger (after MS-3.4)

- `python scripts/check_module_boundaries.py`: PASS, `112/112` observed/ledger direct quoted edges (was `113/113`).
- The checker reports 32 source files: the prior 33-file inventory lost `SIMDSpatialGrid.hpp`.

## Verification (MS-3.4)

- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat check`: PASS.
- `cmd.exe /c build.bat` log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-4-build.log`; contains both `[Build] Build completed successfully.` and `[Build] All steps completed successfully`; no error lines.
- `NoMoreDayTests.exe --test-case="*SIMDSpatialGrid*"`: PASS, 4 cases and 10 assertions (3 unit + 1 performance).
- `NoMoreDayTests.exe --test-case="*SpatialGrid*Benchmark*"`: PASS, 1 case and 0 assertions.
- `rg "game/" src/engine/physics/SIMDSpatialGrid.hpp src/engine/physics/SIMDSpatialGrid.cpp`: no matches (verified via equivalent grep since `rg` is not installed on this machine).
- `git diff --check`: PASS (CRLF warnings only).

## Residual Risk Notes (MS-3.4)

- `SIMDSpatialGrid` remains an Engine-layer header whose `rebuild`/`query` template arguments must be supplied by Game callers with the game `Position` type; the duck-typed `.x`/`.y` contract is documented in the header comments but not statically enforced until instantiation.
- The `const float (&)[2]` overload exists solely for untouchable/existing brace-init call sites (`RenderSystem.cpp` is MS-6/P0-scoped); it is a narrow compatibility surface.
- `SpatialHashGrid` (`SpatialGrid.hpp`) still carries a `game/components/Common.hpp` edge, deferred to MS-3.5, blocked by the stale `RenderSystem` include.
- No P0 rendering, GPU, RenderGraph, or ResourceManager files were changed; `SIMDSpatialGrid.cpp` was not modified.
- The protected design document `docs/designs/modular-split-exe-lib-dll-design.md` was already modified in the working tree before this package; it was not read, edited, or staged.

## MS-3.5: SpatialHashGrid Ownership Migration

## Changes

- `git mv src/engine/physics/SpatialGrid.hpp src/game/systems/physics/SpatialGrid.hpp`; file content byte-identical (rename detected by git as `R`).
- Updated `#include "engine/physics/SpatialGrid.hpp"` -> `#include "game/systems/physics/SpatialGrid.hpp"` in all 38 consumer files under `src/game/` and `tests/` (19 Game, 19 test files; verified each had exactly one occurrence; trailing comments preserved).
- `src/engine/render/RenderSystem.cpp`: deleted line 4 `#include "engine/physics/SpatialGrid.hpp"` — the single user-authorized change to this P0-owned file. Evidence it was unused: `git grep -n "SpatialHashGrid\|GridEntry" -- src/engine/render/RenderSystem.cpp` returned no matches. Line 3 (`SIMDSpatialGrid.hpp`) untouched. No other render/GPU file changed.
- MS-0 ledger: removed the single `src/engine/physics/SpatialGrid.hpp:3:game/components/Common.hpp` edge (MS-3, `move_to_game`). Because the authorized RenderSystem.cpp line deletion shifted that file's include lines by one, the 20 existing MS-6 RenderSystem.cpp ledger rows had their `line` and `id` values decremented by 1 (55..73,87 -> 54..72,86) to stay truthful; no rows were added or removed beyond the SpatialGrid edge.
- `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`: MS-3.5 marked `[x]`; MS-3 remains `[~]` until review/commit; deferred-scope note updated (none remaining for MS-3).

## Boundary Ledger (after MS-3.5)

- `python scripts/check_module_boundaries.py`: PASS, `111/111` observed/ledger direct quoted edges (was `112/112`).
- The checker reports 31 source files: the prior 32-file inventory lost `SpatialGrid.hpp`; the MS-3 `move_to_game` group is now empty (last row removed).

## Verification (MS-3.5)

- `git grep -n "engine/physics/SpatialGrid.hpp" -- src tests`: no matches.
- `git grep -n "game/systems/physics/SpatialGrid.hpp" -- src tests`: 38 matches (all consumers; RenderSystem.cpp no longer includes the grid).
- `python scripts/check_module_boundaries.py`: PASS `111/111` (was `112/112`).
- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat check`: PASS.
- `cmd.exe /c build.bat` log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-5-build.log`; contains both `[Build] Build completed successfully.` and `[Build] All steps completed successfully`; no error/failed lines.
- Focused tests (`.\bin\NoMoreDayTests.exe`):
  - `--test-case="*Dash*"` (covers `tests/integration/CollisionReproTest.cpp`; no registered case matches `*CollisionRepro*` — the file's only case is `[Bugfix] Dash - Dash Tunneling Reproduction`): PASS, 1 case and 2 assertions.
  - `--test-case="*Physics*"` (unit `[Tech] PhysicsSystem - Interaction Logic` + 3 performance benchmarks): PASS, 4 cases and 4 assertions.
  - `--test-case="*Hazard*"` (3 unit + 3 performance): PASS, 8 cases and 12 assertions.
  - `--test-case="*Skill*Matrix*"` (SkillKeyNodeMatrix unit + integration + SkillContract): PASS, 10 cases and 821 assertions.
  - `--test-case="*SIMDSpatialGrid*"` (RenderSystem-adjacent grid regression): PASS, 4 cases and 10 assertions.
- `git diff --check`: PASS (CRLF warnings only).
- `git status --short`: only the MS-3.5 file set (rename `R` for SpatialGrid.hpp + modified consumers/docs) plus the protected design document that was already modified before this package.

## Residual Risk Notes (MS-3.5)

- The checker matches ledger rows on exact `(source, line, include_path)` keys; any future single-line edit inside RenderSystem.cpp requires a matching ledger line-number fix (this package applied the -1 shift for the 20 MS-6 rows). MS-6 review will re-verify.
- `SpatialGrid.hpp` still includes `game/components/Common.hpp`; with the file now Game-owned the include is a same-layer Game -> Game edge, so the reverse-dependency ledger no longer tracks it. No further Engine references remain (`src/engine`/`src/app` contain only the `SIMDSpatialGrid` path and forward declarations).
- The RenderSystem.cpp stale include was removed per user authorization; no render behavior changed (include was unused).
- The protected design document `docs/designs/modular-split-exe-lib-dll-design.md` was already modified in the working tree before this package; it was not read, edited, or staged.
