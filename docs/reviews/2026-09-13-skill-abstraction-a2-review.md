# A2 技能 10/11/12 迁移与持久场原型收敛 — 审查报告

## 审查目标

对 NoMoreDay 技能抽象与 B2 清理计划 A2 批次（A2-1 技能10、A2-2 技能11 + 绝影共噬、A2-3 技能12 + 持久场收敛、A2-4 B2-23）的实施结果进行审查，判定是否满足设计 DoD 与计划出口标准，可否 `提交`。

## 结论

`提交`（工作区修改，未提交；由用户决定提交时机）。

## 审查轮次

| 轮次 | 关注点 | 审查者 | 轮次结论 |
|---|---|---|---|
| R1 | 完成性 + 正确性/潜在问题 | cpp-reviewer（正确性）、Code Reviewer（完成性） | 修改（无阻断/高风险） |
| R2 | 死代码 / 兼容回退 / 双数据源清理 | cpp-reviewer | 修改（均为中低，已清理） |

两轮发现项均已处置或据实登记为残余；清理后复验全绿，故最终结论 `提交`。

## 输入

- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（A2-1..A2-4）
- 设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（§4.4 / §5.3 A2 / §9 / §11）
- Backlog：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（B2-23 / B2-22 / A-03）
- 审查流程：`docs/workflows/review.md`

## 变更文件边界（A2 相关）

源码/数据：
- `src/game/foundation/components/SkillDefs.hpp`（`PersistentFieldHeader` / `PersistentFieldTag` / `HeavenlySwordFieldComponent` / `BloodSeaFieldComponent`）
- `src/game/systems/skill/behaviors/{SevenStarSlash.hpp,SevenStarSlash.cpp,SevenStarSlashShared.hpp,HeavenlySwordDescent.hpp,HeavenlySwordDescent.cpp,BloodSea.hpp,BloodSea.cpp}`
- `src/game/systems/skill/{AreaFieldDeliverySystem.cpp,ProcEngine.cpp,BladeMasteryService.cpp,SkillSystem.cpp}`
- `src/game/application/ui/{PlayerHUD.cpp,GameUiSnapshotBuilder.cpp}`、`src/game/application/render/GameplayRenderAdapter.cpp`
- `assets/data/skill_mechanics.json`

测试/文档：
- `tests/functional/{SevenStarSlashNodes,HeavenlySwordDescentNodes,BloodSeaNodes}.cpp`、`tests/unit/{SpecStateMappingTests,SkillBehaviorGuardTests,SkillMechanicsRegistryTests,TriggerRuleTests}.cpp`、`tests/integration/{SkillKeyNodeMatrixIntegrationTests,SkillSystemTests,GameplaySystems,SkillContractRegistryTests}.cpp`、`tests/tech/UITests.cpp`
- `docs/plans/2026-09-13-*.md`、`docs/designs/2026-09-13-*.md`、`docs/dev-log.md`、本报告

（A1 相关变更与本报告边界之外的工作区改动不在此列。）

## 范围对齐

- **A2-1**：技能10 表驱动信封 + `skill_mechanics.json` `"10"` + 注册核对 —— 已落实。
- **A2-2**：技能11 信封迁移、`PersistentFieldHeader` 抽取、绝影共噬（窗口门控 + 数据驱动）、`"11"` key —— 已落实。
- **A2-3**：技能12 信封迁移、`AreaFieldDeliverySystem` 去类型特判（`any_of<PersistentFieldTag>`）、`"12"` key —— 已落实。
- **A2-4**：B2-23 保留现状仅记录 —— 已落实（无生产改动）。
- **设计 §5.3 A2 DoD**：技能 10/11/12 统一信封；`AreaFieldDeliverySystem.cpp` 无 HeavenlySword/BloodSea 类型特判 —— 已达成。
- **批次 DoD#4 部分达成**：仅技能 10/11/12 交付完整 POD CastSpec 信封；技能 1-9 尚未信封化（已登记为残余，见下）。

## 质量与风险评估

- **行为等价**：A2-ST 字段嵌套/改名逐行等价（`header` NSDMI 还原各组件原默认）；技能11/12 迁移的 `GetMech` 代码默认值 = 旧字面量 = `skill_mechanics.json` 键值，三方一致。
- **热路径**：`GetMech` 走 `const char*` 绑定，零分配；`PersistentFieldHeader` 满足 `standard_layout`/`trivially_destructible`。
- **单一来源**：R2 清除了技能11/12 节点0 `*_default` 与 `skills.json` 参数的双源；R2 删除死字段 `dipperReturnPoints`、`PersistentFieldHeader::cast_id`。
- **兼容回退**：`SkillSystem.cpp` 的 `bakedProfile ? flags : HasNode(...)`、HSD/BSC 的 `GetParam` 回退经确认为**仍可达**，保留。

## 发现项

### Round 1 — 完成性 / 正确性

