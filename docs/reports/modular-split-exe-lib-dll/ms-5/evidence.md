# MS-5 UI Presentation Ownership Evidence

## Changes

- Moved `src/engine/render/UIRenderer.hpp` and `UIRenderer.cpp` to `src/game/systems/ui/` (pure file move via `git mv`; namespace `NoMoreDay` unchanged). Git rename detection confirms a clean move: `git diff -M HEAD --stat` reports `src/{engine/render => game/systems/ui}/UIRenderer.cpp | 2 +-` (exactly one line changed: the line-1 self-include) and `src/{engine/render => game/systems/ui}/UIRenderer.hpp | 0` (byte-identical). UIRenderer is immediate-mode raylib drawing with zero RenderGraph/GPU-buffer/shader usage; no behavior change.
- Updated 20 include sites to `#include "game/systems/ui/UIRenderer.hpp"`:
  - Moved self-include: `UIRenderer.cpp` line 1.
  - `src/game/systems/ui/` (11): `UISystem.hpp`, `UIStash.cpp`, `UISkillTalentTree.cpp`, `UISkillHub.cpp`, `UICharacter.cpp`, `UIAstrolabe.cpp`, `UICrafting.cpp`, `UIInventory.cpp`, `UIMinimap.cpp`, `MonsterHealthBarSystem.cpp`, `PlayerHUD.cpp`.
  - `src/game/states/` (7): `SettingsState.cpp`, `PauseState.cpp`, `MosaicEditorState.cpp`, `MainMenuState.cpp`, `InventoryState.cpp`, `HeirloomVaultState.cpp`, `DimensionalLevelSelectState.cpp`.
  - `tests/integration/` (1): `SkillSystemTests.cpp`.
- `tests/tech/UITests.cpp`: updated the three path-sensitive candidate arrays from `src/engine/render/UIRenderer.cpp` to `src/game/systems/ui/UIRenderer.cpp` (with `../` and `../../` variants), at lines 245-247 (`[Tech] InventoryUI - button text uses shared emoji fallback path`), 764-766 (`[Tech] BuffUI - Blood Sea uses buff lane icon and runtime tooltip overrides`), and 935-937 (`[Tech] SkillUI - tooltip uses static preview payload`). No other content in the file changed.
- No forwarding headers; no CMake/PCH/`build.bat`/Types/GPU changes (CMake globs `src/*.cpp`, so no manifest edits). `RenderSystem.cpp`, PlayerHUD MS-6 ledger edges, and all other render/GPU files untouched. `UIRenderer.cpp` keeps its Game -> Engine includes (`engine/resource/AssetLoadingSystem.hpp`, `engine/resource/UIAssetRegistry.hpp`), which are the allowed direction.
- MS-0 ledger `docs/reports/modular-split-exe-lib-dll/ms-0/reverse-dependency-ledger.json`: removed exactly the 14 MS-5 rows (`"milestone": "MS-5"`), 11 for `src/engine/render/UIRenderer.cpp` (`game/components/Progression.hpp`, `game/data/AstrolabeRegistry.hpp`, `game/data/BuffRegistry.hpp`, `game/data/SkillRegistry.hpp`, `game/systems/combat/DamagePipeline.hpp`, `game/systems/combat/StatsSystem.hpp`, `game/systems/item/InventorySystem.hpp`, `game/components/PlayerState.hpp`, `game/systems/ui/UISystem.hpp`, `game/systems/ui/UICrafting.hpp`, `game/systems/skill/SkillDisplayPreviewService.hpp`) and 3 for `src/engine/render/UIRenderer.hpp` (`game/components/ItemComponent.hpp`, `game/components/Buff.hpp`, `game/systems/ui/UIContext.hpp`). `85` -> `71` entries; `git diff` shows 0 additions and 0 edits, only deletions of those 14 rows.
- Plan `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`: `MS-5.1` marked `[x]` with note "Implemented in this package; awaiting review and commit.", `MS-5.2` remains `[ ]` (pending review/commit), `MS-5` marked `[~]` in both the status line and section heading.

## Boundary Ledger

- `python scripts/check_module_boundaries.py`: PASS, `71/71` observed/ledger direct quoted edges; 20 files reported. The 14 removed edges are exactly the 14 MS-5 rows; the two moved files are no longer Engine candidates.

