# Four Pillars Hardening Design

Date: 2026-03-02  
Status: Approved for execution  
Scope: Runtime hardening and architectural convergence for render, core gameplay systems, testing gates, and config source of truth

## 1. Fixed decisions

This design locks the following non-negotiable decisions:

1. Unique-version runtime policy: runtime code keeps exactly one live version path.
2. Legacy code policy: old version interfaces and compatibility branches are deleted, not preserved.
3. Old data policy: migrate if possible; if not migratable, remove from supported inputs.
4. Save-system policy: no save compatibility work in this initiative.
5. Performance policy for this refactor cycle: regression budget is P95 frame time <= baseline + 5%.

Operational decisions for autonomous execution:

6. Subagent implementation runs in isolated package branches/worktrees and integrates only after package gates pass.
7. Package checkpoint commits are allowed within this initiative; remote push remains explicit and out of scope.

## 1.1 Scope boundary for save-adjacent data

- Save format compatibility/migration is excluded from this initiative.
- Any non-save runtime config/data still follows migrate-or-drop policy.
- If a record is only consumable via save compatibility logic, it is treated as out of scope and not a blocker for this plan.

## 2. Why this hardening is needed

Current implementation shows concentrated risk in four places:

- Render stack carries multi-generation version bridges and fallback paths, creating high complexity and regression surface.
- Game logic is concentrated in very large files in skill/combat/item/ui, increasing coupling and slowing safe iteration.
- Testing depth is uneven across modules; ui/item/progression gates are weaker than combat/skill/render.
- Modifier/skill definitions are distributed across runtime code, assets, scripts, and docs, creating drift risk.

The system works, but maintenance and correctness cost increases with each feature change. This initiative reduces that cost by converging architecture and strengthening verification.

## 3. Goals and non-goals

### Goals

- A. Render convergence to a unique runtime version with no legacy execution branches.
- B. Structural decoupling of skill/combat/item/ui into clearer boundaries.
- C. Module-level verification gates, especially for ui/item/progression.
- D. Single source of truth for modifier/skill config schema, generation, and validation.

### Non-goals

- Re-designing the save/load architecture.
- Rebalancing gameplay values as a primary objective.
- Large visual feature additions unrelated to hardening.

## 4. Target architecture

### A. Render unique-version runtime

Target characteristics:

- One active runtime ABI and one active render behavior path.
- No runtime branch by legacy version markers.
- Legacy migration happens offline via migration tooling.
- Unsupported old records fail early with explicit diagnostics.

Design constraints:

- Remove version-specific runtime struct forks unless they are pure metadata labels with no active branch logic.
- Remove fallback logic that executes old behavior paths.
- Keep quality-tier logic if it is feature-tier control, not version compatibility control.

### B. Core gameplay module decoupling

Target characteristics per domain (skill/combat/item/ui):

- `Orchestrator`: schedules domain flow, no heavy business logic.
- `Domain services`: rule computation and transformations.
- `Adapters`: data translation, registry access, IO boundaries.
- `Contracts`: tests that pin behavior at service and orchestration boundaries.

Design constraints:

- Behavioral parity required; refactor does not intentionally change gameplay outputs.
- Incremental extraction; each extraction protected by targeted tests.

### C. Comprehensive module-level test gates

Target characteristics:

- Explicit per-module gate bundles for ui/item/progression plus existing major domains.
- PR-blocking checks for unit/integration plus config/script validation.
- Nightly full suite including performance and long-run stability scenarios.

Design constraints:

- Test suites must remain deterministic in CI.
- Contract tests prioritize semantic behavior over fragile pixel equality where possible.

### D. Config single source of truth

Target characteristics:

- Modifier/skill schema as canonical source.
- Generated runtime contracts and checks from schema definitions.
- No manual side-channel struct definitions for the same conceptual record.

Design constraints:

- Schema evolution follows explicit migration documents.
- Any config that cannot be migrated to canonical schema is dropped from supported set.

## 5. Phased design and dependency graph

## Phase 0 - Baseline and hard gates

Purpose:

- Establish hard rules and baseline metrics before heavy edits.

Outputs:

- Legacy/version inventory.
- Baseline performance report (P95 and scenario metadata).
- CI checks that fail on reintroduction of legacy runtime branches.
- Migration ledger template for old data.

Dependencies:

- None.

