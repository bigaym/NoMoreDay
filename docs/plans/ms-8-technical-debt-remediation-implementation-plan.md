# MS-8 Technical Debt Remediation Implementation Plan

> **Status:** in progress; W1-W6 implementation complete and verified, W6.7 hardware matrix partially complete (mechanism validated on local RTX 4070S producing a fail-closed `NO_GO` artifact; the full 120-sample/100-toggle/60s-pressure/three-fixture production matrix awaits real-machine sampling). W1: QualityTierManager JSON fail-closed + metadata-only persistence + V3-domain-only serializer + atomic write (`WriteJsonAtomically`, `MoveFileExW MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH`); regression matrix covers sentinel preservation, runtime override/auto-degrade never persisted, unknown V3 child/field preservation, non-object root/render/render.v3 fail-closed; S7PairedGiDeltaTest reinit expectation corrected to preserved GI true. W2: 23 Python tests OK, real checker `0/0` PASS, schema 2.0 per-root policies + quote/angle + casefold/whitespace + P0 fully removed. W3: scalar `elementType` sole contract, real `SkillSystem::EncodeSkillVfxElementType/ResolveSkillVfxElementTypeFromTags` public helpers, `NormalizeSkillVfxElementType/NormalizeSkillVfxEvent` Engine single-boundary normalization, legacy mask/cast/`ResolveLegacyElementType` removed; queue-consumer seam residual P2 recorded. W4: `option(ENABLE_LTO OFF)` + Release-only IPO on 7 targets + `scripts/verify_release_lto.py` isolated proof (822 /GL rows, 2 /LTCG rows incl. NoMoreDay.exe, Release CTest, exe smoke exit 0), evidence in `docs/reports/release-lto-proof/evidence-20260802-100053/`. W5 (M0-B/RG-3): `build.bat` PASS; `*W5*` 4/28, `*GPUResourceRegistry*` 8/57, `*RenderGraph*` 31/217; registry accounting-safe semantics (duplicate reject + LOG_ERROR, missing no-op + LOG_WARN, saturating subtraction, pending age `< 9`), ResourceKind +VertexArray/ShaderProgram, observer binding, `PersistentBuffer::ResetState()`, `GPUEntitySystem` idempotent Shutdown + partial-init rollback + copy/move deleted, exact-one `AdvanceFrame()` in `RenderSystem::render` after `graph.Execute` success, gate manual advance removed. W6 (M0-C game-binary gate): `build.bat` PASS; ctest `-L gpu` contract 0.91s + diagnostic 22.08s (2/328) + gpu.hardware 83.51s `NO_GO` fail-closed; `-L integration` 6/6; `-L ci` only 2 pre-existing failures; Python runner 38 tests OK; real `bin\NoMoreDay.exe --gpu-gate` emits exactly one marker + one 367KB versioned artifact (verified `gate_status=NO_GO`, `return_code=0`, `schema_errors=0`, real NVIDIA vendor/driver, `render_hooks_supplied=true`, 9 cells with real 7-pass `executed_pass_order` from `RenderGraph::CompiledRenderPlan.passOrder`, SDF real `glGetTexImage` JFAPass readback, occupancy `missing_pending_m0a` + `blocks_go=true` fail-closed). Backlog/waivers: external target contract (5c257e22 attachment-query removal) recorded in M0-B as later item; remaining unregistered VAO/VBO owners and ResourceManager shader observability gaps documented as backlog; known local NO_GO root cause = raylib offscreen-FBO integration defect (256x GL_INVALID_OPERATION "Array object is not active" + black ROI) belongs to M0-C/production fix; local tests are registry/lifecycle contract evidence only, NOT production GO. M0-A remains NOT acceptable (R3 occupancy/disocclusion missing, R2 partial, R4/A/B evidence gaps) with its backlog defined; production GPU remains NO-GO.
>
> **Design:** [MS-8 technical debt remediation design](../designs/ms-8-technical-debt-remediation-design.md).
>
> **Scope:** an execution plan for the accepted MS-8 remediation design. This
> document is intentionally dispatchable to independent implementers and
> reviewers; it is not an implementation record and does not claim a GPU
> production `GO`.

## 1. Authority, Outcome, And Boundaries

### 1.1 Governing References

- [MS-8 evidence](../reports/modular-split-exe-lib-dll/ms-8/evidence.md)
- [GPU production remediation follow-up](../designs/gpu-production-remediation-follow-up.md)
- [V5 rendering master specification](../../conductor/specs/rendering_engine_v5_master_spec.md)
- [Rendering system progress](../../conductor/rendering_system_progress.md)
- [M0-A HDR/GI closure Track](../../conductor/tracks/gpu_production_hdr_gi_closure_20260726/spec.md)
- [M0-B RenderGraph resource foundation Track](../../conductor/tracks/gpu_rendergraph_resource_foundation_20260726/spec.md)
- [M0-C hardware validation Track](../../conductor/tracks/gpu_hardware_validation_gate_20260726/spec.md)
- [Testing workflow](../workflows/testing.md)
- [Debugging workflow](../workflows/debugging.md)

Repository documents above are authoritative if they conflict with this plan.
In particular, the production rendering state remains `NO-GO`; only a valid
M0-C game-binary artifact can change that state.

### 1.2 Completion Target

Complete the six packages in the approved design without changing the module
target topology:

```text
W1 Settings persistence data-loss repair
W2 Boundary policy migration and P0 removal
W3 Scalar Skill VFX element contract
W4 Release LTO declaration and MSVC proof
W5 RG-3 resource lifecycle under M0-B
W6 Game-binary hardware gate under M0-C
```

`settings.json` is local user-owned runtime state. It is excluded from every
package and must not be normalized, staged, reverted, or regenerated as part
of this work.

