# Development Log

Append-only. Each completed task / runnable state gets an entry.

## 2026-09-13 — A1 技能抽象与 B2 清理（技能 1~9）

- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（A1-0..A1-7）
- 设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（Rev.2）
- 结果：A1 全部完成，两轮审查（完成性/潜在问题 → 死代码/兼容回退）已过，结论 `提交`（工作区未提交）。

关键产物：
- 新 foundation helper `src/game/foundation/components/SkillPointAccess.hpp`（`skills::ReadPoints/HasNode/GetMech`）；全仓 `allocated_points.(find|contains|count)` 直查归零（仅 helper 自身）。
- `SkillMechanicsRegistry` 加载期 schema 校验 + 运行期零分配查表；新增 `tests/unit/SkillMechanicsRegistryTests.cpp`。
- 删除死字段/死组件：`primary_archetype`、`secondary_archetype`、`SkillSnapshot::payload_context`、`ReactiveWardComponent`、`ChannelingComponent`、`OrbitingSentinelComponent::interception_chance`、`BladeFormationNodes::ColossusRend`。
- `BeamChannelComponent` 承接引导交付语义（`channel_timer`/`conversion_tag`/`bonus_crit_chance`/`bonus_armor_pen`）；技能7 保活单源。
- 伤害管线：`ResolveSkill4Counter` 唯一实现（3 站点）+ `SourceAttribution` 归因 + `BuffKind` 化字符串比较。
- 逐技能效果层：`AilmentRegistry` 契约化 `FlowingThrust`；`BladeFormation` `SpatialGrid` 查询 + `BuffId` 化；`BuffIds.hpp` 增 `SpiritCorrosion{Fire,Lightning}`。
- 新测试：`SkillBakerFlagConsumerTests.cpp`、`SpecStateMappingTests.cpp`、`SkillPrerequisiteRequiredPointsTests.cpp` 扩充；新文档 `docs/designs/2026-09-13-skill-baker-consumer-map.md`。
- 审查报告：`docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-correctness.md`、`-r1-completeness.md`、`-r1-fixes.md`、`-r2-deadcode.md`。
- 性能基线：`docs/performance/2026-09-13-skill-abstraction-baseline.md`（未验证，待补采）。

验证：`build.bat` 0 warning / 0 error；`ctest -L "ci|skill|unit|combat"` 11/11；`ctest -LE "performance|gpu-hardware"` 17/17；`gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS。

下一步：A2（技能 10/11/12 迁移 + 持久场原型收敛），随后两轮审查。

## 2026-09-13 — A2 技能 10/11/12 迁移与持久场原型收敛

- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（A2-1..A2-4）
- 设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（§5.3 A2、§11）
- 结果：A2-1/A2-2/A2-3 完成，A2-4 保留现状仅记录；两轮审查（R1 完成性/正确性 → R2 死代码/兼容回退）已过，结论 `提交`，报告 `docs/reviews/2026-09-13-skill-abstraction-a2-review.md`。

关键产物：
- A2-1 技能10 `SevenStarSlashSpecState`/`ResolveSpecState` 迁入 `SevenStarSlashShared.hpp`（`NoMoreDay::skills`），改为「node id → 成员指针」双静态表；`skill_mechanics.json` 增顶层 `"10"`（13 节点/17 键）。
- A2-2 技能11 `HeavenlySwordDescent` 迁移；抽取 `PersistentFieldHeader` 公共头（`SkillDefs.hpp:1256-1265`）；绝影共噬 1217 落地（窗口门控 + 数据驱动）。
- A2-3 技能12 `BloodSea` 迁移；`AreaFieldDeliverySystem.cpp:48` 改 `any_of<PersistentFieldTag>`，去 HeavenlySword/BloodSea 类型特判；`BloodSeaFieldComponent` 以公共头嵌套组合（`SkillDefs.hpp:1316`）。
- A2-4 B2-23 保留 deprecated `CalculateBatch`/`ResolveDamageBatch` 现状，无生产代码改动。

关键裁决：
- **1217 路径A（忠于设计）**：`BloodSea::DoCast` 在 1217 点亮且 `IsDeathSealActive` 成立时，向新建场写入 2s 增伤/增疗倍率（`GetMech(12,1217,empower_*)`）；联动脉冲机制归属节点 1207 无间血狱，与 1217 解耦。
- **持久场公共头成员嵌套**：`HeavenlySwordFieldComponent`/`BloodSeaFieldComponent` 以 `PersistentFieldHeader header` 成员组合专有字段（禁止继承，POD/standard-layout），不做全量字段合并。
- **DoD#4 部分达成残余**：仅技能 10/11/12 交付完整 POD CastSpec 信封；技能 1-9 经 A1 `skills::ReadPoints/HasNode/GetMech` 就地读点，未建每技能 SpecState；`--gen-specstate` 生成器未实现。

验证基线：`build.bat` 0 warning / 0 error；`ctest -L "ci|skill|unit|combat"` 11/11；`ctest -LE "performance|gpu-hardware"` 17/17；`gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS；`sync_skill_node_icon_ids.py --check` PASS；legacy gate（`rg ChannelingComponent src/` = 0）PASS。

未决项：
- Tracy 性能基线未验证（沿用 A1-0 标注「未验证，待补采」）。
- 禁疗×增疗交互：逆脉/免死窗口内 `ApplyHealing` 禁疗，默认 3s 窗口下 1217 治疗增益不产生实际治疗量（伤害 +20% 不受影响）。
- ProcEngine `DeathSeal` 口径统一为 `IsDeathSealActive`，额外排除 `ending` 终结帧，待设计确认时序。
- 技能 1-9 信封化（POD SpecState）与 `--gen-specstate` 生成器属后续范围。

R2 清理（死代码/兼容回退）：
- 删除技能11/12 `skill_mechanics.json` 节点0 被 `skills.json` 遮蔽的 10 个 `*_default` 键及绑定（HSD 机制表 25→20、BSC 45→40），默认值单源自 `skills.json`（`GetParam`）+ 结构体 NSDMI 字面量。
- 删除死字段 `SevenStarSlashSpecState::dipperReturnPoints`（生产只写）与 `PersistentFieldHeader::cast_id`（归因单源在 `AreaFieldComponent::cast_id`）。
- `PersistentFieldHeader::linked_hit_count` 保留（测试/调试可观测计数）并加注释。
- 残余登记：`Resolve*` 模板 helper 收敛（DoD#9 观测项）、技能10 `GetMech`/`getModifier` 双源与 `slash_count` 无消费者。
