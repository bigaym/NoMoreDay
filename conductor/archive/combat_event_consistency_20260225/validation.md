# Validation — combat_event_consistency_20260225

## Scope

- CalculateBatch 事件载荷统一使用 final_damage。

## Verification Evidence

- Event consistency tests (`tests/unit/EventConsistencyTests.cpp`) → PASS
  - Covered: single-target一致性、batch多目标 final_damage 一致性、mitigation 后值一致性
- `build.bat` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS

## Notes

- 首轮 `ctest` 出现 `HazardSystemTests` 崩溃，根因是新增测试注册的事件 handler 捕获局部引用且未解绑，导致后续用例触发悬空回调。
- 已在 `EventConsistencyTests.cpp` 中改为 RAII 生命周期管理（析构时 `Unregister`），随后重新执行完整验证链并全部通过。

## Post-Closeout Hardening (2026-02-25)

- 统一单体/批量事件载荷语义：
  - `OnTakeDamage/OnDealDamage/OnSkillHit` 增加 `reported_damage` 与 `final_applied_damage` 双字段写入。
  - `CalculateBatch` 按真实 `ApplyDamage` 回填 `final_applied_damage`。
  - 单体 `Calculate` 兼容路径默认不派发伤害事件，避免“计算路径”误触发事件。
- 修复 `OnTakeDamage` 事件消费语义：
  - `MonsterAffixSystem::OnEnemyTakeDamage` 改为使用 `evt.source` 作为受击者（defender），对齐事件合同。
- 统一事件归因：
  - 投射物/剑阵等路径在 `DamagePipeline` 中统一归因到 owner/caster，避免事件 source 漂移。

### Hardening Verification Evidence

- `.\build.bat` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
