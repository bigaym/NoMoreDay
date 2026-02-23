# Skill Spec Safety & UI Hardening Plan

> Track ID: `skill_spec_safety_ui_hardening_20260223`  
> Depends on: current specialization/contract baseline in `main`  
> Workflow: TDD-first, minimal-change hardening

---

## Phase 1: Repro & Test Baseline

- [x] Add failing unit test for duplicate handler registration on repeated `InitHooks()`.
- [x] Add failing integration test for runtime state cleanup on `ResetTalents`.
- [x] Add failing integration test for runtime state cleanup on `ClearAllTalents`.
- [x] Add failing unit test for transmuter-aware effective tags.
- [x] Add focused UI regression check target for skill tree scissor safety.

Verification:

- [x] New tests fail for expected reasons before fix.

---

## Phase 2: Hook Lifecycle Hardening

- [x] Implement `SkillSystem` dispatcher handler ownership (`s_onSkillHitHandlerId`, `s_onTakeDamageHandlerId`).
- [x] Make `InitHooks()` idempotent.
- [x] Implement explicit unregister path (`ShutdownHooks()` or equivalent) and wire into lifecycle.
- [x] Ensure local pre/post hook containers remain consistent with lifecycle changes.

Verification:

- [x] Duplicate registration test passes.
- [x] Existing behavior dispatch tests still pass.

---

## Phase 3: Runtime State & Tag Consistency

- [x] Update `ResetTalents(skill_id)` to clear per-skill runtime state (`active_transmuter_node_by_skill`, related `trigger_cooldowns`).
- [x] Update `ClearAllTalents()` to clear all specialization runtime state.
- [x] Update `GetEffectiveSkillTags()` to enforce transmuter mutex semantics.
- [x] Keep behavior backward-compatible for non-transmuter nodes.

Verification:

- [x] Runtime cleanup tests pass.
- [x] Transmuter effective tag test passes.
- [x] No regression in existing `SkillBehaviorGuard` and `SkillContract` tests.

---

## Phase 4: UI Render Integrity

- [x] Fix unbalanced scissor scope in `UISkillTalentTree`.
- [x] Keep rendering diff minimal and localized.
- [x] Run skill UI smoke/regression checks.

Verification:

- [x] No scissor leakage in post-skill-tree UI rendering path.
- [x] Relevant `tech`/UI tests pass.

---

## Final Verification Gate

- [x] `build.bat`
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- [x] If UI tech tests are labeled: run corresponding `ctest` filter and archive evidence.

---

## Deliverables

- Code fixes in skill runtime + UI specialization renderer path.
- New/updated tests proving non-regression and safety.
- `validation.md` evidence in track folder during implementation.
