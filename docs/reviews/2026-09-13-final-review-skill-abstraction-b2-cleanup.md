# 技能抽象化与 B2 清理 — 最终评审报告

- **日期**:2026-09-13
- **审查对象**:工作区未提交变更(实现 `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` 全部 9 个 wave)
- **结论**:**提交**
- **审查轮次**:最终轮(前置:A1 两轮 + A2 评审已并入;本轮为收口评审 → 修复 → 复审 → 死代码清理 → 终验)

---

## 1. 审查目标

对技能抽象化(技能10/11/12 信封化、A1-1~A1-7、A2-1~A2-3)与 B2 死代码清理变更做整体就绪性评审;按用户指令完成「发现问题→子代理修复→复审→死代码清理(含测试代码)→提交」闭环。

## 2. 输入

- `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`(含 DoD 9 条与验证命令)
- `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`(Rev.2)、`docs/designs/2026-09-13-skill-baker-consumer-map.md`
- 既有评审报告:r1-completeness / r1-correctness / r1-fixes / r2-deadcode / a2-review(结论「修改」项均已由后续实现处置)
- git diff:65 文件修改(+2834/−1772)+ 13 未跟踪新增文件

## 3. 审查方法与轮次

| 轮次 | 形式 | 覆盖 | 产出 |
|---|---|---|---|
| 评审 | 4 个只读子代理分区评审 | A2 三技能迁移 / A1-1·A1-2 基础设施 / A1-3·A1-4 Channeling 与伤害管线 / A1-5 行为层+全部测试 | 发现项:无 Blocker,1 High + 7 Medium + ~15 Low;正面确认:三方数值等价全通过、删除断言逐条有承接、热路径零新增分配 |
| 修复 | 3 个并行子代理 | 组A(共享头/数据键)、组B(基础设施/管线)、组C(行为/测试/文档) | 全部 High/Medium 修复落地,见 §5 |
| 复审 | 主控 | 修复 diff 抽查 + 10 项静态 rg 核验 | 10/10 符合预期 |
| 死代码清理 | 2 个子代理(生产/测试)+ 主控收尾 | 生产 2 项删除、测试 19 项死 include 删除、孤儿函数连带测试同步 | 见 §6 |
| 终验 | 主控 | build + ctest + 数据契约校验 + 终态 rg 清零 | 见 §7 |

## 4. 范围对齐

- 9 个 wave 全部按计划落地;RD-16/17、SpecState 生成化延后、B2-23 保留等裁决与实现一致。
- 计划勾选订正:T5.1/T5.4 改为 `[~]`(部分完成)并注明残留归属(Apply 收编留 B2-18、ArmorShred 重命名留 B2-15);T5.6 的 `AllocateSkillNode` 笔误已订正为 `SkillSystem::AddTalentPoint(SkillSystem.cpp:2128)`。
- 无越权改动、无隐藏失败路径、无范围外行为变化。

## 5. 发现项与处置(本最终轮)

### High(1)
| # | 位置 | 问题 | 处置 |
|---|---|---|---|
| H-1 | `SevenStarSlash.cpp:21` vs `SevenStarSlashShared.hpp:21` | `kSevenStarSlashSkillId` 双定义,单向修改无诊断 | **已修复**:删除 cpp 本地定义,19 处引用改 `seven_star_shared::` 限定名;唯一定义剩共享头 |

