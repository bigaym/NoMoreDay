# M1 Contract Band Physical Convergence Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M1
> **Scope:** physically align the contract headers and contract implementation sources with the existing `NoMoreDayGameContractsCore` and `NoMoreDayGameContracts` targets.

## 1. Implementation Approach

### 1.1 Scope boundary

This milestone is a physical-layout and include-path migration only. The existing CMake target graph, public/private link edges, compile definitions, PCH, APIs, runtime behavior, and test registration remain unchanged.

The contract targets already exist in:

- `src/game/contracts/CMakeLists.txt`: `NoMoreDayGameContractsCore`, an `INTERFACE` target carrying four contract headers through the `src/` include root.
- `src/game/contracts/impl/CMakeLists.txt`: `NoMoreDayGameContracts`, a `STATIC` target compiling six implementation translation units.

The current mismatch is that the target directories are empty while their sources remain in `src/game/systems/combat/`. `src/game/systems/combat/CMakeLists.txt` already excludes these six implementation units and compiles only the remaining combat-domain systems. M1 must preserve that boundary.

### 1.2 Final ownership map for M1

The following files move with `git mv`:

| Ownership | Files after M1 | Source location before M1 |
|---|---|---|
| `NoMoreDayGameContractsCore` | `src/game/contracts/CombatEvents.hpp` | `src/game/systems/combat/CombatEvents.hpp` |
| `NoMoreDayGameContractsCore` | `src/game/contracts/CombatFormula.hpp` | `src/game/systems/combat/CombatFormula.hpp` |
| `NoMoreDayGameContractsCore` | `src/game/contracts/DamagePipelineTypes.hpp` | `src/game/systems/combat/DamagePipelineTypes.hpp` |
| `NoMoreDayGameContractsCore` | `src/game/contracts/DamageResolutionHooks.hpp` | `src/game/systems/combat/DamageResolutionHooks.hpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/CombatAntiMeta.hpp/.cpp` | `src/game/systems/combat/CombatAntiMeta.hpp/.cpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/CombatEventDispatcher.hpp/.cpp` | `src/game/systems/combat/CombatEventDispatcher.hpp/.cpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/CombatTelemetry.hpp/.cpp` | `src/game/systems/combat/CombatTelemetry.hpp/.cpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/ProcBudgetManager.hpp/.cpp` | `src/game/systems/combat/ProcBudgetManager.hpp/.cpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/StatsSystem.hpp/.cpp` | `src/game/systems/combat/StatsSystem.hpp/.cpp` |
| `NoMoreDayGameContracts` | `src/game/contracts/impl/DamageResolutionHooks.cpp` | `src/game/systems/combat/DamageResolutionHooks.cpp` |

`DamageResolutionHooks.hpp` is intentionally not duplicated under `impl/`: it is part of the four-header `ContractsCore` surface, while `DamageResolutionHooks.cpp` remains compiled by the `Contracts` static library.

After the move, `src/game/systems/combat/` keeps its combat-domain implementation files, including `CombatSystem.cpp`, `DamagePipeline.cpp`, `AilmentEngine.cpp`, `ProgressionSystem.cpp`, and the other files listed by its current CMake source list. No combat source is moved merely because it includes a contract header.

### 1.3 Dependency and call-chain preservation

The graph investigation confirms that the move must be transparent to the following existing paths:

- `DamagePipeline` registers the implementation callback through `RegisterDamageResolutionHooks`; skill callers reach `ResolveDamage` and `ResolveDamageBatch` through the contract header.
- `CombatEventDispatcher::Dispatch` calls `ProcBudgetManager::RequestEventEmit` and `CombatTelemetry::RecordCombatEvent`; its callers remain in combat, item, skill, and movement systems.
- `ProcBudgetManager::RequestProc` is called from `AilmentEngine`, `CombatEventDispatcher`, `SkillSystem`, and its unit/integration tests.
- `CombatTelemetry.hpp` depends on `CombatEvents.hpp`; `DamageResolutionHooks.hpp` depends on `DamagePipelineTypes.hpp`.

These relationships are compile-time include relationships and runtime call paths, not new dependencies. The migration changes only the canonical include prefix:

```text
old: game/systems/combat/<contract-or-contract-implementation-header>.hpp
new: game/contracts/<contract-header>.hpp
new: game/contracts/impl/<implementation-header>.hpp
```

The rewrite must be basename-specific. A global replacement of `game/systems/combat/` is forbidden because ordinary combat-domain headers remain in `src/game/systems/combat/` until later milestones.

### 1.4 CMake convergence

