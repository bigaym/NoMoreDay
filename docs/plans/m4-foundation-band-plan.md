# M4 Foundation Band Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M4
> **Scope:** create the `game/foundation/` band — a wrapper `CMakeLists.txt` plus the five bottom sub-target directories (`data/`, `utils/`, `stats/`, `ui_shared/`, `combat_v2/`) and the two game-root shared headers (`Settings.hpp`, `SharedContext.hpp`) — via directory-level `git mv`; rewire `src/game/CMakeLists.txt`; move the generated `TagRegistry.hpp` path in the root `CMakeLists.txt`; rewrite the 288 include sites referencing the moved prefixes.

## 1. Implementation Approach

### 1.1 Scope boundary

M4 is a physical-layout and include-path migration only. The CMake target graph, public/private link edges, compile definitions, PCH, APIs, runtime behavior, and test registration remain unchanged. Every moved sub-target directory carries its own `CMakeLists.txt` verbatim; only the `add_subdirectory` wiring in `src/game/CMakeLists.txt` and the generated-header path in the root `CMakeLists.txt` change.

`components/` and `registry/` are explicitly deferred to M5 (they are header-only and the largest rewrite; design §5 M5).

**Design deviation (adopted):** design §5 M4 lists only `data/ utils/ ui_shared/ combat_v2/` for the foundation move. `stats/` is absent from that list but is present in the §4.2 target layout (`foundation/stats/` holds `NoMoreDayGameStats` + `AttributePipeline.cpp`, which M3 physically placed in `src/game/stats/`) and is required by the acceptance criterion that every sub-target directory lives inside one of the four bands. `stats/` therefore moves in M4 with the other four bottom sub-target directories. This is the same rationale as design §3.2 item 3 ("move target `game/foundation/stats/`").

### 1.2 Final ownership map for M4

| Ownership | After M4 | Before M4 |
|---|---|---|
| `NoMoreDayGameData` | `src/game/foundation/data/` | `src/game/data/` |
| `NoMoreDayGameUtils` | `src/game/foundation/utils/` | `src/game/utils/` |
| `NoMoreDayGameStats` | `src/game/foundation/stats/` | `src/game/stats/` |
| `NoMoreDayGameUiShared` | `src/game/foundation/ui_shared/` | `src/game/ui_shared/` |
| `NoMoreDayGameCombatV2` | `src/game/foundation/combat_v2/` | `src/game/combat_v2/` |
| (no target, shared base headers) | `src/game/foundation/Settings.hpp`, `src/game/foundation/SharedContext.hpp` | `src/game/Settings.hpp`, `src/game/SharedContext.hpp` |

Each moved directory contains its own `CMakeLists.txt`, which moves verbatim — no per-target content, property, link edge, or `add_dependencies(GenerateTags)` change. `data/` carries the generated `TagRegistry.hpp` (a tracked file; the root `TAG_HEADER` moves with it, see §1.4).

### 1.3 Dependency and call-chain preservation

Seven rewrite rules (five directory prefixes + two basenames), all `SimpleMatch` line counts over `src/` + `tests/` (both quote-form includes; verified 2026-08-09). Design-doc estimates are superseded by these exact counts:

| # | Old | New | Sites | Design estimate |
|---|---|---|---|---|
| 1 | `game/data/` | `game/foundation/data/` | 230 | 160 |
| 2 | `game/utils/` | `game/foundation/utils/` | 7 | 6 |
| 3 | `game/ui_shared/` | `game/foundation/ui_shared/` | 13 | 13 |
| 4 | `game/combat_v2/` | `game/foundation/combat_v2/` | 14 | 2 |
| 5 | `game/stats/` | `game/foundation/stats/` | 8 | 0 (dir empty at design time) |
| 6 | `game/Settings.hpp` | `game/foundation/Settings.hpp` | 2 | 2 |
| 7 | `game/SharedContext.hpp` | `game/foundation/SharedContext.hpp` | 14 | 14 |
| | **Total** | | **288** | |

