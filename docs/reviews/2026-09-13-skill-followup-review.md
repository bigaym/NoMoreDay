# 技能专精收尾 followup 包 — 审查报告

- 日期：2026-09-13
- 审查目标：工作区未提交变更（39 修改 + 11 新增，+1381/-178，基线 `0ef68d2d`）
  - R1 契约/生成器/测试债务：生成器 Keystone 兜底删除（skill1/3/10 显式 keystone）、`BuffId::ArmorShred` 枚举化、GDD 四处文本修订、测试隔离与骰子覆盖
  - R2 行为缺口 + 数据键补齐：skill3（372/375/331/355）、skill4（476/472/474/435、402~475 十三节点、counter_swords）、skill5（535/552/532/533/534/512）、skill8（RemoveByKind 重载）、skill10（系数单源）、skill1（153→30%）、skill2（251 满层重置）
  - R3 机制键名 schema 校验（新增生成器 + load-time warning-only 校验）、B2-16 评估后拒绝（附证据）
- 结论：**提交**（第 2 轮复检通过，见 §8）
- 审查轮次：第 2 轮（第 1 轮结论为「修改」）

## 1. 审查输入

- 设计：`docs/designs/2026-09-13-skill-followup-design.md`
- 计划：`docs/plans/2026-09-13-skill-followup-plan.md`
- 计划评审：`docs/reviews/2026-09-13-skill-followup-plan-review.md`
- 标准：`conductor/code_standard.md`（V2.1）
- 证据：`git diff HEAD` 全量对照、子代理三路只读审查、本机独立运行

## 2. 范围对齐

三轮主体全部落地且忠实于设计/计划；未发现越界改动（行为、重构、性能均未越出授权）：

- **R1**：`scripts/gen_skill_contracts.py` 兜底分支删除与登记一致；`skill_contracts_compact.json` 仅 skill1 `[113,154]`、skill3 `[313,330,351]`、skill10 `[1007,1013,1025]` 新增 keystone 列表，与 GDD `<Keystone>` 逐一核对（2/3/3 个）无漏配错配；重生成 role 变更（skill1 110,133、skill3 311,314,353,354、skill10 1016 → Passive）与记忆记录一致，skill2 251 保持 Passive。ArmorShred 五站点（`FlowingThrust.cpp:520-534`、`RendingWave.cpp:538`、`BladeFormation.cpp:530`、`BladeBoomerang.cpp:363-370`）全枚举化，比较站点零分配。GDD 四处修订（L228/L229 去 Toggle+限时 8s、L156 30%、L210 满层重置补出处、L340 扇形洪流）与数据/实现/测试四方一致。测试隔离 `TestCommon.hpp:33-44` 双重置 + `SkillBehaviors.cpp:29-52` `CombatEventHandlerScope` RAII 收编 4 处站点（其中两处原本必泄漏）。D1.1 骰子分布测试为真实统计断言（固定种子 400 次、band [140,260]、断言与 `ProjectileSystem.cpp:511-512` P≈0.4995/剑及 `DamageInterceptors.cpp:56-68` 口径一致）。
- **R2**：键→消费点对照全通过（skill3 372/375/331/355/300、skill4 十三节点+435+counter_swords、skill5 535/552/532/533/534/512）；skill10 生产路径 `rg getModifier` 0 命中，10 站点新键值与原契约逐一相等，`slash_count` 死键删除且 `SevenStarSlash.cpp:448-453` 改读键；skill8 `Buff.hpp:319-331` RemoveByKind 过滤重载与原门控语义严格等价（FreeCast 唯一来源即 skill8）并有异 kind/异 source/通配三路径测试；skill2 251 满 10 层重置（`RendingWave.cpp:107-128`）删 TODO 并引 GDD L210；skill1 153=30% 三方一致（GDD L156 / `skill_mechanics.json` / `AilmentEngine.cpp:841-842`）且 `Skill1FollowupTests.cpp` 以「不加载机制表」专测代码兜底并 `CHECK_FALSE(==100%)` 防恒真。
- **R3**：`gen_skill_mechanics_schema.py` 静态解析 GetMech 三元组 + schema 登记；`SkillMechanicsRegistry.cpp:47-68/160-161` 加载期一次性校验、LOG_WARN 口径一致、非热路径；`SkillMechanicsKeySchemaTests.cpp` 测试①以合成表错键 `probe_typo` 真实触发诊断（断言 warnings≥2 且含键名与 `1:100:probe`）。B2-16 拒绝证据三处登记且一致（`SkillSpecializationBaker.cpp:1070-1079` 代码注释、计划评审剩余风险 5、计划/backlog 未勾选），拒绝理由充分。
- 文档对账（backlog 24 项勾选附销项、`p1-report.md` 973 重论证、2026-09-07 review crit 措辞）完成；C5.2（B2-18 部分完成）与 D1.5（运行时采证阻塞）的 `[~]` 登记理由与代码实况一致。

