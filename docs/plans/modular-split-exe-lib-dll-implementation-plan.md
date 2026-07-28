# Modular Static Target Split Implementation Plan

**Design reference:** `docs/designs/modular-split-exe-lib-dll-design.md`
**Status:** MS-0 [x]; MS-1 through MS-8 [ ] (MS-6 remains P0-blocked)
**Execution model:** Each milestone is implemented by an `implementer` subagent, reviewed by an independent `reviewer`, then committed only after a `提交` conclusion. The design document is user-owned worktree state and is never edited, staged, or committed by this initiative.

## Goal

Replace the current monolithic, misnamed `NoMoreDayCore` compilation unit with the dependency-ordered static target graph:

```text
NoMoreDay.exe -> NoMoreDayApp -> NoMoreDayGame -> NoMoreDayEngine
                                            -> NoMoreDayCore -> NoMoreDayTypes
```

`SkillBehaviors` remains a Game-internal OBJECT input during the transition. DLL creation is explicitly out of scope until the static boundaries have been proven.

## Fixed Constraints

- Do not modify `docs/designs/modular-split-exe-lib-dll-design.md` in this work package.
- Do not extract gameplay components, ECS registries, save schemas, render resources, or `SharedContext` into `NoMoreDayTypes`.
- Do not force a target split while reverse dependencies remain. Source ownership must be corrected before CMake topology changes.
- Do not alter RenderGraph, GPU resource ownership/lifetime, RenderSystem contracts, or ResourceManager ownership while `gpu_rendergraph_resource_foundation_20260726` remains P0/production NO-GO.
- `NoMoreDayTypes` contains only audited cross-module value types, identifiers, and enums. A type must have at least two real target consumers before it moves there.
- Use explicit source manifests and target-specific PCHs only after their source boundaries are valid; do not preserve `GLOB_RECURSE` as the final architecture.
- Existing behavior, save data, renderer behavior, and test labels remain compatible unless a later milestone explicitly defines and validates a migration.

## Current Baseline

- CMake currently creates one `NoMoreDayCore` static library from almost all `src/*.cpp`; it is not the future Core layer.
- `src/pch.hpp` leaks Game and Engine headers into the current aggregate target.
- Engine currently includes Game/App headers and uses `SharedContext`; those reverse dependencies are the first blocking condition.
- `GPUEntitySystem` and RenderSystem-related `SharedContext` paths are blocked by the P0 rendering track.

## Milestones

### MS-0 [x]: Ownership Ledger and Boundary Guard

**Objective:** Establish a complete, machine-checked inventory of all current candidate Core/Engine-to-Game/App include edges and lower-layer PCH leakage. Prevent additions before ownership migration begins.

**Scope:** Documentation, Python validation, and `build.bat` precheck wiring only. No C++ source moves, target creation, RenderGraph changes, GPU lifetime changes, or runtime behavior changes.

**Files:**

- Create `docs/reports/modular-split-exe-lib-dll/ms-0/reverse-dependency-ledger.json`
- Create `scripts/check_module_boundaries.py`
- Modify `build.bat`
- Update this plan when the milestone is accepted

**Ledger contract:**

- Every discovered `#include "game/..."` or `#include "app/..."` under a candidate `src/engine/` or `src/core/` source/header is a separate entry keyed by source path, line, and normalized include path.
- `src/pch.hpp` is also audited as a candidate PCH for Core/Engine, even though it does not currently map to a separate target.
- Each entry declares exactly one `candidate_target`, `candidate_layer`, `current_owner`, `future_owner_layer`, `disposition`, `milestone`, and `p0_blocking` value.
- `disposition` is one of: `move_to_game`, `move_to_app`, `split_engine_primitive_and_game_adapter`, `complete_dto_contract`, `remove_from_lower_pch`, or `remove_dead_code`.
- Entries blocked by P0 name the blocking track/task and remain observable; they are not exempted from the ledger.
- An edge is valid only while its source evidence exists. Removing the include without removing its ledger entry is a stale-ledger error, ensuring the inventory monotonically shrinks rather than becoming a permanent allowlist.

**Guard behavior:**

1. Scan candidate Core/Engine source files and `src/pch.hpp` for quoted Game/App includes.
2. Fail on an observed edge that has no exact ledger entry.
3. Fail on a ledger entry whose source line/include evidence no longer exists.
4. Fail on invalid owner/disposition/milestone metadata or duplicate evidence keys.
5. Print totals by candidate target, target owner, disposition, and milestone so removals are visible in build logs.
6. Run from `build.bat check` and normal validated builds before CMake configuration.

