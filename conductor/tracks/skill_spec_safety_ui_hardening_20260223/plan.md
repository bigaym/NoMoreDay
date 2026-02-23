# Skill Spec Safety & UI Hardening Plan

> Track ID: `skill_spec_safety_ui_hardening_20260223`  
> Depends on: current specialization/contract baseline in `main`  
> Workflow: TDD-first, minimal-change hardening

---

## Phase 1: Repro & Test Baseline

- [ ] Add failing unit test for duplicate handler registration on repeated `InitHooks()`.
- [ ] Add failing integration test for runtime state cleanup on `ResetTalents`.
- [ ] Add failing integration test for runtime state cleanup on `ClearAllTalents`.
- [ ] Add failing unit test for transmuter-aware effective tags.
- [ ] Add focused UI regression check target for skill tree scissor safety.

Verification:

- [ ] New tests fail for expected reasons before fix.

---

## Phase 2: Hook Lifecycle Hardening

- [ ] Implement `SkillSystem` dispatcher handler ownership (`s_onSkillHitHandlerId`, `s_onTakeDamageHandlerId`).
- [ ] Make `InitHooks()` idempotent.
- [ ] Implement explicit unregister path (`ShutdownHooks()` or equivalent) and wire into lifecycle.
- [ ] Ensure local pre/post hook containers remain consistent with lifecycle changes.

Verification:

- [ ] Duplicate registration test passes.
- [ ] Existing behavior dispatch tests still pass.

---

## Phase 3: Runtime State & Tag Consistency

- [ ] Update `ResetTalents(skill_id)` to clear per-skill runtime state (`active_transmuter_node_by_skill`, related `trigger_cooldowns`).
- [ ] Update `ClearAllTalents()` to clear all specialization runtime state.
- [ ] Update `GetEffectiveSkillTags()` to enforce transmuter mutex semantics.
- [ ] Keep behavior backward-compatible for non-transmuter nodes.

Verification:

- [ ] Runtime cleanup tests pass.
- [ ] Transmuter effective tag test passes.
- [ ] No regression in existing `SkillBehaviorGuard` and `SkillContract` tests.

---

## Phase 4: UI Render Integrity

- [ ] Fix unbalanced scissor scope in `UISkillTalentTree`.
- [ ] Keep rendering diff minimal and localized.
- [ ] Run skill UI smoke/regression checks.

Verification:

- [ ] No scissor leakage in post-skill-tree UI rendering path.
- [ ] Relevant `tech`/UI tests pass.

---

## Final Verification Gate

- [ ] `build.bat`
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- [ ] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- [ ] If UI tech tests are labeled: run corresponding `ctest` filter and archive evidence.

---

## Deliverables

- Code fixes in skill runtime + UI specialization renderer path.
- New/updated tests proving non-regression and safety.
- `validation.md` evidence in track folder during implementation.

