# Skill Specialization Key Node Test Matrix Plan

> Track ID: `skill_specialization_keynode_test_matrix_20260223`  
> Depends on: `skill_node_effect_implementation_20260223`  
> Workflow: TDD-first, fixture-driven expansion

---

## Phase 1: Matrix Foundation & Fixture Setup

- [x] Extract key-node list from `assets/data/skill_contracts_compact.json` into test fixture baseline.
- [x] Create `tests/fixtures/skill_specialization_keynodes.json` and validate schema fields.
- [x] Add shared test helpers for caster/target setup and deterministic event dispatch.
- [x] Add matrix helper for per-node expectation registration (`skill_id`, `node_id`, `layer`).
- [x] Add smoke test that validates fixture completeness against contract (`48` key nodes).

Verification:

- [x] Fixture completeness test fails before wiring all nodes and passes after correction.

---

## Phase 2: Unit-Level Key Node Coverage

- [x] Add `tests/unit/SkillKeyNodeMatrixTests.cpp` (new) for per-node unit assertions.
- [x] Cover skill `1` key nodes: `113, 114, 130, 152, 170, 171`.
- [x] Cover skill `2` key nodes: `230, 233, 250, 252, 270`.
- [x] Cover skill `3` key nodes: `330, 352, 370, 371, 373`.
- [x] Cover skill `4` key nodes: `430, 451, 452, 470, 471`.
- [x] Cover skill `5` key nodes: `530, 533, 552, 570, 571`.
- [x] Cover skill `6` key nodes: `630, 633, 652, 670, 671`.
- [x] Cover skill `7` key nodes: `713, 730, 750, 752, 770`.
- [x] Cover skill `8` key nodes: `813, 830, 831, 852, 870, 871`.
- [x] Cover skill `9` key nodes: `913, 930, 950, 951, 952, 970`.
- [x] Add negative-path guards for trigger cooldown/depth and transmuter mutex conflicts.

Verification:

- [x] Unit matrix reports explicit coverage for all `48` key nodes.
- [x] Trigger/mutex guard tests pass with deterministic results.

---

## Phase 3: Integration-Level Effect Validation

- [x] Add `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` (new) for runtime behavior checks.
- [x] Add per-skill integration scenarios (`1..9`) for cast -> update -> effect closure.
- [x] Validate channeling/runtime-tick effects for skills `5` and `7`.
- [x] Validate projectile/return/interception paths for skills `4` and `8`.
- [x] Validate counter-window and flow-reset paths for skill `9`.
- [x] Validate trigger-chain dispatch for skills `1` and `2`.
- [x] Validate area/formation interactions for skills `3` and `6`.

Verification:

- [x] Integration matrix covers all skills and key interaction categories.
- [x] No regression in existing `SkillSystemTests` scenarios.

---

## Phase 4: Cross-Skill and Visual-Signal Guards

- [x] Add at least `12` cross-skill scenarios (trigger/synergy/transmuter combinations).
- [x] Add deterministic visual-signal guard checks using existing runtime state/counters.
- [x] Verify transmuter conversion propagates consistently across cast/tick/counter paths.
- [x] Verify scope-policy checks for global vs skill-only effects under specialization states.
- [x] Minimize duplication by reusing matrix helper and fixture-driven loops.

Verification:

- [x] Cross-skill scenario count reaches target (`>= 12`).
- [x] Visual-signal guard tests pass without screenshot dependency.

---

## Phase 5: Gate, Evidence, and Sync

- [x] Run full verification gate for this track.
- [x] Produce key-node coverage report summary in `validation.md` (node -> test mapping).
- [x] Document unresolved non-blocking test debt (if any) with bug linkage.
- [x] Sync track docs (`plan/index/metadata/validation`) and `conductor/tracks.md`.

Final verification commands:

- [x] `build.bat`
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

Deliverables:

- [x] New fixture-driven specialization key-node test matrix.
- [x] Expanded unit + integration coverage for all key nodes.
- [x] Track validation evidence with explicit node-coverage table.
