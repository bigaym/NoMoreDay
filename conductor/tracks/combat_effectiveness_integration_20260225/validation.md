# Validation — combat_effectiveness_integration_20260225

## Scope

- `added_damage_effectiveness` 接入 DamagePipeline 主公式。
- `trigger.effectiveness` 注入触发上下文并接入主公式。

## Verification Evidence

_(To be filled during implementation)_

- AddedEff coefficient tests → PENDING
- TriggerEff coefficient tests → PENDING
- Combined effectiveness test → PENDING
- `build.bat` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PENDING