## 3. 变更文件边界

设计/计划授权面内：生成器 2 个、契约/机制/图标数据、GDD、12 个行为/管线源文件、`Buff.hpp/BuffIds.hpp/SkillDefs.hpp`、9 个新/改测试文件、3 份文档。无授权外文件改动；`settings.json` 类用户本地配置未卷入。

## 4. 质量与风险评估

独立验证（本机）：`build.bat` EXIT=0（唯一 warning 为 compile_commands.json 环境提示，非编译警告）；`gen_skill_contracts.py --check --check-idempotency --check-determinism`、`sync_skill_node_icon_ids.py --check`、`gen_skill_mechanics_schema.py --check` 全 PASS；`ctest -L integration`、`-L skill` 全绿。

**`ctest -L ci` 不稳定**：5 次运行 2 次失败（40%），失败点固定为 `tests/functional/SkillBehaviors.cpp:656` `CHECK(damageEvents < 7)` 实测 12（skill10 星落分支）。该断言为既有（本批仅将 handler 改 RAII，断言未动，见 `build/review_diff_skillbehaviors.txt`）；失败与通过在同二进制、同用例顺序下交替出现，指向施放路径内未受控 RNG 分支（疑似 1022 星落触发判定）。违反计划 D1.2 DoD「ci 套件无已知 flaky」与设计出口「ctest -L ci 全绿」，构成阻塞项。

code_standard 硬规则（§2.1/2.2、§5.2/5.3、§6.1、§7.2、§8.1）三路核查 + 抽验全部零命中：无 UB/UAF/泄漏、无裸 new/delete、无 dynamic_cast/C cast、无裸线程、无热路径字符串分支（GetMech 全链 string_view）、无热路径堆分配。测试真实性：三个新 Followup 测试文件与全部新断言均为真实行为断言，无 REQUIRE(true)、无既有用例被删除或弱化（`FlowingThrustNodes.cpp:158-161`、`BladeFormationNodes.cpp:113-124` 的断言修改均为设计授权的语义修正且更严格）。

## 5. 发现项

### High

- **H-1**：`tests/functional/SkillBehaviors.cpp:656` — skill10 星落分支 `damageEvents` 无 RNG 控制，`< 7` 上限过窄，`-L ci` 5 跑 2 败（实测 12）。阻塞出口标准。修复建议：定位施放路径随机源并固定种子，或将断言改为区间/统计带并注明随机性来源。

### Medium

