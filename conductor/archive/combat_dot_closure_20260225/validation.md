# Validation — combat_dot_closure_20260225

## Scope

- DoT tick 闭环：Calculate + ApplyDamage。
- DoT 标签: 强制 Tag::DamageOverTime。
- Hit/DoT 语义互斥验证。

## Verification Evidence

- Code implementation:
  - `EffectSystem.cpp` DoT tick now calls `DamagePipeline::Calculate(..., Tag::DamageOverTime)` and then `CombatSystem::ApplyDamage(...)`.
  - `EffectSystem.cpp` DoT element tag now comes from `BuffEffect::tick_damage_tag` (default fallback `Tag::Poison`) instead of hardcoded poison.
  - `EffectSystem.cpp` uses `ApplyDamage(..., showVFX=false)` to avoid duplicate popup emissions (DoT popup remains emitted by `EffectSystem`).
  - `DamagePipeline.cpp` audit complete at guard points `L443/L615/L931`: `Tag::DamageOverTime` still gates `OnSkillHit` and Hit-only branches.

- Test coverage added:
  - New file: `tests/unit/DoTDamageClosureTests.cpp`
  - `[Unit] DoTDamageClosure - tick reduces HP by calculated damage`
  - `[Unit] DoTDamageClosure - DoT tick does not dispatch OnSkillHit events`
  - `[Unit] DoTDamageClosure - element tags drive DoT mitigation path` (Poison/Fire/Cold)
  - `[Unit] DoTDamageClosure - concurrent DoTs tick independently on same target`

- Verification commands:
  - `build.bat` → PASS
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
  - `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` → PASS
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
