# Blade Mastery Correctness And Closure Design

> Date: 2026-03-13
> Status: approved
> Scope: fix the highest-risk Blade Ascendant correctness issues first, then close the remaining Heavenly Sword productization gaps, and finally re-review the result.

---

## 1. Context

Subagent review found three immediate correctness risks in the current Blade Ascendant mastery stack:

- `Seven Star Slash` target acquisition is too permissive and may hit non-enemy entities.
- `BladeResourceComponent.hit_tracking` cleanup appears to mix absolute timestamps with elapsed timers, so stale tracking entries may never expire reliably.
- save restore rehydrates mastery/resource/signature state without a full normalization pass, so stale persisted runtime state can survive load.

The same review also confirmed a larger closure gap:

- `Heavenly Sword` still lacks part of its documented runtime node coverage and does not yet have a proper attunement selection path in shared service/UI flow.

## 2. Approved Repair Order

1. Fix correctness risks first.
2. Fix shared state normalization needed for stable persistence/runtime behavior.
3. Close Heavenly Sword productization gaps.
4. Re-run verification and request another 3-dimension review.

This order is approved because it reduces gameplay correctness risk before expanding feature completeness.

## 3. Design Decisions

### 3.1 Seven Star Slash targeting

- Keep the behavior in `SevenStarSlash.cpp`.
- Narrow candidate selection rather than redesigning the slash system.
- Require the target set to match actual enemy-valid combat recipients used by surrounding combat code.
- Add a regression test that proves nearby non-enemy entities are ignored.

### 3.2 Blade resource tracking cleanup

- Keep `BladeResourceService` as the owner of resource decay/cleanup behavior.
- Unify hit-tracking cleanup onto the same notion of time used when writing `last_gain_time`.
- Add a narrow unit test that fails on the current mixed-timebase behavior.

### 3.3 Save restore normalization

- Preserve persisted mastery/resource counters when compatible.
- After restore, re-run shared Blade mastery normalization so resource kind, signature unlock, and mirrored legacy state match the current selected mastery and profession.
- Add a regression test for stale/incompatible persisted runtime state.

### 3.4 Heavenly Sword closure

- After correctness fixes are green, continue with missing Heavenly Sword runtime nodes and attunement productization.
- Route attunement through shared mastery service instead of direct component mutation.
- Add only the minimum `UISkillHub` affordance needed to make attunement selectable and testable.

## 4. Testing Strategy

- Use TDD for each repair slice.
- Start with narrow doctest coverage for each bug.
- After each slice, run the narrowest relevant test command first.
- After the touched correctness fixes are complete, run broader `unit` and then targeted `integration` verification.
- After Heavenly Sword/UI work, re-run a focused UI/behavior matrix and request another multi-dimensional review.

## 5. Non-goals

- No broad redesign of Blade Ascendant mastery architecture.
- No unrelated Sword Saint feature expansion.
- No full HUD presentation overhaul beyond urgent attunement/productization needs.
- No destructive git cleanup or unrelated file reverts.
