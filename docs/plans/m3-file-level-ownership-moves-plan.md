# M3 File-Level Ownership Moves Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M3
> **Scope:** move the three out-of-place file pairs into their owning target directories — `AttributePipeline.*` → `src/game/stats/`, `AirWallRenderer.*` → `src/game/render/`, `VFXSequencerSystem.*` → `src/game/systems/vfx/` — delete the three emptied directories, and eliminate the last `${CMAKE_SOURCE_DIR}/src/` absolute source entries from game-side `add_library` lists.

## 1. Implementation Approach

### 1.1 Scope boundary

M3 is a physical-layout and include-path migration only. The CMake target graph, public/private link edges, compile definitions, PCH, APIs, runtime behavior, and test registration remain unchanged. The three targets and their directories already exist; only the stray source files and the empty-directory leftovers move.

M3 also completes the "zero absolute source entries" acceptance criterion (design §7.2): after M3, no game-side `add_library` list may contain `${CMAKE_SOURCE_DIR}/src/...` source paths. (Engine-side absolute entries inside `src/engine/render/CMakeLists.txt` were already removed by M2 for `SIMDSpatialGrid`; other engine render sources use absolute paths by design of the fine-grained split — the acceptance criterion targets the game side. See §6 R3.)

### 1.2 Final ownership map for M3

| Ownership | Files after M3 | Source location before M3 |
|---|---|---|
| `NoMoreDayGameStats` | `src/game/stats/AttributePipeline.cpp` | `src/game/systems/stats/AttributePipeline.cpp` |
| `NoMoreDayGameStats` | `src/game/stats/AttributePipeline.hpp` | `src/game/systems/stats/AttributePipeline.hpp` |
| `NoMoreDayGameRender` | `src/game/render/AirWallRenderer.cpp` | `src/game/systems/render/AirWallRenderer.cpp` |
| `NoMoreDayGameRender` | `src/game/render/AirWallRenderer.hpp` | `src/game/systems/render/AirWallRenderer.hpp` |
| `NoMoreDayGameVfx` | `src/game/systems/vfx/VFXSequencerSystem.cpp` | `src/game/vfx/VFXSequencerSystem.cpp` |
| `NoMoreDayGameVfx` | `src/game/systems/vfx/VFXSequencerSystem.hpp` | `src/game/vfx/VFXSequencerSystem.hpp` |

After the moves, `src/game/systems/stats/`, `src/game/systems/render/`, and `src/game/vfx/` are empty and are deleted.

### 1.3 Dependency and call-chain preservation

Three canonical prefix rewrites, each basename-specific (the basenames are unique repository-wide, so a per-basename replacement is safe):

```text
old: game/systems/stats/AttributePipeline.hpp   -> new: game/stats/AttributePipeline.hpp
old: game/systems/render/AirWallRenderer.hpp    -> new: game/render/AirWallRenderer.hpp
old: game/vfx/VFXSequencerSystem.hpp            -> new: game/systems/vfx/VFXSequencerSystem.hpp
```

Verified include sites (grep, 2026-08-09; design-doc estimates — 1/1/2 — are superseded by the exact counts below; self-includes move with the files):

`AttributePipeline` — 7 external + 1 self-include:
- `src/game/contracts/impl/StatsSystem.cpp:24`
- `src/game/render/GPUEntitySync.cpp:11`
- `src/game/systems/ui/UIAstrolabe.cpp:14`
- `tests/unit/AttributePipelineTest.cpp:1`
- `tests/integration/AstrolabeLogicTest.cpp:6`
- `tests/integration/GameplaySystems.cpp:24`
- `tests/unit/TalentModifierTest.cpp:9`
- self: `src/game/systems/stats/AttributePipeline.cpp:1`

`AirWallRenderer` — 1 external + 1 self-include:
- `src/game/systems/world/LevelManager.hpp:6`
- self: `src/game/systems/render/AirWallRenderer.cpp:1`

