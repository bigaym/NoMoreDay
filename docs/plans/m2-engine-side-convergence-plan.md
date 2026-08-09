# M2 Engine-Side Convergence Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M2
> **Scope:** move `SIMDSpatialGrid.*` from `src/engine/physics/` into the owning `NoMoreDayEngineRenderCore` target directory `src/engine/render/`, delete the empty `engine/physics/` directory, and mark `src/engine/audio/` as the reserved home of the future audio module.

## 1. Implementation Approach

### 1.1 Scope boundary

This milestone is a physical-layout and include-path migration only, plus one ownership-comment addition. The existing CMake target graph, public/private link edges, compile definitions, PCH, APIs, namespace declarations, runtime behavior, and test registration remain unchanged.

The engine target graph is fixed (see `docs/designs/directory-structure-reorganization-design.md` and `docs/designs/fine-grained-module-split-design.md`): `NoMoreDayEngineRenderCore` owns `src/engine/render/` (render-core, GPU infrastructure) and additionally compiles `SIMDSpatialGrid.cpp` from `src/engine/physics/` via an absolute path. `src/engine/physics/` contains nothing else, so converging the file into `render/` and removing the directory is a pure ownership alignment with zero target-graph impact.

`SIMDSpatialGrid` is declared in namespace `systems` (not `NoMoreDay`), a pre-existing quirk; M2 does not rename namespaces or symbols.

### 1.2 Final ownership map for M2

| Ownership | Files after M2 | Source location before M2 |
|---|---|---|
| `NoMoreDayEngineRenderCore` | `src/engine/render/SIMDSpatialGrid.cpp` | `src/engine/physics/SIMDSpatialGrid.cpp` |
| `NoMoreDayEngineRenderCore` | `src/engine/render/SIMDSpatialGrid.hpp` | `src/engine/physics/SIMDSpatialGrid.hpp` |

After the move, `src/engine/physics/` is empty and is deleted. `src/engine/audio/AudioSystem.hpp` remains in place; its header comment gains an ownership note marking `src/engine/audio/` as the reserved future `NoMoreDayEngineAudio` module directory.

### 1.3 Dependency and call-chain preservation

The canonical include prefix changes for every consumer:

```text
old: engine/physics/SIMDSpatialGrid.hpp
new: engine/render/SIMDSpatialGrid.hpp
```

