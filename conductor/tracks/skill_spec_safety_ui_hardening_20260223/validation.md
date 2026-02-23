# Validation - skill_spec_safety_ui_hardening_20260223

## Scope

- Hook lifecycle hardening in `SkillSystem`:
  - Added owned handler IDs (`s_onSkillHitHandlerId`, `s_onTakeDamageHandlerId`).
  - Added lifecycle guard flag (`s_hooksInitialized`).
  - Added explicit `ShutdownHooks()` unregister path.
  - Made `InitHooks()` idempotent and duplicate-safe.
- Runtime specialization state safety:
  - `ResetTalents(skill_id)` now clears per-skill runtime transmuter state and related trigger cooldowns.
  - `ClearAllTalents()` now clears all specialization runtime maps.
  - `GetEffectiveSkillTags()` now applies only the selected active transmuter tag set, with deterministic fallback.
- UI rendering integrity:
  - Removed duplicate `BeginScissorMode()` in `UISkillTalentTree` and restored balanced scissor scope.

## Test Coverage Added/Updated

- `tests/unit/SkillBehaviorGuardTests.cpp`
  - Added idempotency guard test for repeated `InitHooks()`.
  - Added trigger mapping guard coverage for skill 2 trigger node.
  - Added transmuter-aware effective tag coverage (runtime-selected and fallback path).
- `tests/integration/SkillSystemTests.cpp`
  - Added runtime cleanup coverage for `ResetTalents()` and `ClearAllTalents()`.
- `tests/tech/UITests.cpp`
  - Added focused scissor balance regression check for `UISkillTalentTree`.

## Verification Evidence

- `build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L tech --output-on-failure` -> PASS (no tests matched label `tech`)