- **M-1**：`src/game/systems/skill/behaviors/BladeFormation.cpp:728-749` — 331 弱点锁定溅射无巨剑形态门控：注释自称「巨剑暴击时」，实现条件仅 `is_crit && WeakPointCrit>0`；GDD（`设计文档/职业设计草案_剑修.md:257`）明文限定巨剑降临形态，同文件 370/372/375 均有 profile 门控。测试（`Skill3FollowupTests.cpp` 331 用例）恰好点亮巨剑掩盖缺口。建议补 `has_giant_sword` 门控并补非巨剑形态负例断言。
- **M-2**：`src/game/systems/skill/SkillSystem.cpp:1124-1132` — 451 借力打力新增键消费注释称「每点闪避提速标量由机制表驱动」，但 `dodge_speed_per_point` 键未登记 `skill_mechanics.json`（GetMech 恒回退默认 1.0f），消费结果与 `BladeWardComponent::dodge_speed_points`（点数量纲）做 max 属量纲巧合恒等；真实系数 `5.0f`（`:1144`）与时长 `2.0f`（`:1149`）仍硬编码；既有键 `speed_bonus_pct_per_point`（`skill_mechanics.json:338`）全库零消费。同时暴露 R3 校验盲区：**代码消费但表内缺失的键**不告警（当前校验仅覆盖「表内键名拼写/未消费」方向）。建议：真实数值入表并消费，或删除恒等读取；盲区列为 followup。
- **M-3**：`src/game/systems/skill/behaviors/BladeFormation.cpp:412`（本批 `field.damage = castDamage`）+ `src/game/systems/skill/BeamChannelDeliverySystem.cpp:1324-1330`（既有 `ApplyAilmentTo(..., Shock, field.damage * 0.1f, ...)`，tick 间隔 0.5s）— 372 静电场存续期每秒附加约 0.1×castDamage 的 Lightning DoT，与「伤害全部由落雷本体承担」注释矛盾；`0.1f` 硬编码无机制键；计划 C1.1 伪代码只授权 radius/duration 两键。建议：按设计确认场应零伤害（置 0 或删 `field.damage` 赋值）或数据化系数并修订注释与设计。
- **M-4**：ArmorShred stacks 编码统一（`.stacks` 展示同步、`max_stacks` 语义）本批新增行为零多层测试覆盖——既有断言均为 1 层（`RendingWaveNodes.cpp:591-613`、`BladeBoomerangNodes.cpp:449-468`），FlowingThrust 152 shred 站点无测试；计划 A3 DoD「按统一语义修正并留断言」未完全满足。建议补 stacks≥2 下 `modifier = -10×stacks` 与刷新同步断言。

### Low

- `src/game/systems/skill/behaviors/FlowingThrust.cpp:520-534` shred 站点未设 `.is_debuff=true`（另两站点均设；同 id 共享下减益标志取决于先施加者，`Buff.hpp:135` 序列化口径不一致）。
- `src/game/foundation/data/BuffIds.hpp:97-98` 以相邻字面量 `"Armor" "Shred"` 拼接使 DoD 检索技术性归零（运行时无成本，但口径应限定「比较站点无字符串」）。
- 三创建站点 `max_stacks = stacks` 自引用（`FlowingThrust.cpp:527`、`RendingWave.cpp:538`、`BladeFormation.cpp:530`），钳制分支永不更新展示层。
- `tests/unit/SkillMechanicsKeySchemaTests.cpp` 测试③注释过时（称 `9.0.storm_pulse_mult` 漂移，实际三处已齐备）；python 缺失时软跳过为已知弱点。
- `docs/plans/2026-09-12-skill1-9-followup-backlog.md` B1-20 保持 `[ ]` 未注记其骰子分布子项已由 D1.1 闭环。
- `docs/reports/damage-pipeline-modernization/phase-P1/p1-report.md:97/:177` 引用 `SkillSystem.cpp:1060` 实际为 `:1068`（偏 8 行）。
- `src/game/systems/skill/behaviors/BladeWard.cpp:362-364/:616-618` 473/474 Shock/Chill magnitude 固定 `1.0f`，无键无 GDD 锚点；434 perfect parry 消费（`DamageInterceptors.cpp:44-53`）无功能测试。
- `tests/functional/Skill5FollowupTests.cpp:192` `mark->stacks >= 1` 应为 `== 1`。
- 372 瞬移（`BladeFormation.cpp:383-388`）无距离上限/碰撞检查；GDD「击中后…释放静电场」为 DoHit 语义而实现置于 DoCast（`:373-426`），与计划 C1.1 伪代码（DoHit 站点）存在偏差，需设计确认。
- 375 引爆仅结算双倍伤害，未消费/清除目标身上异常（GDD「引爆该元素异常」语义歧义），需设计确认。