### 1.3 Fixed System Rules

1. Persisted user preference, effective runtime configuration, and automatic
   metadata are separate ownership domains.
2. A GL resource has exactly one releasing owner. `GPUResourceRegistry` only
   observes handles and never releases one.
3. A successful normal render frame advances resource observation once. It is
   never advanced once per pass or twice by a test gate.
4. A hidden standalone GL context may validate local lifecycle contracts. It
   may not validate visual quality, GI, SDF, ROI, performance, leak pressure,
   or production hardware readiness.
5. A missing GPU prerequisite produces `NOT_RUN`, never a synthetic pass.
6. The Engine owns dependency-neutral interfaces. Game/App own composition
   code that needs the initialized gameplay runtime.
7. Every M0 contract change updates the owning Track specification and plan
   before its source implementation begins.
8. No subagent creates a commit, worktree, or release declaration unless the
   user separately authorizes it.

### 1.4 Non-Goals

- Do not start SPH, Vulkan, visual-feature, target-topology, save-format, or
  tag-generation work.
- Do not widen the obsolete VFX tag mask from 32 to 64 bits.
- Do not add a global GL cleanup service or make registry reset delete GL
  resources.
- Do not turn a standalone doctest success, WARP result, or zero process exit
  into a hardware `GO`.
- Do not preserve the completed MS-6 P0 rule in live checker code merely for
  archival compatibility.

## 2. Execution Model

### 2.1 Dependency Graph

```text
W0 preparation
  +-- W1 settings persistence -----------+
  +-- W2 boundary policy and P0 ---------+-- package review and evidence
  +-- W3 scalar Skill VFX ---------------+
  +-- W4 Release LTO --------------------+
  |
  +-- M0-A owner accepts HDR/GI prerequisite
        +-- W5 M0-B RG-3 lifecycle
              +-- W6 M0-C game-binary gate
```

W1 through W4 can be implemented in parallel only after file reservations are
recorded. W4 serializes all edits to root CMake, presets, and `build.bat` with
any other build-system task. W5 must not start merely because W1-W4 complete:
the M0-A prerequisite review is a hard blocker. W6 begins only after W5/M0-B
acceptance and the owning M0-C Track accepts the required interface changes.

### 2.2 Required Package Lifecycle

Each package follows the same sequence:

1. Capture source baseline, current Track status, relevant CTest registrations,
   and pre-existing worktree changes.
2. Reserve the package files and identify any overlapping agent work.
3. Add or update a focused regression that demonstrates the defect or missing
   contract before the behavior change, where practical.
4. Implement the smallest ownership-preserving change.
5. Run the focused test first, then the package-level build and broader checks.
6. Update the package evidence and, for M0 work, the owning Track documents.
7. Obtain an independent review against the contract and the source diff.
8. Report pass, waiver, failure, and remaining risk separately. Do not state
   completion from an inferred outcome.

### 2.3 Shared Commands

Run these commands from repository root unless a task specifies another
directory. Use the actual configured build directory and configuration if a
package intentionally uses an isolated cache.

```powershell
./build.bat check
./build.bat
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
python -m unittest tests/python/ModuleBoundaryCheckerTest.py -v
python scripts/check_module_boundaries.py
./bin/NoMoreDayTests.exe --list-test-cases
```

For C++ changes, `./build.bat` plus focused tests is the minimum completion
evidence. A broad test failure is reportable only with its baseline, exact
command, owner, retest command, and an explicit waiver. It is never a pass.

### 2.4 Evidence Terminology

| Evidence class | Allowed conclusion | Not an allowed conclusion |
| --- | --- | --- |
| Unit or contract test | Parser, policy, state, or data-contract behavior | GPU production readiness |
| Hidden-context GL lifecycle test | Object ownership, release order, local GL diagnostics | Image quality, timing, leak pressure, hardware GO |
| Release LTO artifact | MSVC Release LTO build proof | Rendering hardware GO |
| Game-binary M0-C artifact | Production verdict on declared hardware | A blanket verdict for other hardware or revisions |

## 3. W0: Coordinator Preparation

**Owner:** coordinator. **Dependencies:** none. **No source changes.**

### Atomic Tasks

- [x] **W0.1 Record a package baseline.**
  - Capture `git status --short`, current branch/ref, relevant Track state,
    package-specific source files, and CTest registrations before delegation.
  - Record that `conductor/bug_registry.md` and `settings.json` may already be
    modified by another actor. Do not overwrite or revert either file.
  - Evidence: baseline note in the package handoff and command output summary.
  - Exit: the implementer can distinguish its edits from pre-existing work.

- [x] **W0.2 Reserve files and serialize conflicts.**
  - W1 reserves QualityTierManager files/tests and any narrowly merged BUG row.
  - W2 reserves checker, ledger, and Python checker tests.
  - W3 reserves the event contract, producer, consumer, and VFX tests.
  - W4 reserves root CMake/build or preset tooling and release evidence docs.
  - W5/W6 reserve their Track documents only after their predecessor gate.
  - Exit: no parallel agent edits the same build registration, Track document,
    or source file without an explicit coordinator decision.

- [x] **W0.3 Confirm task-specific acceptance before source edits.**
  - Translate each package section below into its own checklist in the task
    request. Do not add scope such as persistent settings migration, tag data
    changes, or visual feature work.
  - For W5/W6, require the implementer to read the current owning Track spec,
    plan, validation, and debt register immediately before editing.
  - Exit: each subagent receives one atomic package with no ambiguous `GO` claim.

### Handoff Template

Every implementer and reviewer returns this exact information:

```text
package:
baseline ref and pre-existing worktree changes:
files changed and ownership rationale:
contract added, removed, or preserved:
focused test command and result:
broader build/test command and result:
artifact or evidence path:
Track/document updates:
remaining risk, blocker, or explicit waiver:
```

