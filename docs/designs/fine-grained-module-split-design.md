# Fine-Grained Module Split And Unity Build Removal Design

> **Status:** draft 2026-08-08
> **Primary objective:** remove all Unity Build (分块编译) configuration, and split the current monolithic first-party static libraries into fine-grained sub-project static libraries aligned with the physical directory hierarchy.
> **Related:** [Modular Architecture Split](./modular-split-exe-lib-dll-design.md) established the four-layer ownership model (Types/Core/Engine/Game/App). This design extends that boundary model downward, one level inside `src/engine` and `src/game`.

## 1. Decision Summary

Two independent changes, delivered together:

1. **Unity Build removal.** Delete every `CMAKE_UNITY_BUILD` / `UNITY_BUILD` / `SKIP_UNITY_BUILD_INCLUSION` occurrence. Unity Build is globally disabled today and only `SkillBehaviors` and `NoMoreDayTests` enable it. The remaining references are dead configuration plus one stale comment; removing them makes the build state unambiguous and ends the confusion between "Unity Build" and "parallel compilation" (`/MP`, `UseMultiToolTask`, ccache).

2. **Fine-grained sub-project split.** Convert the first-party layered libraries from a single translation unit set per layer into an explicit DAG of small static libraries, each with its own `CMakeLists.txt` and `add_subdirectory` at the physical subdirectory level, mirroring the folder hierarchy:

```text
NoMoreDay.exe
  -> NoMoreDayApp                 (src/app)
    -> NoMoreDayGame              (src/game, aggregate)
      -> NoMoreDayGameCombatV2    (src/game/combat_v2)
      -> NoMoreDayGameModifier    (src/game/systems/modifier)
      -> NoMoreDayGameNemesis     (src/game/systems/nemesis)
      -> NoMoreDayGamePhysics     (src/game/systems/physics)
      -> NoMoreDayGameAi          (src/game/systems/ai)
      -> NoMoreDayGameVfx         (src/game/systems/vfx)
      -> NoMoreDayGameUtils       (src/game/utils)
      -> NoMoreDayEngine          (src/engine, aggregate)
        -> NoMoreDayEngineRenderCore     (src/engine/render core + graph/resources/lighting/shadow/gi/particle/trail/debug/validation + render root primitives + physics)
        -> NoMoreDayEngineResource       (src/engine/resource)
        -> NoMoreDayEnginePasses         (src/engine/render/passes)
        -> NoMoreDayEngineSystems        (src/engine/render root systems incl. RenderSystem)
        -> NoMoreDayEngineVfx            (src/engine/vfx)
        -> NoMoreDayCore          (src/core)
          -> NoMoreDayTypes      (src, INTERFACE)
```

`SkillBehaviors` remains an OBJECT library owned by Game; only its `UNITY_BUILD ON` property is removed.

A mechanical per-directory split of `src/game/systems/*` produces real CMake target cycles (see §3.4). The Game split therefore requires a **cycle-breaking step first**, using three sanctioned breakers (§5.4):

1. **Contract sinking** — headers plus their acyclic implementations whose fan-in crosses system boundaries are promoted into a new `NoMoreDayGameContracts` sub-project (`CombatEvents`, `CombatFormula`, `CombatEventDispatcher`, `CombatTelemetry`, `ProcBudgetManager`, `StatsSystem`, `CombatAntiMeta`).
2. **Hook inversion for `combat↔skill`** — the `DamageRequest`/`DamageResult`/`DamageExecutionResult` value types and a `DamageResolutionHooks` registration API are sunk into `Contracts`; `skill` (including `SkillBehaviors`) calls damage resolution only through the hooks, while the `combat` domain implements and registers them. This makes `skill` fully independent of `combat`.
3. **Ownership moves** — `TalentLayoutService` moves to the `data` domain (breaks `data↔skill`); `MovementStanceSystem` moves to the `combat` domain (breaks `combat↔world`); `AttributePipeline` stays in `stats` with `combat` depending on it one-way.

After cycle-breaking the entire Game layer is split in one pass into 20+ sub-projects (§5.3).

## 2. Goals And Non-Goals

### 2.1 Goals

- Remove every Unity Build occurrence from CMake and source comments, so the project never amalgamates translation units again.
- Keep all existing layer targets and their public contracts unchanged (`NoMoreDayCore`, `NoMoreDayEngine`, `NoMoreDayGame`, `NoMoreDayApp`, `NoMoreDay`, `NoMoreDayTypes`, `NoMoreDayTests`).
- Give each physical subdirectory an independent `add_subdirectory` + `CMakeLists.txt` + static library target, so the build graph mirrors the folder hierarchy and incremental rebuilds are scoped to smaller units.
- Preserve the strict one-way dependency direction between layers enforced by `scripts/check_module_boundaries.py`.
- Preserve gameplay behavior, save formats, assets, render ordering, and graphics capability behavior.
- Keep `SkillBehaviors` as the same OBJECT library input of `NoMoreDayGame`, with identical static-registration behavior.

