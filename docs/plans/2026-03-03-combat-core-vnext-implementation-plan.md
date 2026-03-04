# Combat Core VNext Implementation Plan

> **Execution directive:** REQUIRED workflow is `executing-plans` + `subagent-driven-development`.
> Every package is implemented by a subagent, then reviewed by parent agent before merge.

**Goal:** Build a high-performance, data-driven combat core skeleton that can scale to POE/Last Epoch style build diversity while keeping deterministic behavior and strict runtime budgets.

**Architecture:** Introduce a compiled combat runtime path (`v2`) with four stable layers: Tag Domain, Condition IR, Modifier Graph, and Damage Kernel. Authoring data remains human-readable JSON/schema, but build-time compilation emits runtime blobs with precomputed masks, IDs, and executable filter structures so hot-path evaluation stays branch-light and allocation-free.

**Tech Stack:** C++20, CMake/MSVC, doctest/CTest, existing repo scripts (`build.bat`, validation scripts, schema/check generators), JSON canonical -> runtime transform pipeline.

---

## Scope and fixed constraints

- Keep old combat path alive during migration (`legacy + v2 dual-run`) until parity gates pass.
- TDD is mandatory for every phase: failing test first, minimal code, pass, then integration gate.
- All phase exits require both targeted test evidence and at least one CTest label gate.
- Runtime core remains deterministic (same input seed/state -> same outputs).
- No save compatibility work in this initiative.

## Performance contract (defined before implementation)

- Primary budget target: no more than `+5%` regression vs baseline P95 for combat-heavy scenarios.
- Micro budget targets (phase-level):
  - Tag checks and filter matches: O(1), no heap allocation in hot path.
  - Per-hit modifier scan: bounded by prefiltered candidate lists, not global linear scan.
  - Damage kernel stage execution: fixed order, fixed data layout, cache-friendly arrays.
- Measurement protocol:
  - Fixed scenario seed + fixed hardware profile + fixed quality tier.
  - 1 warmup + 5 measured runs, compare median P95.
- Baseline artifacts (source of truth):
  - `docs/reports/combat-core-vnext/baseline/perf-baseline.json`
  - `docs/reports/combat-core-vnext/baseline/hardware-profile.md`
  - `docs/reports/combat-core-vnext/baseline/scenario-manifest.json`

## Parity tolerance policy (source of truth)

- `exact_match` class: abs delta must be `0.0`.
- `hit_float` class: abs delta `<= 1e-4` OR relative delta `<= 0.1%`.
- `dot_aggregate` class: abs delta `<= 1e-3` OR relative delta `<= 0.5%`.
- `status_duration` class: abs delta `<= 1e-4` seconds.
- Every dual-run fixture must declare one scenario class in metadata.

## Subagent execution model (mandatory)

- Execute each package with a subagent, then run parent review before merge.
- Package report must include: `files changed`, `tests added`, `commands run`, `residual risks`.
- Parent review loop is required at each phase checkpoint:
  1. subagent implements package under TDD gate.
  2. parent reviews diff and verification evidence.
  3. if issues exist, update docs/code and rerun package verification.
  4. only then mark phase task complete.

## Test binary freshness rule (mandatory)

- Every red/green test step must rebuild test binaries before execution.
- RelWithDebInfo/default path: `./build.bat notest`
- Release perf path: `./build.bat release`
- Do not treat missing/stale binaries as valid red-state evidence.

## Phase plan (TDD-first, each phase independently shippable)

### Phase 0: Baseline, harness, and safety rails

**Objective:** Lock behavior and perf baselines before adding `v2` runtime.