## 4. W1: S1b Settings Persistence Data-Loss Repair

**Owner boundary:** `QualityTierManager` owns application of tier policy and
runtime state; each settings domain owns writes to its own JSON subtree.

**Expected files:**

```text
src/engine/render/core/QualityTierManager.hpp
src/engine/render/core/QualityTierManager.cpp
tests/unit/QualityTierManagerTest.cpp
tests/integration/S7PairedGiDeltaTest.cpp
conductor/bug_registry.md                  (narrow merge only)
```

The exact file list may shrink after source review. Do not introduce a broad
settings rewrite or edit local `settings.json`.

### Implementation Contract

```text
on Initialize:
    load tier and persisted owner-specific preferences
    derive base configuration from tier and persisted preferences
    derive effective configuration from base plus auto-degrade/runtime override
    update selection/auto-detect metadata only

on metadata persistence:
    parse existing JSON
    change only metadata-owned keys
    preserve every unrelated subtree and unknown key

on explicit V3 user save:
    write render.v3 keys only

on explicit GI user save:
    write render.gi.enabled only

on runtime GI override or auto-degrade:
    do not write user preference
```

### Atomic Tasks

- [x] **W1.1 Register the defect without overwriting concurrent registry work.**
  - Read the current `conductor/bug_registry.md` immediately before editing.
  - Add a narrow P1 entry describing automatic metadata persistence overwriting
    unrelated preferences, shortest reproduction, owner, and regression test.
  - If the concurrent change conflicts structurally, stop and ask the
    coordinator rather than replacing the file.
  - Exit: defect tracking points to the focused regression and no unrelated
    bug-record content changes.

- [x] **W1.2 Add preservation regressions before changing serialization.**
  - In `QualityTierManagerTest`, create a temporary settings document with a
    true GI preference and distinct sentinels in GPU text, GPU loot, fluid,
    adaptive, V3, and an unknown JSON subtree.
  - Initialize so metadata persistence executes; verify metadata changes but
    every unrelated sentinel is byte/value-equivalent under its owned subtree.
  - Reinitialize from the output; verify persisted GI preference still affects
    base/effective configuration under documented precedence.
  - Update the paired-GI test's historical assertion that true GI becomes false
    after reinitialization. Retain its non-production diagnostic classification.
  - Exit: the old data-loss behavior fails a focused regression before repair.

- [x] **W1.3 Separate metadata writes from rendering-preference writes.**
  - Narrow the metadata path to a JSON read-modify-write of only its selection
    and auto-detect fields.
  - Split or narrow the current broad V3 serializer/loader so it owns
    `render.v3` only. It must not write GI, GPU text, GPU loot, fluid, or
    adaptive values from `m_v3Config`.
  - If no explicit setter/save operation exists for a domain, do not invent a
    startup write just to preserve old default-synthesis behavior. Define the
    owner-specific save entry point before adding it.
  - Preserve existing valid flat/nested compatibility behavior; do not turn this
    defect repair into a full schema migration.
  - Exit: `PersistSelectionMetadata` cannot invoke a serializer that owns an
    unrelated preference domain.

- [x] **W1.4 Preserve runtime precedence and invalid-input behavior.**
  - Test persisted GI true/false independently from a transient runtime GI
    override and from auto-degrade.
  - Confirm clearing a runtime override restores the persisted/tier result and
    never changes serialized user preference.
  - Test missing or malformed optional fields; preserve safe defaults and all
    unrelated valid JSON content.
  - Exit: no effective runtime field becomes durable configuration.

- [x] **W1.5 Verify and review.**
  - Focused commands:

    ```powershell
    ./build.bat
    ./bin/NoMoreDayTests.exe --test-case="[Unit]*QualityTier*"
    ./bin/NoMoreDayTests.exe --test-case="[Integration]*S7*Gi*"
    ```

  - Broaden with `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`.
  - Reviewer verifies that no path serializes `m_config` as the general user
    settings source and that the bug registry merge is narrowly scoped.
  - Exit: all W1 tests pass; the evidence states this fixes user-preference
    persistence only, not GPU production readiness.

### Rollback Boundary

Revert only the W1 serializer/metadata change if it fails compatibility review.
Never restore behavior that writes unrelated default values during startup.

## 5. W2: Target-Aware Module Boundary Policy And P0 Cleanup

**Owner boundary:** direct source include direction, not CMake dependency
resolution and not transitive include analysis.

**Expected files:**

```text
scripts/check_module_boundaries.py
tests/python/ModuleBoundaryCheckerTest.py
docs/reports/modular-split-exe-lib-dll/ms-0/reverse-dependency-ledger.json
docs/reports/modular-split-exe-lib-dll/ms-8/evidence.md
```

If the evidence document should be immutable, add the migration evidence in an
adjacent current report rather than rewriting historical claims. The
implementer must make that decision with the coordinator before editing it.

### Target Schema Contract

```text
candidate policy:
    src/core   -> Core, forbid engine/, game/, app/
    src/engine -> Engine, forbid game/, app/
    src/game   -> Game, forbid app/
    src/pch.hpp -> Engine-owned PCH, forbid game/, app/

for each direct project include in quote or angle form:
    find owning candidate policy
    if include prefix is forbidden by that policy:
        emit observed edge with the exact applied policy
        require an exactly matching ledger entry
```

The checker remains direct-only. It must not add an ad hoc recursive include
walker, modify target dependencies, or infer policy from CMake at runtime.

### Atomic Tasks

- [x] **W2.1 Define and lock the replacement ledger schema.**
  - Bump schema version once and make candidate roots contain target, layer, and
    ordered forbidden project prefixes.
  - Define the PCH policy explicitly instead of inheriting an opaque global.
  - Define strict field equality for scope, observed edge, and ledger entries.
  - Remove compatibility parsing for the obsolete P0 fields. Historical reports
    remain the compatibility record.
  - Exit: checker and real ledger have one unambiguous schema version.

