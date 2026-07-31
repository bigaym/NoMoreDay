# Modular Architecture Split: dependency-first static libraries

> **Status:** revised 2026-07-28
> **Primary objective:** reduce ownership and dependency coupling. Faster incremental builds are a secondary benefit, not an implementation gate.
> **Related constraint:** [GPU RenderGraph and Resource Foundation](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/index.md) remains P0, In Progress, and production NO-GO.

## 1. Decision Summary

The project will not split the current source tree directly by directory name. It will first establish ownership boundaries and remove reverse dependencies, then express those boundaries through static CMake targets.

The intended end state is a one-way dependency graph:

```text
NoMoreDay.exe
  -> NoMoreDayApp          (STATIC: composition and application lifetime)
    -> NoMoreDayGame       (STATIC: game rules and game-facing adapters)
      -> NoMoreDayEngine   (STATIC: reusable engine services)
        -> NoMoreDayCore   (STATIC: platform-independent core utilities)
          -> NoMoreDayTypes (INTERFACE: minimal shared value types)
```

`SkillBehaviors` remains an OBJECT library during the migration, but is an internal implementation input of `NoMoreDayGame`, not a public architectural layer.

DLL conversion is deliberately removed from the implementation route. It requires a separate design decision after the static boundaries have demonstrated a real external-consumer need and a stable ABI contract.

## 2. Goals And Non-Goals

### 2.1 Goals

- Remove `Engine -> Game` and `Engine -> App` dependencies from both public headers and implementation sources owned by the Engine target.
- Make source ownership explicit: directory placement is temporary; the owning target and its public contract are authoritative.
- Keep gameplay ECS data, save schemas, level policy, and application composition owned by Game or App.
- Build a small, stable shared-type boundary instead of promoting all commonly included game types to a global layer.
- Give every target a minimal PCH and explicit CMake dependency contract.
- Preserve existing gameplay behavior, save formats, assets, render ordering, and graphics capability behavior throughout the refactor.

### 2.2 Non-Goals

- A compile-time baseline is not a prerequisite and no fixed speedup multiplier is promised by this design.
- Splitting the existing folders mechanically into libraries is not acceptable.
- Moving all of `game/components/Common.hpp`, gameplay constants, or game enums into `types/` is not acceptable.
- `NoMoreDayCore.dll`, export macros, and cross-DLL STL/exception contracts are not part of this work.
- This work does not redesign RenderGraph, GPU resource ownership, shader reload, or the active rendering foundation Track.
- This work does not introduce abstraction interfaces merely to hide an unresolved ownership problem.

## 3. Current Architectural Facts

`NoMoreDayCore` currently compiles almost all `src/` implementation files into one static library. The primary problem is therefore not the number of targets, but that target ownership does not match source dependencies.

| Observed dependency | Why it invalidates the former split | Required direction |
|---|---|---|
| `GPUEntitySystem` uses game ECS data and `app/SharedContext` | A reusable Engine cannot accept an App-owned service locator or directly operate on Game-owned state. | Move game adaptation to Game; pass Engine-owned render input instead. |
| `UIRenderer` exposes item, buff, and UI-context types | This is game presentation policy, not a reusable renderer contract. | Keep primitives in Engine and move game HUD/layout adaptation to Game. |
| `SceneManager` stores `LevelManager` and game async result types | Scene lifetime and level policy are still Game/App owned. | Move orchestration to Game/App or replace it with a complete Engine DTO contract. |
| `SaveManager` exposes `SaveData` and game persistence policy | Save schema and character/stash policy belong to Game. | Move game save orchestration to Game/App; retain only generic storage utilities below it. |
| `ResourceManager` depends on render utilities and shader governance | It is not a low-level Core service and owns graphics-device-facing state. | Keep it in Engine and coordinate any change with the P0 rendering Track. |
| `AudioSystem` owns Raylib device state | It is an Engine runtime service, not a platform-independent Core utility. | Keep it in Engine unless a later audit proves a lower boundary. |

`src/app/SharedContext.hpp` is an App-owned composition object. It may construct and wire systems, but it must never appear in Core or Engine public APIs.

## 4. Target Responsibilities

The following table defines the final ownership model. A source file may temporarily remain in its existing directory while being compiled by its correct target; physical relocation follows once the target boundary is stable.