**Files:**
- Create: `tests/perf/CombatCorePerfBaselineTests.cpp`
- Create: `tests/unit/CombatCoreParityHarnessTests.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `docs/reports/combat-core-vnext/phase-0/baseline.md`
- Create: `docs/reports/combat-core-vnext/phase-0/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-0/results.md`
- Create: `docs/reports/combat-core-vnext/phase-0/residual-risk.md`
- Create: `docs/reports/combat-core-vnext/baseline/perf-baseline.json`
- Create: `docs/reports/combat-core-vnext/baseline/hardware-profile.md`
- Create: `docs/reports/combat-core-vnext/baseline/scenario-manifest.json`

**Step 1: Write failing tests**
- Add parity harness test that compares a small deterministic combat fixture against expected legacy outputs.
- Add perf baseline test shell that fails when baseline artifact is missing.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatCoreParityHarness*"`
- Expected: FAIL because harness/perf artifacts are not wired.

**Step 3: Write minimal implementation**
- Implement baseline harness fixture loader and baseline artifact writer/reader.
- Pin and write baseline artifact schema (`perf-baseline.json`) with scenario IDs and P95 values.
- Record hardware profile (`hardware-profile.md`) and bind benchmark runs to profile hash.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatCoreParityHarness*"`
- Expected: PASS with deterministic fixture checks.

**Step 5: Integration gate**
- Run: `./build.bat check`
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add tests/CMakeLists.txt tests/unit/CombatCoreParityHarnessTests.cpp tests/perf/CombatCorePerfBaselineTests.cpp docs/reports/combat-core-vnext/phase-0/baseline.md docs/reports/combat-core-vnext/phase-0/commands.txt docs/reports/combat-core-vnext/phase-0/results.md docs/reports/combat-core-vnext/phase-0/residual-risk.md docs/reports/combat-core-vnext/baseline/perf-baseline.json docs/reports/combat-core-vnext/baseline/hardware-profile.md docs/reports/combat-core-vnext/baseline/scenario-manifest.json`
- `git commit -m "test: establish combat core v2 parity and perf baselines"`

### Phase 1: Tag Domain V2 (data-driven, fixed-width bitset)

**Objective:** Replace static enum bottleneck in the new path with data-driven tag registration and compact runtime IDs.

**Files:**
- Create: `src/game/combat_v2/TagDomain.hpp`
- Create: `src/game/combat_v2/TagDomain.cpp`
- Create: `src/game/combat_v2/TagBitset.hpp`
- Create: `tests/unit/TagDomainV2Tests.cpp`
- Create: `assets/data/combat_v2/tags.json`
- Create: `scripts/combat_v2/compile_tags.py`
- Create: `docs/reports/combat-core-vnext/phase-1/results.md`
- Create: `docs/reports/combat-core-vnext/phase-1/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-1/residual-risk.md`

**Step 1: Write failing tests**
- Unknown tag in runtime blob must fail build-time compile, not silently map to none.
- Alias and canonical names must resolve to same runtime tag ID.
- `all/any/none` mask helpers must match expected truth tables.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*TagDomainV2*"`
- Expected: FAIL (types and compiler not implemented).

**Step 3: Write minimal implementation**
- Implement tag registry compiler (`tags.json` -> compact table) and runtime lookup.
- Implement fixed-width bitset helpers with deterministic index mapping.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*TagDomainV2*"`
- Expected: PASS for aliasing, unknown-tag hard fail, and mask semantics.

**Step 5: Integration gate**
- Run: `./build.bat check`
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/combat_v2/TagDomain.* src/game/combat_v2/TagBitset.hpp tests/unit/TagDomainV2Tests.cpp assets/data/combat_v2/tags.json scripts/combat_v2/compile_tags.py docs/reports/combat-core-vnext/phase-1/commands.txt docs/reports/combat-core-vnext/phase-1/results.md docs/reports/combat-core-vnext/phase-1/residual-risk.md`
- `git commit -m "feat: add data-driven tag domain for combat core v2"`

### Phase 2: Condition IR compiler and evaluator

**Objective:** Compile authoring conditions into a compact runtime predicate representation.

**Files:**
- Create: `src/game/combat_v2/ConditionIR.hpp`
- Create: `src/game/combat_v2/ConditionIR.cpp`
- Create: `src/game/combat_v2/ConditionCompiler.hpp`
- Create: `src/game/combat_v2/ConditionCompiler.cpp`
- Create: `tests/unit/ConditionIRTests.cpp`
- Create: `assets/data/combat_v2/condition_fixtures.json`
- Create: `scripts/combat_v2/compile_conditions.py`
- Create: `docs/reports/combat-core-vnext/phase-2/results.md`
- Create: `docs/reports/combat-core-vnext/phase-2/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-2/residual-risk.md`

**Step 1: Write failing tests**
- Test nested condition expressions (`all + any + none + not`) with deterministic outcomes.
- Test invalid condition schema produces compile-time error.
- Test evaluator performs no allocation for hot-path eval inputs.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*ConditionIR*"`
- Expected: FAIL before compiler/evaluator exists.

