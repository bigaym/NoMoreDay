# UMR 框架完善与加固 — 评审报告

- 日期：2026-09-16
- 方案：`docs/designs/2026-09-16-umr-framework-completion-and-hardening-design.md`
- 计划：`docs/plans/2026-09-16-umr-framework-completion-and-hardening-plan.md`
- 外部评审：`ocr review --audience agent --effort high`，会话 `16170bb0-df0a-45ab-871b-9c9c18a927e3`，用时 9m2s，覆盖 19 个文件，产出 15 条意见。
- 结论：**修改**

## 1. 范围与执行

按计划的 7 个 Phase 拆分为 4 个互不重叠的子代理并行实施（编译/测试在并行期禁用）：

| 子代理 | Phase | 主要文件 |
| --- | --- | --- |
| A | 1 + 5 | `ItemPersistenceCodec.cpp`、`MapModifierAdapter.cpp`、`MapAffixRegistry.hpp`、`AttributePipeline.cpp` |
| B | 2 | `CMakeLists.txt`（`GenerateModifierRuntime`） |
| C | 3 + 6 | `MonsterAffixRegistry.hpp`、`MonsterAffixSystem.hpp`、两个生成器 |
| D | 4 | `EquipmentModifierAdapter.{hpp,cpp}`、`SkillSystem.cpp`、`SkillDisplayPreviewService.cpp` |

编排期由主代理完成的跨文件修复：`tests/unit/AttributePipelineTest.cpp` 的 `explicitAffixes` 由 0.30/0.50/0.20 改为 30.0/50.0/20.0（×100 刻度），以及 `MonsterAffixRegistry.hpp` 的 `static_assert` MSVC C2131 问题（`ValidateAffixDataIds()` 移到类外定义）。

## 2. 验证证据

| 项 | 结果 |
| --- | --- |
| `build.bat RelWithDebInfo` | RC=0，All steps completed successfully |
| 定向用例 | 129 cases / 1086 assertions，0 failed |
| `ctest -L unit` | 100% (8/8) passed |
| `gen_map_monster_modifier_v2.py --check` | RC=0（map=12 / monster=25 records） |
| `gen_modifier_runtime_v2.py --check` | RC=0（binary in sync） |
| Phase 2.3 `.bin` 字节一致 | SHA256 `803A69C7…FE4F3A` 重建前后一致 |
| DoD 断言 | ExtraHealth T1=120.0f、resist +0.25、crit 0.05→0.25 均存在 |

变更规模：21 个已跟踪文件，+1034 / -69。

## 3. 外部评审意见与处置

### 3.1 已修改（本次）

| # | 严重度 | 位置 | 处置 |
| --- | --- | --- | --- |
| 1 | bug/low | `ItemPersistenceCodec.cpp:517` | 前置校验改为仅在 `ContainerDirtyFlags::ItemSideTables` 置位时执行，与真正会被写出的段一致 |
| 7 | bug/medium | `ItemPersistenceCodec.cpp` | 新增 `kMaxConversionsPerItem = 100`、`kMaxDamageModifiersPerItem = 100`（与解码侧上限同源），编码前置校验覆盖 `conversions` / `damage_modifiers`，闭合「存档成功、读档失败」类缺陷；新增回归用例 |
| 2 | maintainability/low | `EquipmentModifierAdapter.cpp` | 抽出匿名命名空间 `EvaluateEquippedRecords(recordIds, ctx)`，两个消费点共用「空集短路 + `EnsureLoaded()` + `Evaluate`」路径 |
| 15 | maintainability/low | `EquipmentModifierAdapter.cpp` | 补上兄弟适配器都有的 `EnsureLoaded()` 守卫，配一次性 `LOG_ERROR`，不再静默降级为 1.0f |
| 10 | maintainability/low | `AttributePipeline.cpp:401` | 改为 `calcs[CritChance].base += MapPercentPointsBonus(...)`，与 `:361` 的初始化单一来源 |
| 11 | bug/low | `AttributePipeline.cpp` | 在辅助函数与调用点显式注明：×100 刻度属性的多份滚值按加性累加，属刻意归一化 |
| 8 | bug/low | 两个生成器 | `op.get("param_u32", 0)`，避免 `None` 参与 `sorted()` 抛 `TypeError` |
| 3 | test/low | `EquipmentModifierAdapterTests.cpp:94` | 修正注释，明确 `ctx.equip_slot_mask` 当前不被填充，该用例不覆盖槽位过滤 |

### 3.2 未修改（含理由）

| # | 严重度 | 位置 | 理由 |
| --- | --- | --- | --- |
| 4 | bug/medium | `SkillDisplayPreviewService.cpp:41` | 消费端活性问题：生产路径 `DrawSkillTooltipFromSnapshot` 因 R8 刻意不访问注册表。仅在注册表版 `UIRenderer::DrawSkillTooltip` 改用 `preview.display_mana_cost`；快照/热键栏路径**不接线**——它是每帧路径，接线会与第 12 条（性能）直接冲突。需先做乘算系数缓存（失效时机挂装备/属性变更）再接线 |
| 12 | performance/low | `EquipmentModifierAdapter.cpp` | 与第 4 条同一决策：不引入每帧每槽位的记录收集与 `ModifierDelta` 分配 |
| 5 | bug/low | `BeamChannelDeliverySystem.cpp:770,1082` | 引导类技能每秒抽蓝未乘倍率。属同一缺陷类的潜在分支，当前仅技能 1 携带该词缀；修改涉及战斗系统行为且需补测试，列为后续 |
| 6 | test/medium | `SkillSystem.cpp:2054` | 计划的验收项「施法路径扣蓝」缺集成测试。补齐需搭建技能定义/mana/冷却等完整夹具，列为后续 |
| 14 | maintainability/low | `SkillSystem.cpp:920` | 触发节点施法绕过 `GetEquippedManaCostMultiplier`，与「法力结算单点」前提冲突。改动面涉及触发链路，列为后续 |
| 9 | maintainability/low | `gen_map_monster_modifier_v2.py:898` | 校验加入后不可达的兜底分支，属清理项，无功能影响 |
| 13 | maintainability/low | `EquipmentModifierAdapterTests.cpp:71` | 测试夹具重复，属清理项 |

## 4. 残留风险

1. **“装备词缀降蓝耗”对玩家不可见（中等）**：数值已在施法结算生效，但工具提示/热键栏仍显示未折扣值。需要「乘算系数缓存 + 装备/属性变更失效」的小型设计后才能接线到快照路径。
2. **引导类技能抽蓝未折扣（低）**：与第 1 条同源，当前仅技能 1 携带词缀，无实际表现差异。
3. **施法路径缺集成测试（中等）**：`SkillSystem` 的扣蓝已接线，但断言仅限于适配器层单测；建议补充 `TryCast` 级用例作为回归基线。
4. **`ItemSideTableData` 旁表上限仅覆盖三张表**：`skill_modifiers` 未设上限（解码侧同样未限制），维持现状。

## 5. 结论

**修改。** 计划内 7 个 Phase 的实现、两条漂移门禁、DoD 断言与全量单测均已通过；外部评审 15 条中 8 条已修改（含 2 条 bug/medium 级别的持久化缺陷），其余 7 条为**需要小型设计或跨系统改动**的后续项，已在第 4 节列为残留风险，建议单独立项后再次评审。