`src/game/contracts/impl/CMakeLists.txt` currently lists six sources through absolute paths. After the move it should list the same six basenames relative to `src/game/contracts/impl/`. The target and every compile/link property remain unchanged.

`src/game/contracts/CMakeLists.txt` has no source list because `NoMoreDayGameContractsCore` is an `INTERFACE` library. Its comments should be updated to describe the headers as physically located in `src/game/contracts/`, rather than `src/game/systems/combat/`. No source should be added to the interface target.

The parent `src/game/CMakeLists.txt` already adds `contracts` before `contracts/impl`; it requires no ordering or target-link change.

## 2. Pseudocode Guidance

The implementation should follow this sequence. The snippets are guidance, not complete code.

```text
contract_headers = [
    CombatEvents.hpp,
    CombatFormula.hpp,
    DamagePipelineTypes.hpp,
    DamageResolutionHooks.hpp,
]

implementation_headers = [
    CombatAntiMeta.hpp,
    CombatEventDispatcher.hpp,
    CombatTelemetry.hpp,
    ProcBudgetManager.hpp,
    StatsSystem.hpp,
]

implementation_sources = [
    CombatAntiMeta.cpp,
    CombatEventDispatcher.cpp,
    CombatTelemetry.cpp,
    ProcBudgetManager.cpp,
    StatsSystem.cpp,
    DamageResolutionHooks.cpp,
]

for file in contract_headers:
    git_mv("src/game/systems/combat/" + file,
           "src/game/contracts/" + file)

for file in implementation_headers + implementation_sources:
    git_mv("src/game/systems/combat/" + file,
           "src/game/contracts/impl/" + file)

for source_file in source_and_test_files:
    rewrite_only_exact_include_prefixes(source_file, {
        "game/systems/combat/CombatEvents.hpp":
            "game/contracts/CombatEvents.hpp",
        "game/systems/combat/CombatFormula.hpp":
            "game/contracts/CombatFormula.hpp",
        "game/systems/combat/DamagePipelineTypes.hpp":
            "game/contracts/DamagePipelineTypes.hpp",
        "game/systems/combat/DamageResolutionHooks.hpp":
            "game/contracts/DamageResolutionHooks.hpp",
        "game/systems/combat/CombatAntiMeta.hpp":
            "game/contracts/impl/CombatAntiMeta.hpp",
        "game/systems/combat/CombatEventDispatcher.hpp":
            "game/contracts/impl/CombatEventDispatcher.hpp",
        "game/systems/combat/CombatTelemetry.hpp":
            "game/contracts/impl/CombatTelemetry.hpp",
        "game/systems/combat/ProcBudgetManager.hpp":
            "game/contracts/impl/ProcBudgetManager.hpp",
        "game/systems/combat/StatsSystem.hpp":
            "game/contracts/impl/StatsSystem.hpp",
    })
```

The implementation source list becomes conceptually:

```text
add_library(NoMoreDayGameContracts STATIC
    CombatAntiMeta.cpp
    CombatEventDispatcher.cpp
    CombatTelemetry.cpp
    ProcBudgetManager.cpp
    StatsSystem.cpp
    DamageResolutionHooks.cpp
)
```

Do not add forwarding headers, compatibility aliases, or a second copy of any moved file. The existing `target_include_directories(... ${CMAKE_SOURCE_DIR}/src)` already supports the new `game/contracts/...` include paths.

## 3. Atomic Tasks

Tasks are ordered. Each task must leave the worktree in a state that the next task can inspect; the complete build gate is required at the end of the migration.

- [x] **T0. Capture the M1 baseline.** Record `git status --short`, the current six absolute source entries in `src/game/contracts/impl/CMakeLists.txt`, and exact old-prefix counts for the nine moved header basenames under `src/` and `tests/`. Preserve the pre-existing untracked `docs/designs/directory-structure-reorganization-design.md`; it is not an M1 source change.
- [x] **T1. Move the four `ContractsCore` headers.** Use `git mv` from `src/game/systems/combat/` to `src/game/contracts/`. Verify that every moved file is byte/content identical before include-path edits and that no duplicate remains in the old directory.
- [x] **T2. Move the `Contracts` implementation set.** Use `git mv` for the five implementation headers and six implementation `.cpp` files into `src/game/contracts/impl/`. Keep `DamageResolutionHooks.hpp` at the `contracts/` root; move only `DamageResolutionHooks.cpp` into `impl/`.
- [x] **T3. Rewrite canonical include paths.** Apply the nine exact mappings in §2 to all C++ files under `src/` and `tests/`, including moved files themselves. Do not rewrite includes for remaining combat-domain headers or edit unrelated documentation/history references. Verify no old path remains for any of the nine basenames.
- [x] **T4. Make the CMake manifest directory-relative.** Replace the six absolute source entries in `src/game/contracts/impl/CMakeLists.txt` with relative basenames. Update stale physical-location comments in `src/game/contracts/CMakeLists.txt`; preserve all target properties, compile definitions, PCH, dependencies, and link edges. Do not modify `src/game/CMakeLists.txt` or `src/game/systems/combat/CMakeLists.txt` except if a stale M1-specific comment is found.
- [x] **T5. Run static ownership checks.** Confirm the 15 moved files exist exactly once under `src/game/contracts/` or `src/game/contracts/impl/`; confirm the old paths do not exist; confirm `src/game/systems/combat/` still contains all non-M1 combat sources; confirm `contracts/impl/CMakeLists.txt` has no `${CMAKE_SOURCE_DIR}/src/game/systems/combat/` source entry. Use `git diff --check` and inspect `git diff --stat`/`git diff --name-status` for scope drift.
- [x] **T6. Build and test the actual targets.** Run the commands in §4, first narrow checks and then the repository CI gate. Verify both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are produced or updated by the RelWithDebInfo build before reporting completion.

