# Directory Structure Reorganization Design

> **Status:** implemented 2026-08-10
> **Primary objective:** reorganize the physical directory structure so that (a) every CMake sub-target owns exactly one physical directory whose name matches the target (方案 A: directory == target), and (b) the `src/game` tree is grouped into explicit dependency bands (方案 B: contracts / foundation / systems / application) that make dependency direction visible and give future modules a rule-based home.
> **Related:** [Fine-Grained Module Split](./fine-grained-module-split-design.md) established the 5 Engine / 20 Game sub-targets; its §5.4 explicitly deferred physical relocation ("physical relocation follows, matching the modular-split convention"). This design executes that convergence, plus a banded layout, in multiple independently verifiable milestones.

## 1. Decision Summary

The CMake target graph is final and **must not change** (no target rename, no link-graph edit, no behavior change). Only the physical location of source files, the CMake source lists, and the include paths change.

Two decisions combined:

1. **Directory == target (方案 A).** Every sub-target gets a physical directory named after it; every absolute-path source reference (`${CMAKE_SOURCE_DIR}/...` entries inside a foreign `add_library` call) is eliminated by physically moving the file. Empty target directories (`contracts/`, `contracts/impl/`, `stats/`) are filled with their owned sources. Orphan header-only directories are given a banded home.
2. **Banded `src/game` layout (方案 B).** `src/game` is reorganized into four bands ordered bottom-up by the existing one-way dependency order (§5.4 of the fine-grained design):

```text
src/
├── app/                          # NoMoreDayApp (unchanged)
├── core/                         # NoMoreDayCore (unchanged)
├── engine/                       # NoMoreDayEngine (aggregate)
│   ├── render/                   # RenderCore (+ physics/SIMDSpatialGrid)
│   │   ├── passes/               #   Passes
│   │   └── systems/              #   Systems
│   ├── resource/                 # Resource
│   ├── vfx/                      # Vfx
│   └── audio/                    # RESERVED for future audio module (header-only today)
└── game/                         # NoMoreDayGame (aggregate)
    ├── contracts/                # band: cross-domain contracts (INTERFACE) + impl/
    │   └── impl/                 #   contract implementations (STATIC)
    ├── foundation/               # band: base domains & shared state
    │   ├── data/  utils/  stats/  combat_v2/  ui_shared/
    │   ├── components/           #   ECS component headers (header-only)
    │   └── registry/             #   GroupRegistry.hpp (header-only)
    ├── systems/                  # band: gameplay domain systems
    │   ├── ai/  combat/  item/  modifier/  nemesis/  physics/  skill/  vfx/  world/
    └── application/              # band: application assembly
        ├── persistence/  render/  scene/  states/
        ├── ui/  input/           #   moved from systems/
        └── states-progression/   #   AchievementSystem/LeaderboardSystem headers
```

`src/game/Settings.hpp` and `src/game/SharedContext.hpp` (game-root shared headers, no target) move into `foundation/` as shared base headers.

The reorganization is executed as **six milestones, each independently verifiable** (build + `ctest -L ci` + `check`), so a failure is always attributable to one small mechanical step.

## 2. Goals And Non-Goals

### 2.1 Goals

- Make every sub-target's physical directory exactly its target name; remove every `${CMAKE_SOURCE_DIR}/src/...` absolute-path source reference from `add_library` lists (path churn only, target names and link edges unchanged).
- Group `src/game` into the four bands above so dependency direction is visible from the folder tree and future modules have a placement rule (§5).
- Fill the empty target directories (`contracts/`, `contracts/impl/`, `stats/`) with the sources they own.
- Give every header-only directory (`components/`, `registry/`, `progression/`, `audio/`) an explicit home and ownership statement.
- Reserve `engine/audio/` for the future audio module; `AudioSystem.hpp` stays in place, unmoved, with an ownership comment.
- Execute in small milestones; every milestone leaves the tree building and the CI suite green.

### 2.2 Non-Goals

- No CMake target rename, no change to `NoMoreDay*` target names, link order, or link edges (the DAG from the fine-grained design is authoritative and untouched).
- No change to `scripts/check_module_boundaries.py` layer policy (Core/Engine/Game/App prefixes are unaffected by intra-game directory moves).
- No gameplay, save-format, asset, render-order, or graphics-capability change of any kind.
- No new types in `NoMoreDayTypes`, no new dependency direction, no behavior change.
- Not a "clean up everything" sweep: unrelated debt (constants, naming, comments) is out of scope; milestones touch only what the move requires.