### Best Practice

- 计划 C5.2「4 条既有用例」与计划评审「3 条」计数口径不一。
- `SkillMechanicsRegistry.{hpp,cpp}`、`skill_mechanics_schema.json` 缺 EOF 换行。
- `tests/TestCommon.hpp:36-39` `SkillRegistry::Get().LoadFromJson(...)` 返回值未断言。
- `DamageInterceptors.hpp:4` combat→skill 反向依赖、`DamagePipeline.cpp:2132-2137` CalculateBatch 无 `is_simulation` 门（既有）。
- `tests/functional/Skill5FollowupTests.cpp:29-32` 手动重复声明 `SampleFollowingShadowRingPoint`（`BeamChannelDeliverySystem.cpp:903-910`），建议共享头。
- HeavyMomentum 仅登记设计文档 §4.4，`docs/designs/2026-09-13-skill-baker-consumer-map.md` node 213 行未交叉引用。

## 6. 剩余风险

- ci flaky 根因未定位（H-1）。
- 运行时采证（B1-21/22/23、772、§11.9）未做（D1.5 已登记阻塞，非本次引入）。
- `Skill10FollowupTests.cpp` 事件计数前提（每段每目标恰 1 次 OnDealDamage）未做运行级确认。
- 骰子分布测试（`SkillSystemTests.cpp:1210-1262`）固定种子下依赖 RNG 消费流稳定，若新增随机消费点理论上可扰动（band 有容差）。

## 7. 下一步动作

1. 修复 H-1：定位 skill10 施放路径随机源，固定种子或改区间断言；`-L ci` 连续 ≥3 次全绿后复验。
2. 修复 M-1/M-2/M-3：331 补巨剑门控+负例断言；451 真实数值入表或删恒等消费（顺带处置 `:338` 死键）；372 按设计确认后处置场伤害。
3. 补 M-4 ArmorShred 多层断言。
4. Low/BP 项按成本择优，随下一轮一并复核。
5. R3 校验盲区（消费键不在表内）登记为 followup。
6. 全部修复后按本报告复检，结论转「提交」。

## 8. 第 2 轮复检（2026-09-13）

### 8.1 整改执行

用户三项裁决（已登记 `设计文档` §4.7）：372 静电场零伤害（删 `field.damage` 附加路径）；372 时序按 GDD 改 DoHit（瞬移后命中才放场，顺带补落点约束）；375 引爆时消费目标元素异常栈。

| 项 | 处置 | 落点 |
| --- | --- | --- |
| H-1 | 根因定位：终斩 4 段固定事件 × ExplodeScars 8 事件，随机源为暴击掷骰（默认 5%）与 3% 有效闪避漏段；修复为测试内固定 `crit_chance=0` + `accuracy=1`（生产代码零改动），断言语义保持精确 4 段 | `tests/functional/SkillBehaviors.cpp:638-667` |
| M-1 | 331 溅射补 `has_giant_sword` 门控 + 非巨剑形态负例断言 | `BladeFormation.cpp:746-750`、`Skill3FollowupTests.cpp:296-315` |
| M-3 | 372 场伤害置零（skill3 分支施加 0.0f 感电；772 神雷场保持原 `field.damage*0.1f` 语义）+ DoHit 时序重排（DoCast 仅瞬移+缓存场参数+置 pending）+ `BladeFormationComponent` 新增 4 个 pending 字段 | `BladeFormation.cpp:373-400/:595-632`、`BeamChannelDeliverySystem.cpp:1324-1333`、`SkillDefs.hpp:1185-1189` |
| 375 引爆 | 引爆时先消费目标 Burn/Ignite 或 Shock 异常栈再结算双倍伤害 | `BladeFormation.cpp:707-732` |
| M-2 | 451 死键/恒等读取删除，系数 `dodge_speed_bonus_per_point=5.0`、`dodge_speed_duration=2.0` 入表；473/474 magnitude 数据化；审查方二次修正见 8.2 | `SkillSystem.cpp:1116-1141`、`skill_mechanics.json:336-378` |
| M-4 | ArmorShred 多层断言补齐（2 层 modifier=-20、刷新不叠、is_debuff） | `RendingWaveNodes.cpp:615-656` |
| Low | shred 站点 `.is_debuff=true`（`FlowingThrust.cpp:534`）；`BuffIds.hpp` 还原完整 `"ArmorShred"` 字面量；434 perfect parry 功能测试（`Skill4FollowupTests.cpp:353-389`）；Skill5 `stacks==1` 收紧；schema 测试注释更新；TestCommon LoadFromJson 断言 | 多处 |
| BP | EOF 换行、文档口径统一（C5.2=3 条）、p1-report 行号修正、B1-20 `[~]` 注记、node 213 交叉引用 | 多处 |

