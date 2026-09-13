# 技能专精跟进包（skill1~12）实施审查报告

## 审查目标

技能专精与伤害管线剩余非性能待办的实施包：设计 `D1~D12` 裁决与 `RD-01~RD-18` 状态收口，按 `docs/plans/2026-09-13-skill-followup-plan.md` 的 Wave A（契约/生成器）、B（GDD+数据+实现）、C（行为缺口）、D（测试与文档）实施。本报告覆盖首审与修复后复审。

## 结论

`提交`

## 审查轮次

第 1 轮（首审，结果为 `修改`）+ 第 2 轮（修复复审，结论 `提交`）。

## 输入

- 设计：`docs/designs/2026-09-13-skill-followup-design.md`
- 实施计划：`docs/plans/2026-09-13-skill-followup-plan.md`；backlog：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`
- 审查标准：`docs/workflows/review.md`、`conductor/code_standard.md`、`conductor/tech-stack.md`、`AGENTS.md`
- 验证证据：`build.bat`（RelWithDebInfo）EXIT 0 / 0 警告；`ctest -L ci`、`-L integration`（6/6）、`-L unit`（8/8）全绿；`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`、`sync_skill_node_icon_ids.py --check`、`gen_skill_mechanics_schema.py --check` PASS。

## 变更文件边界

`git status --short` 共 52 项（39 修改 + 13 新增），`git diff --stat` = 39 files changed, 1381 insertions(+), 178 deletions(-)（不含 13 个新增未跟踪文件）。边界分类：

- 脚本：`scripts/gen_skill_contracts.py`、`scripts/gen_skill_mechanics_schema.py`（新）
- 数据：`assets/data/skill_mechanics.json`、`skill_mechanics_schema.json`（新）、`skills.json`、`skill_contracts_compact.json`、`mastery_skill_trees.json`
- 核心：`foundation/data/BuffIds.hpp`、`SkillMechanicsRegistry.hpp/.cpp`；`foundation/components/Buff.hpp`、`Projectile.hpp`、`SkillDefs.hpp`
- 战斗/技能：`combat/AilmentEngine.cpp`、`DamagePipeline.cpp`、`damage/DamageInterceptors.hpp/.cpp`、`skill/ProcEngine.cpp`、`ProjectileSystem.cpp`、`BeamChannelDeliverySystem.cpp`、`SkillSpecializationBaker.cpp`、`SkillSystem.cpp`、`SummonAISystem.cpp`、`behaviors/{BladeBoomerang,BladeFormation,BladeWard,FlowingThrust,InfiniteBlades,RendingWave,SevenStarSlash}.cpp`、`behaviors/BladeWardRuntime.hpp`（新）
- 测试：新增 `Skill1FollowupTests.cpp`(integration)、`Skill2/3/4/5/10FollowupTests.cpp`(functional)、`Skill8RemoveByKindTests.cpp`、`SkillMechanicsKeySchemaTests.cpp`(unit)；修改 `TestCommon.hpp`、`SkillKeyNodeMatrixTestHelpers.hpp`、`fixtures/skill_specialization_keynodes.json`、`BladeFormationNodes.cpp`、`FlowingThrustNodes.cpp`、`SkillBehaviors.cpp`、`SkillSystemTests.cpp`
- 文档：`设计文档/职业设计草案_剑修.md`、backlog、`p1-report.md`、`2026-09-07-skill1-specialization-nodes-review.md`、本包设计/计划

未发现禁区文件、生成物或工程外文件被改动（`bin/`、`build/` 不在变更集）。

## 范围对齐

Wave A/B/C/D 与计划条目一一对应：A1 keystone 显式化、A2 skill10 系数单源（`getModifier`→`GetMech`）、A3 `BuffId::ArmorShred`、A4 机制键名 schema；B GDD 四处修订（`设计文档/职业设计草案_剑修.md:156` 153=30%、`:210` 251 满层重置、`:228` 灵剑决非 Toggle 且 8s 有限生命、`:340` 万剑归宗扇形）与 D4/D6/D9/RD-13 一致；C1 skill3（372/375/331/355）、C2 skill4（476/472/474/435/counter_swords 等）、C3 skill5（535/552/532/533/534/512）、C4 skill8 `RemoveByKind(kind, source_skill_id)`、C5.1（B2-16 评估后拒绝，附证据）、C5.2（部分，见剩余风险）；D 测试隔离、973 重论证、骰子分布测试、文档口径。无未声明生产越界。

计划勾选状态：Wave A/B/C/D 均为 `[x]`，C5.2、D1.5 为 `[~]`（部分/阻塞），Wave E（A-01 技能1~12 抽象化 + `--gen-specstate`）保留 `[ ]`（独立 Track，未启动，非本包范围）。

## 质量与风险评估

代码审阅结合图谱检索（`codebase-memory-mcp`）与既有实现对照：

- 测试真伪：6 个新增测试均为带真实数据/断言的功能测试，无空测试、无 `CHECK(true)`；`BladeFormationNodes.cpp`（D4 有限生命）、`FlowingThrustNodes.cpp`（153 降至 30%）为设计授权对齐，属加强而非削弱；`SkillBehaviors.cpp` 以 RAII `CombatEventHandlerScope` 替换手工反注册，净改善。
- 重复轮子：`SampleFollowingShadowRingPoint`、`RemoveByKind` 重载、`BladeWardRuntime.hpp`、`UpdateModifierValue` 经图谱检索均无等价既有实现，不构成重复轮子。
- 数据迁移：`skill_contracts_compact.json` 新增 `keystone_node_ids` 为加法字段；`skills.json` 删除 `params.slash_count` 无孤儿读者（唯一消费为 `SevenStarSlash.cpp` 的 `GetMech`）；7 个节点角色 `Keystone→Passive` 为修正性迁移，由契约测试覆盖；`skill_mechanics_schema.json` 为可选产物，缺失时静默跳过、历史行为不变。
- 键名 schema 校验确认为加载期一次、仅 `LOG_WARN`、不改返回值，非热路径。

## 发现项（第 1 轮，均已解决）

- **Blocker（已修复）** `src/game/systems/skill/behaviors/BladeWard.cpp` 原 `:382-438`：`UpdateBladeWardRuntime` 每帧构造 `BuffEffect`（`std::string id/name` 超 SSO 长度 → 堆分配）与 `modifiers` 向量，经 `SkillSystem.cpp:1458` 每帧调用，违反 `conductor/code_standard.md` §2.1/§7.2 与 `review.md:57`/`:62`，且与设计 §6「无热路径结构变化」不符。
- **High（已修复）** `BladeWard.cpp` 原 `:486`：472 法环 `ResolveDamage(registry, pulse, target)` 第三参（击杀归因攻击者）误传 `target`，应传 `entity`。
- **High（已修复）** 技能4 反击剑气实体（`SpawnBladeWardCounterSwords`）未设置 `origin`，命中经 `ProjectileSystem` 按 `DamageOrigin::DirectSkillCast` 结算全额武器+技能伤害，与 `ResolveSkill4Counter` 单源结算重复计数，且与「实体仅承担表现」注释矛盾。
- **Medium（已修复）** 反击剑气 `speed` 与 `lifeTime` 双重 `rangeScale` → 射程二次增长。
- **Medium（已修复）** 472/474 仅按 `FactionComponent` 过滤，缺敌对判定。
- **Medium（已处理）** backlog B2-15 注记与本批 A3 实现不一致，已更新为「ArmorShred 枚举化已完成，仅剩 351 数据消费重命名」。
- **Low / Best Practice（记录）** `SampleFollowingShadowRingPoint` 无共享头（测试前向声明）；`DamageInterceptors.hpp:4` 反向依赖技能行为层；`BladeWard.cpp` 剑气快照冗余复制 `CombatStats`；`SkillMechanicsRegistry.hpp/.cpp` 缺文件尾换行；433/472 仍用具名常量（BP，见剩余风险）；`gen_skill_mechanics_schema.py` 以正则模拟 C++ 语义（限制已文档化）。

## 修复复审（第 2 轮）

- FIX 1：`Buff.hpp:218-230` 新增零分配 `UpdateModifierValue(std::string_view,value,duration)`；`BladeWard.cpp:384-401/:412-435/:450-468` 改为 `static const BuffEffect` 模板 + 命中即就地刷新，首次插入才 `AddOrRefresh`，消除每帧堆分配；语义不变。
- FIX 2：`BladeWard.cpp:516` 改为 `ResolveDamage(registry, pulse, entity)`。
- FIX 3：`Projectile.hpp:35` 新增 `visual_only`；`BladeWard.cpp:560` 反击剑气置 `true`；`ProjectileSystem.cpp:712/718/724` 读取并短路伤害块，销毁/粒子/闪光不变。
- FIX 4：`BladeWard.cpp:541` `speed = 520.0f` 恒定，`lifeTime = 0.5f * rangeScale` 线性。
- FIX 5：`BladeWard.cpp:497`（`all_of<FactionComponent, HealthComponent, EnemyTag>`）与 `:599`（`view<..., EnemyTag, Position>`）加敌对过滤；`tests/functional/Skill4FollowupTests.cpp:12/:96-98/:308/:314/:320` 补 `EnemyTag` 并断言反击剑气 `visual_only`，未削弱既有断言。
- FIX 6：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:117` B2-15 注记更新。
- 越界说明：为满足 0 警告，`BladeWard.cpp:362/:616` 对批次新引入的 `[[nodiscard]]` 调用补 `(void)`，符合工程约定，无行为影响。

