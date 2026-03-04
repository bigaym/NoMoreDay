# Combat Core VNext Design

Date: 2026-03-03  
Status: Reviewed and execution-ready  
Related plan: `docs/plans/2026-03-03-combat-core-vnext-implementation-plan.md`

## 1. Purpose and scope

This design defines the runtime architecture for a high-performance combat core that can support POE/Last Epoch style build combinatorics without collapsing into per-feature branching.

Goals:

1. Replace fixed tag bottlenecks with data-driven runtime tag domain.
2. Compile condition logic into deterministic, branch-light runtime predicates.
3. Unify modifier sources into one execution graph with explicit ordering.
4. Make damage resolution deterministic, testable, and parity-comparable.
5. Migrate safely via dual-run parity gates before cutover.

Non-goals:

- Save compatibility and migration.
- Gameplay rebalance as a primary objective.
- UI/animation redesign.

## 2. Fixed decisions

1. Runtime architecture is layered and acyclic: `TagDomain -> ConditionIR -> ModifierGraph -> DamageKernel -> RuntimeFacade`.
2. Authoring data remains readable (JSON/schema), but runtime consumes compiled blobs.
3. All unknown tags and invalid condition/modifier records hard-fail at compile/check stage.
4. During migration, runtime supports `LegacyOnly`, `V2Only`, and `DualRunCompare` modes.
5. Every phase must follow TDD red -> green -> integration gate.
6. Performance contract: combat-heavy P95 must stay within baseline + 5%.

## 3. Architecture overview

### 3.1 Data flow

1. Canonical authoring records (`assets/data/combat_v2/*.json`) are validated.
2. Compiler scripts generate runtime blobs (tag table, condition programs, modifier graph packs).
3. Runtime loads blobs into immutable in-memory tables.
4. Per combat request, runtime facade prepares evaluation context and calls damage kernel.
5. Damage kernel executes stage pipeline and returns result + trace hash.
6. In dual-run mode, legacy and v2 outputs are compared and diagnostics emitted.

### 3.2 Layer invariants

- `TagDomain`: stable tag IDs for a given schema hash; O(1) tag presence checks.
- `ConditionIR`: pure evaluation, no side effects, no heap allocation during eval.
- `ModifierGraph`: deterministic total order of node application.
- `DamageKernel`: fixed stage order and stable numeric semantics.
- `RuntimeFacade`: only integration boundary for external systems.

## 4. Layer contracts and API sketch

### 4.1 TagDomain

`TagDomain` replaces fixed enum-only logic in v2 runtime path.

```cpp
using TagId = uint32_t;

struct TagDef {
    std::string canonicalName;
    TagId id;
};

class TagDomain {
public:
    [[nodiscard]] std::optional<TagId> TryResolve(std::string_view name) const;
    [[nodiscard]] std::string_view NameOf(TagId id) const;
    [[nodiscard]] bool Contains(TagBitset set, TagId tag) const;
    [[nodiscard]] bool ContainsAll(TagBitset set, std::span<const TagId> tags) const;
    [[nodiscard]] bool ContainsAny(TagBitset set, std::span<const TagId> tags) const;
    [[nodiscard]] uint64_t SchemaHash() const;
};
```

Migration bridge:

- `LegacyTag -> TagId` mapping table generated during compile step.
- Unknown legacy values are compile/check failures, not runtime silent fallback.

### 4.2 ConditionIR

Condition authoring expressions are lowered into compact IR programs.

```cpp
enum class ConditionOp : uint8_t {
    HasTagsAll,
    HasTagsAny,
    Not,
    And,
    Or,
    StatCompare,
};

struct ConditionNode {
    ConditionOp op;
    uint32_t a;
    uint32_t b;
};

struct ConditionProgram {
    std::vector<ConditionNode> nodes;
    uint32_t root;
    uint64_t canonicalHash;
};
```

Compile-time failures:

- Unknown references, invalid operator payload, empty conjunction/disjunction, unsupported nesting.

### 4.3 ModifierGraph

All modifier sources are normalized into one graph format.

```cpp
enum class ModifierStage : uint8_t {
    PreHit,
    Hit,
    PostHit,
    DotTick,
};

enum class ModifierOp : uint8_t {
    Flat,
    Increased,
    More,
    Convert,
    GainExtra,
    ClampMin,
    ClampMax,
};

struct ModifierNode {
    uint32_t nodeId;
    ModifierStage stage;
    ModifierOp op;
    float value;
    uint32_t conditionProgramId;
    uint16_t priority;
    uint32_t sourceId;
};
```

Ordering policy:

- Total order key is `(stage, priority, sourceId, nodeId)`.
- Graph compilation precomputes execution vectors per stage.

### 4.4 DamageKernel

DamageKernel is the only component that computes final numeric result.

```cpp
struct DamageInput {
    EntityId attacker;
    EntityId defender;
    SkillId skillId;
    float baseDamage;
    uint64_t eventSeed;
};

struct DamageResult {
    float finalDamage;
    bool critical;
    bool blocked;
    uint64_t traceHash;
};
```

Stage contract:

1. Build context and baseline damage instances.
2. Apply `Flat` contributions only.
3. Apply `Increased` aggregates.
4. Apply `More` multipliers.
5. Apply `Convert/GainExtra` finalization.
6. Resolve mitigation and post-hit effects.