**Step 3: Write minimal implementation**
- Implement condition AST -> IR lowering.
- Implement branch-light evaluator against `CombatEvalContext`.
- Add instrumentation hooks for per-stage timing and condition-eval counters (used by perf reports).

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*ConditionIR*"`
- Expected: PASS on semantic equivalence fixtures.

**Step 5: Integration gate**
- Run: `./build.bat notest`
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/combat_v2/ConditionIR.* src/game/combat_v2/ConditionCompiler.* tests/unit/ConditionIRTests.cpp assets/data/combat_v2/condition_fixtures.json scripts/combat_v2/compile_conditions.py docs/reports/combat-core-vnext/phase-2/commands.txt docs/reports/combat-core-vnext/phase-2/results.md docs/reports/combat-core-vnext/phase-2/residual-risk.md`
- `git commit -m "feat: add compiled condition IR for combat core v2"`

### Phase 3: Modifier Graph V2 and source adapters

**Objective:** Normalize equipment/talent/skill-spec/global modifiers into one compiled runtime graph.

**Files:**
- Create: `src/game/combat_v2/ModifierGraph.hpp`
- Create: `src/game/combat_v2/ModifierGraph.cpp`
- Create: `src/game/combat_v2/ModifierSourceAdapters.hpp`
- Create: `src/game/combat_v2/ModifierSourceAdapters.cpp`
- Create: `tests/unit/ModifierGraphV2Tests.cpp`
- Create: `tests/integration/ModifierGraphV2IntegrationTests.cpp`
- Create: `scripts/combat_v2/compile_modifier_graph.py`
- Create: `docs/reports/combat-core-vnext/phase-3/results.md`
- Create: `docs/reports/combat-core-vnext/phase-3/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-3/residual-risk.md`

**Step 1: Write failing tests**
- Verify same semantic modifier from different sources yields identical runtime nodes.
- Verify forbidden conditions prune candidates before damage stage evaluation.
- Verify node whitelist/scope constraints are preserved after compile.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*ModifierGraphV2*"`
- Expected: FAIL before graph compiler and adapters exist.

**Step 3: Write minimal implementation**
- Build graph buckets by stage (`pre_hit`, `hit`, `post_hit`, `dot_tick`).
- Add source adapters that emit canonical runtime modifier records.
- Emit candidate/applied modifier counters per stage for diagnostics/perf evidence.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*ModifierGraphV2*"`
- Expected: PASS on source-normalization and filtering assertions.

**Step 5: Integration gate**
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/combat_v2/ModifierGraph.* src/game/combat_v2/ModifierSourceAdapters.* tests/unit/ModifierGraphV2Tests.cpp tests/integration/ModifierGraphV2IntegrationTests.cpp scripts/combat_v2/compile_modifier_graph.py docs/reports/combat-core-vnext/phase-3/commands.txt docs/reports/combat-core-vnext/phase-3/results.md docs/reports/combat-core-vnext/phase-3/residual-risk.md`
- `git commit -m "feat: add unified modifier graph pipeline for combat core v2"`

### Phase 4: Damage Kernel V2 (deterministic stage engine)

**Objective:** Introduce deterministic execution kernel with stable ordering and explicit stage contracts.

**Files:**
- Create: `src/game/combat_v2/DamageKernel.hpp`
- Create: `src/game/combat_v2/DamageKernel.cpp`
- Create: `src/game/combat_v2/DamageStages.hpp`
- Create: `tests/unit/DamageKernelV2Tests.cpp`
- Create: `tests/integration/DamageKernelParityTests.cpp`
- Create: `docs/reports/combat-core-vnext/phase-4/results.md`
- Create: `docs/reports/combat-core-vnext/phase-4/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-4/residual-risk.md`

**Step 1: Write failing tests**
- Verify operation ordering invariants: `flat -> increased -> more -> convert -> gain_extra`.
- Verify deterministic output hash for repeated identical fixtures.
- Verify DoT and hit branches stay behaviorally isolated.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*DamageKernelV2*"`
- Expected: FAIL before kernel is implemented.