- [x] **W2.2 Add failing policy fixtures.**
  - Game self-include is valid and emits no violation.
  - Unledgered Game-to-App include fails in both quoted and angle forms.
  - Existing Engine-to-Game/App and Core-to-Engine/Game/App behavior remains
    rejected under their respective policies.
  - Missing, duplicate, unknown, or malformed policy metadata fails closed.
  - A legacy `p0_blocking` entry is rejected by strict schema validation.
  - Exit: tests demonstrate both the original blind spot and P0 dead-schema
    removal requirements.

- [x] **W2.3 Implement declarative per-root scanning.**
  - Replace global forbidden-prefix evaluation with the candidate's own policy.
  - Extend candidate lookup to `src/game` without causing Game self-includes to
    be flagged.
  - Recognize direct project header syntax in `"..."` and `<...>` forms while
    continuing to ignore external system headers that do not match project
    prefixes.
  - Store the concrete policy metadata in observations so ledger equality is
    deterministic.
  - Exit: a Game-to-App direct include cannot evade the checker.

- [x] **W2.4 Delete live P0 checker behavior.**
  - Remove `REQUIRED_P0_SOURCES`, P0 constants, P0 validation branches, entry
    field requirements, fixtures, and test names.
  - Do not leave inverse checks that only appear active because the required set
    is empty.
  - Exit: a source search finds no active checker policy coupled to the obsolete
    P0 disposition; legacy fields are invalid rather than silently ignored.

- [x] **W2.5 Migrate the real empty ledger and verify.**
  - Update the real ledger to the replacement schema and exact expected policy.
  - Verify it is empty because no current violation exists, not because Game was
    omitted from scan scope.
  - Commands:

    ```powershell
    python -m unittest tests/python/ModuleBoundaryCheckerTest.py -v
    python scripts/check_module_boundaries.py
    ./build.bat check
    ```

  - Reviewer verifies Core's stricter Engine prohibition, Game self-include,
    direct-only scope, and strict legacy-field rejection.
  - Exit: Python tests and real checker pass under the new policy.

### Rollback Boundary

The checker, test fixture, and ledger schema migrate together. Do not revert
only one, and do not reintroduce a global policy to make an incomplete migration
appear green.

## 6. W3: Scalar Skill VFX Element Contract

**Owner boundary:** Game translates gameplay tags to one VFX scalar; Engine
validates and consumes that scalar. No raw gameplay-tag layout crosses the
boundary.

**Expected files:**

```text
src/game/components/SkillVfxEvent.hpp
src/game/systems/skill/SkillSystem.cpp
src/engine/render/GPUSkillEffectSystem.hpp
src/engine/render/GPUSkillEffectSystem.cpp
tests/unit/SkillVfxEventContractTest.cpp
tests/unit/<existing skill VFX behavior test>.cpp
```

Locate the actual producer and consumer paths before editing; do not edit
generated `src/game/data/TagRegistry.hpp`, `assets/data/tags.json`, or
`scripts/gen_tags.py`.

### Contract Pseudocode

```text
Game EncodeElementTypeFromTags(tags):
    if tags include Void: return Void
    if tags include Lightning: return Lightning
    if tags include Cold: return Cold
    if tags include Fire: return Fire
    return Physical

Game EmitSkillVfxEvent(context):
    event.elementType = EncodeElementTypeFromTags(effective/transmuted tags)
    do not attach a raw Tag bit mask

Engine ConsumeSkillVfxEvent(event):
    element = validate event.elementType against documented scalar values
    if invalid: record diagnosable fallback and use Physical
    select recipe from element
```

`Shadow` and `Poison` retain the existing Physical fallback. Supporting either
with a new palette or recipe requires a separate approved gameplay/render
design.

### Atomic Tasks

- [x] **W3.1 Freeze behavior with scalar-focused tests.**
  - Test existing numeric enum values, normal Physical/Fire/Cold/Lightning/Void
    selection, and invalid scalar fallback.
  - Add tag translation cases for high-bit state tags, non-element tags,
    multiple elements, Shadow/Poison, and active transmuter override.
  - Assert the priority is Void, Lightning, Cold, Fire, then Physical.
  - Exit: current intended behavior is expressed without depending on tag bit
    positions.

- [x] **W3.2 Remove the obsolete cross-layer mask.**
  - Delete `effectiveTagMask`, raw `SkillVfxElementTagMask` constants, producer
    cast from gameplay `Tag`, and Engine legacy-mask fallback.
  - Keep scalar numeric values stable for existing serialized/recipe ABI unless
    a separate compatibility analysis proves no external contract exists.
  - Update all aggregate/event construction sites rather than adding a legacy
    field or a compatibility union.
  - Exit: no VFX event field encodes raw gameplay tag layout.

- [x] **W3.3 Validate at the Engine consumer boundary.**
  - Centralize range validation before recipe selection so events produced by a
    future non-SkillSystem caller cannot index an invalid recipe.
  - Make fallback observable in existing diagnostics conventions without adding
    test-only production paths.
  - Exit: invalid scalar input is safe and test-covered.

- [x] **W3.4 Verify and review.**
  - Commands:

    ```powershell
    ./build.bat
    ./bin/NoMoreDayTests.exe --test-case="[Unit]*SkillVfx*"
    ./bin/NoMoreDayTests.exe --test-case="[Unit]*Skill*"
    ctest --test-dir build -C RelWithDebInfo -L skill --output-on-failure
    ```

  - Reviewer checks that generated tag sources did not change, values and
    priority remain stable, and Engine did not acquire a Game header include.
  - Exit: focused behavior and skill label checks pass with scalar-only event
    contract.