## Verification

- `git grep -n "engine/render/UIRenderer" -- src tests`: no matches (exit 1).
- `python scripts/check_module_boundaries.py`: PASS `71/71` (`[Module Boundary] PASS: ledger and observed reverse edges match.`; `Observed/ledger edges: 71/71; files: 20`).
- `python scripts/check_core_candidate_contract.py`: PASS (`[MS-1 Contract] PASS: Core manifest and Types CMake boundary match.`).
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests (`Ran 25 tests in 7.072s`, `OK`).
- `cmd.exe /c build.bat check`: PASS, all pre-build checks OK (`Check mode: Skipping compilation.`, exit 0).
- `cmd.exe /c build.bat` (full build, RelWithDebInfo) log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-5-build.log`; contains both `[Build] Build completed successfully.` (line 33) and `[Build] All steps completed successfully` (line 41); these lines are printed only when `cmake --build` exits 0 (`build.bat` line 537-542), and the shell exit code was 0.
  - `UIRenderer.cpp` compile evidence (limited to verifiable facts; `build.bat` discards the per-run MSBuild detail log at `build.bat` 536-544, so per-file MSBuild lines are not independently auditable): `build\NoMoreDayCore.dir\RelWithDebInfo\UIRenderer.obj` was rebuilt at 21:43:13 during the verification build, `NoMoreDayCore.lib` was linked, and `bin\NoMoreDay.exe` / `bin\NoMoreDayTests.exe` were freshly linked after it; the wrapper log shows no `C1083`/`error` lines and the process exited 0.
  - Note: the stale per-run MSBuild log `%TEMP%\nomoreday_build_31480_21313.log` from an earlier run in this session contains a trailing `^C` stream artifact; it is a leftover whose deletion was locked, not a failure — every `build.bat` run printed both success markers with exit 0 and produced fresh binaries (the final run's own log was created and deleted normally by `build.bat`).
- Focused tests (`.\bin\NoMoreDayTests.exe --test-case="[Tech] InventoryUI - button text uses shared emoji fallback path,[Tech] BuffUI - Blood Sea uses buff lane icon and runtime tooltip overrides,[Tech] SkillUI - tooltip uses static preview payload,[Tech] UIRenderer - Tooltip Logic Smoke Test"`): PASS, `test cases: 4 | 4 passed | 0 failed | 670 skipped`, `assertions: 20 | 20 passed | 0 failed`, `Status: SUCCESS!`, exit 0. Re-run output preserved at `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-5-ui-tests.log` (independent audit trail; not committed). Note: the log shows named `TEST CASE` lines for the three assertion-bearing cases only; `[Tech] UIRenderer - Tooltip Logic Smoke Test` (UITests.cpp:916) is a no-assertion smoke test (0 assertions), so doctest does not emit a `TEST CASE` header for it — verified separately: `--test-case="[Tech] UIRenderer - Tooltip Logic Smoke Test"` -> 1 passed, 0 assertions, Status: SUCCESS!, exit 0.
- `git diff --check`: PASS (CRLF warnings only, no whitespace errors).
- `git status --short`: only the expected MS-5 file set — 2 renames (`RM`/`R` `src/engine/render/UIRenderer.*` -> `src/game/systems/ui/`), 19 modified consumers (7 states, 11 `src/game/systems/ui/` files, `tests/integration/SkillSystemTests.cpp`, `tests/tech/UITests.cpp`), the ledger and the plan — plus the protected design document `docs/designs/modular-split-exe-lib-dll-design.md` that was already modified before this package; it was not read, edited, or staged. Nothing staged or committed.

## Deferred Scope and Risks

- MS-5.2 (verify UI tests and normal build; review and commit) remains `[ ]`; this package is awaiting independent review and a `提交` conclusion before commit.
- `UIRenderer` still draws via raylib through engine resource registries (`AssetLoadingSystem`, `UIAssetRegistry`); no engine drawing primitive has been extracted, matching the plan's objective of moving only presentation policy.
- `src/engine/render/` retains the remaining render/GPU files; `UIRenderer` is not referenced anywhere under `src` or `tests` by its old path.
- No P0 rendering, GPU, RenderGraph, ResourceManager, `src/pch.hpp`, CMake, or `build.bat` changes; no behavior, namespace, or public API changes.