**Step 3: Write minimal implementation**
- Implement stage dispatcher and operation accumulators.
- Implement deterministic ordering key `(stage, priority, source_id, node_id)`.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*DamageKernelV2*"`
- Expected: PASS for invariants and deterministic replay.

**Step 5: Integration gate**
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/combat_v2/DamageKernel.* src/game/combat_v2/DamageStages.hpp tests/unit/DamageKernelV2Tests.cpp tests/integration/DamageKernelParityTests.cpp docs/reports/combat-core-vnext/phase-4/commands.txt docs/reports/combat-core-vnext/phase-4/results.md docs/reports/combat-core-vnext/phase-4/residual-risk.md`
- `git commit -m "feat: add deterministic damage kernel for combat core v2"`

### Phase 5: Legacy/V2 dual-run parity and controlled rollout

**Objective:** Run both paths in parallel, compare output, then gate rollout by parity and perf.

**Files:**
- Modify: `src/game/systems/combat/DamagePipeline.cpp`
- Create: `src/game/combat_v2/CombatV2RuntimeFacade.hpp`
- Create: `src/game/combat_v2/CombatV2RuntimeFacade.cpp`
- Create: `tests/integration/CombatV2DualRunParityTests.cpp`
- Create: `docs/reports/combat-core-vnext/phase-5/parity-report.md`
- Create: `docs/reports/combat-core-vnext/phase-5/results.md`
- Create: `docs/reports/combat-core-vnext/phase-5/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-5/residual-risk.md`

**Step 1: Write failing tests**
- For selected skills/scenarios, assert legacy and v2 outputs are within tolerance.
- Assert mismatch report includes full diagnosis (skill, tags, modifier IDs, stage delta).
- Assert fixture declares scenario class and uses class-specific tolerance from policy.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatV2DualRunParity*"`
- Expected: FAIL before facade and comparator are in place.

**Step 3: Write minimal implementation**
- Add runtime mode switch with `LegacyOnly`, `V2Only`, and `DualRunCompare`.
- Keep sampled dual-run diagnostics available in non-test builds.
- Implement diff reporter and tolerance policy.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatV2DualRunParity*"`
- Expected: PASS for parity fixtures.

**Step 5: Integration and perf gate**
- Run: `./build.bat`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/systems/combat/DamagePipeline.cpp src/game/combat_v2/CombatV2RuntimeFacade.* tests/integration/CombatV2DualRunParityTests.cpp docs/reports/combat-core-vnext/phase-5/parity-report.md docs/reports/combat-core-vnext/phase-5/commands.txt docs/reports/combat-core-vnext/phase-5/results.md docs/reports/combat-core-vnext/phase-5/residual-risk.md`
- `git commit -m "feat: add combat v2 dual-run parity rollout gates"`

### Phase 6: Cutover and legacy removal

**Objective:** Switch production runtime to v2 and remove replaced legacy paths only after gate success.

**Files:**
- Modify: `src/game/systems/combat/DamagePipeline.cpp`
- Modify: `src/game/systems/combat/StatsSystem.cpp`
- Modify: `src/game/systems/modifier/ModifierEvaluator.cpp`
- Modify: `src/game/systems/modifier/ModifierRuntimeRegistry.cpp`
- Modify: `src/game/systems/modifier/EquipmentModifierAdapter.cpp`
- Modify: `src/game/systems/modifier/SkillSpecModifierAdapter.cpp`
- Modify: `src/game/systems/modifier/TalentModifierAdapter.cpp`
- Create: `tests/integration/CombatV2CutoverTests.cpp`
- Create: `docs/reports/combat-core-vnext/phase-6/results.md`
- Create: `docs/reports/combat-core-vnext/phase-6/commands.txt`
- Create: `docs/reports/combat-core-vnext/phase-6/residual-risk.md`
- Update: `docs/plans/2026-03-03-combat-core-vnext-implementation-plan.md`

**Step 1: Write failing tests**
- Assert v2-only mode path is active.
- Assert removed legacy entry points are no longer reachable.

**Step 2: Run tests to verify failure**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatV2Cutover*"`
- Expected: FAIL before final cutover.

