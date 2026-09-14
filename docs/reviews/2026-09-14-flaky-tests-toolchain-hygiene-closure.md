# 方案收口报告：Flaky 测试与工具链卫生 + 持续场组件迁移

- 日期：2026-09-14
- 对应方案：
  - `docs/plans/2026-09-14-flaky-tests-and-toolchain-hygiene-plan.md`
  - `docs/plans/2026-09-14-persistent-field-component-migration-plan.md`
- 上游依据：`docs/reviews/2026-09-14-skill-abstraction-a02-review.md`（R2/R3/R4、Low-2）
- 结论：`提交`

## 1. 编译警告基线（R4 销项）

实测命令（`build.bat` 会吞掉编译告警，故改用直接构建取样）：

```powershell
cmake --build build --config RelWithDebInfo -- /m:7 *> <log>
```

| 指标 | 目标 | 实测 |
|---|---|---|
| C4834 | 0 | **0** |
| C4002 | 0 | **0** |
| `error C` | 0 | **0** |
| 其余 `warning C` | — | 1（`MonsterHealthBarController.cpp:282` C4477，与本基线无关，未处理） |

- 方案原扫描（`rg ... src --glob *.cpp`）**漏扫** tests 目录与 `src/game/application/ui/`，
  实测补清 17 处：`SkillTreeController.cpp:34/50/60/97/105`（`SetNodeVisible` 丢弃返回值）、
  `tests/functional/SwordArrayNodes.cpp:40`（`EnsureLoaded`），以及 11 处多参 `CAPTURE`：
  `tests/unit/SkillBakerFlagConsumerTests.cpp:370/402/479/494/517`、
  `tests/unit/SpecStateMappingTests.cpp:190/215/220/268/293`、
  `tests/unit/SpecStateTableTests.cpp:125`。A-02 Review R4 点名的 `SpecStateMappingTests` /
  `SpecStateTableTests` 已随之清零。
- 53 处 `get_or_emplace` 裸丢弃点按设计 §2.2 全部加 `(void)`，零行为变更。

## 2. 工具链配置（R3 销项）

- `.clang-format:78` → `Standard: c++20`；clang-format 22.1.3 `--style=file --dry-run` 无 `unknown enumerated scalar`。

## 3. 技能 11/12 兜底字面量（Low-2 / R2 销项）

- T4.0：天剑 `kImpactRadiusFallback/kFieldRadiusFallback/kFieldDurationFallback/kTierDamageBonusFallback/kTierRadiusBonusFallback`
  与血海 `kFieldDurationDefault/kFieldRadiusDefault/kFieldTickDefault/kLeechRatioDefault/kBloodthirstDamageBonusDefault`
  提升为行为类公开 `static constexpr`（`HeavenlySwordDescent.hpp:40-44`、`BloodSea.hpp:38-42`），数值不变。
- T4.1/T4.2：新增 `[Functional] ... DoCast constexpr fallback constants` 用例
  （`tests/functional/HeavenlySwordDescentNodes.cpp:321-345`、`tests/functional/BloodSeaNodes.cpp:565-598`），21 断言通过。
  血海 duration/radius 因 `effective_consumed = max(1, consumed)` 恒 ≥1，退化为「常量固化 + 叠加系数」双重锚定。

## 4. 稳定性（DoD 3 销项）

- 新增测试夹具隔离与确定性加固见 T3.1/T3.2。
- 调试追加修复（全量套件暴露的遗留 flake）：
  - `tests/integration/SkillSystemTests.cpp` BladeWard 470 子用例：显式 `accuracy = 1.0f`，消除默认 0.97 带来的 3% 有效闪避（测试夹具隔离）。
  - `src/game/systems/skill/ProjectileSystem.cpp:511-515`：拦截判定加 `chance >= 1.0f ||`，修复 `GetRandomValue(0,1000)` 闭区间在 100% 概率下的漏判（生产逻辑，与 `DamagePipeline` 必中边缘保护一致）。
- 结果：`bin\NoMoreDayTests.exe "--test-case-exclude=*Performance*,*GPU-Diagnostic*"` 连续 5 轮
  `100% tests passed, 0 failed`；BladeWard 412/470 单跑各 120/120 通过。

## 5. 持续场组件迁移

- 新头 `src/game/systems/skill/components/PersistentFieldComponents.hpp`；`PersistentFieldSystem` 编排更新；
  `DestroyOwnedPersistentFields` 共用；`BladeMasteryService` 与 UI/HUD 表现层解耦（render 层保留直读）。
- `python scripts/check_module_boundaries.py` PASS（ledger/observed edges 0/0）。

## 6. 未验证项与风险

1. `ProjectileSystem.cpp` 边界修复使 `chance==1.0` 恒拦截（修复前约 1/1001 漏判），为确定性正向修复。
2. 残留 `MonsterHealthBarController.cpp:282` C4477（`snprintf %s` 与 `string_view`）不在 DoD 范围，建议后续单独处理。
3. 方案任务 T5.2 的 `ctest -R "SkillBehaviorGuard|SingleGpuTimer"` 在本仓匹配为空（CTest 仅注册分组式测试）；已用 doctest 过滤器等价验证。