### 2.2 Non-Goals

- Unity Build is **not** "fixed", "made conditional", or "kept as an option": it is deleted outright.
- No changes to public headers, class layouts, or runtime behavior.
- No DLL conversion, no export macros.
- No renaming of established target names (`NoMoreDayEngine` stays the Engine layer surface even when its sources move to sub-targets).
- Not an attempt to change public damage-resolution behavior: `DamageResolutionHooks` is a pure call-routing change, never a re-balance of damage math.
- No changes to the render engine itself, RenderGraph, shaders, or the active GPU Tracks.
- Not a performance-promise exercise: finer libraries mainly improve incremental rebuild scoping; a compile-time speedup is a secondary benefit, not an acceptance gate.

## 3. Current Architectural Facts

### 3.1 Layered Target Chain (root CMakeLists.txt)

`NoMoreDay`(exe, `src/app/main.cpp`) → `NoMoreDayApp`(STATIC `src/app`) → `NoMoreDayGame`(STATIC `src/game`) → `NoMoreDayEngine`(STATIC `src/engine`) → `NoMoreDayCore`(STATIC `src/core`) → `NoMoreDayTypes`(INTERFACE, header-only, include root `src`).

`SkillBehaviors` is an OBJECT library over `src/game/systems/skill/behaviors/*.cpp` (13 files, GLOB_RECURSE), linked PUBLIC into `NoMoreDayGame`, with PCH `src/game/pch.hpp`.

Source scale per layer: `src/app` 3 cpp / 2 hdr; `src/core` 2 cpp / 12 hdr; `src/engine` 67 cpp / 94 hdr; `src/game` 149 cpp / 221 hdr; `tests` 201 cpp in one `NoMoreDayTests` exe.

### 3.2 Unity Build Inventory (all occurrences to delete)

| Location | Line | Content | Action |
|---|---|---|---|
| root CMakeLists.txt | 7 | `set(CMAKE_UNITY_BUILD OFF)` | delete |
| root CMakeLists.txt | 140 | `set_target_properties(spdlog PROPERTIES UNITY_BUILD OFF)` | delete |
| root CMakeLists.txt | 151-153 | comment "Raylib ... not compatible with Unity Build" + `UNITY_BUILD OFF` | delete both |
| root CMakeLists.txt | 211-212 | comment "Enable Unity Build for Skills" + `UNITY_BUILD ON` | delete both |
| src/core/CMakeLists.txt | 19 | `UNITY_BUILD OFF` | delete |
| src/engine/CMakeLists.txt | 75-80 | `SKIP_PRECOMPILE_HEADERS ON` + `SKIP_UNITY_BUILD_INCLUSION ON` for ResourceManager.cpp | keep `SKIP_PRECOMPILE_HEADERS`, delete the `SKIP_UNITY_BUILD_INCLUSION` line and the "Unity build units" comment wording |
| src/engine/CMakeLists.txt | 106 | `UNITY_BUILD OFF` | delete |
| src/game/CMakeLists.txt | 150 | `UNITY_BUILD OFF` | delete |
| src/app/CMakeLists.txt | 13 | `UNITY_BUILD OFF` | delete |
| tests/CMakeLists.txt | 13-39 | 21 files with `SKIP_UNITY_BUILD_INCLUSION ON` | delete block |
| tests/CMakeLists.txt | 49-51 | comment "Enable Unity Build for Tests" + `UNITY_BUILD ON` | delete both |
| tests/integration/GIHistoryRejectionTest.cpp | 20 | comment mentions compilation under UNITY_BUILD | update comment only (no behavior) |

Historical archive documents that mention Unity Build (`conductor/archive/*`, older `docs/reports/*`, `docs/plans/*`) are **not** modified; they record past state.

### 3.3 Engine Internal Dependencies (`src/engine`, 67 cpp)

- `render/passes/` (21 cpp) depend on `render/graph` (RenderContext/RenderGraph), `render/core` (QualityTierManager, BindingRegistry), `render/resources` (FramebufferManager, FullscreenQuad, TransientResourcePool), `render/GPUUtils`, `render/RenderConstants`, `render/lighting` (LightManager, ClusteredLightingState), `render/resource/TextureArrayManager`, and `engine/resource/ResourceManager.hpp`.
- `RenderSystem.cpp` is the aggregation point: it references nearly every render sub-module plus `engine/resource/AssetLoadingSystem.hpp`.
- `resource/ResourceManager.cpp` depends on `render/debug/ShaderReloadGovernance`, `render/GPUUtils`, `render/resources/GPUResourceRegistry`, and `core/logging`.
- `resource/AssetLoadingSystem.cpp` depends on the various `AssetRegistry` types.

This yields a clean directional order for Engine sub-targets (no cycle):

