# Validation — combat_summon_entry_fix_20260225

## Scope

- 基于当前代码确认召唤路径已接入 Pipeline。
- 收敛残留硬编码伤害语义（orbit `25.0f`、execute `hp->max * 0.1f`）。

## Verification Evidence

Date: `2026-02-25`

### Baseline Audit (Current Code)

- Summon melee orbit routing → PASS
  - `SummonAISystem` 通过 `SummonCombatBridge::ApplyMeleeOrbitContact(...)` 进入战斗路径。
  - `SummonCombatBridge` 内部先 `DamagePipeline::Calculate`，再 `CombatSystem::ApplyDamage` 落地。
- Sword execute routing → PASS
  - `SwordArray` execute 分支先 `DamagePipeline::Calculate`，再 `CombatSystem::ApplyDamage`。
- Residual hardcoded damage semantics → CLOSED
  - Orbit 基值改为 `SummonCombatProfile::melee_orbit_base_damage`。
  - Execute 比例改为 `SwordArrayComponent::execute_damage_max_health_ratio`。

### Final Gate

- Summon convergence tests (orbit/execute damage source) → PASS
  - `tests/unit/SummonStrategyTests.cpp` 新增 profile 驱动基值测试：
    - `[Unit] SummonStrategy - melee orbit uses profile-configured base damage`
  - `tests/unit/SkillBehaviorGuardTests.cpp` 新增 execute 比例合同断言。
- grep audit for inline constants (`25.0f`, `hp->max * 0.1f`) → PASS
  - Runtime path `SummonAISystem.cpp` 无 `25.0f` 参数直传。
  - Runtime path `SwordArray.cpp` 无 `hp->max * 0.1f` 内联表达式。
- `build.bat` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
- 提交前复验（同日）→ PASS
  - `build.bat` PASS
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Notes

- 本 Track 已完成实现与验证，进入归档与提交阶段。
