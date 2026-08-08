# Fine-Grained Module Split And Unity Build Removal Implementation Plan

**Design reference:** `docs/designs/fine-grained-module-split-design.md`
**Status:** M1 [ ]; M2 [ ]; M3 [ ]; M4 [ ]; M5 [ ]
**Execution model:** Each milestone is implemented via minimal atomic tasks with the narrowest verification after each, using `build.bat` (RelWithDebInfo) as the only build path. Milestones M3/M4 are behavior-parity sensitive: any change that reverses the design contract (three break-cycle means: contract sinking, hook inversion, ownership moves) pauses implementation and goes back to the design document first.

## Goal

Remove all Unity Build configuration from the CMake tree and split the first-party layered libraries into a dependency-ordered DAG of fine-grained static sub-projects that mirrors the physical directory hierarchy:

```text
NoMoreDay.exe
  -> NoMoreDayApp
    -> NoMoreDayGame (aggregate)
      -> ui states render scene input persistence
      -> combat
      -> skill nemesis world item physics ai vfx modifier
      -> data utils stats
      -> combat_v2 contracts (contracts-core)
        -> NoMoreDayEngine (aggregate)
          -> render-systems render-passes engine-resource
          -> render-core engine-vfx
            -> NoMoreDayCore -> NoMoreDayTypes
```

`SkillBehaviors` remains a Game-internal OBJECT input. `NoMoreDayEngine` and `NoMoreDayGame` remain as layer-surface aggregate targets (their names are referenced by `check_module_boundaries.py` and the CTest registrations) and compile no sources of their own.

## Fixed Constraints

- Do not modify the design document's finalized contract without a pause-and-redesign cycle first.
- Unity Build references are deleted, not made conditional. `CMAKE_UNITY_BUILD` never becomes ON.
- Public header contracts stay source-compatible for consumers that use `DamagePipeline` value types and hook routing; damage mathematics is byte-for-byte unchanged (hooks only re-route the call).
- No DLL work, no export macros, no renaming of layer targets, no changes to `src/pch.hpp` / `src/game/pch.hpp` contents, no RenderGraph/shader/GPU Track changes.
- Include directories remain `${CMAKE_SOURCE_DIR}/src` (root-relative); source files are not physically relocated during M1/M2; M3 applies exactly two ownership moves (`TalentLayoutService`, `MovementStanceSystem`).
- Existing behavior, save data, renderer behavior, and test labels remain compatible. `ctest -L ci` and `build.bat check` must pass at every milestone end.
- `check_module_boundaries.py` layer rules are unaffected; intra-layer splits must not introduce CMake target cycles.

## Milestones

### M1 [ ]: Unity Build Removal

**Objective:** Delete every `UNITY_BUILD` / `SKIP_UNITY_BUILD_INCLUSION` occurrence. No CMake topology change.

**Scope:** CMake edits, one source comment update, verification. No C++ behavior change.

**Atomic tasks:**

- [ ] M1.1 Root `CMakeLists.txt`: remove `set(CMAKE_UNITY_BUILD OFF)` (line 7), spdlog `UNITY_BUILD OFF` (line 140), raylib comment + `UNITY_BUILD OFF` (lines 151-153), SkillBehaviors comment + `UNITY_BUILD ON` (lines 211-212).
- [ ] M1.2 `src/core/CMakeLists.txt` line 19, `src/app/CMakeLists.txt` line 13: remove `UNITY_BUILD OFF`.
- [ ] M1.3 `src/engine/CMakeLists.txt`: remove `UNITY_BUILD OFF` (line 106); in the `ResourceManager.cpp` block (lines 75-80) keep `SKIP_PRECOMPILE_HEADERS ON`, remove `SKIP_UNITY_BUILD_INCLUSION ON`, and drop the "or the Unity build units" wording.
- [ ] M1.4 `src/game/CMakeLists.txt` line 150: remove `UNITY_BUILD OFF`.
- [ ] M1.5 `tests/CMakeLists.txt`: remove the 21-file `SKIP_UNITY_BUILD_INCLUSION` block (lines 13-39) and the `UNITY_BUILD ON` property + "Enable Unity Build for Tests" comment (lines 49-51).
- [ ] M1.6 `tests/integration/GIHistoryRejectionTest.cpp:20`: update the comment that mentions compiling under Unity Build (comment-only).
- [ ] M1.7 Verify: `rg -n "UNITY_BUILD|SKIP_UNITY_BUILD" CMakeLists.txt src tests` returns zero (historical `docs/` and `conductor/archive` excluded); `build.bat` builds clean; `ctest -L ci` passes.