### Medium(7)
| # | 位置 | 问题 | 处置 |
|---|---|---|---|
| M-1 | `Game.cpp:259` | 机制表加载失败返回值被忽略,静默降级违背注册表「避免掩盖」注释 | **已修复**:失败 `LOG_CRITICAL` + `throw std::runtime_error`,对齐 `ModifierRuntimeRegistry`(Game.cpp:271-275)口径 |
| M-2 | `SevenStarSlashShared.hpp:23/24/26/276` | 4 个零读者死常量 | **已删除**(rg 前后自证;`BladeBoomerangCatchTests.cpp:36-37` 同名常量为测试自建局部定义,不涉) |
| M-3 | `skill_mechanics.json:935` + `HeavenlySwordDescent.hpp:205` + `HeavenlySwordDescentNodes.cpp:151` | node 1107 `impact_damage_mult` 加成语义 vs node 1113 乘法语义,同名异义 | **已改名**:1107 → `impact_damage_bonus`(json/绑定表/测试断言三点同步);1113 及结构字段 `HeavenlySwordFieldComponent::impact_damage_mult`(SkillDefs.hpp:1285)语义正确,保留 |
| M-4 | `BladeMasteryService.cpp:84-88` | 注释缺 501 构筑下写点被每 tick 重算覆盖的限制登记 | **已补**:注释登记「仅非 501 构筑生效」(BeamChannelDeliverySystem.cpp:997-1006 覆盖关系) |
| M-5 | `GameplayState.cpp:873-881` | 技能7 范围圈 350.0f 与机制表双源(存量跟随项) | **已外置**:`skills::GetMech(7,0,"base_range",350.0f)`,描边圈改 `range+2.0f`,TODO 注明 703 range_pct_per_point 未接入 |
| M-6 | `BladeFormation.cpp:350` + :372-373 | ForEachChainCandidate 注释失真(帧初缓存参照系);线性回退不排除 DormantTag,与网格路径命中集合不等价 | **已修**:注释订正 + 回退路径补 `reg.any_of<DormantTag>(e)` 过滤(与网格 rebuild 及 SwordArray 口径一致) |
| M-7 | `docs/plans/...plan.md:345/:348` | T5.1/T5.4 勾选超前 | **已改** `[~]` 部分完成并注明残留归属 |

### Low(已处置或登记)
- `BloodSea.cpp:447-456` burst 治疗乘区补 `shared_devour_timer>0` 门控,与脉冲路径(:278-280)同构(两态零行为变化)。
- `SkillMechanicsRegistry` 无锁死接口 `EnsureLoaded` 删除(.hpp:23/.cpp 定义),类注释同步改为「启动期显式 LoadFromFile,失败由调用方阻断启动」。
- `SkillMechanicsRegistry.cpp:97` 日志 "positive integer" → "unsigned integer"(node 0 合法)。
- `BladeWard.cpp:103` 注释订正(拦截双消费方:ProjectileSystem 投射物 + DamageInterceptors 非投射物源)。
- `SkillMechanicsRegistryTests.cpp:43` 补 `REQUIRE(out.is_open())`。
- `BeamChannelDeliverySystem.cpp` include 改为 `SkillPointAccess.hpp`(解除 systems→behaviors 依赖边)。
- `DamagePipeline.cpp:1469-1479` 反击块缩进归位。
- `BladeFormation.cpp:505-506` 链式候选超上限时命中子集取决于遍历序(网格桶序/回退 view 序)——注释登记。
- `SkillPrerequisiteRequiredPointsTests.cpp:11-13` 全局单例依赖顺序声明注释。
- `FlowingThrustNodes.cpp` 末尾补换行。
- MindBlade `tick_interval` 断言:复核已存在(`MindBladeNodes.cpp:165`),无需改动。

## 6. 死代码清理轮(含测试代码)

**生产侧(2 项删除)**
1. `SkillRegistry::SanitizeLoadedSkillSlots`(`SkillRegistry.hpp:39-44` / `.cpp:776-789`):全仓仅 `SerializationSkillSanitizeTests` 调用,生产加载路径(SaveManager::ParseProgressionJson → restoreFromSnapshot)不依赖未知槽复位语义 → 删除;连带删除该测试文件中 2 个随函数用例及其专用夹具(`MakeActiveSkillsJson`/`RegisterSanitizeFixtureSkill`/未知 id 常量),**保留 M4 回归用例**(from_json prunes non-positive allocated points,文件重写为最小形态)。
2. `HeavenlySwordFieldComponent::linked_cut_effectiveness`(`SkillDefs.hpp:1289`):`rg -w` 全仓零引用,删除。

