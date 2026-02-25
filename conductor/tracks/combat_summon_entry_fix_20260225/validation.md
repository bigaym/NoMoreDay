# Validation — combat_summon_entry_fix_20260225

## Scope

- SummonSystem 直写 ApplyDamage 路由至 DamagePipeline。
- SwordArray 直写 ApplyDamage 路由至 DamagePipeline。

## Verification Evidence

_(To be filled during implementation)_

- Summon Pipeline routing tests → PENDING
- grep audit → PENDING
- DPS comparison data → PENDING
- `build.bat` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PENDING