## 3. Current Architectural Facts (Physical vs. Target Register)

### 3.1 Target graph (unchanged, authoritative)

Engine: `RenderCore ← Resource ← Passes ← Systems`, `Vfx` branches from `RenderCore`, all under aggregate `NoMoreDayEngine`.
Game (bottom-up): `ContractsCore (INTERFACE) / CombatV2 / Data / Utils / Stats / UiShared → Contracts (STATIC) → Modifier / Skill / World / Item / Physics / Ai / Vfx / Nemesis → Combat → Persistence / Scene / Ui / Input / Render / States → NoMoreDayGame (aggregate) + SkillBehaviors (OBJECT)`.

### 3.2 Physical-vs-target mismatch register (verified 2026-08-09)

Include-site counts are across `src/` + `tests/` (both quote form; no angle-bracket project includes exist).

| # | Item | Physical location today | Owned by target | include sites | Move target |
|---|---|---|---|---|---|
| 1 | `CombatEvents.hpp` (10) `CombatFormula.hpp` (7) `DamagePipelineTypes.hpp` (2) `DamageResolutionHooks.hpp` (9) | `systems/combat/` | `ContractsCore` (dir empty!) | 28 | `game/contracts/` |
| 2 | `CombatAntiMeta.cpp` (5) `CombatEventDispatcher.cpp` (32) `CombatTelemetry.cpp` (11) `ProcBudgetManager.cpp` (8) `StatsSystem.cpp` (20) `DamageResolutionHooks.cpp` | `systems/combat/` | `Contracts` (dir empty!) | 76 | `game/contracts/impl/` |
| 3 | `AttributePipeline.cpp` (1) | `systems/stats/` | `Stats` (dir empty!) | 1 | `game/foundation/stats/` |
| 4 | `AirWallRenderer.cpp/.hpp` (0) | `systems/render/` | `Render` | 0 | `game/application/render/` |
| 5 | `VFXSequencerSystem.cpp/.hpp` (2) | `game/vfx/` | `Vfx` | 2 | `game/systems/vfx/` |
| 6 | `SIMDSpatialGrid.cpp/.hpp` (4) | `engine/physics/` | `RenderCore` | 4 | `engine/render/` |
| 7 | `game/components/*` (37 headers) | `game/components/` | none (header-only) | 538 | `game/foundation/components/` |
| 8 | `GroupRegistry.hpp` (4) | `game/registry/` | none (header-only) | 4 | `game/foundation/registry/` |
| 9 | `AchievementSystem.hpp` (0) `LeaderboardSystem.hpp` (1) | `game/systems/progression/` | none (header-only) | 1 | `game/application/states/` (sole consumer: `NightmareFloorState`) |
| 10 | `Settings.hpp` (2) `SharedContext.hpp` (14) | `game/` root | none | 16 | `game/foundation/` |
| 11 | `AudioSystem.hpp` (1) | `engine/audio/` | none (header-only) | 1 | stays `engine/audio/` (reserved) |
| 12 | `GameplayState`'s `GroupRegistry` include chain | via `systems/combat/StatsSystem.cpp` comment | — | — | fixed by #2 |

Additional path-churn facts:

- **Directory moves in banding** (dir contains files today, moving as a whole): `game/data/` (160 includes, incl. generated `TagRegistry.hpp`), `game/utils/` (6), `game/stats/` (0 — empty until #3), `game/ui_shared/` (13), `game/combat_v2/` (2), `game/persistence/` (5), `game/render/` (16), `game/scene/` (28), `game/states/` (21), `game/systems/ui/` (93), `game/systems/input/` (2).
- `game/systems/combat/` keeps `CombatSystem`, `DamagePipeline`, `HazardSystem`, `XPAwardingSystem`, `DamageMitigationService`, `BossFrameworkSystem`, `CombatHistorySystem`, `EffectSystem`, `EliteModifierSystem`, `EndgameModifierContract`, `ProgressionSystem`, `VisualFXSystem`, `MovementStanceSystem`, `AilmentEngine` (no move); only items #1/#2 leave.
- `tests/` includes game headers 733 times; the same prefix rewrites apply there.
- Generated header path is wired in root `CMakeLists.txt` (`set(TAG_HEADER ".../src/game/data/TagRegistry.hpp")`); it moves with `data/` to `foundation/data/`.
- PCH files (`src/pch.hpp`, `src/game/pch.hpp`), `SkillBehaviors` OBJECT, `GenerateTags`, `check_module_boundaries.py` (checks `src/core`, `src/engine`, `src/game` prefixes only) are all unaffected by intra-layer moves.

## 4. Target Directory Layout (After All Milestones)

### 4.1 Engine

```text
src/engine/
├── CMakeLists.txt              # aggregate: add_subdirectory + PUBLIC links
├── render/                     # NoMoreDayEngineRenderCore
│   ├── core/ debug/ gi/ graph/ lighting/ particle/ resource/ resources/ shadow/ trail/ validation/
│   ├── passes/                 # NoMoreDayEnginePasses
│   ├── systems/                # NoMoreDayEngineSystems
│   └── SIMDSpatialGrid.*       # moved from physics/
├── resource/                   # NoMoreDayEngineResource
├── vfx/                        # NoMoreDayEngineVfx
└── audio/                      # RESERVED: future NoMoreDayEngineAudio target; AudioSystem.hpp header-only today
```

### 4.2 Game

```text
src/game/
├── CMakeLists.txt              # aggregate: add_subdirectory + PUBLIC links
├── pch.hpp  Settings.hpp  SharedContext.hpp -> foundation/ (Settings/SharedContext move)
├── contracts/                  # band: cross-domain contracts
│   ├── CMakeLists.txt          #   NoMoreDayGameContractsCore (INTERFACE)
│   ├── CombatEvents.hpp CombatFormula.hpp DamagePipelineTypes.hpp DamageResolutionHooks.hpp
│   └── impl/
│       ├── CMakeLists.txt      #   NoMoreDayGameContracts (STATIC)
│       └── CombatAntiMeta.cpp CombatEventDispatcher.cpp CombatTelemetry.cpp
│           ProcBudgetManager.cpp StatsSystem.cpp DamageResolutionHooks.cpp
├── foundation/                 # band: base domains & shared state (bottom-up)
│   ├── CMakeLists.txt          #   wrapper (add_subdirectory of sub-dirs)
│   ├── data/                   #   NoMoreDayGameData (incl. TagRegistry.hpp generated)
│   ├── utils/                  #   NoMoreDayGameUtils
│   ├── stats/                  #   NoMoreDayGameStats (+ AttributePipeline.cpp)
│   ├── combat_v2/              #   NoMoreDayGameCombatV2
│   ├── ui_shared/              #   NoMoreDayGameUiShared
│   ├── components/             #   ECS component headers (header-only)
│   ├── registry/               #   GroupRegistry.hpp (header-only)
│   ├── Settings.hpp SharedContext.hpp   #   game-root shared headers
│   └── CMakeLists.txt per sub-dir
├── systems/                    # band: gameplay domain systems (middle)
│   ├── ai/ combat/ item/ modifier/ nemesis/ physics/ skill/ vfx/ world/   # one target each
│   └── vfx/                    #   NoMoreDayGameVfx (+ VFXSequencerSystem.*)
├── application/                # band: application assembly (top)
│   ├── CMakeLists.txt          #   wrapper
│   ├── persistence/            #   NoMoreDayGamePersistence
│   ├── render/                 #   NoMoreDayGameRender (+ AirWallRenderer.*)
│   ├── scene/                  #   NoMoreDayGameScene
│   ├── states/                 #   NoMoreDayGameStates (+ AchievementSystem.hpp LeaderboardSystem.hpp)
│   ├── ui/                     #   NoMoreDayGameUi (moved from systems/)
│   └── input/                  #   NoMoreDayGameInput (moved from systems/)
└── (deleted after M3)          #   old empty dirs: systems/stats, systems/render, game/vfx
```

### 4.3 Band semantics and future placement rules

| Band | Meaning | Future module goes here |
|---|---|---|
| `game/contracts/` | cross-domain value types, events, formula, hook registrations that several bands share | new cross-domain contract headers/impls |
| `game/foundation/` | base domains (data registries, numeric kernels, shared UI state, ECS components) that nothing above may feed back into | new data registry, stat/attribute logic, shared state, ECS component sets |
| `game/systems/` | gameplay domain systems with one-way dependencies up to combat | new gameplay system (skill-like, item-like, ai-like) |
| `game/application/` | app assembly: save, scene flow, state screens, rendering adapter, UI, input | new state/screen, save service, input binding, render adapter |
| `engine/audio/` | reserved audio module | future `NoMoreDayEngineAudio` target, `AudioSystem` expansion |

Rule: a file may only depend on files in the same or lower bands (contracts → foundation → systems → application, bottom-up), mirroring the existing target DAG. New code placement is decided by band first, target second.

## 5. Milestones (Independent Tasks, Sequential)

Each milestone ends with: `build.bat` (RelWithDebInfo) clean, `ctest --test-dir build -C RelWithDebInfo -L ci` green, `build.bat check` green, and a `git mv`-based history-preserving move. Include-path rewrites are prefix-only (`"game/data/` → `"game/foundation/data/`) so no unrelated include is touched.

### M1 — Contract band physical convergence
- `git mv` 4 contract headers from `systems/combat/` → `contracts/`; 6 impl cpp/hpp from `systems/combat/` → `contracts/impl/`.
- Rewrite 104 include sites (rows #1/#2 counts) prefix `systems/combat/` → `contracts/` resp. `contracts/impl/` for those 10 basenames only.
- Rewrite `contracts/impl/CMakeLists.txt` `add_library` list to directory-relative names; drop absolute paths.
- **Verify:** build, ci ctest, check.

### M2 — Engine-side convergence
- `git mv` `SIMDSpatialGrid.*` `engine/physics/` → `engine/render/`; rewrite 4 include sites; delete empty `engine/physics/`.
- Add ownership comment in `engine/audio/AudioSystem.hpp` marking the directory as the reserved future audio module home.
- **Verify:** build, ci ctest, check.

### M3 — File-level ownership moves
- `git mv` `AttributePipeline.cpp/.hpp` `systems/stats/` → `game/stats/`; rewrite 1 include site; update `stats/CMakeLists.txt` to relative name.
- `git mv` `AirWallRenderer.*` `systems/render/` → `game/render/`; update `render/CMakeLists.txt`.
- `git mv` `VFXSequencerSystem.*` `game/vfx/` → `systems/vfx/`; update `systems/vfx/CMakeLists.txt`.
- Delete now-empty dirs `systems/stats/`, `systems/render/`, `game/vfx/`.
- **Verify:** build, ci ctest, check; grep confirms zero `${CMAKE_SOURCE_DIR}/src/` source entries in `add_library` lists.

### M4 — Foundation band (band creation + first moves)
- Create `game/foundation/` with per-module `CMakeLists.txt` files (moved verbatim from current locations) and a wrapper `CMakeLists.txt`; rewire `src/game/CMakeLists.txt` `add_subdirectory` calls.
- `git mv` `data/` (update root `TAG_HEADER` to `foundation/data/TagRegistry.hpp`), `utils/`, `ui_shared/`, `combat_v2/` into `foundation/`; rewrite includes (160+6+13+2).
- `git mv` `Settings.hpp` `SharedContext.hpp` → `foundation/`; rewrite 16 include sites.
- **Verify:** build (tag regeneration), ci ctest, check.

### M5 — Foundation band (components + registry)
- `git mv` `components/` and `registry/` → `foundation/`; rewrite 542 include sites (538+4).
- **Verify:** build, ci ctest (component-heavy suites: combat/skill/item/world/ui gates), check.

### M6 — Application band + closure
- Create `game/application/` with per-module `CMakeLists.txt`; rewire `add_subdirectory`.
- `git mv` `persistence/ render/ scene/ states/` → `application/`; `git mv` `systems/ui/` `systems/input/` → `application/`; rewrite includes (5+16+28+21+93+2).
- `git mv` `AchievementSystem.hpp` `LeaderboardSystem.hpp` from `systems/progression/` → `application/states/`; rewrite 1 include; delete `systems/progression/`.
- **Closure checks:** grep zero absolute-path source entries; directory==target 1:1 assertion (each sub-target's dir holds exactly its sources); `check_module_boundaries.py` passes; full `ctest -L ci`; update `docs/designs/fine-grained-module-split-design.md` §5.4 note (physical convergence done) and this document status.
- **Status:** completed 2026-08-10 — 8 `git mv` moves (`persistence/ render/ scene/ states/` → `application/`; `systems/ui/` → `application/ui`; `systems/input/` → `application/input`; `AchievementSystem.hpp` `LeaderboardSystem.hpp` → `application/states/`), `systems/progression/` deleted, 257 include rewrites, `application/CMakeLists.txt` wrapper + single-band fold in `src/game/CMakeLists.txt`. Closure checks all green: zero residual old-prefix includes (incl. relative forms), zero absolute game-side `add_library` source entries, all 22 game sub-targets directory==target 1:1, `check_module_boundaries.py` passes; `build.bat` + `ctest -L unit/integration/ci` + `build.bat check` all green.

Each milestone is an atomic unit; no milestone may start before the previous one is green. Milestones M4–M6 may each be split further (e.g., M5 components alone) if a single milestone's include-rewrite count is judged too large in review.

## 6. Impact Assessment

| Aspect | Impact |
|---|---|
| CMake targets & link graph | None. Target names, link order, PUBLIC/PRIVATE edges unchanged; only source lists and `add_subdirectory` paths change. |
| Public headers | Include paths change for moved headers; no signature/layout change. All include-path rewrites are prefix-only mechanical edits. |
| Generated files | `TagRegistry.hpp` moves with `data/` (root `TAG_HEADER` updated in M4); regeneration required after M4. |
| PCH | `src/pch.hpp`, `src/game/pch.hpp` paths unchanged. |
| Tests | `tests/` include paths rewritten in the same prefix edits; `NoMoreDayTests` link list unchanged (aggregates only). |
| Governance | `check_module_boundaries.py` untouched (layer prefixes unchanged); ABI/JSON validation unaffected. |
| Git history | All moves use `git mv` so history tracks renames. |
| Build time | Negligible; no compilation unit set changes, only paths. |

## 7. Acceptance Criteria

1. Every milestone leaves `build.bat` (RelWithDebInfo) configuring and building clean, `ctest -L ci` green, `check` green.
2. After M3: `Select-String -Pattern 'CMAKE_SOURCE_DIR/src'` finds no `add_library` source entries (absolute paths gone).
3. After M6: directory == target for all 25 sub-targets (5 Engine + 20 Game); every source file lives in its owning target's directory; `game/` has exactly the four band dirs plus `pch.hpp`.
4. `scripts/check_module_boundaries.py` passes unchanged.
5. `NoMoreDayTests` runs with the same labels; no CTest label or registration change.
6. Full suite passes after M6; `git status` shows only the intended moves/rewrites.

## 8. Risks, Open Questions, Dependencies

- **R1 (include-rewrite completeness).** Some moved headers may be referenced via non-prefix forms or relative paths. Mitigation: after each milestone, build failure triage is confined to that milestone's file set; a grep for the old prefix must return zero hits as part of verification.
- **R2 (TagRegistry regeneration).** Moving `data/` in M4 changes the generated-header path; the build must regenerate before compile. Mitigation: update `TAG_HEADER` in the same commit as the directory move; `GenerateTags` dependency already wires rebuild.
- **R3 (components churn).** 538 include sites is the single largest mechanical rewrite. Mitigation: M5 is isolated to components/registry; CI gates for combat/skill/item/world/ui exercise it; review may split M5 further.
- **R4 (states-progression headers).** `AchievementSystem.hpp`/`LeaderboardSystem.hpp` are header-only and unused today (0/1 includes); moving them into `application/states/` documents intent without behavior risk. Open question: keep them under `systems/` as a future progression domain instead.
- **R5 (band naming).** `application/` was chosen over alternatives (`shell/`, `entry/`, `composition/`) to match the established App-layer vocabulary; `ui_shared` sits in `foundation/` because its dependency order is bottom (interchange state), not top.
- **Dependency.** This design is the physical-layout completion of the fine-grained split (its §5.4 deferred item); no other design is affected. `docs/workflows/planning.md` applies per milestone when implementation plans are written.

## 9. Milestone Order Rationale

M1 first because it fills the emptiest dirs with the highest-fan-in files (104 includes) while `systems/combat` still physically hosts them — smallest logical unit, immediately verifiable. M2 is engine-only and independent. M3 clears all single-file cross-dir refs before band creation. M4/M5 create `foundation/` (components last because it is the largest rewrite). M6 creates `application/` and closes the loop. Any milestone can be delivered as its own task; the sequence is the dependency order.