## 4. Test Method

### 4.1 Test level and regression coverage

This is a path-only migration, so no new production behavior or new test fixture is required. Existing tests provide regression protection for the moved contract surfaces:

- Unit coverage: combat formulas, anti-meta configuration, event dispatch, proc budgets, telemetry, damage hooks, and event consistency.
- Integration coverage: proc-budget integration, combat-v2 cutover, combat balance, skill-system and gameplay-system paths.
- Compile/link coverage: every consumer of the nine moved headers, including `src/app/Game.cpp`, combat, item, skill, nemesis, UI, `tests/TestCommon.hpp`, and performance test translation units.

Do not add a production-only test branch or compatibility facade. The expected behavioral result is identical before and after the path migration.

### 4.2 Verification commands

Run from `D:\PRJ\NoMoreDay` in this order. Redirect high-volume build output to a log and inspect the key result/error sections.

```powershell
./build.bat > "$env:TEMP\nomoreday-m1-build.log" 2>&1
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
./build.bat check
```

Static checks before and after the build:

```powershell
# No old include may remain for any M1 basename.
rg 'game/systems/combat/(CombatEvents|CombatFormula|DamagePipelineTypes|DamageResolutionHooks|CombatAntiMeta|CombatEventDispatcher|CombatTelemetry|ProcBudgetManager|StatsSystem)\.hpp' src tests

# The M1 contract manifest must contain no old absolute source path.
rg '\$\{CMAKE_SOURCE_DIR\}/src/game/systems/combat/(CombatAntiMeta|CombatEventDispatcher|CombatTelemetry|ProcBudgetManager|StatsSystem|DamageResolutionHooks)\.cpp' src/game/contracts/impl/CMakeLists.txt

# Confirm both real build products, not only the test target.
Test-Path .\bin\NoMoreDay.exe
Test-Path .\bin\NoMoreDayTests.exe
```

The two `rg` checks are expected to return no matches after T4. A global search for `game/systems/combat/` is not a valid M1 assertion because ordinary combat headers intentionally remain there until later milestones.

If a focused test name is needed, first inspect `./bin/NoMoreDayTests.exe --list-test-cases`, then run the exact existing case using the repository's supported `--test-case` form. Do not infer a case name from the source filename.

Performance tests are not required for this milestone: the compiled source set and runtime logic do not change. They may be used as a secondary compile/runtime smoke check if the normal CI gate exposes a relevant failure.

## 5. Completion Definition

M1 is complete only when all of the following evidence is available:

1. The four contract headers exist only in `src/game/contracts/`.
2. The five implementation headers and six implementation sources exist only in `src/game/contracts/impl/`; `DamageResolutionHooks.hpp` is present only at the contract root.
3. `src/game/contracts/impl/CMakeLists.txt` compiles the same six basenames as before, now with directory-relative paths; target name and all target properties are unchanged.
4. No C++ include under `src/` or `tests/` uses the old path for any of the nine moved headers.
5. `src/game/systems/combat/CMakeLists.txt` still compiles the same combat-domain source set and does not regain any M1 implementation source.
6. `git diff --check` passes and the diff contains only the planned `git mv` operations, include-prefix updates, and M1 CMake/comment corrections. The existing directory-reorganization design document remains preserved.
7. `./build.bat` succeeds in its default RelWithDebInfo configuration, and both `bin/NoMoreDay.exe` and `bin/NoMoreDayTests.exe` are available from that build.
8. Unit, integration, and `ci` CTest labels pass with `-C RelWithDebInfo`; `./build.bat check` passes.
9. No target name, link edge, compile definition, PCH path, generated-header path, or runtime behavior changes are observed.