### M2 [ ]: Engine Sub-Project Split

**Objective:** Split `NoMoreDayEngine` sources across five sub-targets plus the aggregate. Pure CMake topology change; no source moves.

**Pre-step:** Run an include scan to resolve risk R1 (does `RenderSystem` / any render-root system drive `render/passes` directly?). If yes, `NoMoreDayEngineSystems` links `NoMoreDayEnginePasses`; if no, it does not. Record the scan result in the plan when the milestone is accepted.

**Atomic tasks:**

- [ ] M2.1 Create `src/engine/CMakeLists.txt` as aggregate-only (`add_subdirectory` for each sub-project + PUBLIC link list of all sub-targets + existing PUBLIC deps/definitions/PCH policy + `add_dependencies(NoMoreDayEngine GenerateTags)`).
- [ ] M2.2 Create `NoMoreDayEngineRenderCore` manifest: `physics/SIMDSpatialGrid.cpp`, `render/core/*` (4), `render/debug/*` (5), `render/gi/*` (1), `render/graph/RenderGraph.cpp`, `render/lighting/*` (3), `render/particle/*` (2), `render/resource/*` (2), `render/resources/*` (4), `render/shadow/*` (3), `render/trail/*` (1), `render/validation/*` (1), render-root `{GPUABIContract, GPUUtils, MaterialManager, MDIRenderer, PersistentBuffer}.cpp`. Links `NoMoreDayCore` + third-party; reuses `src/pch.hpp`.
- [ ] M2.3 Create `NoMoreDayEngineResource`: `resource/AssetLoadingSystem.cpp`, `resource/ResourceManager.cpp` (keep `SKIP_PRECOMPILE_HEADERS ON`). Links `NoMoreDayEngineRenderCore`.
- [ ] M2.4 Create `NoMoreDayEnginePasses`: `render/passes/*` (21). Links `NoMoreDayEngineResource` + `NoMoreDayEngineRenderCore`.
- [ ] M2.5 Create `NoMoreDayEngineSystems`: render-root `{RenderSystem, GPUEntitySystem, GPUFlowFieldSystem, GPULootSystem, GPUParticleSystem, GPUSkillEffectSystem, GPUTextSystem, LootTextBatcher, PopupRenderer, GPUTrailRenderer}.cpp`. Links per R1 result; at minimum `RenderCore` + `Resource`.
- [ ] M2.6 Create `NoMoreDayEngineVfx`: `vfx/VFXBudgetEstimator.cpp`, `vfx/VFXSequenceManager.cpp`. Links `NoMoreDayEngineRenderCore`.
- [ ] M2.7 Verify: `build.bat` configures/builds; `cmake --build build --target help` lists all new targets; no cyclic link; `ctest -L ci` passes; `build.bat check` passes.

### M3 [ ]: Game Part 1 — Contract Sinking, Ownership Moves, Hook Inversion

**Objective:** Establish the acyclic bottom of the Game graph (contracts + data + stats + utils) and break the combat↔skill / combat↔world / data↔skill cycles via contract sinking, two ownership moves, and the `DamageResolutionHooks` inversion. Game CMake topology is NOT yet split per-directory (that is M4); behavior parity is the gate.

**Atomic tasks:**