`VFXSequencerSystem` — 6 external + 1 self-include:
- `src/game/states/GameplayState.cpp:85`
- `tests/performance/MaterialVFXBenchmark.cpp:13`
- `tests/performance/VFXLightingIntegrationBenchmark.cpp:6`
- `tests/integration/VFXTierMatrixIntegrationTest.cpp:9`
- `tests/integration/VFXLightingIntegrationTest.cpp:7`
- `tests/unit/VFXSequencerTest.cpp:9`
- self: `src/game/vfx/VFXSequencerSystem.cpp:1`

Runtime call chains (`AttributePipeline::Calculate` from StatsSystem/GPUEntitySync/UIAstrolabe; `AirWallRenderer` owned by LevelManager; `VFXSequencerSystem::Update/DispatchEvent` from GameplayState) are compile-time relationships — unchanged by the path migration.

### 1.4 CMake convergence

Three `add_library` lists reference the stray files via absolute paths; each becomes directory-relative:

- `src/game/stats/CMakeLists.txt:9`: `${CMAKE_SOURCE_DIR}/src/game/systems/stats/AttributePipeline.cpp` → `AttributePipeline.cpp`; header comment line 3 ("Owns systems/stats/AttributePipeline.cpp") → "Owns stats/AttributePipeline.cpp".
- `src/game/render/CMakeLists.txt:21`: `${CMAKE_SOURCE_DIR}/src/game/systems/render/AirWallRenderer.cpp` → `AirWallRenderer.cpp`; header comment line 4 ("plus systems/render/AirWallRenderer.cpp (absolute path)") → "plus render/AirWallRenderer.cpp".
- `src/game/systems/vfx/CMakeLists.txt:11`: `${CMAKE_SOURCE_DIR}/src/game/vfx/VFXSequencerSystem.cpp` → `VFXSequencerSystem.cpp`; header comment line 3 ("plus VFXSequencerSystem.cpp (src/game/vfx, referenced by absolute path)") → "plus VFXSequencerSystem.cpp (systems/vfx)".

All other target properties, definitions, PCH, `add_dependencies(GenerateTags)`, and link edges stay unchanged. `src/game/systems/ui/CMakeLists.txt:7` mentions "stats (AttributePipeline)" in a dependency-description comment only — no edit needed (comment describes the dependency, not the physical location; §6 R4).

**Post-implementation finding (2026-08-09):** the initial survey missed a fourth absolute entry — `src/game/utils/CMakeLists.txt:7` listed `${CMAKE_SOURCE_DIR}/src/game/utils/MonsterScaling.cpp`. It violates the same acceptance criterion (§1.1) and was normalized to `MonsterScaling.cpp` during implementation (same-category minimal edit; no target or link change). The authoritative closure gate is the T5 grep (`${CMAKE_SOURCE_DIR}/src/game`), not the survey table.

## 2. Pseudocode Guidance

```text
move_set = [
    ("src/game/systems/stats/AttributePipeline.cpp", "src/game/stats/AttributePipeline.cpp"),
    ("src/game/systems/stats/AttributePipeline.hpp", "src/game/stats/AttributePipeline.hpp"),
    ("src/game/systems/render/AirWallRenderer.cpp",  "src/game/render/AirWallRenderer.cpp"),
    ("src/game/systems/render/AirWallRenderer.hpp",  "src/game/render/AirWallRenderer.hpp"),
    ("src/game/vfx/VFXSequencerSystem.cpp",          "src/game/systems/vfx/VFXSequencerSystem.cpp"),
    ("src/game/vfx/VFXSequencerSystem.hpp",          "src/game/systems/vfx/VFXSequencerSystem.hpp"),
]
for old, new in move_set: git_mv(old, new)

for source_file in src/** and tests/** *.hpp/*.cpp:
    replace("game/systems/stats/AttributePipeline.hpp", "game/stats/AttributePipeline.hpp")
    replace("game/systems/render/AirWallRenderer.hpp",  "game/render/AirWallRenderer.hpp")
    replace("game/vfx/VFXSequencerSystem.hpp",          "game/systems/vfx/VFXSequencerSystem.hpp")

# CMake: three add_library absolute entries -> bare basenames; fix their location comments.
# Then: delete now-empty src/game/systems/stats/, src/game/systems/render/, src/game/vfx/.
```