```text
NoMoreDayEngineRenderCore
  <- NoMoreDayEngineResource      (resource/ depends on RenderCore)
    <- NoMoreDayEnginePasses      (passes depend on Resource + RenderCore)
  <- NoMoreDayEngineSystems       (RenderSystem + render-root GPU* systems; depends on RenderCore + Resource, and Passes iff scan proves RenderSystem drives passes)
  <- NoMoreDayEngineVfx           (vfx/ depends on RenderCore + render resources)
    <- NoMoreDayEngine (aggregate: links all sub-targets, defines no sources of its own)
```

Rule: if the include scan shows `EngineSystems -> EnginePasses` does not exist, `EngineSystems` simply does not link `EnginePasses`. The aggregate `NoMoreDayEngine` keeps the historical target name and PUBLIC dependency surface so Game/App/tests link list is untouched.

### 3.4 Game Internal Dependencies (`src/game`, 149 cpp) — Cycle Risk

The `systems/*` subdirectories are **not** acyclic. Verified cycles / cross-domain edges:

| Edge | Evidence |
|---|---|
| combat → skill | `CombatSystem.cpp` → `SkillSystem.hpp`; `DamageMitigationService.cpp` → `BladeResourceService.hpp`; `XPAwardingSystem.cpp` → `SkillSystem.hpp` |
| skill → combat | `SkillSystem.cpp` → CombatEventDispatcher/CombatSystem/CombatTelemetry/DamagePipeline/ProcBudgetManager/StatsSystem; `BladeResourceService.cpp` → CombatEventDispatcher; `AstrolabeSystem.cpp` → StatsSystem; `ProjectileSystem.cpp` → CombatSystem+DamagePipeline; `SummonSystem.cpp` → CombatTelemetry |
| world → combat | `MovementStanceSystem.cpp` → CombatEventDispatcher |
| item → combat | `InventorySystem.cpp`/`FragmentDropSystem.cpp` → CombatEventDispatcher/CombatEvents |
| states → combat/skill/world | `GameplayState.cpp` |
| game/render → combat/world | `GameplayRenderAdapter.cpp` |
| ui → many | `UISystem.cpp` couples 18+ files across skill/item/world/combat/data/persistence/physics |
| persistence → skill | `SaveManager.cpp` → SkillSystem |
| data → skill | `TalentLoader.cpp` → SkillSystem |
| ai → world | `EnemyAIBehaviors.cpp` |
| data ↔ skill | `data/SkillRegistry.cpp` is used by skill systems while `data/TalentLoader.cpp` includes skill |

Additional cycles discovered during M4 (Phase C) implementation, each with a concrete break (see §5.3 Phase C, §5.4):

| Edge | Evidence | Break |
|---|---|---|
| item ↔ persistence | `StashSystem.cpp:28/38/265` → `SharedStash::Get().getTab/getUnlockedTabCount/unlockNextTab`; `SaveManager.cpp:65/75/135/201/211/276` + `SharedStash.cpp:89/122` → `ItemFactory::serializeItem/restoreItem` | Move `SharedStash.{hpp,cpp}` from `persistence/` to `systems/item/`; persistence → item one-way |
| render ↔ ui | `GameplayRenderAdapter.cpp:58/676/796` → `UISystem::GetFont/GetRarityColor/State.hoveredItem`; `UISystem.cpp:586` → `GameplayRenderAdapter::VisibleItemCache::visibleItems` | Sink the shared UI/render state to a new `NoMoreDayGameUiShared` target (§5.3 Phase C) |
| item → render → ui → item | 17 writes to `GameplayRenderAdapter::s_itemGridDirty/s_itemGrid` from `LootGridSystem.cpp:9/13/15`, `FragmentDropSystem.cpp:85`, `InventorySystem.cpp:113/153/232/269/309/1082`, `DropSystem.cpp:129/145/328/345/491/536/566`; plus 25 ui→item calls | Same `UiShared` sink: item writes the grid flags, render writes `visibleItems` and reads UI theme, ui reads both — all via `UiShared`, removing `item→render`, `render→ui`, `ui→render` |
| world → persistence (one-way order violation) | `PortalSystem.cpp:139-140` → `SaveManager::Get().IsInitialized()/saveCharacterAsync` | Layering fix: place `persistence` below `world` in the order (§5.4) |

Additional direction-legal edges (link-only, no cycle): combat→utils (`XPAwardingSystem.cpp:53/58`), world→utils (`EnemySpawnSystem.cpp:427/435`), skill→modifier (`SkillSystem.cpp:2169-2177`), input→ui (`InputSystem.cpp:20-22/56`), render→vfx (`GameplayRenderAdapter.cpp:156/158/320`), persistence→skill (`SaveManager.cpp:249/255`), ui→persistence (`UIStash.cpp:163`). `AirWallRenderer.cpp` (`systems/render/`, header-only dep) belongs to `NoMoreDayGameRender`.

`src/game/combat_v2/` (8 cpp) is self-contained and acyclic (CombatV2RuntimeFacade, ConditionCompiler/IR, DamageKernel, ModifierGraph, ModifierSourceAdapters, TagBitset, TagDomain). `systems/modifier`, `systems/nemesis`, `systems/physics`, `systems/ai`, `systems/vfx`, `utils/` are low-coupling candidates pending an include scan.