### Rollback Boundary

Rollback the event contract and producer/consumer together. Never restore the
mask merely to support a test fixture; update that fixture to the scalar API.

## 7. W4: Release LTO Declaration, Isolation, And Proof

**Owner boundary:** CMake describes configuration policy; build/preset tooling
selects an isolated cache; retained evidence proves actual MSVC output.

**Expected files:**

```text
CMakeLists.txt
build.bat and/or CMakePresets.json
scripts/<release-lto verification helper, if required>
docs/reports/<revisioned release-lto evidence path or procedure>
```

Do not make unrelated optimization changes while establishing this contract.

### Configuration Contract

```text
ENABLE_LTO is declared with default OFF

if ENABLE_LTO:
    verify IPO support for the selected MSVC toolchain
    enable IPO on required first-party targets for Release only
    report/assert the expected Release property for each required target
else:
    leave Release IPO disabled

RelWithDebInfo uses a separate, non-LTO policy and cache
release-lto verification uses an isolated build directory or preset
```

The implementation must decide and document the exact required deliverable
targets before adding assertions. A global option alone is insufficient proof.

### Atomic Tasks

- [x] **W4.1 Establish a failing configuration/evidence checklist.**
  - Confirm current cache behavior for default RelWithDebInfo and `build.bat
    release` without deleting another agent's useful build artifacts.
  - Record that `ENABLE_LTO` is currently passed but not declared and that the
    existing helper deletes transient logs.
  - Select an isolated build directory or CMake preset name that cannot share a
    cache with ordinary `build/` developer work.
  - Exit: the proof path has an explicit cache boundary and artifact location.

- [x] **W4.2 Declare the CMake policy.**
  - Add explicit `option(ENABLE_LTO ... OFF)` at the root option boundary.
  - Apply interprocedural optimization to Release only, using target/property
    scope consistent with existing first-party deliverables.
  - Emit a clear configure-time report or assertion identifying the targets and
    Release IPO state. Do not treat `check_ipo_supported` alone as proof.
  - Exit: a fresh configure shows selected policy without inheriting a prior
    cache value.

- [x] **W4.3 Add retained MSVC proof collection.**
  - Make the isolated release invocation preserve its configure cache and
    compiler/link command or response-file evidence before any helper cleanup.
  - Store a revisioned evidence bundle containing configuration, generator,
    target list, `/GL` compile evidence, `/LTCG` link evidence, command status,
    and timestamps. Do not store credentials or large unfiltered logs.
  - Keep ordinary `build.bat` behavior intact unless its interface expressly
    promises the new proof mode; prefer a dedicated preset/helper to avoid
    surprising developers.
  - Exit: an auditor can inspect the LTO proof without reconstructing deleted
    console output.

- [x] **W4.4 Execute isolated Release verification on MSVC.**
  - Configure and build using the chosen isolated path with `ENABLE_LTO=ON`.
  - Verify the compiled command evidence contains `/GL` and final executable
    link evidence contains `/LTCG`.
  - Run:

    ```powershell
    ctest --test-dir <release-lto-build-dir> -C Release -L ci --output-on-failure
    <release-lto-bin-dir>/NoMoreDay.exe <documented noninteractive smoke option>
    ```

  - If the executable lacks a safe noninteractive option, define one as a
    separately reviewed product contract before claiming smoke coverage. Do not
    substitute a hidden GPU gate invocation.
  - Exit: retained evidence proves a successful MSVC Release LTO build, Release
    CI label, and representative smoke.

- [x] **W4.5 Prove cache isolation and review.**
  - Fresh-configure default RelWithDebInfo and confirm it remains non-LTO.
  - Reviewer checks default option semantics, target scope, evidence contents,
    no accidental RelWithDebInfo IPO, and no GPU-readiness language.
  - Exit: Release LTO claim is empirical, scoped, and reproducible.

### Rollback Boundary

Remove the isolated LTO option/preset/helper as one package if it causes a
Release regression. Do not silently leave a polluted shared cache or weaken
Release diagnostics to force `/LTCG` through.

## 8. W5: M0-B RG-3 Resource Lifecycle And Registry Closure

**Hard prerequisite:** the coordinator records that the owner of M0-A has
accepted the required HDR/GI resource/history correctness preconditions. If
that acceptance is absent, W5 remains blocked.

**Track requirement:** before any W5 source edit, update or approve the
appropriate contract changes in:

```text
conductor/tracks/gpu_rendergraph_resource_foundation_20260726/spec.md
conductor/tracks/gpu_rendergraph_resource_foundation_20260726/plan.md
conductor/tracks/gpu_rendergraph_resource_foundation_20260726/debt_register.md
```

**Expected implementation files, subject to current-source confirmation:**

```text
src/engine/render/resources/GPUResourceRegistry.hpp
src/engine/render/resources/GPUResourceRegistry.cpp
src/engine/render/resources/ComputeBuffer.hpp
src/engine/render/resources/ComputeBuffer.cpp
src/engine/render/resources/PersistentBuffer.hpp
src/engine/render/resources/PersistentBuffer.cpp
src/engine/render/GPUEntitySystem.hpp
src/engine/render/GPUEntitySystem.cpp
src/engine/render/RenderSystem.cpp
src/engine/render/GPUHardwareValidationGate.cpp
tests/unit/<registry tests>.cpp
tests/integration/<GL lifecycle or render advancement tests>.cpp
```

### W5 Ownership Map To Confirm Before Editing

