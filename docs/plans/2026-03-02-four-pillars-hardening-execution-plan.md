# Four Pillars Hardening Execution Plan

Date: 2026-03-02  
Execution mode: phased, subagent-driven, verification-gated  
Related design: `docs/plans/2026-03-02-four-pillars-hardening-design.md`

## Goal

Execute the four-pillar hardening program with strict gates and auditable progress:

- A. Render unique-version convergence (no runtime legacy compatibility branches).
- B. Structural decoupling of skill/combat/item/ui without behavior drift.
- C. Expanded module-level verification, with stronger ui/item/progression coverage.
- D. Canonical modifier/skill schema pipeline with migration-or-drop policy.

## Fixed constraints

- Runtime uniqueness: only one active version path after convergence.
- Legacy code: remove, do not preserve runtime compatibility layers.
- Old data: migrate if possible, otherwise remove from supported set.
- Save compatibility: explicitly out of scope in this initiative.
- Performance budget: `P95 frame time <= baseline + 5%`.

## Repository verification commands

Use only repo-supported commands from `D:\PRJ\NoMoreDay`:

- `./build.bat check`
- `./build.bat debug`
- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

Single-test triage when needed:

- `./bin/NoMoreDayTests.exe --list-test-cases`
- `./bin/NoMoreDayTests.exe --test-case="<case or wildcard>"`

## Subagent package contract

Every implementation package must include:

1. Package ID and objective.
2. Allowed file scope.
3. Invariants that cannot change.
4. Test-first checklist and exact commands.
5. Evidence block: changed files, command outputs, residual risks.

Package merge rule:

- Parent agent merges package results only when package gates pass.

## Autonomous Git and worktree operating mode

- Each package runs in dedicated worktree `worktrees/<package-id>/` created from current phase head.
- Each package must declare both include scope and explicit exclusion scope before edits.
- Before any build in worktree, run a mapping check that mandatory shared directories are available.
- Mandatory mapping baseline: `third_party` must be available in worktree context.
- Phase 0 may extend required mappings (for example `assets`, generated contract outputs, or other shared build/runtime folders) and update this baseline.
- Parent integration uses non-destructive history (`merge` or `rebase` as configured for phase), and rollback is done by revert commits, not history rewrite.
- Package checkpoint commits are authorized for this initiative; push remains explicit and user-directed.

## Evidence artifact contract

- Store package evidence under `docs/reports/four-pillars/<phase>/<package-id>/`.
- Required files per package:
  - `commands.txt`: exact commands executed.
  - `results.md`: pass/fail outcomes and key output excerpts.
  - `residual-risk.md`: known risks and follow-up actions.
- Optional files by package type:
  - `inventory.json` for code inventory packages.
  - `baseline.md` for performance and test baselines.
  - `migration-report.md` and `drop-list.md` for migrate-or-drop packages.

## Deterministic performance protocol

- Fixed benchmark conditions: scene seed, resolution, quality tier, and hardware profile must match baseline.
- Run one warmup pass, then five measured runs.
- Compare median P95 frame time of candidate vs baseline.
- Gate fails if median P95 delta is > 5% and outside a 1% run-to-run noise band.
- Phase 0 defines the canonical benchmark scenario list and runner command; later phases must reuse it.

## Phase plan

## Phase 0 - Baseline hardening shell (Week 1)

Objective:

- Install guardrails before major deletion/refactor work.

### Package P0-1: Legacy/version inventory

Scope:

- Scan `src/engine/render`, `src/game/systems/{combat,skill,item,ui}`, related data registries.

Allowed file scope:

- `scripts/**`
- `docs/reports/four-pillars/phase-0/P0-1/**`
- `docs/plans/**` (only if references need synchronization)

Disallowed scope:

- Runtime source changes under `src/**`.

Steps:

1. Create machine-readable inventory of legacy/version markers and branch points.
2. Classify entries: removable runtime branch, metadata-only, or migration-path dependent.
3. Produce target deletion map for Phase 1+.