**Implication:** mechanically splitting every `systems/*` subdirectory into its own static library today would produce CMake target cycles and fail to configure. The split must break cycles first (§5.4).

### 3.5 Contract Implementation Facts (determines what can sink)

The fan-in headers above differ sharply in their `.cpp` dependencies. This decides which files can sink to a shared contract layer with zero behavior change:

| Header | `.cpp` deps on skill/world? | Verdict |
|---|---|---|
| `CombatEvents.hpp` | header-only: enum + struct + inline factories (`CreateDealDamage` etc.) | sink as pure INTERFACE header |
| `CombatFormula.hpp` | header-only: inline math (`LevelFactor`, `CalculateArmorMultiplier`, …) | sink as pure INTERFACE header |
| `CombatEventDispatcher.cpp` | none (only components + CombatTelemetry/ProcBudgetManager) | sink with implementation |
| `CombatTelemetry.cpp` | none (only its own header) | sink with implementation |
| `ProcBudgetManager.cpp` | none (only its own header) | sink with implementation |
| `CombatAntiMeta.cpp` | none (own header + core/logging + json; hpp deps components/Stats + data/SkillContract + data/TagRegistry) | sink with implementation |
| `StatsSystem.cpp` | none (components + data registries + CombatFormula/Telemetry/AntiMeta + AttributePipeline + MonsterScaling + render/GPUParticleSystem) | sink with implementation; requires contract layer to sit above `data`/`stats`/`utils` |
| `DamagePipeline.cpp` | **yes** → SkillSystem (`GetTriggerEffectivenessForCast`/`GetActiveTransmuterNode`/`ShadowCast`), CombatSystem, DamageMitigationService, combat_v2, data/SkillRegistry | **stays in combat domain**; only its value structs + resolution signature sink |
| `CombatSystem.cpp` | **yes** → SkillSystem::ShadowCast, MovementStanceSystem::OnTakeDamage | stays in combat domain |
| `TalentLayoutService.cpp` | none (own header + STL; hpp deps data/TalentData + components/Common) | **moves to data domain** (breaks `data↔skill`) |
| `MovementStanceSystem.cpp` | none (own header + components + CombatEventDispatcher + core/logging; hpp deps entt) | **moves to combat domain** (breaks `combat↔world`) |
| `AttributePipeline.cpp` | stats → combat/CombatFormula (CalculateDodgeChance/BlockEffectiveness/ArmorMultiplier) | stays in stats; the CombatFormula dependency is removed by sinking CombatFormula to Contracts |
| `MonsterScaling.cpp` | none (own header + components) | stays in utils |

Call-surface facts (verified by inspection):

- `skill → combat` hard calls are only `DamagePipeline::Execute`/`CalculateBatch` (SkillSystem:993, ProjectileSystem:604/642, SummonCombatBridge:228, plus behaviors BloodSea/HeavenlySwordDescent/SevenStarSlash/SwordArray/MindBlade/FlowingThrust). `CombatSystem.hpp` is included by SkillSystem but never called — that include is cleaned up.
- `combat → skill` real calls: `DamagePipeline` (GetTriggerEffectivenessForCast, GetActiveTransmuterNode, ShadowCast), `CombatSystem` (ShadowCast). `XPAwardingSystem` and `DamageMitigationService` include skill headers but are secondary.
- `world → combat` is only via `CombatEventDispatcher` (sinks to Contracts).
- `item → combat` is only via `CombatEventDispatcher`/`CombatEvents` (both sink).
- `data → skill` is only `TalentLoader` → `TalentLayoutService` (moves into data).

This makes the entire Game split feasible in one pass: sink the contract layer (§5.3 Phase A), invert the single real `combat↔skill` call surface through `DamageResolutionHooks` (§5.3 Phase B), move two files across domains (§5.3 Phase B), then split every remaining directory (§5.3 Phase C).

## 4. Unity Build Removal

Straightforward deletion per §3.2. `build.bat` needs no change: it never referenced Unity Build. After removal the only build-acceleration mechanisms left are parallel compilation (`/MP` + `UseMultiToolTask`, `build.bat` default) and the compiler cache (`NMD_ENABLE_COMPILER_CACHE`, default ON). No behavioral risk.

## 5. Sub-Project Split Design

### 5.1 Naming And Layout Rule

Every sub-project is a new directory CMake entry: `add_subdirectory(<dir>)` plus `<dir>/CMakeLists.txt` that defines one static (or INTERFACE) library named `NoMoreDay<Area>`. The parent layer's CMakeLists removes the moved sources and links the sub-targets. Include directories remain root-relative (`${CMAKE_SOURCE_DIR}/src`) so `#include` paths in sources do not change.

### 5.2 Engine Sub-Targets (sources relocated from src/engine/CMakeLists.txt)