| Object | Creator/releaser | Required shutdown point | Registry role |
| --- | --- | --- | --- |
| ResourceManager compute shaders | ResourceManager | `ResourceManager::unloadAll()` | Do not double-release from GPUEntitySystem |
| GPUEntitySystem raw render shader | GPUEntitySystem | `Game::cleanup()` before context loss | Track only if policy adds ShaderProgram |
| GPUEntitySystem VAO/VBO | GPUEntitySystem | `Game::cleanup()` before context loss | Observe as VertexArray/VertexBuffer |
| Compute buffers | Their RAII owner | Explicit pre-context shutdown | Observe as storage/buffer kind |
| Persistent buffers/mappings | Their RAII owner | Explicit pre-context shutdown | Unregister mapping before backing buffer |
| Framebuffer resources | Existing owner | Existing manager shutdown | Preserve current observation behavior |

### Contract Pseudocode

```text
register(handle, kind, metadata):
    require handle is valid
    if (handle, kind) already exists:
        reject with diagnostic OR apply documented idempotent update
        do not increment active/created/byte counters
    otherwise:
        insert record and update all counters once

owner release(resource):
    unregister observer record if registered
    issue the owner's GL release exactly once
    zero/reset local state

GPUEntitySystem Shutdown():
    release every locally owned raw, compute, and persistent object
    do not unload ResourceManager-owned compute shaders
    reset initialization/allocation state
    allow a second call without GL work

RenderSystem render():
    execute graph successfully
    advance registry exactly once
    continue normal post-frame work
```

### Atomic Tasks

- [x] **W5.1 Reconcile the M0-B spec and write the resource inventory.**
  - Verify actual current constructors, move operations, destructors, context
    lifetime, `Game::cleanup` order, ResourceManager ownership, existing
    registration sites, and render/gate advancement sites.
  - Specify duplicate-registration behavior as either rejection or idempotent
    update before code is changed. Specify byte accounting, size-update, missing
    record, unregister, and handle-reuse semantics.
  - Add `VertexArray` kind. Add `ShaderProgram` only if M0-B policy requires
    raw shader observation; correct raw shader ownership regardless.
  - Exit: M0-B Track documents contain a reviewed, testable contract and source
    inventory. W5 source work is blocked until this exit condition is met.

- [x] **W5.2 Add registry accounting tests before expanding coverage.**
  - Test first registration, duplicate rejection/idempotence, byte update,
    unregister, numeric handle reuse after unregister, snapshot totals, and
    pending-age behavior.
  - Include invalid/missing record paths according to existing diagnostic
    conventions. Tests must prove counters cannot inflate on a duplicate map key.
  - Exit: registry accounting behavior is unambiguous and regression-protected.

- [x] **W5.3 Implement accounting-safe registry semantics.**
  - Change registry insertion/update logic so map state and every aggregate
    counter remain mutually consistent.
  - Guard subtraction/size updates against underflow and missing records.
  - Preserve observer-only behavior; registry functions must not delete GL
    handles, own a context, or become a fallback cleanup path.
  - Exit: W5.2 passes with the implementation.

- [x] **W5.4 Bind RAII buffer owners to observer records.**
  - Add narrow optional registry metadata or owner-aware binding to the existing
    `ComputeBuffer` and `PersistentBuffer` owners rather than a global registry.
  - Register only after successful allocation; unregister before unmap/delete.
  - Fully reset persistent-buffer state after destroy/move so repeated shutdown
    and later destruction are context-safe no-ops.
  - Preserve existing owners and all move/copy constraints.
  - Exit: buffers and persistent mappings have balanced local observation and
    actual RAII deletion remains in the wrapper owner.

- [x] **W5.5 Make GPUEntitySystem explicit and idempotent.**
  - Release currently omitted physics output, block sum, raw shader, VAO, and
    VBO in explicit `Shutdown`.
  - Unregister each applicable observer record before releasing its handle.
  - Reset raw handles and all initialization/allocation state. Clean up every
    successfully acquired resource if initialization fails midway.
  - Do not add a destructor call that can issue GL after `CloseWindow`; retain
    explicit `Game::cleanup()` as the required pre-context shutdown path.
  - Do not manually unload the five ResourceManager-loaded compute shaders.
  - Exit: normal, partial-init, and duplicate shutdown paths have one owner and
    no post-context GL dependency.

- [x] **W5.6 Establish exact-one normal frame advancement.**
  - Place `GPUResourceRegistry::AdvanceFrame()` immediately after successful
    `RenderGraph::Execute()` in the normal `RenderSystem::render()` path.
  - Do not call it inside each render pass or before a failed/aborted execute.
  - Remove the hardware gate stress loop's manual advance and check every gate
    loop relies on normal render advancement consistently.
  - Document any nonstandard rendering path that legitimately owns a completed
    frame, or deliberately leave it non-advancing if it is not a completed
    render frame.
  - Exit: all normal frame consumers see one monotonically advancing epoch.

- [x] **W5.7 Add local GL lifecycle and frame tests.**
  - Use a real hidden GL context solely to record baseline registry snapshots,
    initialize resources, and release them before context teardown.
  - Verify expected kind/owner records, one real frame advance, active bytes and
    counts return to baseline after two shutdown calls, and diagnostics contain
    no unexpected GL error.
  - Cover a partial acquisition failure without a post-context destructor GL
    call. Keep image, ROI, timer, and hardware-quality assertions out of this
    test.
  - Add an advancement test for successful normal render, failed/aborted render
    if observable, gate stress, and gate toggle paths.
  - Exit: ownership and observer evidence is locally reliable; it is not a
    substitute for W6 hardware evidence.

- [x] **W5.8 Build, update M0-B evidence, and review.**
  - Run the focused registry/lifecycle tests, then:

    ```powershell
    ./build.bat
    ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
    ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
    ```

  - Update M0-B validation and debt register with command results, scope,
    remaining unregistered owners, and explicit non-production status.
  - Reviewer checks ownership of every resource, context order, observer-only
    registry, duplicate counters, exact-one advancement, and gate de-duplication.
  - Exit: M0-B accepts RG-3 according to its own Track criteria. Production
    remains `NO-GO` until W6 receives a valid hardware artifact.