复审后复跑：`build.bat` EXIT 0 / 0 警告（仅 `compile_commands.json not found` 非编译器提示）；`ctest -L ci`（1510 cases / 112318 assertions）、`-L integration` 6/6、`-L unit` 8/8 全绿。

## 剩余风险（显式接受）

1. `tests/TestCommon.hpp` 的 `TestSetupScope` 不重置 `SkillBehaviorRegistry`（其 `Clear()` 会清除 `REGISTER_SKILL_BEHAVIOR` 静态注册），建议独立跟进。
2. skill4 433/472 仍用具名常量（`kBloodBarrierArmorPerMissingHp`、`kStaticAuraBaseDamage`），未数据化。
3. 434 完美招架仅在 `DamageInterceptors` 消费，未接入投射物拦截路径（`ProjectileSystem.cpp` 判定分支）。
4. C5.2 `FrostSlow` 部分收编：legacy `BuffType` 字面量已改走 `AilmentAdapter::ToLegacyBuffType`，手工 `AddOrRefresh` 载体保留（`BuildAilmentEffect` 不携带 modifier、`AilmentType::Slow` 未注册、3 条既有用例锁定 `id="FrostSlow"`：`FlowingThrustNodes.cpp:471`、`SkillSpecializationBakerTests.cpp:777` SUBCASE :885、`SkillSpecializationBakerTests.cpp:1257`）。
5. B2-16（skill7 714→711）评估后拒绝：需在 `CombatEvent/DamageRequest/DamagePipeline/CombatSystem::ApplyDamage` 增补逐命中节点来源，且会破坏 `tests/functional/MindBladeNodes.cpp` 既有用例。
6. D1.5 运行时采证阻塞：当前无交互式运行时环境，B1-21/22/23 仅有自动化证据，转 headless/人工。
7. skill10 1000/1006 系数契约为 `1.0`（非字面 0.06/0.08），保持行为所需；潜在设计意图待确认。
8. `DamageInterceptors.hpp` 反向依赖技能行为层；`DamagePipeline::CalculateBatch` 路径 `ResolveSkill4CounterEffects(...,true)` 无条件产生副作用（仅测试/基准消费，B2-23 保留现状）。
9. 已知非确定性：`tests/functional/SkillBehaviors.cpp:656`（七星闪 D，`damageEvents 12 < 7`）偶发失败，隔离运行 40/40 通过、整轮复跑通过，与本次改动文件无关（skill10 行为文件本次未改动该子用例）。
10. Wave E（A-01 技能1~12 抽象化 + `--gen-specstate`）未启动，属独立 Track，不在本包验收范围。

## 下一步动作

1. 本包结论为 `提交`，可按仓库流程提交（待用户显式指示后执行，勿自行提交）。
2. 将剩余风险 1~3、5~7 登记为后续 Track/backlog 条目；风险 4、10 归入 Wave E 与技能1/2 收尾。
3. 风险 9 的非确定性建议单独立项排查测试隔离（与 `TestSetupScope` 一并处理）。