**Atomic tasks:**

- [x] MS-0.1 Create the plan, define the ledger schema and freeze the milestone order.
- [x] MS-0.2 Generate and review the complete reverse-edge/PCH ledger from current source evidence.
- [x] MS-0.3 Implement the deterministic boundary guard with positive, unregistered-edge, stale-entry, and malformed-entry test fixtures or self-tests.
- [x] MS-0.4 Wire the guard into normal `build.bat` validation and prove `build.bat check` exercises it.
- [x] MS-0.5 Run the focused guard, `build.bat check`, and `git diff --check`; record verification and obtain an independent `提交` review.

**Pseudocode:**

```text
ledger = load_and_validate_json(ledger_path)
observed = scan(candidate_core_engine_sources + pch, quoted_game_or_app_includes)
require every observed evidence key in ledger
require every ledger evidence key in observed
require each entry has one permitted ownership disposition and milestone
print grouped summary
exit nonzero on any mismatch
```

**Interface sketch:**

```text
NoMoreDayTypes (INTERFACE) <- NoMoreDayCore (STATIC)
  <- NoMoreDayEngine (STATIC) <- NoMoreDayGame (STATIC)
  <- NoMoreDayApp (STATIC) <- NoMoreDay.exe
```

The ledger guard is intentionally independent of that future CMake graph: it
models current source ownership while the graph remains unchanged. No DLL work
is in scope. GPUEntitySystem, RenderSystem, RenderGraph, render passes,
ResourceManager, and resource/GPU lifecycle changes remain deferred to the P0
GPU/render track.

**Acceptance evidence:**

```powershell
python scripts/check_module_boundaries.py
python -m unittest tests/python/ModuleBoundaryCheckerTest.py
.\build.bat check
git diff --check
```

**Review state (2026-07-29):** Two corrective review rounds concluded `修改`; the final independent review concluded `提交`. Evidence: the checker passed `129/129` entries across 37 files, six focused Python tests and `py_compile` passed, and `build.bat check` ran the precheck before CMake. Accepted residual risks are limited to direct quoted includes in the candidate scope; transitive, generated, and angle-bracket dependencies remain future audit work. The 66 P0 rendering/GPU edges remain fixed-policy tracked and MS-6 remains blocked.

### MS-1 [ ]: Minimal Types and Core Candidate Contract

**Objective:** Add an empty `NoMoreDayTypes` INTERFACE target and an audited Core candidate manifest/PCH inventory without moving Game-owned types.

**Atomic tasks:**

- [ ] MS-1.1 Add the empty Types target and verify it has no Game/App/Engine dependency.
- [ ] MS-1.2 Define Core candidate source and PCH manifests without enabling the final split.
- [ ] MS-1.3 Build RelWithDebInfo and run `ci` tests; review and commit the milestone.

### MS-2 [ ]: PhysicsUtils Ownership Correction

**Objective:** Move the Game-ECS-specific `PhysicsUtils` helper to Game ownership rather than promoting `Common.hpp`, `Position`, or `Velocity` to Types.

**Atomic tasks:**

- [ ] MS-2.1 Move only the gameplay helper and repair direct include consumers.
- [ ] MS-2.2 Add/adjust focused knockback regression coverage.
- [ ] MS-2.3 Verify build plus relevant physics tests; review and commit.

### MS-3 [ ]: Input and ECS Physics Ownership

**Objective:** Move Input action mapping and ECS physics/spatial grid policy to Game. Retain only future-proof, dependency-free Engine primitives when they have a real consumer.

**Atomic tasks:**

- [ ] MS-3.1 Move Game input mapping and add focused regression coverage.
- [ ] MS-3.2 Move ECS physics/spatial-grid adapters without changing collision semantics.
- [ ] MS-3.3 Run build, unit/integration and relevant performance coverage; review and commit.

### MS-4 [ ]: Persistence, Scene, and State Ownership

**Objective:** Place save schema, stash, scene orchestration, and state orchestration in Game while keeping App as composition only.

**Atomic tasks:**

- [ ] MS-4.1 Move persistence ownership without changing serialization format.
- [ ] MS-4.2 Move scene/state orchestration without inventing a premature `ILevelManager` Types abstraction.
- [ ] MS-4.3 Verify save and scene-transition behavior, review, and commit.

### MS-5 [ ]: UI Presentation Ownership

