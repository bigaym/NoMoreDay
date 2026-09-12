# 伤害管线重构后续：死代码与兼容遗留清理报告

- 日期：2026-09-12
- 范围：提交 `4e7d2ac2`（伤害管线重建 + combat_v2 删除）之后的二次死代码/遗留审计
- 结论：清理完成，构建与测试门禁通过；未发现行为回归。

## 1. 清理清单

### 源码死代码（零引用，rg 全仓校验）

| 符号 | 位置 | 说明 |
| --- | --- | --- |
| `DefenseSnapshot` | `src/game/systems/combat/damage/DamageSnapshot.hpp` | 设计中的预留防御快照接口，从未接线；删除。 |
| `damage::DamageTypeToResistIndex` | `src/game/systems/combat/damage/DamageTypes.hpp` | P4 为“反向场景”添加，实际无调用方。 |
| `Constants::Combat::Conversion` 命名空间 + `MAX_CONVERSION_DEPTH` | `src/game/systems/combat/CombatConstants.hpp` | D2 明确转换无级联、与顺序无关，深度上限无意义；此前 P4-review-blockers 建议保留，本次按零引用删除。 |
| `Constants::Combat::System::CRIT_DAMAGE_FALLBACK` | `src/game/systems/combat/CombatConstants.hpp` | 零引用。 |
| `damage::EvaluateTargetConditions` | `src/game/systems/combat/damage/DamageConditions.hpp/.cpp` | 仅为 `EvaluateTargetConditionState().mask` 的转发，唯一调用方是测试。 |
| `scripts/combat_v2/__pycache__/` 忽略规则 | `.gitignore` | 目录已随 combat_v2 删除。 |
| 头文件残留注释 `// Added` | `src/game/systems/combat/DamagePipeline.cpp` | 重构过程中的临时标记。 |

### 测试与测试基建

- 删除 `tests/unit/CombatCoreParityHarnessTests.cpp`、`tests/performance/CombatCorePerfBaselineTests.cpp`（commit `7aaf796f` 的 combat-vNext 门禁残留）：两者只校验 `docs/reports/combat-core-vnext/baseline/` 下 JSON/硬件档案的存在与字段，其中 perf 场景 `[Performance] CombatCorePerfBaseline - SingleTargetHit P95` 在测试集中已不存在。
- 删除对应 ctest 条目 `nmd.tests.combat.parity.unit`、`nmd.tests.combat.perf.baseline`（`tests/CMakeLists.txt`）。
- 删除仅被上述两个文件引用的 `tests/TestJsonArtifact.hpp`。
- 更新过时注释（不改断言逻辑）：`tests/unit/SkillSpecializationBakerTests.cpp`、`tests/unit/MonsterAffixTests.cpp`、`tests/functional/BladeBoomerangElementPathTests.cpp`、`tests/unit/DamageConversionTypesTests.cpp`、`tests/functional/DamagePipelineConversionTest.cpp` 中残留的 CandidateOnly 桩 / CombatV2 / “旧结算路径” / `IsValidConversion` 描述，改为描述当前语义（`is_simulation` 仅跳过防御判定与拦截器）。
- `tests/functional/DamagePipelineP3Tests.cpp` 改用 `EvaluateTargetConditionState(...).mask`，并移除因转发函数删除而变成同义反复的断言。

## 2. 明确保留（非遗留）

- `DamagePipeline::Calculate` 8 参兼容重载：设计 §6.1 要求 `Calculate/Execute` 签名保持不变，仅测试使用但属对外契约。
- 已废弃的 `CalculateBatch`（测试/基准专用，注释与 `[[deprecated]]` 已标注）、`ResolveDamageBatch`（生产批量结算按计划推迟到 P5，当前仅测试调用）、`BatchKernelStats` 系列测试观测接口。
- `is_simulation`（跳过防御判定/拦截器，仍有生产与测试语义）、`resolved_element_tags`、`payload_context`、`skip_mitigation` 等字段均有生产使用。
- `tests/unit/DamagePipelineUnifiedEntryTests.cpp` 的 `ComputeLegacyDamageReference` 是现行回归对照预言，保留。
- `docs/` 内历史文档中的 combat_v2 记载按 P4 先例保留。

## 3. 验证证据

| 项目 | 命令 | 结果 |
| --- | --- | --- |
| 构建 | `build.bat`（RelWithDebInfo） | exit 0，含模块边界与遗留标记门禁 |
| 目标用例 | `NoMoreDayTests --test-case=*DamagePipeline* --test-case=*DamageConversion* --test-case=*MonsterAffix* --test-case=*BladeBoomerang*` | 30/30 用例、375/375 断言通过 |
| CI 非性能门禁 | `ctest -C RelWithDebInfo -L ci` | 通过（1449 用例） |
| 集成门禁 | `ctest -C RelWithDebInfo -L integration` | 6/6 通过 |

日志：`post-cleanup-build.log`、`post-cleanup-targeted.log`、`post-cleanup-ci.log`、`post-cleanup-integration.log`。

### 已知预存不稳定（与本次清理无关）

- `[Functional] Skill - Seven Star Slash Branch Behaviors` D 分支：`CHECK(damageEvents < 7)` 在终结斩暴击触发 `ExplodeScars`（额外 skill_id=10 事件）时可超过上界。测试上界与 `ExplodeScars` 均由 `55fd0fa1` 引入，早于本次重构；单独运行必过，整套运行约 1/4 概率失败（`post-cleanup-ci-run1-flake.log`）。建议后续固定暴击或在断言中排除裂痕爆炸事件。
- `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235`：GPU 查询状态偶发非 `Valid`（`post-cleanup-ci-run4-gpu-flake.log`），环境相关。

## 4. 后续事项

- 生产 AoE 批量结算迁移 `ResolveDamageBatch`（P5）。
- `DamageConditions.cpp` 中 `id.find("Slow")` 文本匹配的数据化治理（P4 记录 N1）。
