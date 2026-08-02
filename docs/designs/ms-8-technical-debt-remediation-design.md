# MS-8 Technical Debt Remediation Design

> **Status:** proposed, research complete; implementation has not started.
>
> **Purpose:** turn the MS-8 follow-up register into independently reviewable
> repair packages without weakening the active rendering production gate.
>
> **Primary evidence:**
> [`docs/reports/modular-split-exe-lib-dll/ms-8/evidence.md`](../reports/modular-split-exe-lib-dll/ms-8/evidence.md),
> [GPU production remediation follow-up](gpu-production-remediation-follow-up.md),
> [V5 master specification](../../conductor/specs/rendering_engine_v5_master_spec.md#12-2026-07-生产整改路线), and the three active M0 Tracks.

## 1. Decision Summary

MS-8 closed the static-library split while explicitly accepting seven follow-up
items. Six require code, build, or policy work. `settings.json` is a
user-owned working-tree residue and is not a product defect to repair.

The repairs are divided into five independently reviewable work packages and
one ordered rendering route:

| ID | Work package | Outcome | May proceed independently? |
| --- | --- | --- | --- |
| W1 | S1b settings persistence | Automatic metadata writes cannot destroy user render preferences. | Yes |
| W2 | Module-boundary policy and P0 cleanup | The checker detects Game-to-App reverse includes and has no dead P0 schema. | Yes |
| W3 | Skill VFX element contract | Engine VFX consumes an intentional scalar element, not a truncated gameplay tag mask. | Yes |
| W4 | Release LTO proof | Release LTO is declared, isolated, and empirically proven on MSVC. | Yes |
| W5 | M0-B RG-3 lifecycle closure | GPU ownership, registry accounting, and frame advancement are correct. | Only after M0-A accepts its prerequisite work. |
| W6 | M0-C game-binary hardware gate | Production evidence is collected by the initialized game binary, not a test harness. | Only after M0-A and W5/M0-B acceptance. |

The mandatory rendering order remains:

```text
M0-A: HDR/GI production correctness
  -> M0-B: RenderGraph/resource foundation (W5)
    -> M0-C: real Gameplay hardware gate (W6)
```

This document does not replace the `spec.md`, `plan.md`, `validation.md`, or
debt register of the active M0 Tracks. A package that changes an M0 contract
must first update its owning Track specification and then receive its own
implementation plan.

## 2. Goals, Non-Goals, And Fixed Constraints

### 2.1 Goals

1. Make settings persistence preserve the user's selected render preferences
   across initialization, auto-detection, runtime overrides, and restart.
2. Make the module-boundary checker encode the actual one-way target graph:
   `App -> Game -> Engine -> Core -> Types`.
3. Remove the accidental coupling between a 64-bit gameplay `Tag` layout and
   the Engine-side skill VFX event.
4. Replace the unproven Release/LTO claim with a reproducible, MSVC-specific
   build proof.
5. Make every tracked GPU resource have one explicit owner, while the registry
   remains an observer suitable for leak evidence.
6. Ensure each normal rendered frame advances GPU resource observation exactly
   once.
7. Separate unit and contract testing from the only form of GPU evidence that
   can support a Gameplay production `GO`.

### 2.2 Non-Goals

- Do not change the static target topology, source ownership, asset formats,
  save formats, or `SkillBehaviors` ownership established by the module split.
- Do not redesign GI, add a new visual feature, enable SPH, start Vulkan work,
  lower a hardware threshold, or claim Gameplay production readiness.
- Do not turn the observer registry into a GPU-resource owner or add a global
  cleanup singleton.
- Do not preserve the obsolete P0 checker behavior merely for historical
  compatibility. Historical evidence remains in MS-6/MS-8 reports.
- Do not write, revert, stage, or otherwise normalize the user-owned
  `settings.json` working-tree modification.
- Do not treat a passing doctest process, a WARP result, or a hidden 1x1
  context as a hardware-gate `GO`.

### 2.3 Governing Constraints

- The V5 master specification makes M0-C the sole source of Gameplay
  production `GO`; current production state is `NO-GO`.
- The existing split design continues to prohibit Engine-to-Game/App
  dependencies. New M0-C composition code belongs to Game/App, never Engine.
- Source resource ownership and registry observation are separate contracts:
  only the owner creates, destroys, or releases a GL handle.
- Missing GPU evidence is `NOT_RUN`, never a defaulted success.
- Every implementation package is atomic: implementation, focused tests,
  relevant track/document update, independent review, then a user-authorized
  commit if requested.

## 3. Verified Baseline And Problem Boundaries

### 3.1 S1b Is A User-Preference Data-Loss Defect

`QualityTierManager` already distinguishes:

```text
persisted user settings / tier policy -> m_baseConfig -> m_config
                                                ^             ^
                                                |             |
                                     m_giEnabledOverride  auto-degrade and
                                                         runtime gate override
```

`m_config` is effective runtime state. It can contain adaptive degradation and
a transient runtime GI override, so it must never become the source for a
general settings write-back.

The current defect is instead an ownership violation in the persistence path:
`PersistSelectionMetadata` calls a broadly named V3 serializer with
`m_v3Config`, even though that serializer also writes GI, GPU text, GPU loot,
fluid, and adaptive fields that `m_v3Config` neither owns nor fully loads. A
metadata refresh can therefore replace explicit user values with defaults.

### 3.2 The Current GPU Test Harness Is Not A Production Runtime

The standalone `NoMoreDayTests` GPU gate creates a hidden 1x1 GL context and
uses `GameplayRuntimeHarness`. That harness can deterministically construct
test data, but it does not initialize the normal Game/App lifetime,
`ResourceManager`, `RenderSystem`, or a real `RenderContext`; its render input
contains null resource/context pointers. It cannot prove that real Gameplay
passes, GI resources, valid timer samples, or image readbacks occurred.

This is a boundary classification, not a reason to delete all GPU tests. The
tests that exercise real local contracts remain valuable. The invalid part is
the claim that this isolated process can supply production hardware evidence.

### 3.3 RG-3 Has Both Ownership And Observation Gaps

`GPUEntitySystem` is Game-owned and is explicitly shut down by `Game::cleanup`
before `ResourceManager::unloadAll()` and `CloseWindow()`. This is the correct
place to release its local GPU handles. Its default destructor runs after the
`Game` destructor body, after the window is closed, and must not be relied on
to perform GL calls.

The system currently owns a raw rendering shader, VAO, VBO, two persistent
buffers, and five compute buffers, but its `Shutdown` only releases a subset.
The five compute shaders loaded through `ResourceManager` remain owned by that
manager and must not be manually unloaded by `GPUEntitySystem`.

`GPUResourceRegistry` is observer-only, has no normal rendering-frame
advance, and currently inflates counters if the same `(handle, kind)` is
registered twice. These defects make its resource-growth evidence unreliable
until corrected.

### 3.4 Remaining Independent Debts

- The checker currently scans only Engine/Core with one global forbidden-prefix
  set, so it cannot reject a future Game-to-App include without also rejecting
  valid Game self-includes.
- `SkillVfxEvent` carries both a valid scalar element and an obsolete
  `uint32_t` raw-tag projection. The latter truncates `Tag` and mirrors tag-bit
  layout unnecessarily.
- `ENABLE_LTO` is not a declared CMake option. The release batch path requests
  it, but no retained Release compile/link evidence proves `/GL` and `/LTCG`.
- The module checker keeps an empty `REQUIRED_P0_SOURCES` set and unreachable
  P0 validation branches. No active ledger entry needs this policy.

## 4. Cross-Cutting Ownership And Evidence Rules

### 4.1 Configuration Ownership

| Domain | Owner in memory | Persistent source | May metadata refresh write it? |
| --- | --- | --- | --- |
| Tier selection and auto-detect provenance | Selection/metadata state | `settings.json` selection metadata | Yes, only its own keys |
| V3 options | `m_v3Config` | `render.v3` | No |
| GI user preference | `m_giEnabledOverride` | `render.gi.enabled` | No |
| GPU text, GPU loot, fluid preferences | Their independent optional setting state | Their owned `render.*` domains | No |
| Adaptive preference | `m_adaptiveQualitySettings` | Adaptive owned domain | No unless a user-owned adaptive save action caused the write |
| Auto-degrade and runtime GI override | Effective runtime state | None | Never |

All settings writes must be read-modify-write operations that preserve unknown
keys and domains owned by other systems. An automatic startup action has no
authority to synthesize defaults for unrelated user settings.

### 4.2 GPU Resource Ownership

For every GL object, record these facts before implementation:

| Fact | Required meaning |
| --- | --- |
| Creator/owner | The only component permitted to release the handle. |
| Context requirement | The explicit shutdown point before context teardown. |
| Registry record | Observer metadata: kind, handle, byte estimate, owner label, and lifetime epoch. |
| Release order | Registry unregistration occurs before actual GL deletion. |
| Failure path | Partially created resources are released by the same owner without double release. |

Registration is not ownership. Registry reset, snapshot, or destruction must
never call a GL release function.

### 4.3 Evidence Terminology

| Term | Meaning |
| --- | --- |
| Contract test | Tests a parser, schema, state transition, ownership invariant, or deterministic fixture construction. |
| GL lifecycle test | Uses a local hidden context only to verify creation/release and GL diagnostics. It is not a visual or hardware-performance test. |
| Game-binary gate | Starts normal Game/App initialization and drives real Gameplay rendering. It can create production evidence. |
| `NOT_RUN` | Required context, capability, input, fixture, or artifact field was absent. |
| `NO_GO` | Gate executed with enough evidence to identify an unmet required condition. |
| `GO` | Every mandatory M0-C condition passed on the declared target hardware and produced a valid artifact. |

Only the final row is production evidence. A process exit of zero only proves
that an invocation completed; the runner determines gate success from the
versioned artifact verdict.

## 5. W1: Settings Persistence Repair

### 5.1 Design

1. Re-scope `PersistSelectionMetadata` to selection and auto-detection
   metadata only. It may create or update its own metadata subtree, but it may
   not call a broad rendering-settings serializer.
2. Narrow the V3 serializer and loader to `render.v3` fields only. It must not
   serialize GI, GPU text, GPU loot, fluid, or adaptive fields merely because
   they exist in `RenderConfig`.
3. Persist a render preference only through an explicit operation that owns the
   preference. A future GI user-toggle operation writes `render.gi.enabled`;
   it never copies auto-degraded or runtime override state.
4. Keep the documented precedence intact:

   ```text
   transient runtime GI override
     > persisted GI user preference
       > quality-tier policy and base configuration
         > auto-degrade derives effective configuration only
   ```

5. Preserve the established flat/nested settings compatibility where it already
   exists. Do not make a broad file-format migration part of this defect fix.

### 5.2 Required Regression Coverage

- Start with `render.gi.enabled=true` and sentinel GPU text/GPU loot/fluid
  values; initialize and verify metadata is updated without changing any
  sentinel subtree.
- Reinitialize from the resulting file and prove the GI preference still
  changes effective configuration as expected.
- Apply and clear a transient GI runtime override; prove the serialized user
  preference did not change.
- Verify V3-specific persistence updates V3 only.
- Verify missing or invalid optional settings retain existing safe defaults and
  do not delete unrelated JSON content.

### 5.3 Implementation Boundaries

- Before implementation, add a P1 bug record following the debugging workflow.
  `conductor/bug_registry.md` is currently modified by another actor; merge a
  new entry narrowly and never replace their work.
- This package modifies configuration behavior, not GPU production status. The
  paired-GI standalone test must change its old expectation that persistence
  flips a true GI preference to false.

## 6. W2: Boundary Policy And P0 Cleanup

### 6.1 Target-Aware Policy

Replace the single global forbidden-prefix policy with a declarative policy per
candidate root:

| Candidate root | Candidate layer | Forbidden project include prefixes |
| --- | --- | --- |
| `src/core` | Core | `engine/`, `game/`, `app/` |
| `src/engine` | Engine | `game/`, `app/` |
| `src/game` | Game | `app/` |
| `src/pch.hpp` | Engine-owned PCH | `game/`, `app/` |

The parser checks direct project includes in both quote and angle forms. It
does not become a transitive-include analyzer; CMake target compilation and
the existing source-boundary policy remain responsible for their respective
levels of enforcement.

### 6.2 Ledger Schema Migration

Use one deliberate breaking schema revision, rather than retaining two
partially compatible modes. The new scope records each candidate root's
target/layer and forbidden prefixes. Observed edges record the policy that
caused observation, preserving strict checker-to-ledger equality.

Remove all of the following from runtime checker schema and logic:

- `REQUIRED_P0_SOURCES`
- P0 blocker constants and validations
- entry field `p0_blocking`
- P0-only fixture and test cases

The MS-6/MS-8 reports retain the historical P0 explanation. The generic
boundary ledger must only represent live source-direction evidence.

### 6.3 Required Regression Coverage

- Game self-include is not observed as a violation.
- A direct Game-to-App quote or angle include fails when unledgered.
- A correctly ledgered Game-to-App fixture is governed by Game policy metadata.
- Current Engine/Core violations retain their rejection behavior.
- Missing, duplicate, or malformed root policy metadata fails closed.
- A legacy `p0_blocking` entry field is rejected by the strict new schema.
- The empty real ledger remains valid and the normal checker reports no edge.

## 7. W3: Scalar Skill VFX Element Contract

### 7.1 Design

`SkillVfxEvent::elementType` is the only element payload consumed across the
Game-to-Engine VFX boundary. It already represents the present renderer model:
one selected element, not an arbitrary set of gameplay tags.

The repair removes the raw `effectiveTagMask`, its bit-layout constants, the
lossy cast from `Tag`, and Engine-side legacy fallback resolution. Game owns
one explicit translation from skill tags to the scalar element; Engine treats
the result as a validated VFX enum.

The existing semantic priority is retained unless a later gameplay design
changes it:

```text
Void > Lightning > Cold > Fire > Physical
```

`Shadow` and `Poison` intentionally fall back to `Physical` because the
current VFX recipe set has no defined palette/recipe for them. Adding a visual
representation for either is a separate gameplay/render design, not a mask
compatibility workaround.

### 7.2 Contract And Tests

- Preserve the existing scalar numeric ABI values for current recipes.
- Validate an out-of-range scalar at the Engine boundary and fall back safely
  to Physical with diagnosable behavior.
- Cover non-element tags, tags above bit 31, multiple elements, transmuter
  override, and Shadow/Poison fallback.
- Update `SkillVfxEventContractTest` and actual recipe-selector tests.
- Do not edit generated `TagRegistry.hpp`; no tag data or generator change is
  required.

## 8. W4: Release LTO Contract And Proof

### 8.1 Configuration Contract

1. Declare `ENABLE_LTO` as an explicit CMake option with default `OFF`.
2. Make its intended scope explicit: the `release` command promises Release
   LTO, so only Release receives the IPO property. RelWithDebInfo must not
   silently retain LTO because of a previous cache configuration.
3. Use an isolated Release-LTO configure directory or an equivalently isolated
   preset so default developer builds cannot inherit the LTO cache value.
4. Add a configuration-level assertion/report for each first-party deliverable
   target whose Release IPO setting is required.

### 8.2 Proof Contract

A completed W4 package requires retained evidence from an actual MSVC build,
not only `check_ipo_supported`:

- The configure cache records `ENABLE_LTO=ON` for the isolated Release build.
- The compiler command or response file contains `/GL` for relevant first-party
  compilation.
- The final executable link command or response file contains `/LTCG`.
- Release CTest runs its designated CI label with `--output-on-failure`.
- A representative Release executable smoke starts and exits through its
  normal non-interactive path.

The build helper may delete transient console logs, so the verification command
must copy the relevant cache/command evidence into a revisioned artifact path
before cleanup. LTO proof does not imply a GPU hardware `GO`.

## 9. W5: M0-B RG-3 Lifecycle And Registry Closure

### 9.1 Prerequisite And Scope

W5 starts only after the owning M0-A work accepts the required HDR/GI resource
and history correctness preconditions. It is recorded in the M0-B debt
register as the closure of RG-3; it is not a parallel replacement for M0-A.

### 9.2 Registry Accounting Contract

1. Add resource kinds needed by actual owners, including `VertexArray`; add
   `ShaderProgram` if raw non-ResourceManager shader ownership is included in
   the coverage policy.
2. `RegisterResource(handle, kind, ...)` creates a new observation record only
   when the key is absent. A duplicate is a contract failure or a clearly
   defined idempotent operation with no counter change; it must never silently
   inflate bytes, active count, or created count.
3. Size mutations occur through one accounting-safe update path. Underflow,
   missing records, and invalid byte changes fail closed in validation builds.
4. A resource unregisters before its actual GL deletion. Reuse of a numeric
   handle is valid only after prior unregistration.
5. Persistent mappings have an explicit observer record and are removed before
   their backing buffer record. Query-ring objects follow the same rule.

### 9.3 `GPUEntitySystem` Lifecycle Contract

`GPUEntitySystem::Shutdown` becomes the explicit, idempotent pre-context-loss
cleanup operation. It must:

1. Release every locally owned persistent and compute buffer, including physics
   output and block-sum buffers.
2. Unregister and release the locally owned raw render shader, VAO, and VBO
   exactly once.
3. Leave the five `ResourceManager::loadComputeShader` handles for
   `ResourceManager::unloadAll`, because that manager owns them.
4. Zero/reset raw handles, allocation state, and initialization state so a
   second call and later member destruction perform no GL work.
5. Clean up every successfully acquired object if initialization fails midway.

The destructor must not introduce a fallback GL shutdown after `CloseWindow()`.
Debug assertions may enforce that explicit shutdown left the object empty, but
the normal destructor must be context-safe.

### 9.4 Frame Advancement Contract

`GPUResourceRegistry::AdvanceFrame()` occurs exactly once after a successful
normal `RenderGraph::Execute()` in `RenderSystem::render`, before downstream
resource snapshot consumers inspect the frame. It is not called per pass.

The hardware-gate stress loop must remove its manual advance after this change;
otherwise it would double-age pending records. Any nonstandard render path must
explicitly document whether it owns a completed rendered frame and must not
silently advance on an aborted render.

### 9.5 Required Tests

- Registry duplicate registration, idempotent/rejected semantics, byte updates,
  unregister, handle reuse, snapshot counts, and pending-age behavior.
- Hidden-context GL lifecycle test: baseline registry snapshot, initialize the
  system, verify expected owner/kind records, perform one real frame advance,
  shut down twice, and verify active counts/bytes return to baseline with no GL
  diagnostics.
- Partial initialization failure cleanup and no post-context GL release.
- Normal render path proves one advance per successful frame; gate stress and
  toggle loops do not double-advance or omit advancement.

These tests prove ownership and observation integrity. They do not replace the
M0-C hardware artifact.

## 10. W6: M0-C Game-Binary Hardware Gate

### 10.1 Test Stratification Decision

| Test class | Keep? | Permitted claim |
| --- | --- | --- |
| GateReport JSON schema and Python parser | Yes | Artifact and fail-closed contract |
| Missing-driver and unavailable-capability paths | Yes | `NOT_RUN` behavior |
| Deterministic fixture recipe/hash construction | Yes | Fixture input determinism |
| RenderGraph, registry, and GL lifecycle tests | Yes | Local contract/resource correctness |
| Hidden-context standalone gate matrix | Reclassify | No production or visual claim |
| `S7PairedGiDeltaTest` hidden-context capture | Reclassify | Non-production diagnostic only |
| Game-binary gate on declared hardware | Required | M0-C production evidence only |

The standalone `RunGate` matrix must be removed from broad
`nmd.tests.ci.nonperf` and generic integration execution as a purported
hardware test. Contract-level test cases can remain in normal CI under narrow
labels. A doctest success is not a `GO` verdict.

### 10.2 Production Invocation Contract

Introduce a non-interactive Game/App mode conceptually equivalent to:

```text
NoMoreDay.exe --gpu-gate <revision and fixture options>
```

The exact option grammar belongs to the implementation plan. Its required
behavior is:

1. Follow normal application initialization until the real GL context,
   `ResourceManager`, `RenderSystem`, render hooks, and Gameplay state exist.
2. Construct deterministic fixture scenes through a Game/App-owned concrete
   driver. The Engine-facing `FixtureRenderDriver` interface remains
   dependency-neutral; its concrete implementation must not live in `tests/`
   or cause Engine to include Game/App.
3. Use the standard render path and an owned, real-resolution offscreen target.
   The driver supplies actual registry, shared/render context, resources,
   fixture provenance, scene input hash, and frame input.
4. Execute the required three fixtures: cave color bleed, dynamic combat
   emissive/occluder, and outdoor light pressure.
5. Emit exactly one machine-readable status marker and one versioned JSON
   artifact. The artifact records revision, target GPU/driver, fixture version
   and seed, camera/ROI/extent, tier/GI mode, pass trace, valid sample frames,
   probes, diagnostics, resource snapshots, failure reasons, and verdict.
6. Mark missing context, null runtime hook, incomplete data, invalid artifact,
   or unsupported capability as `NOT_RUN`; no synthetic fallback may produce a
   passing value.

The process may return zero for a completed gate invocation. The Python runner
passes only when process result, artifact schema, and artifact verdict are all
valid and the verdict is exactly `GO`.

### 10.3 CI And Hardware Execution Contract

- The Python runner launches the game binary, not a doctest filter.
- Register a separate `gpu-hardware` CTest/CI label. It is opt-in, serial or
  resource-locked, and runs only on workers with declared target GPU, driver,
  display/runtime prerequisites, and artifact upload.
- `NO_GO` and `NOT_RUN` fail this hardware job. They do not make unrelated
  unit/contract CTests fail or get relabeled as a green production result.
- Archive artifacts in a revisioned path and upload them from CI. An ignored
  local artifact directory is not, by itself, audit evidence.
- Hardware evidence must include High, Ultra, GI-off, resize, tier switch, and
  capability matrix cases; at least 120 distinct valid samples for every
  declared pass; zero high-severity GL diagnostics; 60-second pressure with no
  five-second net resource growth; and 100 configuration toggles.

## 11. Implementation Order And Subagent Work Packages

The user intends to use subagents. Each package below should be handed to one
implementer and then an independent reviewer. Do not run two packages that
touch the same Track document, test registration, or build script concurrently.

### 11.1 W0: Shared Preparation

**Owner:** coordinator only.

1. Record the actual source baseline, working-tree status, Track state, and
   test labels before each package.
2. Confirm no agent overwrites `settings.json` or the concurrently modified
   bug registry.
3. Create the package-specific plan from this design and its owning Track
   specification. Include exact files, pseudocode, test commands, exit
   criteria, and rollback.
4. Require agents to report observed evidence, not inferred completion.

### 11.2 W1 Through W4: Independent Packages

| Package | Suggested implementer scope | Reviewer focus | Dependencies |
| --- | --- | --- | --- |
| W1 | QualityTierManager persistence and focused tests; narrow BUG record merge | Preference ownership, non-destructive JSON, transient override safety | W0 |
| W2 | Checker, ledger, Python fixtures/tests, evidence schema update | Directional policy, strict schema, no accidental Game self-include rejection | W0 |
| W3 | Skill VFX event, Game producer, Engine consumer, event tests | ABI values, scalar validation, no generated tag edits | W0 |
| W4 | Root CMake, build/preset verification tooling, Release-LTO evidence tests/docs | MSVC `/GL` and `/LTCG` evidence, cache isolation, no RelWithDebInfo surprise | W0 |

W1-W4 may be developed in parallel only after each agent reserves its touched
files. W4's root CMake/build edits must be serialized with any other build
system work.

### 11.3 M0-A Prerequisite Review

Before W5 starts, the coordinator verifies M0-A's owning Track accepts the
needed HDR/GI input/history/resource preconditions. If it remains incomplete,
W5 is blocked rather than implemented around it. This preserves the V5 master
specification's ordering.

### 11.4 W5: RG-3 Under M0-B

**Implementer scope:** registry contract, resource-wrapper observation,
`GPUEntitySystem` explicit lifecycle, single frame advancement, focused GL
lifecycle and registry tests, and corresponding M0-B documentation.

**Reviewer focus:** resource-manager shader ownership; post-context safety;
duplicate-accounting correctness; exactly-one advancement; no registry-owned
release; partial initialization; consistency with M0-B debt register.

### 11.5 W6: Game-Binary Gate Under M0-C

**Implementer scope:** Game/App command mode and composition driver, runner
invocation, test labels/CI resource isolation, artifact provenance, and M0-C
documents. It must not alter Engine-to-Game dependency direction.

**Reviewer focus:** proof that the binary follows normal initialization;
actual resource/context supply; fixture provenance; fail-closed artifact and
runner behavior; removal of standalone production claims; separation between
process completion and gate `GO`.

### 11.6 Required Handoff Template

Each subagent report must include:

```text
package:
source baseline and pre-existing worktree changes:
files changed and ownership rationale:
contract changed:
focused tests and exact result:
broader build/test result and known unrelated failures:
artifact/evidence path, if applicable:
Track/document state updated:
remaining risk or blocker:
```

No report may state `GO` unless it includes the W6 artifact path and all M0-C
criteria. No agent commits unless separately authorized by the user.

## 12. Acceptance Matrix

| Work package | Required automated evidence | Additional mandatory evidence | Completion condition |
| --- | --- | --- | --- |
| W1 | Focused QualityTierManager unit tests | Reinitialize preserved settings file and runtime-override non-persistence | No automatic metadata action changes an unrelated preference. |
| W2 | `python scripts/check_module_boundaries.py`; focused Python tests | Empty real ledger validates under new schema | Game-to-App regression is detectable; dead P0 policy is gone. |
| W3 | VFX event contract and behavior tests | High-bit and multi-element cases | No Event field represents raw gameplay tag layout. |
| W4 | Release CTest label and smoke | Retained `/GL` and `/LTCG` command evidence | Release LTO succeeds on MSVC in isolated configuration. |
| W5 | Registry and GL lifecycle tests; normal render-frame advancement test | Build plus M0-B evidence update | All owned resources balance and registry statistics are trustworthy. |
| W6 | Runner/schema negative tests | Target-hardware game-binary artifact satisfying every M0-C criterion | Only then may a production `GO` be considered. |

For every C++ package, run the repository build workflow and the narrowest
relevant tests first. A failing unrelated test may be recorded only with its
identifier, baseline, owner, retest command, and an explicit valid waiver; it
must not be hidden by a broad success statement.

## 13. Risks, Rollback, And Open Decisions

| Risk | Mitigation and rollback |
| --- | --- |
| A settings fix persists effective runtime state | Test runtime override and auto-degrade paths; roll back only the package if user-preference contract fails. |
| Checker migration hides a real direction violation | Strict schema equality, positive Game-to-App fixtures, and direct include scanning in both syntaxes. |
| Scalar VFX change alters current recipe behavior | Preserve enum numeric values and priority; compare existing Fire/Cold/Void selectors before accepting. |
| LTO causes a Release-only link or runtime regression | Isolated build directory, retained command evidence, Release tests/smoke, and no cache leakage to default builds. |
| Lifecycle changes issue GL calls after context destruction | Explicit pre-context `Game::cleanup` shutdown, idempotence tests, and no destructor fallback GL work. |
| Registry expansion creates inaccurate leak numbers | Define duplicate/resize/unregister accounting first, then add owners incrementally with snapshot tests. |
| Game-binary gate adds an Engine-to-Game dependency | Keep the interface Engine-owned and construct its concrete driver solely in Game/App composition. |
| Hardware runner is unavailable in ordinary CI | Make it a dedicated capability-labelled job; emit `NOT_RUN`, never a fake green result. |

The only open product decision is whether Shadow and Poison should receive
dedicated VFX recipes. This design intentionally preserves the current
Physical fallback until such a gameplay/visual design is approved. The
implementation plan must also choose the exact command-line grammar and CI
worker provisioning for `--gpu-gate`; neither changes the ownership or
fail-closed contracts above.

## 14. Definition Of Overall Completion

The MS-8 follow-up register is closed only when all statements below are true:

1. W1-W4 meet their package acceptance matrices and their evidence is recorded.
2. The boundary checker policy matches the four-layer target graph and has no
   dead P0 runtime schema.
3. The VFX event contains only its documented scalar cross-layer element
   contract.
4. Release LTO is build-verified, not merely configured.
5. M0-A, M0-B/W5, and M0-C/W6 records are consistent with source and evidence.
6. M0-C has either a valid target-hardware `GO` artifact or an honestly
   recorded `NO_GO`/`NOT_RUN`; only the former changes production readiness.
7. `settings.json` remains outside the repair change set unless its owner later
   explicitly requests a separate local-settings policy.

Until item 6 is a valid `GO`, Gameplay production remains `NO-GO` regardless of
all unit, integration, build, or standalone gate test results.