### 4.5 RuntimeFacade

Runtime facade integrates with existing systems while migration is active.

```cpp
enum class CombatEvalMode : uint8_t {
    LegacyOnly,
    V2Only,
    DualRunCompare,
};

struct ParityDiff {
    float absDelta;
    float relDelta;
    uint32_t differingStageMask;
    uint64_t legacyTraceHash;
    uint64_t v2TraceHash;
};
```

`DualRunCompare` behavior:

- Executes both paths with same request snapshot.
- Emits diagnostic record if outside tolerance.
- Supports sampled diagnostics in non-test builds to control overhead.

## 5. Determinism and numeric policy

1. Stable operation ordering is mandatory and tested.
2. No unordered parallel reductions in runtime hot path.
3. RNG usage (if any) uses request-derived deterministic seed stream.
4. Trace hash is generated from ordered application events for replayability.
5. Tolerance policy for dual-run parity is explicit per scenario class.

## 6. Performance model

Hot path requirements:

- No per-request heap allocation in tag checks, condition eval, modifier traversal.
- Condition and modifier structures are contiguous arrays.
- Prefiltering occurs before heavy numeric operations.
- Data-oriented loops over stage buckets, not nested source loops.

Instrumentation requirements:

- Per-stage timing counters.
- Candidate count and applied node count metrics.
- Dual-run overhead metric when compare mode is enabled.

Performance baseline artifacts (source of truth):

- `docs/reports/combat-core-vnext/baseline/perf-baseline.json`
- `docs/reports/combat-core-vnext/baseline/hardware-profile.md`
- `docs/reports/combat-core-vnext/baseline/scenario-manifest.json`

Gate metric:

- Compare median P95 from 5 measured runs against baseline P95.
- Regression threshold is `+5%` max for combat-heavy scenarios.

## 7. Parity and diagnostics format

Diagnostic record format: `combat_diag_v1` JSON lines.

Required fields:

- `request_id`, `mode`, `skill_id`, `attacker`, `defender`
- `tag_schema_hash`, `condition_hash`, `modifier_graph_hash`
- `legacy_final`, `v2_final`, `abs_delta`, `rel_delta`
- `differing_stage_mask`, `legacy_trace_hash`, `v2_trace_hash`
- `applied_modifiers[]` (`source_id`, `node_id`, `stage`, `op`, `value`, `condition_result`)

Classification:

- `match`
- `tolerance_match`
- `mismatch`

Scenario classes and tolerance policy:

- `exact_match`: abs delta must be `0.0`.
- `hit_float`: abs delta `<= 1e-4` OR relative delta `<= 0.1%`.
- `dot_aggregate`: abs delta `<= 1e-3` OR relative delta `<= 0.5%`.
- `status_duration`: abs delta `<= 1e-4` seconds.
- Every dual-run fixture must declare one scenario class.

## 8. TDD and verification model (phase-aligned)

Each phase must satisfy this template:

1. Add at least one failing doctest case first.
2. Implement minimal code to pass targeted tests.
3. Run label-level integration gate.
4. Write phase evidence report under `docs/reports/combat-core-vnext/phase-<n>/`.

Phase mapping:

- Phase 0: baseline harness + perf baseline evidence.
- Phase 1: TagDomain tests (alias, unknown hard-fail, bitset semantics).
- Phase 2: ConditionIR tests (truth tables, compile errors, no-allocation eval).
- Phase 3: ModifierGraph tests (normalization, ordering, filter pruning).
- Phase 4: DamageKernel tests (order invariants, deterministic replay).
- Phase 5: Dual-run parity tests and diagnostics completeness.
- Phase 6: Cutover tests (v2-only routing + legacy path removal assertions).

Repo verification commands:

- `./build.bat check`
- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

## 9. Design risks and mitigations

1. Tag ID instability across builds.
   - Mitigation: canonical tag registry snapshot + schema hash checks.
2. Condition compiler drift causing silent semantic change.
   - Mitigation: canonical hash golden tests and fixture truth tables.
3. Modifier ordering ambiguity.
   - Mitigation: explicit total ordering key and dedicated tests.
4. Floating-point parity noise in dual-run.
   - Mitigation: fixed stage order + explicit tolerance policy.
5. Dual-run overhead impacting frame budget.
   - Mitigation: sampling and bounded diagnostics payload.
6. Migration gaps from legacy tags.
   - Mitigation: generated legacy mapping + unknown hard-fail in check pipeline.
7. Debuggability regression in complex build combos.
   - Mitigation: trace hash + full per-stage diagnostic records.
8. Authoring errors in complex conditions.
   - Mitigation: compile-time validator with actionable error messages.

## 10. Acceptance criteria

1. V2 core layers compile and pass phase-specific tests.
2. TagDomain covers existing legacy tags and rejects unmapped values.
3. ConditionIR and ModifierGraph outputs are deterministic for identical inputs.
4. DamageKernel replay produces stable `finalDamage` and `traceHash`.
5. Dual-run suite reaches agreed parity threshold and emits diagnosable mismatch records.
6. Performance suite remains within baseline + 5% budget.
7. Final cutover leaves no active legacy runtime branch in targeted combat core path.