Verification:

- Inventory diff reviewed and accepted.

### Package P0-2: Baseline measurement and test map

Scope:

- Existing tests and perf scenarios.

Allowed file scope:

- `docs/reports/four-pillars/phase-0/P0-2/**`
- Benchmark/stability runner scripts under `scripts/**`

Disallowed scope:

- Gameplay or render behavior changes under `src/**`.

Steps:

1. Capture baseline performance (P95 frame time + scenario metadata).
2. Capture baseline pass/fail for unit/integration/performance suites.
3. Map tests to modules and identify hard gaps (ui/item/progression).

Verification:

- Baseline report committed in docs.

### Package P0-3: Legacy reintroduction gate

Scope:

- Check scripts and CI plumbing.

Allowed file scope:

- `scripts/**`
- `build.bat`
- `tests/**` (only for gate helper tests)
- `docs/reports/four-pillars/phase-0/P0-3/**`

Disallowed scope:

- Feature behavior changes in runtime systems.

Steps:

1. Add checks that fail when banned runtime legacy patterns are added.
2. Integrate checks into pre-check and CI path.
3. Add clear error messages for developers.

Verification:

- `./build.bat check`
- `./build.bat debug`

Phase 0 exit gate:

- All P0 packages merged.
- Guardrail checks active and green.

## Phase 1 - Pillar A render convergence (Weeks 2-4)

Objective:

- Remove runtime multi-version behavior and converge render to one active path.

### Package A1-1: RenderSystem branch convergence

Allowed file scope:

- `src/engine/render/RenderSystem.cpp`
- `tests/**` (render contract tests)
- `docs/reports/four-pillars/phase-1/A1-1/**`

Disallowed scope:

- `src/game/**` unless explicitly required to fix compile break introduced by removal.

Steps (TDD-aligned):

1. Add/adjust tests asserting expected behavior for retained path only.
2. Validate tests fail when old branch assumptions remain.
3. Delete legacy branch routing/fallback execution logic.
4. Verify retained behavior path passes tests.

Verification:

- Targeted doctest cases for render contracts.
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

### Package A1-2: GPU data structure convergence

Allowed file scope:

- `src/engine/render/GPUData.hpp`
- Related ABI generation/check scripts under `tools/**` or `scripts/**`
- `tests/**` (ABI and render contract checks)
- `docs/reports/four-pillars/phase-1/A1-2/**`

Disallowed scope:

- Unrelated gameplay systems under `src/game/**`.

Steps:

1. Define canonical runtime GPU data layout.
2. Remove old generation-specific runtime structs/paths.
3. Keep version labels only where metadata is required and branch-free.

Verification:

- `./build.bat check`
- `./build.bat`

### Package A1-3: Render pass cleanup

Allowed file scope:

- `src/engine/render/passes/*`
- Related pass wiring files under `src/engine/render/**`
- `tests/**` (render pass contracts)
- `docs/reports/four-pillars/phase-1/A1-3/**`

Disallowed scope:

- Combat/skill/item/ui runtime behavior changes.

Steps:

1. Remove pass-level legacy version toggles and fallback paths.
2. Keep quality-tier feature controls that are not version compatibility.
3. Update pass contracts.

Verification:

- Integration render contracts.
- Performance smoke run.

### Package A1-4: Old render-related data migration or drop

Scope:

- Data/assets/config records read by converged runtime.

Allowed file scope:

- `assets/**`
- `scripts/**` (migration tooling)
- `tests/**` (migration and rejection tests)
- `docs/reports/four-pillars/phase-1/A1-4/**`

Disallowed scope:

- Reintroducing legacy runtime parse paths in `src/**`.

Steps:

1. Implement offline migration for migratable records.
2. Produce explicit drop list for non-migratable records.
3. Add negative tests to ensure dropped formats are rejected.

Verification:

- Migration test set passes.
- Negative tests pass.

Phase 1 exit gate:

- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Phase perf check within budget.

## Phase 2 - Pillar B wave 1 (skill/combat) (Weeks 5-8)

Objective:

- Reduce coupling and isolate business logic in skill/combat.

### Package B2-1: Skill orchestrator/service split

Allowed file scope:

- `src/game/systems/skill/SkillSystem.cpp`
- extracted files under `src/game/systems/skill/`
- `tests/**` (skill contracts)
- `docs/reports/four-pillars/phase-2/B2-1/**`

Disallowed scope:

- Item/ui modules except compile-fix shims approved by parent package gate.

Steps:

1. Add contract tests around behavior-critical skill flows.
2. Extract pure rule services from orchestration code.
3. Move translation and registry access into adapters.
4. Keep external behavior stable.

Verification:

- Unit skill contracts.
- Integration skill scenario tests.

### Package B2-2: Combat pipeline extraction

Allowed file scope:

- `src/game/systems/combat/DamagePipeline.cpp`
- related combat service files
- `tests/**` (combat contracts)
- `docs/reports/four-pillars/phase-2/B2-2/**`

Disallowed scope:

- Render runtime behavior changes unrelated to combat verification.

Steps:

1. Add contract tests for damage and mitigation invariants.
2. Extract pipeline stages to explicit services.
3. Remove hidden legacy paths discovered during extraction.

Verification:

- Unit combat contracts.
- Integration combat scenarios.

Phase 2 exit gate:

- `./build.bat debug`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## Phase 3 - Pillar B wave 2 (item/ui) + progression gate expansion (Weeks 9-12)

Objective:

- Apply the same decoupling and contract discipline to item/ui.

### Package B3-1: Item system decomposition

Allowed file scope:

- `src/game/systems/item/ItemFactory.cpp`
- `src/game/systems/item/InventorySystem.cpp`
- related item service/adapters
- `tests/**` (item contracts)
- `docs/reports/four-pillars/phase-3/B3-1/**`

Disallowed scope:

- Render pipeline or schema-generator behavior changes.

Steps:

1. Add missing item edge-case tests first.
2. Extract generation/validation/flow services.
3. Resolve TODO placeholders through explicit behavior or explicit rejection.

Verification:

- Unit item contracts.
- Integration inventory flow tests.

### Package B3-2: UI system decomposition

Allowed file scope:

- `src/game/systems/ui/UISystem.cpp`
- related ui subsystem files
- `tests/**` (ui contracts)
- `docs/reports/four-pillars/phase-3/B3-2/**`

Disallowed scope:

- Item progression balancing changes beyond structural extraction.

Steps:

1. Add tests for interaction state and boundary inputs.
2. Extract state transition logic into testable services.
3. Replace placeholder TODO behavior with explicit implementation or hard fail states.

Verification:

- Unit UI state tests.
- Integration UI workflow tests.

### Package B3-3: Progression gate expansion

Scope:

- Progression-related tests and CI label/gate registration.

Allowed file scope:

- `tests/**` (progression unit/integration contracts)
- `tests/CMakeLists.txt`
- CI workflow config files
- `docs/reports/four-pillars/phase-3/B3-3/**`

Disallowed scope:

- Runtime progression feature redesign.

Steps:

1. Add progression contracts for level-up, unlock, and rollback constraints.
2. Wire progression tests into module gate mapping.
3. Validate deterministic behavior in CI configuration.

Verification:

- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

Phase 3 exit gate:

- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## Phase 4 - Pillar D schema single source of truth (Weeks 13-14)

Objective:

- Canonical schema for modifier/skill and enforced generation pipeline.

### Package D4-1: Canonical schema definition

Scope:

- Schema files and generators for modifier/skill records.

Allowed file scope:

- Schema and generator files under `scripts/**` and `tools/**`
- Schema-owned data under `assets/**`
- `tests/**` (schema fixtures/tests)
- `docs/reports/four-pillars/phase-4/D4-1/**`