| New target | Sources |
|---|---|
| `NoMoreDayEngineRenderCore` | `physics/SIMDSpatialGrid.cpp`; `render/core/*` (4); `render/debug/*` (5); `render/gi/*` (1); `render/graph/RenderGraph.cpp`; `render/lighting/*` (3); `render/particle/*` (2); `render/resource/*` (2); `render/resources/*` (4); `render/shadow/*` (3); `render/trail/*` (1); `render/validation/*` (1); `render/GPUABIContract.cpp`; `render/GPUUtils.cpp`; `render/MaterialManager.cpp`; `render/MDIRenderer.cpp`; `render/PersistentBuffer.cpp` |
| `NoMoreDayEngineResource` | `resource/AssetLoadingSystem.cpp`; `resource/ResourceManager.cpp` (keep `SKIP_PRECOMPILE_HEADERS ON`) |
| `NoMoreDayEnginePasses` | `render/passes/*` (21) |
| `NoMoreDayEngineSystems` | `render/GPUEntitySystem.cpp`, `render/GPUFlowFieldSystem.cpp`, `render/GPULootSystem.cpp`, `render/GPUParticleSystem.cpp`, `render/GPUSkillEffectSystem.cpp`, `render/GPUTextSystem.cpp`, `render/LootTextBatcher.cpp`, `render/PopupRenderer.cpp`, `render/RenderSystem.cpp`, `render/GPUTrailRenderer.cpp` |
| `NoMoreDayEngineVfx` | `vfx/VFXBudgetEstimator.cpp`; `vfx/VFXSequenceManager.cpp` |
| `NoMoreDayEngine` (aggregate) | no sources; `target_link_libraries(... PUBLIC RenderCore Resource Passes Systems Vfx)` + all existing PUBLIC deps (NoMoreDayCore, raylib, glfw, spdlog, EnTT, Taskflow, xsimd, winmm/opengl32/dbghelp, definitions, PCH policy) |

All Engine sub-targets reuse `src/pch.hpp` where it matches their includes; sub-targets whose translation units must not be PCH'd keep per-source `SKIP_PRECOMPILE_HEADERS`. `add_dependencies(... GenerateTags)` stays on the aggregate (and any sub-target that includes the generated header).

### 5.3 Game Sub-Targets

**Phase A — contract layer `NoMoreDayGameContracts` (sink-first, zero behavior change):**

Sub-targets sink the cross-system headers whose implementations do not depend on skill/world (§3.5):

| New target | Kind | Sources |
|---|---|---|
| `NoMoreDayGameContractsCore` | INTERFACE (header-only) | `systems/combat/CombatEvents.hpp`, `systems/combat/CombatFormula.hpp`; new `DamagePipelineTypes.hpp` (struct `DamageRequest`/`DamageResult`/`DamageExecutionResult` moved from `DamagePipeline.hpp`) and new `DamageResolutionHooks.hpp` (hook registration API) |
| `NoMoreDayGameContracts` | STATIC | `systems/combat/CombatEventDispatcher.{hpp,cpp}`, `CombatTelemetry.{hpp,cpp}`, `ProcBudgetManager.{hpp,cpp}`, `CombatAntiMeta.{hpp,cpp}`, `StatsSystem.{hpp,cpp}` (implementation + header sink together) |

`NoMoreDayGameContracts` links PUBLIC `NoMoreDayGameContractsCore` and the data/stats/utils sub-targets it needs (`StatsSystem` requires `data` registries, `stats/AttributePipeline`, `utils/MonsterScaling`). If `StatsSystem`'s `data` dependency cannot be satisfied without reintroducing a cycle, `StatsSystem` stays in the `combat` domain and only its header usage is re-pointed at the contract layer; this is a per-file fallback, not the default.

**Phase B — cycle-breaking code changes (only these two files move, plus hook wiring):**

1. `TalentLayoutService.{hpp,cpp}` moves from `systems/skill/` to `data/`; `TalentLoader` keeps its call — now `data → data`, cycle gone.
2. `MovementStanceSystem.{hpp,cpp}` moves from `systems/world/` to `systems/combat/`; `world` no longer references `combat`, cycle gone.
3. **Damage-resolution hook inversion** (`combat↔skill` cycle):
   - `DamagePipeline.hpp` keeps only the pipeline class (static methods) in the combat domain; its value structs move to `DamagePipelineTypes.hpp` in `ContractsCore`.
   - New `DamageResolutionHooks` in `ContractsCore` (pseudocode in §5.4): a registered `std::function` set for `Execute` and `CalculateBatch`; `RegisterDamageResolutionHooks(hooks)` / `ClearDamageResolutionHooks()`.
   - `DamagePipeline.cpp` (stays in combat) registers the hooks at startup and implements the same math; no behavior change.
   - All `skill` domain callers (SkillSystem, ProjectileSystem, SummonCombatBridge) and all 13 `SkillBehaviors` switch from `DamagePipeline::Execute(...)` to `DamageResolutionHooks::Execute(...)`; they no longer include combat headers.
   - `SkillSystem.cpp` drops its inert `CombatSystem.hpp` include.
   - `combat → skill` calls (`ShadowCast`, `GetTriggerEffectivenessForCast`, `GetActiveTransmuterNode`) stay as-is: `combat` is the upper domain depending on `skill` one-way.