Verified include sites (grep, 2026-08-09; the design doc's "4 sites" estimate is superseded by the exact count below — 8 external + 1 self-include):

| File | Line |
|---|---|
| `src/game/ui_shared/UiShared.hpp` | 9 |
| `src/game/render/GameplayRenderAdapter.hpp` | 3 |
| `src/game/render/GameplayRenderAdapter.cpp` | 13 |
| `src/game/systems/item/LootGridSystem.cpp` | 3 |
| `src/game/systems/skill/ProjectileSystem.hpp` | 4 |
| `src/game/systems/skill/ProjectileSystem.cpp` | 4 |
| `tests/performance/SpatialGridBenchmark.cpp` | 3 |
| `tests/unit/SIMDSpatialGridTest.cpp` | 3 |
| `src/engine/physics/SIMDSpatialGrid.cpp` (self, moves with the file) | 1 |

All sites use the quote form `"engine/physics/SIMDSpatialGrid.hpp"`; no angle-bracket form exists. A global replacement of the `engine/physics/SIMDSpatialGrid.hpp` basename is safe because no other file in the repository shares that basename. Note `UiShared.hpp:28`, `ProjectileSystem.hpp:37` reference the type as `systems::SIMDSpatialGrid` — namespace-qualified use is untouched.

### 1.4 CMake convergence

`src/engine/render/CMakeLists.txt:15` lists `${CMAKE_SOURCE_DIR}/src/engine/physics/SIMDSpatialGrid.cpp`. After the move it must list the bare basename `SIMDSpatialGrid.cpp` relative to `src/engine/render/`. Target name, properties, definitions, PCH, dependencies, and link edges stay unchanged. The header comment block (lines 1-12) describes the target's role and does not enumerate `physics`; it needs no edit unless a stale location phrase is found.

### 1.5 Audio directory reservation

Per the design and the user's directive ("音频模块可以暂时预留一个目录"), `src/engine/audio/AudioSystem.hpp` gains a short ownership comment at the top of the file (after line 2) marking `src/engine/audio/` as the reserved home of the future `NoMoreDayEngineAudio` module. No code, symbol, or target change.

## 2. Pseudocode Guidance

```text
git_mv("src/engine/physics/SIMDSpatialGrid.hpp",
       "src/engine/render/SIMDSpatialGrid.hpp")
git_mv("src/engine/physics/SIMDSpatialGrid.cpp",
       "src/engine/render/SIMDSpatialGrid.cpp")

# 8 external sites + the moved self-include; rewrite only this basename.
for source_file in src/** and tests/** *.hpp/*.cpp:
    replace("engine/physics/SIMDSpatialGrid.hpp",
            "engine/render/SIMDSpatialGrid.hpp")

# engine/render/CMakeLists.txt line 15:
#   ${CMAKE_SOURCE_DIR}/src/engine/physics/SIMDSpatialGrid.cpp
# -> SIMDSpatialGrid.cpp

# engine/audio/AudioSystem.hpp: insert ownership comment after line 2.

# After T1-T4: engine/physics/ is empty -> delete directory.
```

Do not add forwarding headers, compatibility aliases, or a second copy of the moved file. `target_include_directories(... ${CMAKE_SOURCE_DIR}/src)` already resolves the new path.

## 3. Atomic Tasks

Tasks are ordered. Each task must leave the worktree in a state the next task can inspect; the complete build gate runs at the end.

- [x] **T0. Capture the M2 baseline.** Record `git status --short`, the absolute entry at `src/engine/render/CMakeLists.txt:15`, and the exact old-prefix count of `engine/physics/SIMDSpatialGrid.hpp` under `src/` and `tests/` (expected 8 external + 1 self-include in the moved `.cpp`). *Evidence 2026-08-09: baseline status = 2 untracked plan files only; CMakeLists.txt:15 = `${CMAKE_SOURCE_DIR}/src/engine/physics/SIMDSpatialGrid.cpp`; grep = 9 matches (8 external + 1 self), matching §1.3 table.*
- [x] **T1. Move `SIMDSpatialGrid.*` into `render/`.** Use `git mv` from `src/engine/physics/` to `src/engine/render/`. Verify no duplicate remains in the old directory. *Evidence: `git mv` succeeded (no index.lock contention); status shows `R src/engine/physics/SIMDSpatialGrid.cpp -> src/engine/render/SIMDSpatialGrid.cpp` and same for `.hpp`; old dir held only these 2 tracked files.*
- [x] **T2. Rewrite the include path.** Replace `engine/physics/SIMDSpatialGrid.hpp` with `engine/render/SIMDSpatialGrid.hpp` in all 8 external sites and in the moved `SIMDSpatialGrid.cpp` self-include. Do not touch namespace-qualified uses (`systems::SIMDSpatialGrid`). Verify no `engine/physics/SIMDSpatialGrid` string remains under `src/` or `tests/`. *Evidence: 9/9 files replaced exactly once (BOM preserved); grep for `engine/physics/SIMDSpatialGrid` in *.hpp/*.cpp = 0; `systems::SIMDSpatialGrid` uses intact at UiShared.hpp:28, UiShared.cpp:8/51, ProjectileSystem.hpp:37.*
- [x] **T3. Make the CMake manifest directory-relative.** Replace the absolute source entry at `src/engine/render/CMakeLists.txt:15` with `SIMDSpatialGrid.cpp`. Update any stale physical-location comment found in the same file; preserve all target properties. *Evidence: diff shows `- ${CMAKE_SOURCE_DIR}/src/engine/physics/SIMDSpatialGrid.cpp` / `+ SIMDSpatialGrid.cpp`; no stale physics comment in header block; target props untouched.*
- [x] **T4. Mark the audio directory as reserved.** Add the ownership comment to `src/engine/audio/AudioSystem.hpp` (content per §1.5). No functional change. *Evidence: single comment line `// 本目录 (src/engine/audio/) 为未来 NoMoreDayEngineAudio 模块的保留目录,请勿移动或合并。` inserted after line 2; rest of file byte-identical.*
- [x] **T5. Run static ownership checks.** Confirm `engine/physics/` is empty and delete it; confirm `SIMDSpatialGrid.*` exist exactly once under `engine/render/`; confirm zero `engine/physics/` references in CMake and in `src/`+`tests/` C++ files. Use `git diff --check` and inspect `git diff --stat`/`git diff --name-status` for scope drift. *Evidence: `engine/physics/` removed (Test-Path = False); `SIMDSpatialGrid.*` count under render = 2 (cpp/hpp); zero `engine/physics` matches in all CMakeLists.txt and src+tests C++ files; `git diff --check` exit 0 (CRLF warnings only); M2 diff = 2 renames + 11 one-line edits only.*
- [x] **T6. Build and test the actual targets.** Run the commands in §4, first narrow checks then the repository CI gate. Verify both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are produced or updated by the RelWithDebInfo build before reporting completion. *Evidence 2026-08-09: combined M2+M3 build (`./build.bat`, RelWithDebInfo) exit 0 — log ends "All steps completed successfully", 0 error lines; ctest unit 9/9, integration 6/6, ci 1/1 passed (`--output-on-failure`); `./build.bat check` all OK; `Test-Path .\bin\NoMoreDay.exe` and `.\bin\NoMoreDayTests.exe` both True; static checks (zero old-prefix includes, zero game-side `${CMAKE_SOURCE_DIR}/src/game` entries, 4 deleted dirs absent, 8 moved files in place, `git diff --check` exit 0) all pass.*

## 4. Test Method

### 4.1 Test level and regression coverage

Path-only migration; no new behavior or fixture required. Existing coverage for the moved surface: `tests/unit/SIMDSpatialGridTest.cpp` (rebuild/query/boundary/empty cases), `tests/performance/SpatialGridBenchmark.cpp`, and compile/link coverage via every consumer (UiShared loot grid, GameplayRenderAdapter, LootGridSystem, ProjectileSystem enemy grid).

### 4.2 Verification commands

Run from `D:\PRJ\NoMoreDay` in this order. Redirect high-volume build output to a log. Run ctest invocations serially (parallel runs contend for the headless GPU/window context).

```powershell
./build.bat > "$env:TEMP\nomoreday-m2-build.log" 2>&1
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
./build.bat check
```

Static checks:

```powershell
# No old include may remain (Select-String, since rg is not on PATH here).
Select-String -Path 'src\*','tests\*' -Include '*.hpp','*.cpp' -Pattern 'engine/physics/SIMDSpatialGrid' -ErrorAction SilentlyContinue

# Confirm both real build products, not only the test target.
Test-Path .\bin\NoMoreDay.exe
Test-Path .\bin\NoMoreDayTests.exe
```

## 5. Completion Definition

M2 is complete only when all of the following evidence is available:

1. `SIMDSpatialGrid.cpp/.hpp` exist only in `src/engine/render/`; `src/engine/physics/` no longer exists.
2. `src/engine/render/CMakeLists.txt` compiles `SIMDSpatialGrid.cpp` with a directory-relative name; target name and all target properties unchanged.
3. No C++ include under `src/` or `tests/` uses `engine/physics/`.
4. `src/engine/audio/AudioSystem.hpp` carries the reserved-module ownership comment; its code is byte-identical otherwise.
5. `git diff --check` passes; the diff contains only the planned `git mv`, the include-prefix updates, the one-line CMake change, and the comment addition.
6. `./build.bat` succeeds in RelWithDebInfo; both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are produced.
7. Unit, integration, and `ci` CTest labels pass with `-C RelWithDebInfo`; `./build.bat check` passes.
8. No target name, link edge, compile definition, PCH path, generated-header path, namespace, or runtime behavior change.

## 6. Risks And Mitigations

- **R1: stale include sites missed.** The design estimated 4 sites; the actual count is 8 external. The T0 baseline and T2 verification use the exact basename `engine/physics/SIMDSpatialGrid`, so any residual site is caught before the build.
- **R2: namespace confusion during review.** The class is `systems::SIMDSpatialGrid` despite living under `engine/`; reviewers may "fix" the namespace. M2 forbids namespace or symbol renames — that is a design change outside this milestone.
- **R3: `engine/audio/` comment language mix.** The file uses Chinese doxygen comments; the ownership note may be written in either English or Chinese but must be a comment only, never an `#if 0` block or code.
- **R4: unrelated worktree state.** The current worktree contains the uncommitted teardown fix (`tests/main.cpp`, `conductor/bug_registry.md`, `docs/workflows/debugging.md` — BUG-20260809-003, pre-existing SEGFAULT fix) plus the two new plan files. Do not revert or fold them into M2 source changes; report separately in status evidence. Any M2 commit, if requested, must exclude the unrelated fix files unless the user says otherwise.

## 7. Handoff To Implementation

Implementation must load `docs/workflows/implementation.md` before editing. The implementer executes T0-T6 in order, updates this checklist from `[ ]` to `[~]`/`[x]` only with evidence, and stops before M3. Any discovery requiring a target/link-graph change, a namespace change, or a compatibility header is a design change: update `docs/designs/directory-structure-reorganization-design.md` first, then revise this plan.