Disallowed scope:

- Runtime save pipeline design changes.

Steps:

1. Define canonical schema fields and constraints.
2. Document evolution rules and migration requirements.
3. Add fixtures for valid and invalid samples.

Verification:

- Schema validation tests.

### Package D4-2: Generation and runtime contract integration

Scope:

- Generator scripts and generated runtime contracts.

Allowed file scope:

- `scripts/**`
- `tools/**`
- generated contracts under `src/game/data/**` (schema-owned outputs only)
- `tests/**`
- `docs/reports/four-pillars/phase-4/D4-2/**`

Disallowed scope:

- Non-schema gameplay behavior tuning.

Steps:

1. Route all runtime contracts through canonical schema generation.
2. Remove manual side-channel definitions.
3. Enforce checks in pre-check path.

Verification:

- `./build.bat check`
- `./build.bat`

### Package D4-3: Config migration completion

Scope:

- Existing config/assets touching modifier/skill.

Allowed file scope:

- `assets/**`
- `scripts/**` (migration tooling)
- `tests/**` (migration/rejection checks)
- `docs/reports/four-pillars/phase-4/D4-3/**`

Disallowed scope:

- Introducing compatibility-only runtime loaders.

Steps:

1. Migrate all migratable records.
2. Drop unsupported non-migratable records.
3. Add rejection tests for dropped formats.

Verification:

- Config migration test set.

Phase 4 exit gate:

- `./build.bat check`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

## Phase 5 - Pillar C final gate expansion and full validation (Weeks 15-16)

Objective:

- Lock comprehensive verification as standard operating mode.

### Package C5-1: Module gate expansion

Scope:

- Test registration and CI mapping.

Allowed file scope:

- `tests/**`
- CI workflow config files
- `docs/reports/four-pillars/phase-5/C5-1/**`

Disallowed scope:

- Runtime feature changes.

Steps:

1. Ensure ui/item/progression gate coverage is explicit and measurable.
2. Add module-triggered suites where practical.
3. Keep ci runtime within acceptable bounds by balancing PR/nightly scope.

Verification:

- Gate dry run in CI configuration branch.

### Package C5-2: End-to-end full validation report

Allowed file scope:

- `docs/reports/four-pillars/phase-5/C5-2/**`
- Stability runner scripts under `scripts/**`

Disallowed scope:

- Runtime behavior edits except test harness fixes.

Steps:

1. Run build matrix and all suites.
2. Run long-run stability command: `python scripts/perf/run_long_stability.py --profile phase5 --minutes 60`.
3. Compare performance vs Phase 0 baseline.
4. Produce final go/no-go report.

Mandatory full validation matrix:

- `./build.bat check`
- `./build.bat debug`
- `./build.bat`
- `./build.bat release`
- `./build.bat analyze`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

Performance gate:

- P95 frame time <= baseline + 5%.

Phase 5 exit gate:

- All matrix commands pass.
- Performance gate passes.
- Final report accepted.

## Execution control and rollback rules

- Do not start next phase until current phase exit gate passes.
- If a package fails after integration, rollback via `git revert` of package merge commit and retry with a new package branch/worktree.
- No broad refactors outside current package scope.
- No destructive git commands.
- Package checkpoint commits are allowed for this initiative; do not push unless explicitly requested.

## Reporting format per package

Each completed package reports:

1. Package ID and objective.
2. Files changed.
3. Tests added/updated.
4. Commands executed and results.
5. Performance impact if relevant.
6. Residual risk and follow-up items.

## Definition of done

Program is done when:

1. Pillars A/B/C/D targets are met as defined in design doc.
2. Runtime unique-version policy is enforced in code and tests.
3. Unsupported old data is rejected without compatibility fallback.
4. Full validation matrix passes with performance budget respected.
5. Final report includes reproducible evidence and remaining non-blocking debt.