**Phase C — full split in one pass (all remaining directories):**

| New target | Sources |
|---|---|
| `NoMoreDayGameCombatV2` | `combat_v2/*` (8) |
| `NoMoreDayGameCombat` | `systems/combat/*` remaining after Phases A+B (CombatSystem, DamagePipeline, HazardSystem, XPAwardingSystem, DamageMitigationService, BossFrameworkSystem, CombatHistorySystem, EffectSystem, EliteModifierSystem, EndgameModifierContract, ProgressionSystem, VisualFXSystem, MovementStanceSystem, …) |
| `NoMoreDayGameSkill` | `systems/skill/*` remaining after Phase B (13 systems) |
| `NoMoreDayGameData` | `data/*` (8) + moved `TalentLayoutService` |
| `NoMoreDayGameWorld` | `systems/world/*` remaining after Phase B (13) |
| `NoMoreDayGameItem` | `systems/item/*` (12) + moved `SharedStash.{hpp,cpp}` (from `persistence/`; breaks `item↔persistence`) |
| `NoMoreDayGameUi` | `systems/ui/*` (18) |
| `NoMoreDayGameStats` | `systems/stats/AttributePipeline.cpp` |
| `NoMoreDayGameModifier` | `systems/modifier/*` (8) |
| `NoMoreDayGameNemesis` | `systems/nemesis/*` (2) |
| `NoMoreDayGamePhysics` | `systems/physics/PhysicsSystem.cpp` |
| `NoMoreDayGameAi` | `systems/ai/*` (3) |
| `NoMoreDayGameVfx` | `systems/vfx/*` (4) |
| `NoMoreDayGameInput` | `systems/input/InputSystem.cpp` |
| `NoMoreDayGameScene` | `scene/*` (2) |
| `NoMoreDayGamePersistence` | `persistence/SaveManager.cpp` only (after `SharedStash` move) |
| `NoMoreDayGameRender` | `render/*` (7) + `systems/render/AirWallRenderer.cpp` |
| `NoMoreDayGameStates` | `states/*` (10) |
| `NoMoreDayGameUtils` | `utils/MonsterScaling.cpp` |
| `NoMoreDayGameUiShared` | new shared-state target holding the render/UI/item interchange: `GameplayRenderAdapter::s_itemGridDirty`/`s_itemGrid`, `VisibleItemCache::visibleItems`, and UI theme/state consumed by the render adapter (`UISystem::GetFont`, `GetRarityColor`, `State.hoveredItem`, `UIRenderer::GetRarityColor`). Holds only state + POD/accessors; links `NoMoreDayCore`/`NoMoreDayTypes` and any component headers it needs. |
| `NoMoreDayGame` (aggregate) | no sources; PUBLIC-links every Game sub-target + SkillBehaviors |

The `UiShared` target is the low-level interchange for the `item↔render↔ui` cycle: `item` writes the grid-dirty flags through it, `render` writes `visibleItems` and reads the UI theme through it, `ui` writes the UI state and reads `visibleItems` through it. No direct `item→render`, `render→ui`, or `ui→render` edges remain. The specific relocation (which state lives where, accessor names) is finalized by the M4 implementation with an include scan, per the same rules as §5.4.

`NoMoreDayGamePersistence` links `NoMoreDayGameItem` (SaveManager uses `SharedStash`/`ItemFactory`), `NoMoreDayGameWorld` links `NoMoreDayGamePersistence` (PortalSystem save), `NoMoreDayGameCombat` links `NoMoreDayGameSkill`+`NoMoreDayGameWorld`+`NoMoreDayGameStats`+`NoMoreDayGameCombatV2`, `NoMoreDayGameRender` links `NoMoreDayGameUiShared`+`NoMoreDayGameVfx`+`NoMoreDayGameCombat`+`NoMoreDayGameWorld`, `NoMoreDayGameUi` links `NoMoreDayGameUiShared`+`NoMoreDayGameItem`+`NoMoreDayGamePersistence`+`NoMoreDayGameCombat`+`NoMoreDayGameSkill`+`NoMoreDayGameWorld`. Every sub-target links exactly the sub-targets it scans to depend on. The static order inside the aggregate follows the one-way dependency order of §5.4.

### 5.4 Cross-Sub-Project Contract Rules