| Target | Kind | Owns | Must not own or expose |
|---|---|---|---|
| `NoMoreDayTypes` | `INTERFACE` header-only target | Small stable IDs, enums, and POD/value types with genuine multi-layer ownership | Gameplay components, game constants, Raylib/OpenGL resources, `entt::registry`, JSON schemas, task/future ownership, service interfaces with game policy |
| `NoMoreDayCore` | static library | Logging, math, threading, platform-neutral utilities, and other code with no Engine/Game/App dependency | Rendering, audio-device lifetime, GPU resources, Game/App includes, `SharedContext` |
| `NoMoreDayEngine` | static library | Generic rendering primitives, graphics resource ownership, raw input, generic physics/query primitives, and audited runtime services | Game components, game save data, level policy, item/buff UI policy, `SharedContext` |
| `NoMoreDayGame` | static library | ECS registry and components, gameplay, level and save policy, game UI layout, game-facing Engine adapters, and `SkillBehaviors` objects | Application startup, executable entry point, global composition ownership |
| `NoMoreDayApp` | static library | `SharedContext`, settings composition, startup/shutdown order, and high-level orchestration | Game-rule implementation or Engine internals |
| `NoMoreDay` | executable | `main.cpp` and process entry | Reusable implementation code |

Third-party libraries are linked by the lowest target that owns their use. They are not propagated globally merely because a previous monolithic target exposed them.

## 5. Cross-Layer Contracts

### 5.1 Ownership And Lifetime

- Game owns the ECS registry, game components, game state, level data, item data, and save schemas.
- App owns application-wide composition and startup/shutdown sequencing through `SharedContext` or a successor composition root.
- Engine owns the execution of generic services and graphics resources. App controls when such services are created and destroyed through narrow Engine-facing APIs.
- Core owns no graphics device state, gameplay state, or application state.
- A target may only destroy resources it created or was explicitly assigned to own. Resource observation must not become alternate ownership.

### 5.2 Game-To-Engine Presentation Boundary

Game-facing rendering code must not make Engine operate directly on `entt::registry`, `Common.hpp`, item/buff types, or `SharedContext`.

The required direction is:

```text
Game ECS and game policy
  -> Game-owned adapter projects stable frame data
    -> Engine-owned render/input/physics DTO contract
      -> Engine executes rendering or generic service work
```

The projected data must contain only what the Engine operation needs, such as transforms, render handles, camera data, draw attributes, or engine-defined events. It must not retain references into the Game registry or expose game-domain types.

This applies first to the following areas:

| Current area | Intended boundary |
|---|---|
| `GPUEntitySystem` and entity synchronization | Game-owned adapter prepares Engine render input; generic GPU execution remains Engine owned. |
| `UIRenderer` | Engine owns drawing primitives; Game owns inventory, buff, rarity, HUD layout, and presentation decisions. |
| `SceneManager` | Level transitions and `LevelManager::LevelData` remain Game/App owned. Only a complete Engine-owned scene request/result DTO may cross downward. |
| `SaveManager` | Character/stash/global save policy remains Game/App owned. Generic file or serialization utilities, if independently useful, may remain below it. |
| Input systems | Raw device sampling can remain Engine owned; action mapping and gameplay response belong to Game. |
| Physics/spatial systems | Generic queries and collision primitives may remain Engine owned; Game ECS adapters belong to Game. |

Do not add `ILevelManager` to `types/` merely to legalize the current dependency. An interface is justified only after its input, result, error, threading, and ownership contract no longer contains Game or App types.

### 5.3 Public Header Rules

- Core public and private headers may not include `engine/`, `game/`, or `app/`.
- Engine public and private sources owned by `NoMoreDayEngine` may not include `game/` or `app/`, and may not take `SharedContext`.
- Engine public APIs may not expose game components, game data schemas, game-specific futures, or Game-owned registry state.
- Game may consume Engine public APIs, but Engine must not call back into Game. Use Engine-defined data/results or Game-owned adapters instead.
- App may compose Game and Engine but must not become a shared dependency of either layer.

During migration, these rules apply to the CMake target that compiles a source file, not solely to its current filesystem path. Each temporary mismatch must be recorded and removed during consolidation.

## 6. Shared Types And PCH Policy

### 6.1 `NoMoreDayTypes` Admission Rule

`NoMoreDayTypes` is an `INTERFACE` target, not a static library. Its public include root is `src`, so consumers use the consistent form `#include "types/<header>.hpp"`.

A type may enter `src/types/` only when all of the following are true:

1. At least two intended architectural targets genuinely need it.
2. Its owner is not Game-specific, App-specific, or render-resource-specific.
3. It has stable value semantics and no hidden service, task, registry, graphics, or serialization lifetime.
4. Changing it is expected to be rare and semantically cross-cutting.
5. Its public dependencies obey the same lower-layer boundary.

The initial extraction is intentionally minimal. `core/math/PhysicsUtils.hpp` should stop including `game/components/Common.hpp`, but only the audited data it needs should move downward. `Common.hpp`, gameplay constants, `HealthComponent`, `WeaponComponent`, `PlayerTag`, item/UI types, and biome/level policy stay in Game unless a separate ownership review proves otherwise.

### 6.2 Per-Target PCH Contract

PCH is a private build optimization, never a layer contract.

| Target | PCH rule |
|---|---|
| `NoMoreDayTypes` | No PCH. |
| `NoMoreDayCore` | No PCH initially, or a Core-only minimal PCH with no graphics, Game, or App headers. |
| `NoMoreDayEngine` | Engine-local PCH may contain approved Engine and third-party headers, never Game or App headers. |
| `NoMoreDayGame` | Game-local PCH may use Game, Types, and Engine public headers, never App composition headers. |
| `NoMoreDayApp` | App-local PCH may use composition dependencies. |

The existing global `src/pch.hpp` cannot be reused unchanged because it includes Game and Engine implementation-facing headers. It must be retired from cross-target use before the final target split.

## 7. CMake Boundary Rules

- CMake targets model ownership, not folder names. New targets are introduced only after their source assignment is acyclic.
- Maintain explicit source manifests or module-local CMake source lists. Do not use `GLOB_RECURSE` as the final authority for module membership, because a new file must not silently enter the wrong target.
- `PUBLIC` include directories and link dependencies are limited to requirements visible in a target's public headers or required static link interface. Implementation-only dependencies remain `PRIVATE`.
- Compiler optimization flags and warning policy are private build configuration, not public API propagation.
- `SkillBehaviors` object files are consumed by `NoMoreDayGame`; consumers do not link an independent behavior architecture layer.
- `NoMoreDayTests` links the narrowest target(s) needed by each test. Integration tests link the final App/Game composition. No DLL deployment rule is added in this design.
- Target-level dependency checks run in CI and reject new prohibited edges. A temporary source directory exception is allowed only when the source is assigned to its documented owner target and has a tracked physical-relocation task.

## 8. Implementation Route

### Phase 0: Ownership Inventory And Guardrails

1. Build a source/API ownership ledger for every current `Engine -> Game` and `Engine -> App` edge.
2. For each edge, choose exactly one disposition: move it to Game/App, split an Engine primitive from a Game adapter, or define a complete lower-layer DTO contract.
3. Add target-aware forbidden-include checks for Core and Engine candidate sources.
4. Identify PCH content that violates the intended layer of each candidate target.
5. Do not use compile-time benchmark collection as a gate for this phase.

**Exit condition:** every reverse edge has an owner and a migration disposition; no new reverse edge can enter unnoticed.

### Phase 1: Minimal Shared Boundary And Core Extraction

1. Create the empty or minimal `NoMoreDayTypes` interface target with the admission rule above.
2. Move only audited shared values needed to remove the `PhysicsUtils` dependency on Game.
3. Create `NoMoreDayCore` from code that is already independent of Engine, Game, and App. Do not place `engine/resource` or `engine/audio` in Core by default.
4. Establish Core and Types PCH rules and replace cross-target use of the existing project PCH.

**Exit condition:** Core has no Engine/Game/App include or link dependency, and Types contains no gameplay policy.

### Phase 2: Remove Game/App Dependencies From Engine

Migrate one vertical area at a time, retaining behavior while making ownership explicit:

1. Separate generic render execution from Game ECS synchronization and game UI policy.
2. Move level transition, save orchestration, and game-state policy to Game/App unless a complete Engine DTO contract is justified.
3. Split raw input from action mapping, and generic physics from Game ECS adapters.
4. Keep `ResourceManager`, shader reload, RenderGraph, and GPU resource lifetime in Engine. Do not alter their architecture while the P0 rendering Track is unfinished.
5. Assign transitional Game-owned adapter sources to the Game target before physically relocating them.

**Exit condition:** the Engine candidate target has no Game/App source or public-header dependency, no `SharedContext`, and no Game-owned state in its public API.

### Phase 3: Introduce Static Target Graph