**Step 3: Write minimal implementation**
- Remove legacy runtime branch points and wire v2 as default.

**Step 4: Run tests to verify pass**
- Run: `./build.bat notest`
- Run: `./bin/NoMoreDayTests.exe --test-case="*CombatV2Cutover*"`
- Expected: PASS.

**Step 5: Full verification gate (mandatory)**
- Run: `./build.bat check`
- Run: `./build.bat`
- Run: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Run: `ctest --test-dir build -C Release -L performance --output-on-failure`

**Step 6: Commit**
- `git add src/game/systems/combat/DamagePipeline.cpp src/game/systems/combat/StatsSystem.cpp src/game/systems/modifier tests/integration/CombatV2CutoverTests.cpp docs/reports/combat-core-vnext/phase-6/commands.txt docs/reports/combat-core-vnext/phase-6/results.md docs/reports/combat-core-vnext/phase-6/residual-risk.md docs/plans/2026-03-03-combat-core-vnext-implementation-plan.md`
- `git commit -m "refactor: cut over to combat core v2 and remove legacy branches"`

## Cross-phase testing contract (hard requirement)

Every phase must include all of the following, otherwise phase is incomplete:

1. At least one new failing doctest case before implementation changes.
2. One targeted single-suite run proving red -> green for that phase.
3. One integration-label CTest run for cross-system safety.
4. One performance-label CTest run to enforce regression budget.
5. Updated phase evidence report under `docs/reports/combat-core-vnext/phase-<n>/`.
6. Package evidence files: `commands.txt`, `results.md`, `residual-risk.md`.
7. If full `-L performance` fails in untouched pre-existing modules, run scoped module gate tests for this phase and attach a root-cause investigation report before marking phase as provisionally complete.

### Performance-failure exception protocol (scoped, temporary)

Only for pre-existing failures outside touched files:

1. Confirm repro in full suite and gather failing test evidence.
2. Verify failing performance tests are outside files modified by the current phase package.
3. Run scoped module-gate tests for this phase and require clean pass.
4. Record blocker and root-cause evidence in `phase-<n>/results.md` and `phase-<n>/residual-risk.md`.
5. Mark phase status as `provisionally complete` (not final complete) until full suite is green.

## Risk and mitigation

- Behavioral drift during dual-path period.
  - Mitigation: parity fixtures and diagnostic diff artifacts per phase.
- Performance regressions from richer condition model.
  - Mitigation: compile-time lowering, stage prefiltering, mandatory per-phase perf gate.
- Pre-existing unrelated perf-suite flakes blocking scoped phase delivery.
  - Mitigation: scoped module-gate pass + documented root-cause evidence + provisional completion status.
- Data migration inconsistencies (tag aliases, historical keys).
  - Mitigation: build-time hard fail for unknown tags, explicit alias table versioning.

## Definition of done

Initiative is done only when:

1. Combat v2 path is default runtime for supported gameplay scope.
2. Legacy combat branches replaced by v2 are removed.
3. Phase parity and perf reports show pass status within budget.
4. CI/unit/integration/performance suites pass on final branch.
5. Authoring-to-runtime compile path is the only accepted input route for v2 combat modifiers/tags/conditions.
