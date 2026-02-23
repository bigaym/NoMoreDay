# Skill Spec Safety & UI Hardening Spec

> Track ID: `skill_spec_safety_ui_hardening_20260223`  
> Type: bugfix/hardening  
> Priority: P0  
> Scope: Skill specialization runtime safety + skill specialization UI rendering integrity

---

## 1. Overview

This track fixes four confirmed defects in skill specialization:

1. Repeated `SkillSystem::InitHooks()` causes duplicate combat event handlers.
2. `ResetTalents/ClearAllTalents` does not fully clear specialization runtime state.
3. `GetEffectiveSkillTags()` is not transmuter-mutex aware.
4. `UISkillTalentTree` has an unbalanced scissor stack.

Primary goals:

- Specialization nodes must apply exactly once per event.
- Specialization reset must be safe and complete (no stale runtime state).
- Transmuter behavior must be consistent across tags/stats/damage.
- Skill specialization panel rendering must be stable and complete.

---

## 2. Constraints

- ECS: EnTT; no ownership/lifecycle logic in components.
- Build and runtime target: Windows + MSVC.
- Avoid allocations and string-driven branching in hot paths.
- Keep existing skill contract model (`SkillContract`, `NodeContractData`) intact.
- Keep RenderGraph/frame order constraints unchanged.

---

## 3. Data Model & Runtime Ownership

### 3.1 Existing Runtime State (authoritative)

`SkillContractRuntimeComponent` (existing):

- `active_transmuter_node_by_skill: map<skill_id, node_id>`
- `trigger_cooldowns: map<node_id, remaining_seconds>`

### 3.2 New Lifecycle Metadata (static runtime ownership)

In `SkillSystem` static state, add explicit handler ownership:

- `uint32_t s_onSkillHitHandlerId`
- `uint32_t s_onTakeDamageHandlerId`
- `bool s_hooksInitialized`

Lifecycle API additions:

- `InitHooks()` becomes idempotent.
- `ShutdownHooks()` (or equivalent) unregisters dispatcher handlers and clears local hooks.

No new ECS component is introduced.

---

## 4. Behavioral Contract

### 4.1 Hook Safety Contract

- Calling `InitHooks()` repeatedly must not duplicate handlers.
- After `ShutdownHooks()`, no handler previously owned by `SkillSystem` remains registered.

### 4.2 Reset Safety Contract

When calling `ResetTalents(skill_id)`:

- Remove all allocated nodes for that skill.
- Refund points as before.
- Clear `active_transmuter_node_by_skill[skill_id]`.
- Remove cooldown entries in `trigger_cooldowns` that belong to this skill's allocated nodes.

When calling `ClearAllTalents()`:

- Existing behavior + clear both runtime maps for all skill-specialization state.

### 4.3 Tag Consistency Contract

`GetEffectiveSkillTags(entity, skill_id)` must:

- Apply add/remove tags only for active nodes.
- For transmuter nodes, only apply the selected active transmuter node when runtime state exists.
- Keep behavior deterministic when runtime state is absent (fallback to contract selection rule).

### 4.4 UI Rendering Contract

`UISkillTalentTree` must keep scissor calls balanced:

- Every `BeginScissorMode` has one `EndScissorMode`.
- No accidental nested scissor without a matching close.

---

## 5. Implementation Targets

- `src/game/systems/skill/SkillSystem.cpp`
- `src/game/systems/skill/SkillSystem.hpp`
- `src/game/systems/ui/UISkillTalentTree.cpp`
- `tests/unit/SkillBehaviorGuardTests.cpp`
- `tests/integration/SkillSystemTests.cpp`
- `tests/tech/UITests.cpp` (if needed for regression guard)

---

## 6. Test Strategy

### Unit

- Hook lifecycle idempotency:
  - call `InitHooks()` twice, dispatch once, verify one trigger execution path.
- Transmuter tag consistency:
  - multiple transmuters allocated, one runtime active, assert tags from active only.

### Integration

- `ResetTalents` clears runtime transmuter/cooldown state for the target skill.
- `ClearAllTalents` clears all specialization runtime state.

### Tech/UI

- Skill spec UI draw smoke/regression:
  - open skill tree, draw path executes without scissor leakage side effects in subsequent UI draw calls.

---

## 7. Acceptance Criteria

- [ ] Repeated `SkillSystem::InitHooks()` does not duplicate event handling.
- [ ] `ResetTalents` and `ClearAllTalents` clear runtime specialization state safely.
- [ ] `GetEffectiveSkillTags` honors transmuter mutex semantics.
- [ ] `UISkillTalentTree` scissor stack is balanced.
- [ ] Added/updated tests cover all four defects and pass in CI labels (`unit` + `integration`; `tech` where available).
- [ ] No regression in existing skill contract behavior.

---

## 8. Risks & Mitigations

- Risk: Unregistering wrong dispatcher handler IDs.
  - Mitigation: Store handler IDs owned by `SkillSystem` only; zero after unregister.
- Risk: Tag behavior mismatch before first cast (runtime state not yet materialized).
  - Mitigation: deterministic fallback selection policy aligned with contract order.
- Risk: UI regressions due to scissor scope refactor.
  - Mitigation: minimal diff, focused regression test and manual smoke check path.