**Objective:** Move the current Game-policy `UIRenderer` ownership to Game; only extract pure Engine drawing primitives if a concrete consumer requires them.

**Atomic tasks:**

- [ ] MS-5.1 Move presentation-policy code and update path-sensitive UI tests.
- [ ] MS-5.2 Verify UI tests and normal build; review and commit.

### MS-6 [ ]: Render Boundary Adapter (Blocked)

**Objective:** Replace Game ECS/`SharedContext` exposure in GPU entity rendering with a Game adapter and narrow Engine DTO/upload contract.

**Status:** Blocked until the P0 rendering track accepts the relevant RG-3 resource-lifetime coordination. No implementation begins merely because prior milestones complete.

### MS-7 [ ]: Explicit Static Targets and Target PCHs

**Precondition:** The MS-0 guard has no remaining Engine/Core reverse edges except explicitly P0-blocked work that has subsequently been released and removed.

**Objective:** Replace aggregate globbing with explicit manifests and create the static dependency graph with target-specific PCHs and correctly scoped transitive dependencies.

**Atomic tasks:**

- [ ] MS-7.1 Create Types/Core/Engine/Game/App targets with explicit source lists and no cycle.
- [ ] MS-7.2 Replace global PCH with target-specific PCHs and forbid cross-layer PCH leakage.
- [ ] MS-7.3 Update test linkage by actual target dependency.
- [ ] MS-7.4 Verify Debug, RelWithDebInfo, and Release builds plus `ci`, `unit`, `integration`, and performance labels; review and commit.

### MS-8 [ ]: Physical Layout Convergence

**Objective:** Move files into final layer directories after target ownership is stable; remove temporary forwarding headers and ledger entries as their edges disappear.

**Atomic tasks:**

- [ ] MS-8.1 Move one completed ownership domain at a time with explicit manifest updates.
- [ ] MS-8.2 Require a zero-stale/zero-unregistered boundary guard after every domain.
- [ ] MS-8.3 Run full multi-configuration verification, final review, and commit.

## Validation Policy

- Documentation/script-only milestones run their focused Python guard, `build.bat check`, and `git diff --check`.
- C++ or CMake milestones additionally run `./build.bat` and the narrowest relevant test label; target-graph milestones run Debug, RelWithDebInfo, Release, `ci`, `unit`, `integration`, and Release `performance`.
- Build/test output is redirected to a log when large; review reports cite command, result, and log path rather than embedding raw output.
- Existing unrelated test failures are never hidden. They must be reproduced, scoped away from changed files, and reported as residual risk before a `提交` conclusion.

## Risks and mitigations

- **Line drift:** a moved or edited include fails the exact source/line/path
  comparison; update the ledger in the same ownership change.
- **PCH leakage:** `src/pch.hpp` is scanned as an explicit lower-layer input,
  preventing global-PCH imports from bypassing target boundaries.
- **P0 coupling:** render/GPU reverse edges remain ledger-visible and carry a
  blocking field; this package performs no GPU or resource-lifecycle change.
- **Dirty worktree:** only the package files listed in the milestone are
  eligible for a later commit; the existing design-document modification and
  unrelated work remain untouched.

## MS-0 completion criteria

MS-0 is complete only when the ledger has one entry per observed edge, the
checker passes the repository baseline, a synthetic untracked edge returns 1,
malformed input returns 2, focused Python verification and `git diff --check`
pass, and validated `build.bat check` runs the guard before CMake. No commit is
created by the implementation worker.

## Commit and Review Gates

- Do not create micro-commits for individual ledger rows or documentation wording.
- Commit only after an important milestone package has a `docs/reviews/YYYY-MM-DD-<topic>-review.md` report with conclusion exactly `提交`.
- Stage only the accepted package files; always verify `docs/designs/modular-split-exe-lib-dll-design.md` is excluded.
- Suggested messages: `build: add modular boundary inventory guard`, `refactor(game): move ecs physics ownership`, `build: split static module targets`.

## Definition of Done

1. The explicit static target graph is configured without cycles.
2. `NoMoreDayTypes` is minimal and contains no gameplay, ECS, rendering, or persistence ownership.
3. Core and Engine compile without Game/App direct includes or PCH leakage.
4. All ledger entries have been removed because their source edges are gone.
5. Debug, RelWithDebInfo, and Release builds plus `ci`, `unit`, `integration`, and performance validation have acceptable recorded evidence.
6. DLL work remains a separately approved follow-up, not an implicit consequence of this plan.