1. Replace the monolithic source glob with explicit target source manifests.
2. Create `NoMoreDayEngine`, `NoMoreDayGame`, and `NoMoreDayApp` as static libraries in the one-way graph from Section 1.
3. Integrate `SkillBehaviors` into the Game implementation boundary.
4. Restrict target include/link visibility to each real API contract.
5. Update unit and integration test linkage to use the new target ownership model.

**Exit condition:** CMake configures an acyclic target graph; each target compiles with only its allowed PCH and dependencies.

### Phase 4: Consolidate Physical Layout

1. Relocate transitional sources to the directory matching their now-stable owner.
2. Reduce public headers, remove obsolete forwarding includes, and delete temporary dependency exceptions.
3. Re-run the forbidden-edge checks with no transitional exemptions.

**Exit condition:** directory layout, target ownership, include rules, and public APIs describe the same architecture.

### Phase 5: Separate DLL Decision, If Needed

No DLL conversion occurs in this design. A follow-up design may be proposed only when all conditions below are met:

- A concrete external-consumer requirement exists, such as a separately built tool or supported plugin boundary.
- The candidate module has no graphics-device or duplicate-third-party ownership ambiguity.
- Its public ABI has an explicit ownership, allocator, error, exception, and lifetime contract.
- The ABI avoids exporting mutable class layout or STL ownership across the boundary, preferably through opaque handles or a C-compatible boundary.
- Debug and Release deployment, test-runtime loading, and compatibility verification are specified.

## 9. Rendering Foundation Constraint

The [GPU RenderGraph and Resource Foundation Track](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/index.md) is P0, in remediation, and production NO-GO. It owns the active contract for typed resources, compiled plans, GPU resource observation, RAII ownership, timing, ABI/binding, reload, and capability governance.

Therefore:

- `ResourceManager` remains an Engine/render concern, not a Core or DLL candidate.
- This module refactor may define a future render data boundary, but must not simultaneously redesign resource ownership or RenderGraph execution.
- Any source touch in these areas must preserve the Track's typed-resource and single-owner contracts and be coordinated with its plan.
- The [rendering engine V5 master specification](../../conductor/specs/rendering_engine_v5_master_spec.md) remains authoritative for V5 rendering behavior and dependencies.

## 10. Verification And Acceptance

Compilation-speed measurement is intentionally not an acceptance criterion. Correctness and boundary evidence are required instead.

| Area | Required evidence |
|---|---|
| Dependency direction | Target-aware checks report no forbidden Core/Engine dependency; after Phase 4, no temporary exception remains. |
| CMake graph | Configure succeeds with no target cycle; each non-entry source appears in exactly one owning library or explicit object-library input, and `main.cpp` appears only in the executable. |
| PCH isolation | Each target compiles with its own approved PCH; Engine/Core compilation does not include Game/App headers through PCH. |
| Build correctness | Debug, Release, and RelWithDebInfo builds succeed at the completion of a phase that changes target topology. |
| Automated tests | Affected unit tests and integration tests pass; test linkage reflects the target under test. |
| Runtime behavior | The game starts, loads a representative level and save, and exercises the migrated presentation path without changing gameplay behavior. |
| Rendering safety | Changes touching rendering/resource code preserve the active Track's required build, contract-test, and hardware-gate evidence. |
| Data compatibility | No save schema, asset path, or user configuration format changes occur unless separately designed and verified. |

The following are explicitly not acceptance criteria: a claimed compile multiplier, a fixed link-time target, or the existence of a DLL artifact.

## 11. Risks, Rollback, And Open Decisions

| Risk | Mitigation and rollback |
|---|---|
| A false interface preserves Game coupling behind a new name | Require complete DTO/lifetime/error ownership before adding an Engine abstraction; otherwise assign the code to Game/App. |
| A broad component move expands recompilation fan-out | Apply the Types admission rule; keep high-churn gameplay components and constants in Game. |
| Static library split reveals missing link dependencies | Use explicit CMake link interfaces and target-scoped tests; revert the atomic target manifest change if needed. |
| PCH recreates the old dependency leak | Enforce per-target PCH allowlists and reject Game/App includes in Core/Engine PCHs. |
| Render/resource work conflicts with the P0 Track | Defer resource ownership changes; make only non-overlapping adapter or source-assignment changes until the Track accepts its contracts. |
| Future DLL creates ABI or duplicated graphics-state hazards | Keep DLL work out of this route; require the Phase 5 decision criteria in a separate design. |

No additional user decision is required to begin Phases 0 and 1. The only deferred product decision is whether a future external-consumer use case justifies a DLL boundary after the static architecture is stable.
