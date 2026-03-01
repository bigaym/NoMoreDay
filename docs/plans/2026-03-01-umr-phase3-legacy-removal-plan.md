# UMR Phase-3 Legacy Path Removal Plan

## Objective

全面删除怪物行为分发中的旧回退路径（`HasAffix` / 组件兜底分支），统一仅通过 UMR 行为 Op 分发执行。

## Scope

1. `MonsterAffixSystem` 中 Update / OnHit / OnDeath 分发分支。
2. 生成链路与运行时合同保持严格一致：
   - `gen_map_monster_modifier_v2.py`
   - `check_monster_behavior_dispatch.py`
3. 测试覆盖：`MonsterAffixTests`、`MonsterModifierAdapterTests`、`ModifierEvaluatorTests`。

## Migration Rules

1. 分发条件从 `op-first + fallback` 改为 `op-only`。
2. 保留现有处理器函数本体和公式，不做玩法重平衡。
3. 若某行为词缀未生成行为 Op，构建必须在 precheck 阶段失败。
4. 不允许在 `MonsterAffixSystem` 引入新的 `HasAffix` 分发旁路。

## Execution Steps

1. 删除 Update 分发回退条件（Molten/Teleporter/Frozen/Storm/Berserker/VoidZone/SoulEater/Suppressor/SoulLink/ManaSiphon/Shielding/Vortex/Waller）。
2. 删除 OnHit/OnTakeDamage 回退条件（Vampiric/Entangler/Nullifier/Void/MirrorImage/StormStrider）。
3. 删除 OnDeath 回退条件（SoulEater/Avenger/Toxic）。
4. 更新测试断言，确保行为依赖 Op 集合，且不存在回退依赖。
5. 运行全量回归并提交。

## Verification Gates

1. `python scripts/gen_map_monster_modifier_v2.py --check`
2. `python scripts/check_monster_behavior_dispatch.py`
3. `./build.bat`
4. `./bin/NoMoreDayTests.exe --test-case="*MonsterAffix*"`
5. `./bin/NoMoreDayTests.exe --test-case="*MonsterModifierAdapter*"`
6. `./bin/NoMoreDayTests.exe --test-case="*ModifierEvaluator*"`
7. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
8. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
