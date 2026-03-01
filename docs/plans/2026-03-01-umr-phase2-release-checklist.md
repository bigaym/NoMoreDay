# UMR Phase-2 Release Checklist

## Scope lock

- Player-side stat modifiers in UMR: DONE
- Map/Monster stat modifiers in UMR: DONE
- Monster behavior dispatch op-only: DONE

## Build gates

- `python scripts/gen_map_monster_modifier_v2.py --check`
- `python scripts/check_monster_behavior_dispatch.py`
- `python scripts/gen_modifier_runtime_v2.py --check`

All must pass in `build.bat` precheck.

## Regression gates

- `./bin/NoMoreDayTests.exe --test-case="*ModifierEvaluator*"`
- `./bin/NoMoreDayTests.exe --test-case="*MonsterModifierAdapter*"`
- `./bin/NoMoreDayTests.exe --test-case="*MonsterAffix*"`
- `./bin/NoMoreDayTests.exe --test-case="*AttributePipeline*"`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## Runtime consistency

- Behavior-op coverage contract passes (implemented behavior affixes emit behavior ops; behavior-less affixes do not).
- Monster dispatch coverage contract passes (expected behavior ops appear in context enum, runtime mapping, and system dispatch).
- Duplicate dispatch protections active for per-tick update behaviors and death-event edge cases.

## Known defer list

- Phase-3 已完成：legacy fallback 分支已移除，行为分发统一为 op-only。
- 后续仅保留 opcode contract 与分发覆盖门禁，不再引入回退路径。