- Sub-target dependency graph must remain a DAG. Any include edge that would close a cycle is forbidden; the three sanctioned breakers are (a) **sink** the shared header+implementation into a lower-level sub-project, (b) **invert** the call surface through a registered hook/interface in the lower level, or (c) **move ownership** of the file to the domain that produces the dependency.
- Public include directories stay `${CMAKE_SOURCE_DIR}/src` (no per-sub-project include-root churn); source files are not physically moved during this design unless a sub-project's manifest is the only expression of ownership (physical relocation follows, matching the modular-split convention). **Physical convergence completed 2026-08-10** (M6 of [Directory Structure Reorganization](./directory-structure-reorganization-design.md)): the seven top-band directories now live under `game/application/` (`scene/ persistence/ render/ states/ ui/ input/`, with the two header-only progression systems under `states/`); include paths were prefix-rewritten to `game/application/...` and closure checks are green.
- A sub-target may not re-export include directories or definitions it does not own; PUBLIC vs PRIVATE follows the existing Engine/Game split convention.
- `NoMoreDayTypes` admission rules from the modular-split design are unchanged; no new types enter `src/` INTERFACE target during this work.
- `scripts/check_module_boundaries.py` keeps running in `build.bat` pre-checks; its layer-level rules are unaffected by intra-layer splitting.

The final Game one-way dependency order (bottom-up) is:

```text
NoMoreDayGameContractsCore / NoMoreDayGameContracts / NoMoreDayGameCombatV2
  -> NoMoreDayGameData / NoMoreDayGameUtils / NoMoreDayGameStats / NoMoreDayGameModifier
       / NoMoreDayGameUiShared
    -> NoMoreDayGameSkill / NoMoreDayGameNemesis / NoMoreDayGamePhysics / NoMoreDayGameAi
         / NoMoreDayGameItem / NoMoreDayGameVfx
      -> NoMoreDayGamePersistence
        -> NoMoreDayGameWorld
          -> NoMoreDayGameCombat
            -> NoMoreDayGameScene / NoMoreDayGameInput / NoMoreDayGameRender
                 / NoMoreDayGameStates / NoMoreDayGameUi
              -> NoMoreDayGame (aggregate + SkillBehaviors)
```

Differences from the pre-M4 order: `NoMoreDayGameUiShared` is a new bottom layer for the `item↔render↔ui` interchange; `NoMoreDayGameItem` sits above `Skill`/`UiShared` (and carries the moved `SharedStash`); `NoMoreDayGamePersistence` moves below `World` (breaking `world→persistence`). The ordering is advisory for link order; actual links follow the include scan (DAG-only).

**Hook inversion pseudocode** (contract layer, `ContractsCore`):

```text
namespace NoMoreDay {
// moved value types, in DamagePipelineTypes.hpp
struct DamageRequest { /* attacker, victim, skill, … unchanged */ };
struct DamageResult  { /* … unchanged */ };
struct DamageExecutionResult { /* … unchanged */ };

// in DamageResolutionHooks.hpp
struct DamageResolutionHooks {
    std::function<DamageExecutionResult(entt::registry&, const DamageRequest&, entt::entity)> execute;
    std::function<std::vector<DamageResult>(entt::registry&, const DamageRequest&)> calculateBatch;
};
void RegisterDamageResolutionHooks(const DamageResolutionHooks& hooks);  // called by combat domain
void ClearDamageResolutionHooks();                                       // called at shutdown
DamageExecutionResult ResolveDamage(entt::registry&, const DamageRequest&, entt::entity);
}

// skill / behaviors call ONLY:
ResolveDamage(registry, request, victim);   // -> registered hook, no combat include
```

The combat domain's `DamagePipeline` implements the same math behind the hooks and registers them during `CombatSystem`/pipeline initialization. If no hook is registered the resolver no-ops with a logged warning (startup ordering guarantee: hooks register before gameplay starts).

### 5.5 Tests

`NoMoreDayTests` remains one executable with the same source glob and CTest registration. Delete the `SKIP_UNITY_BUILD_INCLUSION` block and the `UNITY_BUILD ON` property (§3.2). Its link list keeps the four layer targets (`NoMoreDayCore NoMoreDayEngine NoMoreDayGame NoMoreDayApp`); no new test target is introduced in this design.

## 6. Impact Assessment

| Aspect | Impact |
|---|---|
| Public interfaces / headers | Damage value structs (`DamageRequest`/`DamageResult`/`DamageExecutionResult`) relocate from `DamagePipeline.hpp` to contract headers; a new `DamageResolutionHooks` registration API is added. All other public headers unchanged. `SkillBehaviors` and skill systems route damage through the hooks with identical math. |
| Save / asset / gameplay data | None: zero gameplay-balance change; damage math is untouched (only call routing). |
| PCH | `src/pch.hpp` (Engine) and `src/game/pch.hpp` (Game) unchanged. ResourceManager.cpp keeps `SKIP_PRECOMPILE_HEADERS`. |
| Build time | No guaranteed speedup (faster than current due to dead Unity config removal is not expected to be measurable); finer incremental rebuild scope expected. |
| Link order / static registration | `SkillBehaviors` OBJECT stays linked PUBLIC into Game; registration objects still reach exe and tests. Static-library link order inside the aggregate follows target order in `target_link_libraries`; verified at gate. |
| `build.bat` / CI | No option changes; existing `check`/`gate`/`combat-gate` flows unchanged. |
| Codebase governance | `check_module_boundaries.py`, ABI governance, JSON validation untouched. |

## 7. Acceptance Criteria And Verification