- [ ] M3.1 Create `src/game/systems/combat/DamagePipelineTypes.hpp`: move `struct DamageRequest` / `DamageResult` / `DamageExecutionResult` out of `DamagePipeline.hpp` verbatim (no field changes). `DamagePipeline.hpp` keeps its class + static method declarations and includes the types header.
- [ ] M3.2 Create `src/game/systems/combat/DamageResolutionHooks.hpp` (namespace `NoMoreDay`): `DamageResolutionHooks { std::function<DamageExecutionResult(entt::registry&, const DamageRequest&, entt::entity)> execute; std::function<std::vector<DamageResult>(entt::registry&, const DamageRequest&)> calculateBatch; }`; `void RegisterDamageResolutionHooks(const DamageResolutionHooks&)`, `void ClearDamageResolutionHooks()`, `DamageExecutionResult ResolveDamage(entt::registry&, const DamageRequest&, entt::entity)`.
- [ ] M3.3 Wire `DamagePipeline` as the registered implementation: on pipeline/combat initialization register hooks whose bodies call the existing static `DamagePipeline::Execute` / `CalculateBatch` (identical math); keep the static methods for any remaining in-combat callers.
- [ ] M3.4 Migrate skill-domain callers to `ResolveDamage`: `SkillSystem.cpp` (:993), `ProjectileSystem.cpp` (:604, :642), `SummonCombatBridge.cpp` (:228), and all `SkillBehaviors` callers (BloodSea, HeavenlySwordDescent, SevenStarSlash, SwordArray, MindBlade, FlowingThrust comment). Remove now-dormant `CombatSystem.hpp` include in `SkillSystem.cpp` (verify no call remains). Do NOT touch combat-domain internal `DamagePipeline` calls (DamagePipeline.cpp, CombatSystem.cpp).
- [ ] M3.5 Ownership move 1 — `TalentLayoutService.{hpp,cpp}` from `systems/skill/` to `data/` (its `.cpp` has no skill dependency). Update includes/graph so `TalentLoader` (data) → `TalentLayoutService` (data), breaking the data↔skill cycle.
- [ ] M3.6 Ownership move 2 — `MovementStanceSystem.{hpp,cpp}` from `systems/world/` to `systems/combat/` (world no longer references combat; `CombatSystem.cpp` calls `OnTakeDamage` within the same domain). Breaking the combat↔world cycle.
- [ ] M3.7 Contract-sunk module `NoMoreDayGameContractsCore` (INTERFACE): `CombatEvents.hpp`, `CombatFormula.hpp`, `DamagePipelineTypes.hpp`, `DamageResolutionHooks.hpp`; and `NoMoreDayGameContracts` (STATIC): `CombatEventDispatcher`, `CombatTelemetry`, `ProcBudgetManager`, `CombatAntiMeta`, `StatsSystem` (hpp+cpp). Manage R3 (StatsSystem needs `data` registries + `utils/MonsterScaling` + `combat_v2`): split the `data`/`utils`/`stats`/`combat_v2` sub-targets in this milestone if needed to satisfy the contracts layer, otherwise document the temporary placement and retreat.
- [ ] M3.8 Game CMake: introduce `add_subdirectory` structure for the bottom of the graph only (contracts, data, utils, stats, combat_v2); `NoMoreDayGame` continues compiling the rest until M4. Ensure `SkillBehaviors` still links PUBLIC into the aggregate and its registration reaches the final exe.
- [ ] M3.9 Verify behavior parity: `build.bat` clean; `ctest -L ci` passes; `build.bat check` passes; run combat/skill module-gate labels (`--test-case=[Unit]*Combat*` etc.). No damage-math assertions change.

### M4 [ ]: Game Part 2 — Phase C Full Split

**Objective:** Split the remaining Game sources per-directory into independent sub-targets with a DAG-only dependency graph, ending with `NoMoreDayGame` as a source-free aggregate.

**Atomic tasks:**