### Rollback Boundary

Rollback registry contract, wrapper bindings, GPUEntitySystem release changes,
and frame advancement together if accounting or context safety fails. Do not
remove registry records while retaining a changed frame epoch semantic.

## 9. W6: M0-C Game-Binary Hardware Gate

**Hard prerequisites:** M0-A and W5/M0-B accepted under their owning Tracks;
M0-C specification and plan updated before source implementation.

**Track documents to update before and during implementation:**

```text
conductor/tracks/gpu_hardware_validation_gate_20260726/spec.md
conductor/tracks/gpu_hardware_validation_gate_20260726/plan.md
conductor/tracks/gpu_hardware_validation_gate_20260726/validation.md
conductor/tracks/gpu_hardware_validation_gate_20260726/release_posture.md
```

### Test Stratification Contract

| Existing test surface | Disposition | Permitted evidence |
| --- | --- | --- |
| GateReport JSON schema | Keep in normal test binary | Schema only |
| Python artifact parser/runner | Keep in normal test environment | Fail-closed invocation only |
| Missing driver/capability | Keep | `NOT_RUN` semantics |
| Fixture recipe/hash construction | Keep | Deterministic inputs |
| Registry/RenderGraph/GL lifecycle | Keep | Local ownership contracts |
| Hidden 1x1 `RunGate` matrix | Reclassify or remove from broad labels | Non-production diagnostic only |
| Hidden-context S7 paired GI capture | Reclassify | Non-production diagnostic only |
| Game binary on target hardware | Add as dedicated gate | M0-C verdict evidence |

The standalone matrix must not continue to run as a claimed hardware test under
`nmd.tests.ci.nonperf` or generic `nmd.tests.integration`. Contract cases may
remain under narrow normal CI registration.

### Intended Composition

```text
NoMoreDay.exe --gpu-gate [documented options]
    -> normal App/Game initialization
    -> real GL context, ResourceManager, RenderSystem, and Gameplay state
    -> Game/App concrete FixtureRenderDriver
    -> Engine GPUHardwareValidationGate interface
    -> real-resolution owned offscreen target and standard render path
    -> versioned artifact + exactly one status marker

Python runner
    -> launches NoMoreDay.exe, not a doctest filter
    -> validates exit/invocation, artifact schema, and verdict
    -> passes only if all are valid and artifact verdict is GO
```

The Engine must not include Game/App to achieve this. The concrete driver and
command composition belong to the Game/App composition root.

### Atomic Tasks

- [x] **W6.1 Update M0-C contract before source changes.**
  - Define exact command-line grammar, revision and output options, fixture
    selection, GPU/capability preflight, artifact path policy, timeout, and
    process-exit semantics in the M0-C Track spec/plan.
  - Define artifact schema version and required fields: revision, GPU/driver,
    fixture id/version/seed/hash, camera/ROI/extent, tier/GI/capability matrix,
    pass trace, valid timer frames, GI/SDF probes, diagnostics, resource
    snapshots, failures, and verdict.
  - Every absent mandatory field produces `NOT_RUN`; no defaulted or synthetic
    values are allowed.
  - Exit: M0-C documents approve a testable fail-closed protocol.

- [x] **W6.2 Separate standalone contracts from hardware execution.**
  - Split/retag `GPUHardwareValidationGateTest` so schema and missing-driver
    cases can run in normal CI without executing the invalid hidden-context
    hardware matrix.
  - Reclassify S7 paired capture output as non-production diagnostic evidence;
    update old tests that imply otherwise.
  - Amend `tests/CMakeLists.txt` to exclude reclassified matrix execution from
    broad `ci;nonperf` and generic integration paths. Avoid merely skipping it
    based on a test-only production flag.
  - Exit: ordinary CTest green no longer appears to be a hardware-gate result.

- [x] **W6.3 Implement Game/App gate composition.**
  - Add a noninteractive command entry after normal application initialization.
  - Create the concrete `FixtureRenderDriver` in Game/App only. It supplies real
    ResourceManager, registry, shared/render contexts, actual frame input,
    standard render invocation, and a real-resolution owned target.
  - Construct deterministic real Gameplay fixtures for cave bleed, dynamic
    combat emissive/occluder, and outdoor lights. Record fixture provenance and
    hashes in the artifact rather than embedding test-only fake scene paths in
    Engine.
  - Exit: the gate driver proves non-null real render services and follows the
    same initialization/lifetime boundaries as normal gameplay.

- [x] **W6.4 Implement artifact and status emission.**
  - Emit exactly one machine-readable `GPU_HARDWARE_GATE_RESULT` marker and one
    versioned JSON artifact per invocation.
  - Keep a completed invocation process exit separate from verdict: the process
    may exit successfully after producing a valid `NO_GO`/`NOT_RUN` artifact;
    the runner determines pass only from valid exact `GO`.
  - Validate artifact write failure, duplicate marker, corrupt JSON, null
    runtime hooks, unsupported capability, missing pass samples, and missing
    readback inputs as fail-closed `NOT_RUN`/failure conditions.
  - Exit: an external runner can make its decision without parsing human logs.

- [x] **W6.5 Convert the Python runner to the game binary.**
  - Replace doctest-filter invocation with the configured game executable and
    documented arguments.
  - Validate process launch, exactly one marker, artifact location, schema,
    revision/fixture provenance, and exact verdict. Preserve forwarded
    `NMD_GATE_*` options only when their semantics are defined by M0-C.
  - Add parser tests for malformed/missing/duplicate marker, absent artifact,
    schema mismatch, `NOT_RUN`, `NO_GO`, and `GO`.
  - Exit: runner fails closed for every incomplete evidence path.