1. `rg -n "UNITY_BUILD|SKIP_UNITY_BUILD" CMakeLists.txt src tests` returns zero hits (historical `docs/` and `conductor/archive` excluded).
2. `build.bat` configures and builds clean in `RelWithDebInfo` (default) with `j=7`.
3. New CMake structure: `src/engine/CMakeLists.txt` and `src/game/CMakeLists.txt` contain only `add_subdirectory` + aggregate definitions; each sub-directory has its own CMakeLists with one target.
4. Configure-time graph check: `cmake --build build --target help` lists all new targets; a script (or manual `cmake` inspection) confirms no cyclic `target_link_libraries`.
5. `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` passes (nonperf suite).
6. `build.bat check` passes (JSON validation + module boundaries + ABI governance).
7. `NoMoreDayTests` still runs with the same labels; GPU-Diagnostic and performance suites unaffected (verify spot runs `-L gpu;contract`, `-L performance` if hardware allows).
8. Game split: `scripts/check_module_boundaries.py` and a cycle guard (see §8 R5) both pass on the final graph; skill/combat/item/world/ui module-gate CTest labels pass after the `DamageResolutionHooks` inversion.

## 8. Risks, Open Questions, Dependencies

- **R1 (EngineRenderSystems → EnginePasses direction).** Whether `RenderSystem.cpp` drives pass targets directly is unverified. Resolve by include scan before finalizing §5.2 links; if a cycle appears, merge `Systems`+`Passes` into one render sub-project.
- **R2 (hook registration ordering).** `DamageResolutionHooks` must be registered before any skill/behavior resolves damage, or damage silently no-ops. Mitigation: register during `CombatSystem`/pipeline init (before gameplay starts), log a warning when invoked with no hook, and add a unit test that asserts registration happens (combat init test).
- **R3 (StatsSystem sink dependency).** `StatsSystem.cpp` needs `data` registries and `utils/MonsterScaling`; sinking it before those sub-targets exist is impossible. Mitigation: split `data`/`utils`/`stats` in the same milestone as the contract layer, or keep `StatsSystem` in combat domain (per-file fallback in §5.3 Phase A).
- **R4 (PCH ownership in sub-targets).** Each new static library gets its own PCH or inherits none; ensure no sub-target compiles with two conflicting PCH flags. Mitigation: sub-targets reuse the layer PCH file; per-source `SKIP_PRECOMPILE_HEADERS` carries over with the source.
- **R5 (cycle guard).** Consider a small `tools/cmake_graph_check.py` that parses `target_link_libraries`/`add_library` and asserts DAG-ness after configure, wired into `build.bat check`. Open decision: implement now or gate on Phase C.
- **R6 (target count churn).** ~21 Game targets and ~6 Engine targets will appear in VS solutions; acceptable per user preference (更细粒度), monitored via `--target help` listing.
- **R7 (UiShared extraction scope).** Sinking `s_itemGridDirty`/`s_itemGrid`/`visibleItems`/UI-theme state into `NoMoreDayGameUiShared` is a small, mechanical relocation of static state and accessors — it must not change grid logic, item rendering, or UI layout. It is exercised by the existing LootGridSystem/InventorySystem/UI tests. If an accessor is entangled with a large system, that accessor stays with its owner and only the dependency direction is re-routed (per-file fallback).
- **R8 (persistence below world).** Moving `NoMoreDayGamePersistence` below `NoMoreDayGameWorld` is purely a layering fix; save/load behavior, ordering, and the save schema are unchanged. PortalSystem's `SaveManager::Get()` call is unchanged.
- **Dependency.** This design sits on the completed MS-0…MS-6 split; MS-7 (explicit static targets + per-target PCH) and MS-8 (physical layout convergence) remain future work and are compatible with this design's sub-project layout.
- **Open question:** should `NoMoreDayEngine`/`NoMoreDayGame` aggregate libraries be kept at all after the split (for a stable link surface), or should Game/App/tests link the sub-targets directly? Default in this design: keep the aggregates, because `check_module_boundaries.py` and existing test/CTest registrations reference the layer targets.

## 9. Milestones

- **M1 — Unity Build removal.** Apply §3.2 deletions; build + `check`; grep clean. (Small, self-contained, low risk.)
- **M2 — Engine sub-project split.** §5.2; resolve R1 via include scan; build + ci tests.
- **M3 — Game split Part 1: contracts + ownership moves.** §5.3 Phase A+B: create `NoMoreDayGameContractsCore`/`Contracts`, move `TalentLayoutService` and `MovementStanceSystem`, wire `DamageResolutionHooks` (all damage callers switched to hooks, same math). Build + full ctest; behavior parity via existing combat/skill tests.
- **M4 — Game split Part 2: full sub-project split.** §5.3 Phase C: split all remaining directories into ~20 sub-targets, including the `UiShared` state target, the `SharedStash` move to item, the `AirWallRenderer` ownership, and the persistence-below-world layering fix; one-way links per §5.4 order; build + all module-gate CTest labels.
- **M5 — Optional cycle guard tooling (R5).**