High-fan-in examples per rule (full T0 list is captured per task):

- Rule 1 (`data/`, 230): `src/app/Game.cpp` (5), `tests/tech/UITests.cpp` (5), `src/game/states/GameplayState.cpp` (4), `src/game/data/SaveData.hpp` (4, self-band), `src/game/systems/world/EnemySpawnSystem.cpp` (4), plus 100+ one-hit files across `src/` and `tests/`.
- Rule 2 (`utils/`, 7): `src/game/contracts/impl/StatsSystem.cpp`, `src/game/stats/AttributePipeline.cpp`, `src/game/systems/combat/MonsterAffixSystem.hpp`, `src/game/systems/combat/XPAwardingSystem.cpp`, `src/game/systems/world/EnemySpawnSystem.cpp`, `src/game/utils/MonsterScaling.cpp` (self), `tests/unit/MonsterScalingTest.cpp`.
- Rule 3 (`ui_shared/`, 13): 12 external (`render/GameplayRenderAdapter.cpp`, `states/InventoryState.cpp`, item/ui system files) + `src/game/ui_shared/UiShared.cpp` (self).
- Rule 4 (`combat_v2/`, 14): 13 external (9 tests: `ConditionIRTests` 3, `ModifierGraphV2*` 4, `CombatV2*`/`DamageKernel*`/`TagDomainV2*`; `systems/combat/DamagePipeline.cpp`) + `src/game/combat_v2/CombatV2RuntimeFacade.cpp` (self).
- Rule 5 (`stats/`, 8): 7 external (`contracts/impl/StatsSystem.cpp`, `render/GPUEntitySync.cpp`, `systems/ui/UIAstrolabe.cpp`, 4 tests) + `src/game/stats/AttributePipeline.cpp` (self).
- Rule 6 (`Settings.hpp`, 2): `src/game/SharedContext.hpp:3` (self-band, moves with the file) and `tests/integration/GameplayRuntimeHarness.hpp:23`.
- Rule 7 (`SharedContext.hpp`, 14): `src/app/Game.hpp:2`, `src/app/GpuGateDriver.hpp:21`, `src/game/render/GameplayRenderAdapter.hpp:2`, `src/game/scene/State.hpp:3`, `src/game/scene/StateManager.hpp:7`, `src/game/states/GameplayState.cpp:2`, `src/game/systems/combat/CombatSystem.cpp:2`, `src/game/systems/combat/HazardSystem.cpp:2`, `src/game/systems/skill/behaviors/BladeFormation.cpp:19`, `src/game/systems/vfx/HoloBladeRenderSystem.hpp:2`, `tests/integration/GameplayRuntimeHarness.hpp:22`, `tests/integration/MDIRenderTest.cpp:4`, `tests/performance/MDIRenderBenchmark.cpp:5`, `tests/performance/RenderingBenchmark.cpp:5`.

`src/game/pch.hpp` includes `game/data/` headers (1 site, rule 1) — rewritten like any other site; the PCH file itself does not move. Runtime relationships (registries accessed through `NoMoreDayGameData`; `Settings`/`SharedContext` consumed by app/engine/game entry points) are compile-time relationships — unchanged by the path migration.

### 1.4 CMake convergence

- **Root `CMakeLists.txt:169`**: `set(TAG_HEADER "${CMAKE_SOURCE_DIR}/src/game/data/TagRegistry.hpp")` → `set(TAG_HEADER "${CMAKE_SOURCE_DIR}/src/game/foundation/data/TagRegistry.hpp")`. Updated in the same commit as the directory move so `GenerateTags` regenerates before any compile (design R2; the `add_custom_command` at lines 171-177 needs no other change).
- **New `src/game/foundation/CMakeLists.txt`** — wrapper only, defines no target; internal `add_subdirectory` order mirrors the current `src/game/CMakeLists.txt` lines 16-20 (dependency order, ui_shared first as the ring-2 break):