**测试侧(19 项删除,全部死 include)**
- 未跟踪新文件 6 项 + 修改文件 13 项(BloodSeaNodes/HeavenlySwordDescentNodes/SpecStateMappingTests/SkillBakerFlagConsumerTests/DeliveryArchetypesTests/InfiniteBladesNodes/MindBladeNodes/RendingWaveNodes/SkillBehaviors/SkillSpecializationBakerTests)。
- 误报存活已复核保留(如 SevenStarSlashNodes 的 SkillExecution、BloodSeaNodes 的 SpatialHashGrid)。

**清理轮失误与收尾(主控)**
- 2 处传递依赖失误由编译暴露:`DeliveryArchetypesTests.cpp` 缺 `AIComponent.hpp`(EnemyTag 定义于 AIComponent.hpp:70)、`SkillSpecializationBakerTests.cpp` 缺 `SkillBehaviorBase.hpp`(`ResolveElementalConversion` 定义于 SkillBehaviorBase.hpp:48)与 `AIComponent.hpp` → 主控恢复 3 个 include,重建通过。
- 豁免保留(显式登记):`PersistentFieldHeader::linked_hit_count`(测试/调试断言注释)、BuffIds 死位 bit24/26/27/28、`ReactiveWard=11` 枚举位、DeliveryArchetype taxonomy 注释、`SkillMechanicsRegistry::HasNode`(测试消费的 registry 查询 API,属存量符号)。

## 7. 验证证据

| 命令 | 结果 |
|---|---|
| `build.bat`(RelWithDebInfo) | **0 错误**(清理轮后重建) |
| `ctest --test-dir build -C RelWithDebInfo -LE "performance|gpu"` | **15/15 通过**(doctest 1483 用例;`SingleGpuTimerOwnerRegressionTest.cpp:235` 为已知环境 flaky,复现 1 次后单独重跑通过) |
| `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` | **OK**(skill_contract blocks up to date) |
| `python scripts/sync_skill_node_icon_ids.py --check` | **0 diff**(76/76 unchanged) |
| 终态 rg 清零 | `kSevenStarSlashSkillId` 定义=1;`SanitizeLoadedSkillSlots`=0;`linked_cut_effectiveness`=0;`ChannelingComponent`=0;`impact_damage_mult` 残留=预期保留项(1113 链+结构字段) |

## 8. 最佳实践建议

1. **include 显式性**:测试文件(及一般翻译单元)应直接 include 所用符号的定义头,而非依赖传递包含——本轮清理的 19 项死 include 中 2 项正因此失误回滚,后续如遇 MSVC 递归依赖重构需警惕。
2. 机制表键命名建议形成约定:加成语义用 `_bonus` 后缀、乘法语义用 `_mult` 后缀(本轮 1107 改名即为此约定),避免同名异义再生。
3. consumer 映射文档的串行号引用建议改为稳定锚点(符号/节点 id),既有 a2-review 登记延续。

## 9. 剩余风险

1. **Tracy 性能基线未验证**:`docs/performance/2026-09-13-skill-abstraction-baseline.md` 标注待补采——本批对引导/星象等热路径有结构变化,建议合入后按 performance 工作流补采。
2. **DoD#4 残余**:技能1-9 未信封化、`--gen-specstate` 生成器未实现(计划已登记延后)。
3. **技能10系数双源**:`GetMech`/`getModifier` 并存(SevenStarSlash.cpp 多处),登记边界待后续统一。
4. **设计确认待定**:禁疗×增疗交互(设计§11.9)、ProcEngine ending 帧口径(设计§11.8)需设计侧最终确认。
5. **已知 flaky**(非本批引入,环境性):`SkillSystemTests.cpp:1269`(BladeWard470 跨用例污染)、`SkillBehaviors.cpp:635`、`SingleGpuTimerOwnerRegressionTest.cpp:235`(GPU 环境,本轮复现)。
6. schema 不校验机制表键名拼写(加载期),键改名依赖全链同步(本次已完成)。

## 10. 下一步动作

1. 提交本批全部变更(代码+数据+文档+评审报告)。
2. 按 `docs/plans/2026-09-12-skill1-9-followup-backlog.md` 排期 B2-15/B2-18 等登记项。
3. 补采 Tracy 性能基线;跟踪三处设计确认项(§11.8/§11.9/技能10双源)。