Do not add forwarding headers, compatibility aliases, or second copies.

## 3. Atomic Tasks

- [x] **T0. Capture the M3 baseline.** Record `git status --short`, the three absolute entries (lines `stats/CMakeLists.txt:9`, `render/CMakeLists.txt:21`, `systems/vfx/CMakeLists.txt:11`), and exact old-prefix counts for the three basenames under `src/` and `tests/`.
- [x] **T1. Move `AttributePipeline.*` to `game/stats/`.** `git mv` both files; rewrite the 7 external include sites + self-include (`game/systems/stats/AttributePipeline.hpp` → `game/stats/AttributePipeline.hpp`); update `src/game/stats/CMakeLists.txt:9` to `AttributePipeline.cpp` and the location comment (line 3).
- [x] **T2. Move `AirWallRenderer.*` to `game/render/`.** `git mv` both files; rewrite `src/game/systems/world/LevelManager.hpp:6` + self-include; update `src/game/render/CMakeLists.txt:21` to `AirWallRenderer.cpp` and the location comment (line 4).
- [x] **T3. Move `VFXSequencerSystem.*` to `systems/vfx/`.** `git mv` both files; rewrite the 6 external include sites (GameplayState + 5 tests) + self-include; update `src/game/systems/vfx/CMakeLists.txt:11` to `VFXSequencerSystem.cpp` and the location comment (line 3).
- [x] **T4. Delete the emptied directories.** Remove `src/game/systems/stats/`, `src/game/systems/render/`, `src/game/vfx/` (verify each is empty first). Also confirm `src/engine/physics/` is gone from M2.
- [x] **T5. Run static ownership checks.** Grep for zero remaining old-prefix includes under `src/`+`tests/`; grep `add_library` lists for zero remaining `${CMAKE_SOURCE_DIR}/src/game/` source entries on the game side; confirm each moved file exists exactly once in its new directory. Use `git diff --check` and inspect `git diff --stat`/`--name-status` for scope drift.
- [x] **T6. Build and test the actual targets.** Run the commands in §4, first narrow checks then the repository CI gate. Verify both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are produced or updated before reporting completion. *Evidence 2026-08-09: combined M2+M3 build (`./build.bat`, RelWithDebInfo) exit 0 — log ends "All steps completed successfully", 0 error lines; ctest unit 9/9, integration 6/6, ci 1/1 passed (`--output-on-failure`); `./build.bat check` all OK; `Test-Path .\bin\NoMoreDay.exe` and `.\bin\NoMoreDayTests.exe` both True; static checks (zero old-prefix includes, zero game-side `${CMAKE_SOURCE_DIR}/src/game` entries, 4 deleted dirs absent, 8 moved files in place, `git diff --check` exit 0) all pass.*

## 4. Test Method

### 4.1 Test level and regression coverage

Path-only migration; no new behavior or fixture required. Existing coverage for the moved surfaces: `AttributePipelineTest.cpp` (tag filter / struct layout / calculation), `TalentModifierTest.cpp`, `AstrolabeLogicTest.cpp`, `GameplaySystems.cpp` (integration), `VFXSequencerTest.cpp` (unit, shadow pulse / material phase shift / distortion), `VFXTierMatrixIntegrationTest.cpp`, `VFXLightingIntegrationTest.cpp` (+benchmarks), and compile/link coverage via every consumer (StatsSystem, GPUEntitySync, UIAstrolabe, LevelManager, GameplayState).

### 4.2 Verification commands

Run from `D:\PRJ\NoMoreDay` in this order. Redirect high-volume build output to a log. Run ctest invocations serially (parallel runs contend for the headless GPU/window context).

```powershell
./build.bat > "$env:TEMP\nomoreday-m3-build.log" 2>&1
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
./build.bat check
```

