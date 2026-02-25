# Validation — combat_dot_closure_20260225

## Scope

- DoT tick 闭环：Calculate + ApplyDamage。
- DoT 标签: 强制 Tag::DamageOverTime。
- Hit/DoT 语义互斥验证。

## Verification Evidence

_(To be filled during implementation)_

- DoT HP reduction test → PENDING
- DoT OnSkillHit exclusion test → PENDING
- Multi-element DoT coverage → PENDING
- `build.bat` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PENDING