## 6. Risks And Mitigations

- **R1: broad include replacement damages remaining combat files.** Use the nine basename-specific mappings only; never replace the entire `game/systems/combat/` prefix.
- **R2: implementation/header split is confused.** Keep `DamageResolutionHooks.hpp` in `contracts/`; only its `.cpp` moves to `contracts/impl/`. The other five implementation headers move with their implementation sources.
- **R3: high fan-in exposes an incomplete rewrite.** The contract headers are used by combat, skill, item, nemesis, UI, app, tests, and benchmarks. Run the old-prefix search before building so omissions are detected independently of compiler ordering.
- **R4: stale comments make the target appear physically misaligned after the move.** Update only comments that describe the M1 physical location; do not rewrite design-history statements outside the M1 scope.
- **R5: unrelated worktree state.** The current worktree already contains the untracked design document `docs/designs/directory-structure-reorganization-design.md`. Do not revert or fold it into source changes; report it separately in status evidence.

## 7. Handoff To Implementation

Implementation must load `docs/workflows/implementation.md` before editing. The implementer should execute T0-T6 in order, update this checklist from `[ ]` to `[~]`/`[x]` only with evidence, and stop before M2. Any discovery requiring a target/link-graph change, a different contract ownership decision, or a compatibility header is a design change: update `docs/designs/directory-structure-reorganization-design.md` first, then revise this plan.

## 8. T0-T6 Verification Record (2026-08-09)

All evidence below was captured during execution of this plan against the default RelWithDebInfo configuration via `./build.bat`.

- **T0**: Baseline captured before any edit: `git status --short` clean except pre-existing untracked `docs/designs/directory-structure-reorganization-design.md`; six absolute source entries present in `src/game/contracts/impl/CMakeLists.txt` (CombatAntiMeta, CombatEventDispatcher, CombatTelemetry, ProcBudgetManager, StatsSystem, DamageResolutionHooks `.cpp`).
- **T1**: `git mv` moved `CombatEvents.hpp`, `CombatFormula.hpp`, `DamagePipelineTypes.hpp`, `DamageResolutionHooks.hpp` from `src/game/systems/combat/` to `src/game/contracts/` (R100, no duplicates in old directory).
- **T2**: `git mv` moved `CombatAntiMeta.hpp/.cpp`, `CombatEventDispatcher.hpp/.cpp`, `CombatTelemetry.hpp/.cpp`, `ProcBudgetManager.hpp/.cpp`, `StatsSystem.hpp/.cpp`, `DamageResolutionHooks.cpp` into `src/game/contracts/impl/`; `DamageResolutionHooks.hpp` stayed at the contracts root.
- **T3**: Nine basename-specific include mappings applied to all `.hpp`/`.cpp` under `src/` and `tests/` (70 files rewritten); post-check confirmed 0 remaining old-prefix includes. Full diff: 79 files, 114 insertions / 114 deletions — no scope drift.
- **T4**: `src/game/contracts/impl/CMakeLists.txt` source entries converted to six bare basenames; physical-location comment updated in `src/game/contracts/CMakeLists.txt`; `src/game/CMakeLists.txt` and `src/game/systems/combat/CMakeLists.txt` untouched (their comments were already accurate).
- **T5**: 15 moved files exist exactly once under the contracts tree; old paths absent; combat dir retains all 33 non-M1 sources; `git diff --check` passes (only LF->CRLF informational notices; mixed line endings are a pre-existing repo condition, not introduced here).
- **T6 build**: `./build.bat` exit=0; `bin/NoMoreDay.exe` (22:13) and `bin/NoMoreDayTests.exe` (22:15) both produced.
- **T6 tests**: `-L unit` 8/9 pass; `-L integration` 6/6 pass; `-L ci` pass; `./build.bat check` exit=0. Serial ctest runs required (parallel ctest invocations contend for the headless GPU/window context).
- **Pre-existing failure (not M1)**: `nmd.tests.item.unit` SEGFAULT (exit -1073741819, 0xC0000005) occurs after doctest reports full success, during process teardown. Bisected to the single case `[Unit] Lighting - LightAdapter skips hidden loot items` (tests/unit/LightingTest.cpp, added by d02ce146 before M1; include chain touches engine/render and game/render only — no M1-moved file). Deterministic reproduction on a freshly built pre-M1 binary obtained via `git stash` of the M1 changes: identical SEGFAULT. Conclusion: pre-existing teardown crash unrelated to M1; filed as a follow-up outside this plan's scope.
