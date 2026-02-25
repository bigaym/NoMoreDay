# Validation — combat_summon_strategy_v1_20260225

## Scope

- Summon data model v1: `SummonCombatProfile` / `SummonAIProfile` / `SummonRuntimeState`.
- `SummonComponent` 从 `name` 迁移为 `archetype_id`（去字符串热路径）。
- SummonSystem 拆分为 Lifecycle / AI / CombatBridge。
- 召唤伤害事件补齐三元归因字段：`owner/summon/source_skill`。
- 灵剑远程（ShadowCast）与近战环绕路径都通过 SummonCombatBridge。

## Verification Evidence

- System split → PASS
  - `SummonLifecycleSystem` / `SummonAISystem` / `SummonCombatBridge` 已落地并由 `SummonSystem::Update` 编排调用。
- Attribution tests → PASS
  - `tests/unit/SummonStrategyTests.cpp` 覆盖召唤近战路径 `OnDealDamage` 的 `summon_owner/summon_entity/summon_source_skill` 三元字段校验。
- Inheritance tests → PASS
  - `tests/unit/SummonStrategyTests.cpp` 覆盖 `Snapshot/Dynamic/Mixed` 三种继承模式输出校验。
- Spirit sword regression → PASS
  - `tests/unit/SummonStrategyTests.cpp` 覆盖灵剑经 `SummonCombatBridge::CastSpiritSwordShadow` 进入 ShadowCast 路径与归因透传。
- `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
  - `build.bat` PASS（最终验证轮）
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS
  - 提交前回归复验：`build.bat` PASS；`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Notes

- 首轮 sandbox 下 `build/Testing` 与 `build/CMakeCache` 写入权限受限，切换授权执行后完成验证链。
