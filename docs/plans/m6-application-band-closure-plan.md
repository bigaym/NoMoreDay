# M6 Application Band + Closure Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M6
> **Scope:** create the `game/application/` band — a wrapper `CMakeLists.txt` plus the six sub-target directories (`persistence/`, `render/`, `scene/`, `states/`, and `systems/ui/`, `systems/input/` relocated from the systems band) — via directory-level `git mv`; move the two header-only progression headers into `application/states/`; delete `systems/progression/`; rewrite the 257 include sites; then run the reorganization closure checks (zero absolute source entries, directory==target 1:1, `check_module_boundaries.py`, full suite) and update the two design documents.

## 1. Implementation Approach

### 1.1 Scope boundary

M6 is the final physical-layout milestone: it creates the application band, moves the top-band directories into it, and performs the design's closure verification (§5 M6, §7). CMake target names, link edges, properties, PCH, APIs, runtime behavior, and test registration remain unchanged; only `add_subdirectory` wiring and include prefixes change.

The closure item "update `docs/designs/fine-grained-module-split-design.md` §5.4 note (physical convergence done) and this document status" (design §5 M6) is in scope for this plan and is the **only** design-document edit permitted: the two status notes, nothing else (the fine-grained doc's §5.4 explicitly deferred physical relocation to this design; M6 records that it is done).

Design note (R4 resolution): `AchievementSystem.hpp`/`LeaderboardSystem.hpp` move into `application/states/` (their sole consumer is `NightmareFloorState`), not into a new `application/progression/` — per design §3.2 item 9 and the layout in §4.2.

### 1.2 Final ownership map for M6

| Ownership | After M6 | Before M6 |
|---|---|---|
| `NoMoreDayGamePersistence` | `src/game/application/persistence/` | `src/game/persistence/` |
| `NoMoreDayGameRender` | `src/game/application/render/` | `src/game/render/` |
| `NoMoreDayGameScene` | `src/game/application/scene/` | `src/game/scene/` |
| `NoMoreDayGameStates` | `src/game/application/states/` | `src/game/states/` |
| `NoMoreDayGameUi` | `src/game/application/ui/` | `src/game/systems/ui/` |
| `NoMoreDayGameInput` | `src/game/application/input/` | `src/game/systems/input/` |
| (none — header-only) | `src/game/application/states/AchievementSystem.hpp`, `LeaderboardSystem.hpp` | `src/game/systems/progression/` (deleted) |

Each moved sub-target directory carries its `CMakeLists.txt` verbatim. `systems/progression/` has no `CMakeLists.txt` (header-only) and is deleted after its two headers move.

### 1.3 Dependency and call-chain preservation

Seven rewrite rules, `SimpleMatch` line counts over `src/` + `tests/` (verified 2026-08-09). Design-doc estimates are superseded by these exact counts:

| # | Old | New | Sites | Design estimate |
|---|---|---|---|---|
| 1 | `game/persistence/` | `game/application/persistence/` | 7 | 5 |
| 2 | `game/render/` | `game/application/render/` | 36 | 16 |
| 3 | `game/scene/` | `game/application/scene/` | 28 | 28 |
| 4 | `game/states/` | `game/application/states/` | 28 | 21 |
| 5 | `game/systems/ui/` | `game/application/ui/` | 152 | 93 |
| 6 | `game/systems/input/` | `game/application/input/` | 3 | 2 |
| 7 | `game/systems/progression/` | `game/application/states/` | 3 | 1 |
| | **Total** | | **257** | 165 |

Notes:

- Rule 7's new prefix is `application/states/`, not `application/progression/` — the headers land inside `states/` (sole consumer `src/game/states/NightmareFloorState.hpp:6`); the other 2 hits are the file-header path comments inside the two moved headers themselves (updated in the same edit).
- Rules 1-6 include self-band references (e.g. `game/render/` sites inside `application/render/` files, `game/systems/ui/` sites inside the ui files) — all rewritten uniformly, none exempt.
- Rules 2/4 fan-in examples: `game/render/` is consumed heavily by `systems/ui/` (`UIRenderer.cpp`, `AstrolabeRenderer.cpp`, `GPUEntityAdapter.hpp` users) and tests; `game/states/` fan-in centers on `GameplayState` (modified by M2/M3 — no special handling, just another rewrite site).
- Runtime relationships (save services, scene flow, state screens, render adapter, UI, input binding) are compile-time include relationships — unchanged by the path migration.

### 1.4 CMake convergence

- **New `src/game/application/CMakeLists.txt`** — wrapper only, defines no target; internal `add_subdirectory` order mirrors the current `src/game/CMakeLists.txt` lines 31-36 (dependency order):

```cmake
add_subdirectory(scene)       # NoMoreDayGameScene (top)
add_subdirectory(persistence) # NoMoreDayGamePersistence (top)
add_subdirectory(ui)          # NoMoreDayGameUi (top)
add_subdirectory(input)       # NoMoreDayGameInput (top)
add_subdirectory(render)      # NoMoreDayGameRender (top)
add_subdirectory(states)      # NoMoreDayGameStates (top)
```

- **`src/game/CMakeLists.txt` lines 31-36**: the six entries collapse into a single `add_subdirectory(application)      # band: application (scene/persistence/ui/input/render/states)` inserted at line 31's position. Lines 15-30 (`contracts`, `foundation`, `systems/*` middle band) stay untouched. The PUBLIC link list (lines 47-72) is unchanged.
- **Closure edits (design §5 M6):** `docs/designs/fine-grained-module-split-design.md` §5.4 note — record physical convergence done; `docs/designs/directory-structure-reorganization-design.md` — update status (draft → implemented) and mark milestone table complete. No other document edits.

### 1.5 Closure checks (design §5 M6, §7.2-7.4)

- **Zero absolute source entries:** `Select-String -Pattern 'CMAKE_SOURCE_DIR/src'` over all game-side `CMakeLists.txt` — no `add_library` source entry may reference `${CMAKE_SOURCE_DIR}/src/...` (engine-side absolute entries under `src/engine/render/` are the fine-grained split's established convention and are exempt; see M3 plan §1.1/R3).
- **Directory == target 1:1:** for all 22 game sub-targets (5 Engine sub-targets were converged in M2 and are spot-checked only), each target's physical directory holds exactly its own sources — assert by comparing each `add_library` source list against the directory listing. Mapping: `ContractsCore`→`contracts/`, `Contracts`→`contracts/impl/`, `Data`/`Utils`/`Stats`/`UiShared`/`CombatV2`→`foundation/*`, `Modifier`/`Skill`/`World`/`Item`/`Physics`/`Ai`/`Vfx`/`Nemesis`/`Combat`→`systems/*`, `Persistence`/`Scene`/`Ui`/`Input`/`Render`/`States`→`application/*`.
- **`check_module_boundaries.py`** passes unchanged (`scripts/check_module_boundaries.py`, layer prefixes `src/core|src/engine|src/game` unaffected by intra-game moves).
- **Design §7.3 final tree:** `game/` root holds exactly `contracts/ foundation/ systems/ application/` + `pch.hpp` + `CMakeLists.txt`.

## 2. Pseudocode Guidance

```text
moves = [
    ("src/game/persistence",       "src/game/application/persistence"),
    ("src/game/render",            "src/game/application/render"),
    ("src/game/scene",             "src/game/application/scene"),
    ("src/game/states",            "src/game/application/states"),
    ("src/game/systems/ui",        "src/game/application/ui"),
    ("src/game/systems/input",     "src/game/application/input"),
    ("src/game/systems/progression/AchievementSystem.hpp",  "src/game/application/states/AchievementSystem.hpp"),
    ("src/game/systems/progression/LeaderboardSystem.hpp",  "src/game/application/states/LeaderboardSystem.hpp"),
]
for old, new in moves: git mv old new
rmdir src/game/systems/progression          # empty after the two header moves

rules = [
    ("game/persistence/",       "game/application/persistence/"),
    ("game/render/",            "game/application/render/"),
    ("game/scene/",             "game/application/scene/"),
    ("game/states/",            "game/application/states/"),
    ("game/systems/ui/",        "game/application/ui/"),
    ("game/systems/input/",     "game/application/input/"),
    ("game/systems/progression/", "game/application/states/"),
]
for old, new in rules:
    rewrite(old, new, files=src/**/*.hpp,*.cpp + tests/**/*.hpp,*.cpp)  # SimpleMatch per line

# src/game/CMakeLists.txt lines 31-36: 6 add_subdirectory entries -> one add_subdirectory(application)
# create src/game/application/CMakeLists.txt wrapper (see 1.4)
# then closure checks (see 1.5) and the two design-doc note edits
```

## 3. Atomic Tasks

- **T0 — Baseline.** Capture the exact 257-site list per rule (Select-String output to a scratch file); confirm counts match §1.3; confirm `git status` clean.
- **T1 — `git mv`.** Move the 6 directories and 2 progression headers per §2; delete the emptied `systems/progression/`.
- **T2 — Include rewrite.** Apply the 7 rules to `src/` + `tests/`; rule 5 (`ui`, 152) is the largest batch — chunk by directory (`src/game/` then `src/app/` then `tests/`), verifying per-chunk hit counts add up.
- **T3 — CMake.** `src/game/CMakeLists.txt` lines 31-36 → single `add_subdirectory(application)`; create `src/game/application/CMakeLists.txt` wrapper.
- **T4 — Static closure.** Old-prefix zero-residual grep (7 rules); absolute-source-entry grep (game side, §1.5); directory==target 1:1 assertion; `scripts/check_module_boundaries.py` run; final tree shape check (§1.5); `git status` shows only intended moves/rewrites/edits.
- **T5 — Build and test.** `./build.bat` (RelWithDebInfo, log redirected to file); `ctest --test-dir build -C RelWithDebInfo -L unit`, `-L integration`, `-L ci` (serial); `./build.bat check`. Top-band suites receive attention: `GameplaySystems`, `RiftProgressStateTest`, `MapAirWallTest`, `UITests`, `MDIRenderTest`, save/scene suites.
- **T6 — Documentation closure.** Update the fine-grained design §5.4 note and this design's status per §1.4 closure item; run §5 checks; mark checklist `[x]` with evidence; no commit (hand-off).

## 4. Test Method

### 4.1 Test level and regression coverage

Full CI suite (no label/registration change, design §7.5). M6 touches the widest runtime surface (states, save, scene, UI, input, render adapter), so the full `ctest -L ci` run is the gate; the top-band suites in T5 are the highest-fan-in consumers of the moved prefixes.

### 4.2 Verification commands

```text
./build.bat > build_m6.log 2>&1
ctest --test-dir build -C RelWithDebInfo -L unit          # serial
ctest --test-dir build -C RelWithDebInfo -L integration   # serial
ctest --test-dir build -C RelWithDebInfo -L ci            # serial
./build.bat check
python scripts/check_module_boundaries.py                 # or via build.bat check
```

## 5. Completion Definition

1. `src/game/application/` contains exactly `persistence/ render/ scene/ states/ ui/ input/` + `CMakeLists.txt`; `src/game/` root holds exactly `contracts/ foundation/ systems/ application/` + `pch.hpp` + `CMakeLists.txt`; `systems/progression/` deleted.
2. `Select-String` for the 7 old prefixes returns zero hits across `src/` + `tests/`.
3. No game-side `add_library` list contains `${CMAKE_SOURCE_DIR}/src/...` source entries (design §7.2).
4. Directory == target 1:1 assertion passes for all 22 game sub-targets (§1.5); `scripts/check_module_boundaries.py` passes unchanged (design §7.4).
5. `./build.bat` (RelWithDebInfo) builds clean; `ctest -L unit` / `-L integration` / `-L ci` green; `./build.bat check` green.
6. Both design documents carry the convergence/status notes (fine-grained §5.4; directory-structure-reorganization design status); `git status` shows only intended moves/rewrites/edits.

## 6. Risks And Mitigations

- **R1 (largest single-rule batch, `ui`).** 152 sites under one prefix. Mitigation: chunked rewrite with per-chunk verification (T2); milestone isolated to the 7 prefixes, failures attributable to it.
- **R2 (top-band runtime surface).** `GameplayState` (states), save/scene/UI systems are the most-exercised runtime paths. Mitigation: full CI suite (not a subset) is the T5 gate; ctest runs serially.
- **R3 (progression target dir).** Rewriting `game/systems/progression/` to `game/application/states/` (not `application/progression/`) is intentional (§1.1 R4 resolution) but easy to misread. Mitigation: rule 7 is spelled out in §1.3/§2 and verified by T4 grep; the two moved headers' path comments are updated in the same edit.
- **R4 (closure assertion gaps).** The 1:1 directory==target assertion is a hand-rolled check; a missed cross-directory source would pass the prefix greps. Mitigation: the assertion compares each `add_library` list against its directory listing (not just file counts), and the final tree-shape check (§1.5) verifies the band structure.
- **R5 (documentation scope creep).** Closure edits are limited to the two named notes. Mitigation: T6 touches only those two files' designated sections; the design doc's register/milestone sections are updated only per the design's own closure item.

## 7. Handoff To Implementation

Single implementer agent executes T0-T6 in one session (six directory moves + 257 rewrites + CMake wiring + closure checks form one coherent unit; no parallelizable disjoint subsets). All build/test commands run serially on the shared build directory with logs redirected to files. The agent must not commit; it returns the T0/T2 per-rule counts, closure-check outputs, test results, and the exact `git status` file list.