```cmake
add_subdirectory(ui_shared)      # NoMoreDayGameUiShared (ring 2 break: item/render/ui interchange)
add_subdirectory(data)           # NoMoreDayGameData
add_subdirectory(utils)          # NoMoreDayGameUtils
add_subdirectory(stats)          # NoMoreDayGameStats
add_subdirectory(combat_v2)      # NoMoreDayGameCombatV2
```

- **`src/game/CMakeLists.txt` lines 16-20**: the five entries collapse into a single `add_subdirectory(foundation)      # band: foundation (ui_shared/data/utils/stats/combat_v2)` inserted at line 16's position. Line 15 (`contracts`) and lines 21-36 (`contracts/impl`, `systems/*`, top-band dirs) stay untouched. The PUBLIC link list (lines 47-72) is unchanged — target names, not paths.
- The header comment at lines 1-13 describes target structure, not physical layout; no edit (its "M4" reference is to the fine-grained split phase, not this milestone).

## 2. Pseudocode Guidance

```text
moves = [
    ("src/game/data",        "src/game/foundation/data"),
    ("src/game/utils",       "src/game/foundation/utils"),
    ("src/game/stats",       "src/game/foundation/stats"),
    ("src/game/ui_shared",   "src/game/foundation/ui_shared"),
    ("src/game/combat_v2",   "src/game/foundation/combat_v2"),
    ("src/game/Settings.hpp",     "src/game/foundation/Settings.hpp"),
    ("src/game/SharedContext.hpp", "src/game/foundation/SharedContext.hpp"),
]
for old, new in moves: git mv old new          # git mv creates foundation/ parents

rules = [
    ("game/data/",         "game/foundation/data/"),
    ("game/utils/",        "game/foundation/utils/"),
    ("game/ui_shared/",    "game/foundation/ui_shared/"),
    ("game/combat_v2/",    "game/foundation/combat_v2/"),
    ("game/stats/",        "game/foundation/stats/"),
    ("game/Settings.hpp",  "game/foundation/Settings.hpp"),
    ("game/SharedContext.hpp", "game/foundation/SharedContext.hpp"),
]
for old, new in rules:
    rewrite(old, new, files=src/**/*.hpp,*.cpp + tests/**/*.hpp,*.cpp)  # SimpleMatch per line

# root CMakeLists.txt line 169:
#   ${CMAKE_SOURCE_DIR}/src/game/data/TagRegistry.hpp
# -> ${CMAKE_SOURCE_DIR}/src/game/foundation/data/TagRegistry.hpp

# src/game/CMakeLists.txt lines 16-20:
#   5 add_subdirectory(<dir>) entries -> one add_subdirectory(foundation)
# create src/game/foundation/CMakeLists.txt wrapper (see 1.4)
```

## 3. Atomic Tasks

- **T0 — Baseline.** Record the exact 288-site list (Select-String output per rule, saved to a scratch file); confirm `git status` clean; confirm the 7 old-prefix counts match §1.3.
- **T1 — `git mv`.** Move the 5 directories and 2 headers per §2; do not delete anything else.
- **T2 — Include rewrite.** Apply the 7 rules to `src/` and `tests/` (`*.hpp`/`*.cpp`); after each rule, verify the expected count was hit; total must be 288.
- **T3 — CMake.** Root `CMakeLists.txt:169` path update; `src/game/CMakeLists.txt` lines 16-20 → single `add_subdirectory(foundation)`; create `src/game/foundation/CMakeLists.txt` wrapper.
- **T4 — Static verification.** `Select-String` for the 7 old prefixes over `src/` + `tests/` returns zero hits; tree check: `game/foundation/` holds exactly `data/ utils/ stats/ ui_shared/ combat_v2/ Settings.hpp SharedContext.hpp CMakeLists.txt`; `git status` shows only the 7 moves + rewrites + 2 CMake edits + new wrapper (and the generated `TagRegistry.hpp` following its directory).
- **T5 — Build and test.** `./build.bat` (RelWithDebInfo, log redirected to file) — must regenerate `TagRegistry.hpp` at the new path and build clean; then `ctest --test-dir build -C RelWithDebInfo -L unit`, `-L integration`, `-L ci` (serial, shared build dir); then `./build.bat check`.
- **T6 — Completion.** Run §5 checks, mark checklist `[x]` with evidence; no commit (hand-off).

