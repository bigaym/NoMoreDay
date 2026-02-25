# Validation — combat_anti_meta_layer_20260225

## Scope

- 互斥 Keystone：合同层定义互斥组，分配层执行互斥替换，UI 层展示禁用态。
- 代价词缀：节点合同新增 `cost_affix`，在 `StatsSystem` 与 `DamagePipeline` 生效，并在 tooltip 可见。
- 收益递减：同源 `More` 聚合后走 `effective = base * (1 - e^(-actual / scale))` 配置化曲线。
- 平衡门禁：补充 anti-meta 单测与集成测试，覆盖互斥、词缀与四流派差距阈值。

## Verification Evidence

- Keystone exclusion tests → PASS
  - `tests/unit/SkillBehaviorGuardTests.cpp`
    - `Keystone exclusion swaps active node even at zero free points` 子用例通过。
  - 专项复验：
    - `bin/NoMoreDayTests.exe --test-case="*[Unit] SkillBehaviorGuard - Transmuter mutex and scope policy*"` PASS

- Diminishing returns / cost affix tests → PASS
  - `tests/unit/CombatAntiMetaLayerTests.cpp`
    - 公式子线性断言通过（`ApplyDiminishingReturns(0.44) < 2 * ApplyDiminishingReturns(0.22)`）。
    - 单/双 HeavyMomentum 节点增益比断言通过（双节点增益低于线性叠加上限）。
  - 专项复验：
    - `bin/NoMoreDayTests.exe --test-case="*CombatAntiMeta*"` PASS

- Contract materialization tests → PASS
  - `tests/integration/SkillContractRegistryTests.cpp`
    - anti-meta 字段（`keystone_exclusion_group`/`cost_affix`）加载断言通过。

- Balance regression gate (4 archetypes) → PASS
  - `tests/integration/CombatAntiMetaBalanceIntegrationTests.cpp`
  - 断言：`P50 <= 15%` 且 `P90 <= 30%`（在 `ci` 运行内通过）。

- Build/Test gate → PASS
  - `build.bat` PASS
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Notes

- 初次验证曾被 `skill_contract` 漂移门禁阻断：
  - 原因：`scripts/gen_skill_contracts.py` 与 `assets/data/skill_contracts_compact.json` 尚未支持 `keystone_exclusion_group`/`cost_affix`。
  - 修复：补齐 compact 映射与生成器校验/输出逻辑后，`--check --check-idempotency --check-determinism` 全通过。