R3 消费侧盲区（消费键不在表内告警）按评审建议登记为 followup（backlog R3 追加项），本轮不扩表。

### 8.2 审查方二次修正（2 项，属整改回归修复）

1. **451 提速点数真源回归**：整改初版删除了 OnDodge 处理器内 `ReadPoints(spec, 451u)` 运行时读取，仅依赖 `BladeWardComponent::dodge_speed_points` 烘焙快照，导致未烘焙实体（如既有集成测试 `SkillSystemTests.cpp:1328` 直接构造 spec）丢失速度 buff（`[Integration] SkillSystem - BladeWard 451 & 455 OnDodge triggers` FATAL）。已修正为 **spec 分配记录为真源、烘焙快照 max 兜底**（`SkillSystem.cpp:1122-1133`），系数/时长数据化保留，死键删除保留。
2. **测试链接配置**：整改重链后 `NoMoreDayTests` 触发 LNK1140（PDB 聚合上限，全量重建复现）。处置为测试目标 RelWithDebInfo 下加 `/DEBUG:NONE`（`tests/CMakeLists.txt:44-50`，测试运行不依赖 PDB）；备选方案为拆分测试二进制，如需保留测试 PDB 请拍板。

### 8.3 验证证据

- `build.bat`（RelWithDebInfo）EXIT=0，零编译警告。
- `ctest -L ci` 连续 5 轮全绿（`nmd.tests.ci.nonperf`：1512 用例 / 113293 断言 / 0 failed，含 94 skipped 计入 GPU-Diagnostic 排除集）。
- `ctest -L integration`、`ctest -L skill` 全绿。
- `gen_skill_contracts.py --check --check-idempotency --check-determinism` OK；`sync_skill_node_icon_ids.py --check` OK（76 节点无缺失图标）；`gen_skill_mechanics_schema.py` 幂等（465 entries / 7 dynamic / 87 unreferenced，重生成与工作区一致）。
- H-1 flaky 复现路径已关闭：ci 连续 5 轮无 `damageEvents < 7` 失败。

### 8.4 剩余风险（第 2 轮更新）

- 372 pending 静电场无超时窗口：若命中事件永不发生，pending 字段滞留至下次施放刷新（重施写回，无脏状态外泄），可接受但登记。
- 372 落点约束半径取 `formation.search_radius`（默认 200，随 310 节点缩放），属设计合理外延，未单独立键。
- R3 消费侧盲区（代码消费但表内缺失不告警）已登记 followup。
- 运行时采证（B1-21/22/23、772、§11.9）维持 D1.5 阻塞登记。
- `SingleGpuTimerOwnerRegressionTest:235` 在本轮验证中未复现（第 1 轮失败为环境性偶发，该文件不在本批变更内），保持观察。

### 8.5 结论

**提交**。全部 High/Medium 整改闭环并有测试锚点；Low/BP 择优修复，其余登记；构建/测试出口标准全数满足。