## 4. Test Method

### 4.1 Test level and regression coverage

Full CI suite (no label/registration change, design §7.5). Data-dependent suites get special attention: `TalentLayoutTests`, `SkillRegistryMasteryTreeTests`, `BiomeRegistryTest`, `TalentDataTest`, `MonsterAffixTests`, `SystemMechanics`, plus the combat/skill/item/world/ui integration gates that consume `data/` headers (`GameplaySystems`, `SkillSystemTests`, `SkillContractRegistryTests`).

### 4.2 Verification commands

```text
./build.bat > build_m4.log 2>&1                                  # full RelWithDebInfo build
ctest --test-dir build -C RelWithDebInfo -L unit                 # serial
ctest --test-dir build -C RelWithDebInfo -L integration          # serial
ctest --test-dir build -C RelWithDebInfo -L ci                   # serial
./build.bat check
```

GPU-dependent tests must not run concurrently (shared window/context contention).

## 5. Completion Definition

1. `src/game/foundation/` contains exactly `data/ utils/ stats/ ui_shared/ combat_v2/` + `Settings.hpp` + `SharedContext.hpp` + `CMakeLists.txt`; `src/game/` root holds no `Settings.hpp`/`SharedContext.hpp`.
2. Root `CMakeLists.txt:169` points `TAG_HEADER` at `src/game/foundation/data/TagRegistry.hpp`.
3. `src/game/CMakeLists.txt` has one `add_subdirectory(foundation)` replacing lines 16-20; lines 15 and 21-36 untouched.
4. `Select-String` for the 7 old prefixes returns zero hits across `src/` + `tests/`.
5. `./build.bat` (RelWithDebInfo) configures and builds clean (tag regenerated), `ctest -L unit` / `-L integration` / `-L ci` green, `./build.bat check` green.
6. `git status` shows only the intended moves/rewrites/edits.

## 6. Risks And Mitigations

- **R1 (prefix-collision false positives).** `game/data/` etc. could appear in comments or string literals unrelated to includes. Mitigation: rewrites are per-rule SimpleMatch; T4's zero-residual grep plus a review of the per-rule diff; any comment hit is updated consistently (path comments move with their subject).
- **R2 (TagRegistry regeneration).** The generated header moves with `data/`; a stale `TAG_HEADER` breaks every compile. Mitigation: root `CMakeLists.txt:169` and the directory move land in the same commit (T1+T3 before any build); T5 build order guarantees regeneration (design R2).
- **R3 (non-prefix include forms).** Any `#include "data/..."` or relative form without the `game/` prefix escapes the rules. Mitigation: T4 additionally greps `"data/` / `"utils/` / `"stats/` / `"ui_shared/` / `"combat_v2/` without the `game/` prefix; build-failure triage is confined to this milestone's file set (design R1).
- **R4 (design-list gap on `stats/`).** Covered in §1.1 — `stats/` moves in M4 per the §4.2 layout; the plan supersedes the §5 list wording.
- **R5 (shared build directory).** All build/test steps must run serially; no parallel agents for M4.

## 7. Handoff To Implementation

Single implementer agent executes T0-T6 in one session (directory moves + 288 rewrites + generated-header path + CMake wiring form one coherent unit; no parallelizable disjoint subsets). All build/test commands run serially on the shared build directory with logs redirected to files. The agent must not commit; it returns the T0/T2/T4 evidence counts, test results, and the exact `git status` file list.