| # | severity | 位置 | 问题 | 处置 | 状态 |
|---|---|---|---|---|---|
| R1-1 | 中 | `BloodSea.cpp:351-358` / `:192-196` | 默认 3s 逆脉窗口下 1217 的 +20% 治疗增益被同窗禁疗抵消 | 记录交互 + 增补测试（默认 3s / sub-2s 两态）；`ApplyHealing` 禁疗分支加注释 | 已处置（据实记录，设计待确认） |
| R1-2 | 中 | `ProcEngine.cpp:64-65` | DeathSeal 口径改用 `IsDeathSealActive` 新增 `!ending`，无测试且与计划 T4.2 表述矛盾 | 新增 `tests/unit/TriggerRuleTests.cpp:498-560` 覆盖 `ending` 帧；计划 `:307` 追加订正 | 已处置 |
| R1-3 | 低 | `BloodSea.cpp:448-455` | 1211 爆发治疗未纳入增疗乘数 | 乘 `(1 + shared_devour_heal_mult)`，窗口外等价 | 已处置 |
| R1-4 | 低 | `SevenStarSlash.cpp:440,549,...` | 技能10 系数 `GetMech`/`getModifier` 双源 | 登记边界（计划 §5.1、设计 §11.12） | 已记录（后续统一） |
| R1-5 | 低 | `SkillMechanicsRegistry.cpp:16` | 加载期 schema 不校验具体键名拼写 | 登记限制（计划 §5.1） | 已记录 |
| R1-6 | 低 | `plan:455,467,485,486` | 复选框未据实勾选 | 补齐勾选并附证据 | 已处置 |
| R1-C1 | 中 | 同上 | 完成性：勾选不一致 + ProcEngine/禁疗×增疗未记录 + 工件缺失 | 已补文档、dev-log、本报告 | 已处置 |
| R1-C2 | 低 | `docs/designs/...consumer-map.md:13` | 未声明技能 11/12 不经 Baker switch | 补覆盖范围说明 | 已处置 |

### Round 2 — 死代码 / 兼容回退 / 双源

| # | severity | 位置 | 问题 | 处置 | 状态 |
|---|---|---|---|---|---|
| R2-1 | 中 | `skill_mechanics.json` 技能11/12 节点0；`skills.json` params | 节点0 `*_default` 键被 `skills.json` 同名参数永久遮蔽（双源） | 删 10 个 mech 键 + 绑定（HSD 25→20、BSC 45→40）；默认单源自 `skills.json`（`GetParam`）+ NSDMI 字面量；加注释 | 已处置 |
| R2-2 | 中 | `SevenStarSlashShared.hpp:307,356-357` | 死字段 `dipperReturnPoints`（生产只写） | 删除字段 + 绑定；测试尺寸 18→17 | 已处置 |
| R2-3 | 中 | `SkillDefs.hpp:1258` | `PersistentFieldHeader::cast_id` 生产只写 | 删除字段 + 2 写入 + 1 测试断言；归因单源保留在 `AreaFieldComponent::cast_id` | 已处置 |
| R2-4 | 低 | `SkillDefs.hpp:1262` | `linked_hit_count` 生产只写、仅测试读取 | 保留并加注释（测试/调试可观测，无生产消费者） | 已裁决保留 |
| R2-5 | 低 | `SevenStarSlashShared.hpp` / `HeavenlySwordDescent.hpp` / `BloodSea.hpp` | `Resolve*` 绑定填充同型重复（DoD#9 观测项） | 登记为残余（设计 §11.11） | 已记录 |
| R2-6 | 低 | `assets/data/skills.json` 技能10 `slash_count` | 无消费者（非 A2 引入） | 登记（设计 §11.12） | 已记录 |

## 最佳实践建议

1. `Resolve*` 绑定填充可抽 `FillSpecFromBindings<Spec,PointB,FlagB>` 模板 helper 统一（并统一技能10 缺 clamp 的差异），作为后续 DoD#9 收敛项。
2. 技能10 的 `GetMech` / `getModifier` 双源应择一冻结，避免节点数据与机制数据漂移。
3. 加载期 schema 可扩展为按技能声明必需键集合，将键名拼写错误从静默回退升级为加载失败。

## 剩余风险

- **Tracy 性能基线未验证**：无交互运行环境，`docs/performance/2026-09-13-skill-abstraction-baseline.md` 标注「未验证，待补采」（不计入本批出口阻断）。
- **禁疗×增疗交互**：默认逆脉窗口下 1217 治疗增益不体现实际治疗量，需设计确认是否符合意图（设计 §11.9）。
- **ProcEngine `ending` 帧口径**：需设计确认终结帧不应触发 DeathSeal 规则（设计 §11.8）。
- **DoD#4 残余**：技能 1-9 尚未信封化；`--gen-specstate` 生成器未实现（设计 §11.10）。
- **既有环境/顺序 flaky**：`tests/integration/SkillSystemTests.cpp:1269`（BladeWard 470 计数器，全局 Hook/singleton 污染，A1 已记录）、`SingleGpuTimerOwnerRegressionTest.cpp:235`（GPU timer 环境 flaky）；隔离与复跑均通过，与本批改动无关。

## 验证证据（清理后复验）

- `build.bat`：**0 warning / 0 error**。
- `ctest --test-dir build -C RelWithDebInfo -L "ci|skill|unit|combat"`：**11/11**（首跑遇上述环境 flaky，隔离确认后整轮复跑通过）。
- `ctest --test-dir build -C RelWithDebInfo -LE "performance|gpu-hardware"`：**17/17**。
- `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`：PASS。
- `python scripts/sync_skill_node_icon_ids.py --check`：0 diff。
- `python scripts/check_legacy_reintroduction.py`：PASS（基线 total 不增长）。
- 静态：`rg -n "HeavenlySword|BloodSea" src/game/systems/skill/AreaFieldDeliverySystem.cpp` = 0；`rg -n "allocated_points\.(find|contains|count)" src/` 仅 helper；`behaviors/` + `DamageConditions.cpp` 零行为字符串比较；`rg dipperReturnPoints` / `rg "header\.cast_id"` = 0。

## 下一步

- 由用户决定提交时机与提交粒度（工作区大改动，未提交）。
- 后续范围（不在本批）：技能 1-9 信封化与 `--gen-specstate` 生成器；技能10 双源统一；`Resolve*` 模板 helper 收敛；Tracy 基线补采。
- 设计侧待确认：禁疗×增疗交互、ProcEngine `ending` 帧口径。