- [x] **W6.6 Register dedicated hardware execution.**
  - Add a separate `gpu-hardware` CTest/CI job or equivalent external job that
    launches the Python runner, is serial/resource-locked, and requires a
    declared target GPU, driver, display/runtime prerequisites, and artifact
    upload location.
  - Keep it out of default `ci`, unit, and generic integration labels.
  - Make `NO_GO` and `NOT_RUN` fail the hardware job while leaving unrelated
    unit/contract CTests correctly classified.
  - Exit: CI topology makes the resource and verdict requirements visible.

- [~] **W6.7 Validate on target hardware and update the M0-C record.**
  - Run the real game-binary gate on declared hardware through the dedicated
    runner. Required matrix: High, Ultra, GI-off, resize, tier switch, and
    capability conditions across all three fixtures.
  - Require at least 120 distinct valid samples for every declared pass, zero
    high-severity GL diagnostics, 60 seconds pressure with no five-second net
    resource growth, 100 configuration toggles, and a reproducible artifact.
  - Archive/upload artifact and record hardware identity, command, revision,
    result, omissions, and failure reasons in M0-C validation/release posture.
  - Exit: only a complete exact `GO` artifact may request a production posture
    change. A valid `NO_GO` or `NOT_RUN` is successful evidence collection but
    leaves production `NO-GO`.

- [x] **W6.8 Review evidence integrity.**
  - Reviewer traces the driver from command entry through normal initialization
    to real render invocation; confirms Engine did not gain Game/App includes.
  - Reviewer independently validates artifact schema, marker cardinality,
    runner pass predicate, CTest/CI isolation, fixture provenance, sample count,
    diagnostics, resource-pressure evidence, and Track status wording.
  - Exit: a reviewer can reproduce the verdict decision from retained artifacts.

### Rollback Boundary

Rollback command composition, runner, labels, and artifact schema together if
the driver is not demonstrably real-runtime backed. Keep contract tests, but do
not restore the standalone harness as a production-gate substitute.

## 10. Package-Level Verification Matrix

| Package | Focused verification | Broader verification | Required recorded evidence |
| --- | --- | --- | --- |
| W1 | QualityTier and paired GI persistence cases | RelWithDebInfo unit CTest | Input/output settings preservation and runtime override result |
| W2 | Python fixture tests | Checker plus `build.bat check` | New schema ledger and no current violation |
| W3 | Event contract and skill tests | Skill CTest label | Scalar priority/high-bit/invalid input behavior |
| W4 | Isolated Release LTO configure/build | Release `ci` CTest and smoke | Cache plus `/GL` and `/LTCG` evidence bundle |
| W5 | Registry, lifecycle, advancement tests | Build, unit, integration, M0-B update | Balanced snapshots and no GL diagnostic evidence |
| W6 | Schema/runner negative tests | Dedicated target-hardware job | Versioned game-binary artifact and verdict |

For every documentation-only subtask, validate links, terminology, headings,
and navigation with `git diff --check`. Documentation validation does not
replace required C++ or hardware verification after source changes.

## 11. Final Coordinator Closeout

- [ ] **F1. Reconcile package reports.**
  - Verify all package handoffs include baseline, files, contracts, focused and
    broad test results, artifacts, Track updates, and unresolved risks.
  - Reject handoffs that report a bare "passed" without command/configuration or
    artifact location where required.

- [ ] **F2. Reconcile source, Track, and evidence status.**
  - Confirm W1-W4 completion claims match source and focused evidence.
  - Confirm W5 status is recorded by M0-B and preserves observer-only resource
    ownership.
  - Confirm W6 status is recorded by M0-C. Do not translate a test-only or
    incomplete result into production readiness.

- [ ] **F3. Run final repository checks appropriate to merged scope.**
  - At minimum after all non-hardware packages:

    ```powershell
    ./build.bat check
    ./build.bat
    ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
    python -m unittest tests/python/ModuleBoundaryCheckerTest.py -v
    python scripts/check_module_boundaries.py
    ```

  - Run isolated Release LTO verification only in its isolated cache.
  - Run M0-C hardware verification only through the declared dedicated job.
  - Record any unavailable hardware job as `NOT_RUN`, not a waived `GO`.

- [ ] **F4. Perform final review and user acceptance.**
  - Confirm no package touched user-owned `settings.json` and no concurrent
    modifications were overwritten.
  - Confirm target graph remains `App -> Game -> Engine -> Core -> Types`.
  - Confirm no M0-C `GO` claim lacks a complete target-hardware artifact.
  - Present final evidence and residual risks to the user. Do not create a
    commit unless separately requested.

## 12. Overall Exit Criteria

This plan is complete only when all statements are true:

1. W1 metadata persistence cannot overwrite unrelated user preferences and its
   precedence regression tests pass.
2. W2 scans Core, Engine, Game, and PCH with target-aware direct-include policy
   and has no live P0 schema or unreachable P0 branch.
3. W3 carries only a validated scalar VFX element across the Game-to-Engine
   boundary while preserving current element priority and ABI values.
4. W4 has retained MSVC Release `/GL` and `/LTCG` proof, Release CI evidence,
   smoke evidence, and isolated cache behavior.
5. W5 has M0-B acceptance for owner-balanced RG-3 resources, trustworthy
   observer accounting, and exactly-one normal frame advancement.
6. W6 distinguishes contract tests from hardware evidence and has a real
   game-binary gate. Production remains `NO-GO` unless its valid artifact is
   exactly `GO` and meets every M0-C condition.
7. All reported failures, waivers, unavailable hardware, and residual risks are
   explicit; no local settings residue or pre-existing unrelated edit is
   included in the repair set.