Static checks:

```powershell
# No old include may remain (Select-String, since rg is not on PATH here).
Select-String -Path 'src\*','tests\*' -Include '*.hpp','*.cpp' -Pattern 'game/systems/stats/AttributePipeline|game/systems/render/AirWallRenderer|game/vfx/VFXSequencerSystem' -ErrorAction SilentlyContinue

# No game-side add_library may contain an absolute source path.
Select-String -Path 'src\game' -Include 'CMakeLists.txt' -Pattern 'CMAKE_SOURCE_DIR}/src/game' -ErrorAction SilentlyContinue

# Confirm both real build products, not only the test target.
Test-Path .\bin\NoMoreDay.exe
Test-Path .\bin\NoMoreDayTests.exe
```

## 5. Completion Definition

M3 is complete only when all of the following evidence is available:

1. `AttributePipeline.cpp/.hpp` exist only in `src/game/stats/`; `AirWallRenderer.cpp/.hpp` only in `src/game/render/`; `VFXSequencerSystem.cpp/.hpp` only in `src/game/systems/vfx/`.
2. The three directories `src/game/systems/stats/`, `src/game/systems/render/`, `src/game/vfx/` no longer exist.
3. No C++ include under `src/` or `tests/` uses any of the three old paths.
4. No game-side `add_library` source entry uses `${CMAKE_SOURCE_DIR}/src/game/...`; the three CMake manifests compile their sources directory-relative with unchanged target properties.
5. `git diff --check` passes; the diff contains only the planned `git mv` operations, include-prefix updates, CMake location-comment corrections, and directory deletions.
6. `./build.bat` succeeds in RelWithDebInfo; both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are produced.
7. Unit, integration, and `ci` CTest labels pass with `-C RelWithDebInfo`; `./build.bat check` passes.
8. No target name, link edge, compile definition, PCH path, generated-header path, or runtime behavior change.

## 6. Risks And Mitigations

- **R1: basename collision in a global replace.** Each basename is unique repository-wide (verified by grep), so per-basename replacement is safe. Do not replace the directory prefixes `game/systems/stats/`, `game/systems/render/`, `game/vfx/` globally — unrelated includes (e.g., future `stats/`-relative paths) must not be touched.
- **R2: VFXSequencerSystem direction is counterintuitive.** The file moves from `game/vfx/` INTO `systems/vfx/` (it belongs to the `NoMoreDayGameVfx` target, whose directory is `systems/vfx/`; `game/vfx/` is the stray empty shell). Do not reverse the direction.
- **R3: engine-side absolute paths remain after M3.** `src/engine/render/CMakeLists.txt` still lists engine sources via `${CMAKE_SOURCE_DIR}/src/engine/render/...` absolute paths — that is the engine target's established convention from the fine-grained split and is outside M3's game-side acceptance criterion. The design's "zero absolute paths" closure check (§7.2) targets game-side `add_library` lists.
- **R4: dependency-description comments.** Some CMake comments (e.g., `systems/ui/CMakeLists.txt:7`, `render/CMakeLists.txt:8`) mention moved files as dependency descriptions, not location statements. Only edit comments that state a physical location; leave dependency descriptions untouched.
- **R5: unrelated worktree state.** The worktree contains the uncommitted BUG-20260809-003 teardown fix (`tests/main.cpp`, `conductor/bug_registry.md`, `docs/workflows/debugging.md`) plus M2/M3 plan files. Do not revert or fold them into M3 changes; report separately. Any M3 commit, if requested, must exclude the unrelated fix files unless the user says otherwise.

## 7. Handoff To Implementation

Implementation must load `docs/workflows/implementation.md` before editing. The implementer executes T0-T6 in order, updates this checklist from `[ ]` to `[~]`/`[x]` only with evidence, and stops before M4. Any discovery requiring a target/link-graph change or a compatibility header is a design change: update `docs/designs/directory-structure-reorganization-design.md` first, then revise this plan.