- [ ] M4.1 Split `combat` domain (post-M3 remaining sources incl. CombatSystem, DamagePipeline, HazardSystem, XPAwardingSystem, DamageMitigationService, MovementStanceSystem) → `NoMoreDayGameCombat`; links skill + world + stats + combat_v2 + contracts.
- [ ] M4.2 Split `skill` (13 systems after TalentLayoutService move) → `NoMoreDayGameSkill`; links contracts + data + combat_v2; keeps `SkillBehaviors` OBJECT dependency only at aggregate level.
- [ ] M4.3 Split `data` (8 + TalentLayoutService), `world` (13), `item` (12), `ui` (18), `states` (10), `render` (7), `scene` (2), `persistence` (2), `modifier` (8), `nemesis` (2), `physics` (1), `ai` (3), `vfx` (4), `input` (1), `stats` (`AttributePipeline`), `utils` (`MonsterScaling`) into their own targets per §5.3 Phase C.
- [ ] M4.4 Convert `NoMoreDayGame` to aggregate-only: `add_subdirectory` + PUBLIC link of every sub-target + `SkillBehaviors`; remove all direct sources. Ensure link order follows the design's bottom-up order.
- [ ] M4.5 Verify: `build.bat` clean with default j=7; `cmake --build build --target help` lists all new targets; dependency graph is a DAG; `ctest -L ci` passes; module-gate labels pass (`skill`, `combat`, `item`, `world`, `ui`, `ai`); `build.bat check` passes; spot-run `-L gpu;contract` / `-L performance` if hardware allows.

### M5 [ ]: Optional Cycle Guard Tooling

**Objective (open decision — implement now or gate on a later plan):** Add `tools/cmake_graph_check.py` that parses `target_link_libraries` / `add_library` and asserts DAG-ness after configure, wired into `build.bat check`.

**Atomic tasks:**

- [ ] M5.1 Write the parser (static-library target graph only, ignore INTERFACE/OBJECT pseudo-edges that cannot cycle in practice or document why they are safe).
- [ ] M5.2 Wire into `build.bat check` pre-checks; add self-test fixtures (one acyclic, one cyclic) mirroring `check_module_boundaries.py` self-test style.
- [ ] M5.3 Verify `build.bat check` runs the guard and passes.

## Test Method

| Level | Command | Purpose |
|---|---|---|
| Build | `build.bat` (default RelWithDebInfo, j=7) | Compile-clean gate after every task |
| Unit/integration CI | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | Non-performance regression gate |
| Module gates | `ctest ... -L skill`, `-L combat`, `-L item`, `-L world`, `-L ui`, `-L ai` | Domain regression after M3/M4 |
| Graph sanity | `cmake --build build --target help` | New targets exist; manual cycle inspection (M5 automates) |
| Pre-checks | `build.bat check` | JSON/module-boundary/ABI governance |
| Static grep | `rg -n "UNITY_BUILD\|SKIP_UNITY_BUILD" CMakeLists.txt src tests` | M1 completion proof |

## Verification / Exit Criteria

The work package is complete when all of the following hold:

1. M1: `rg` over `CMakeLists.txt src tests` returns zero `UNITY_BUILD`/`SKIP_UNITY_BUILD` hits (historical docs excluded).
2. M2: Engine is a source-free aggregate over five sub-targets; builds clean.
3. M3: contract layer exists (`ContractsCore` INTERFACE + `Contracts` STATIC); `TalentLayoutService` lives in `data/`; `MovementStanceSystem` lives in `systems/combat/`; skill-domain damage calls route through `DamageResolutionHooks`; `ctest -L ci` green with unchanged damage assertions.
4. M4: Game is a source-free aggregate over ~20 sub-targets; the target graph is acyclic; all module-gate labels green.
5. `build.bat check` green at each milestone end; no behavior/save/asset/render-order changes.
6. Milestone M5 (if implemented) runs the automated cycle guard in `build.bat check`.

Note: this plan was generated from the finalized design document `docs/designs/fine-grained-module-split-design.md`; if the design contract changes during implementation, stop and update the design first, then return to this plan.