## Phase 1 - Pillar A (render convergence)

Purpose:

- Remove runtime multi-version branches and converge to one active version.

Outputs:

- Legacy render runtime paths removed.
- Offline migration tooling for old render-related data inputs.
- Negative tests proving old formats are rejected by runtime.

Dependencies:

- Phase 0 gates active.

## Phase 2 - Pillar B wave 1 (skill/combat)

Purpose:

- Reduce coupling and split high-risk logic with behavior preserved.

Outputs:

- Extracted services/adapters for skill and combat.
- Contract tests pinning behavior at module boundaries.

Dependencies:

- Phase 1 integrated, baseline stable.

## Phase 3 - Pillar B wave 2 (item/ui) + progression gate expansion

Purpose:

- Apply same decoupling patterns to item and ui.

Outputs:

- Extracted services/adapters for item and ui.
- Strengthened edge-case and interaction tests.
- Explicit progression gate bundle (unit + integration label coverage) wired into CI mapping.

Dependencies:

- Phase 2 stable.

## Phase 4 - Pillar D (schema unification)

Purpose:

- Enforce one canonical schema flow for modifier/skill config.

Outputs:

- Canonical schema definitions.
- Generation and validation integrated into check/build paths.
- Migration completion report and dropped-record list.

Dependencies:

- Phase 3 integrated.

## Phase 5 - Pillar C finalization and full-system validation

Purpose:

- Finalize module gates and execute end-to-end validation.

Outputs:

- Full gate report (build, static checks, unit, integration, performance, stability).
- Regression and risk report with go/no-go recommendation.

Dependencies:

- Phases 1-4 completed.

## 6. Verification model

Every phase follows the same gate model:

1. Targeted failing tests first (for the exact area being touched).
2. Minimal implementation/refactor to pass targeted tests.
3. Module-scope integration checks.
4. Global checks required by phase exit.

Standard commands used as gates in this repo:

- `./build.bat check`
- `./build.bat debug`
- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

Performance acceptance rule for this initiative:

- P95 frame time <= baseline + 5%.

Deterministic performance protocol:

- Use fixed scene seed, fixed resolution, fixed quality tier, and fixed hardware profile for baseline/candidate runs.
- Run one warmup pass, then collect five measured runs.
- Compare median P95 between baseline and candidate.
- Performance gate fails when median P95 delta is > 5% and outside a 1% run-to-run noise band.

## 7. Subagent orchestration model

Execution uses bounded independent subagent packages:

- Package definition: objective, files in scope, invariants, tests, rollback conditions.
- One subagent per package when tasks are independent.
- Parent agent integrates only after package-level verification passes.
- Each package produces an audit block: files changed, commands run, results, residual risk.

Worktree operating constraints:

- Each package uses a dedicated worktree rooted under `worktrees/<package-id>/`.
- Package worktrees must map mandatory shared directories before build/test (`third_party` plus any required project-shared directories discovered during Phase 0).
- Build or test in a package worktree is blocked until mapping check passes.

Evidence artifact contract:

- Each package stores evidence at `docs/reports/four-pillars/<phase>/<package-id>/`.
- Required artifacts: `commands.txt`, `results.md`, `residual-risk.md`.
- Package-specific artifacts (as needed): `inventory.json`, `baseline.md`, `migration-report.md`, `drop-list.md`.

This keeps parallelism high without losing correctness control.

## 8. Risk management

Primary risks and mitigation:

- Hidden dependency on removed version branches.  
  Mitigation: negative tests for removed paths + staged deletion + compile gates.

- Behavioral drift during large-file decomposition.  
  Mitigation: contract tests before extraction, no mixed refactor/feature PR scope.

- Config drift during schema unification.  
  Mitigation: canonical schema checks in `build.bat check` and migration ledger reviews.

- Performance regressions from architecture cleanup.  
  Mitigation: phase-level perf checkpoints and final perf suite gate.

## 9. Completion criteria

Initiative is complete only when all are true:

1. Runtime has one active version path in render and related hardening scope.
2. Legacy runtime compatibility branches are removed from targeted modules.
3. Non-migratable old data is explicitly dropped from supported inputs.
4. Skill/combat/item/ui decomposition completed with passing contract gates.
5. Modifier/skill schema is canonical and enforced by generation/validation checks.
6. Full validation matrix passes, including perf threshold.
