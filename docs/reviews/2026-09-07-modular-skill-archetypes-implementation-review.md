# 模块化技能原型库与专精系统实施审查报告

## 1. 审查目标

审查工作区为实施 `docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md`（技能原型库与专精系统重构，Phase 1–3）所做的全部变更。审查依据配套设计 `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`。

## 2. 结论

**修改**

## 3. 审查轮次

首次审查（实施完成后第一轮独立审查）。

## 4. 输入

- 设计：`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`
- 计划：`docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md`
- 前序计划审查：`docs/reviews/2026-09-06-modular-skill-archetypes-and-specialization-plan-review.md`
- 审查标准：`docs/workflows/review.md`（硬否决清单 + 报告必填字段）
- 代码标准：`conductor/code_standard.md`（§5 内存与资源、§6 现代C++、§7 ECS/DOD）
- 独立验证证据：
  - 构建：`build.bat RelWithDebInfo` 通过（日志 `C:\Users\yuminao\AppData\Local\Temp\opencode\build_review.log`）
  - 测试：`ctest -C RelWithDebInfo -R "\.(unit|integration)$"` **15/15 通过**（`ctest -L ci` 仅匹配 1 个聚合用例，覆盖不足；多配置生成器必须带 `-C`）
  - 源码检视：9 个行为文件、Baker、Rebake、ProcEngine、TriggerRuleComponent、4 个交付系统、3 个新测试文件、设计 5.1/7.1 节点映射比对、与重构前 `conductor/specs/` 归档旧代码的平价性比对

## 5. 变更文件边界

`git status --short`：17 modified + 14 untracked（+958/−3062）。

新建（关键）：
- `src/game/foundation/components/CompactEntitySet.hpp`
- `src/game/foundation/components/DeliveryArchetypes.hpp`
- `src/game/systems/skill/SkillSpecializationBaker.{hpp,cpp}`
- `src/game/systems/skill/BoomerangDeliverySystem.{hpp,cpp}`
- `src/game/systems/skill/MobilityDeliverySystem.{hpp,cpp}`
- `src/game/systems/skill/OrbitingSentinelDeliverySystem.{hpp,cpp}`
- `tests/unit/CompactEntitySetTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`、`tests/integration/DeliveryArchetypesTests.cpp`

修改（关键）：
- `src/game/foundation/components/{Projectile.hpp,SkillDefs.hpp,TriggerRuleComponent.hpp}`
- `src/game/systems/skill/{SkillSystem.cpp,CMakeLists.txt,ProjectileSystem.cpp,ProcEngine.cpp}`
- `src/game/systems/skill/{AreaFieldDeliverySystem.cpp,BeamChannelDeliverySystem.cpp}`
- 9 个行为文件：`FlowingThrust/RendingWave/BladeFormation/BladeWard/InfiniteBlades/SwordArray/MindBlade/BladeBoomerang/PhantomFlash.cpp`
- `tests/performance/SkillSystemBenchmark.cpp`（+64 行）

行为 diff 合计 −2991/+545。技能↔行为映射（已核对）：ID1 流云刺=FlowingThrust、ID2 裂空斩=RendingWave、ID3 灵剑决=BladeFormation、ID4 剑气护体=BladeWard、ID5 万剑归宗=InfiniteBlades、ID6 剑阵·诛仙=SwordArray、ID7 心剑·无影=MindBlade、ID8 御剑·回旋=BladeBoomerang、ID9 绝影绝剑=PhantomFlash。

## 6. 范围对齐

Phase 1–3 主体落地：SkillSpecializationBaker 数据驱动烘焙（契约 + 节点 switch）、12 原型组件定义（10 个新组件 + AreaField/Ballistic 复用）、4 个交付系统拆分、CompactEntitySet + 全套 static_assert、Rebake 幂等写回（`SkillSystem.cpp:2272` 等值跳过）。测试覆盖合理（Baker 4 用例、交付系统 7 用例、CompactEntitySet 4 用例）。

未对齐项：
- 设计 5.1 节点映射覆盖不完整（见发现项 4/5/6/7）。
- 计划 DoD 两条不实：「BeamChannel 5 处特例全部剔除」与「热路径零堆分配」（见发现项 2/8）。
- 计划任务 2.4c「迁移 ChannelingComponent 全部消费方」未完成：该组件仍被 15 个文件引用，旧通道管线整体保留，Phase 4 清理任务（4.1）未实施。

## 7. 质量与风险评估

测试与构建全部通过（15/15 + 零警告），但按 `docs/workflows/review.md` 不单凭测试判定，直接检视代码发现：

- **正确性缺陷**：TriggerRule 重复累积随高频属性重算持续恶化（发现项 1，Blocker）。现有测试每次只 Rebake 一次，且幂等测试（`tests/unit/SkillSpecializationBakerTests.cpp:80-104`）只比较 `BakedSkillProfile` 等值，不校验 TriggerRuleComponent 规则数，故未暴露。
- **硬否决命中**：热路径每帧堆分配（发现项 2），违反 `docs/workflows/review.md` 硬否决「热路径堆分配」与计划 DoD「零堆分配」；依据 `conductor/code_standard.md` §7.3（系统线性迭代、避免逐帧分配语义）应使用静态复用缓冲或栈上结构。
- **语义缺陷**：拦截概率从未掷骰、多个天赋节点烘焙后零消费（发现项 3/4/5/7），`code_standard.md` §7.1 组件为纯数据，行为必须消费烘焙字段；死字段/死值违反设计的「单一事实源」目标。
- **EnTT 安全**：发现项 6 的残影创建路径绕过完整组件装配，并依赖 `ShadowComponent`/`ShadowCastTag` 的判定组合；`code_standard.md` §5.3（指针/实体生命周期）在 Rebake 与触发规则联动处同样需要复核（发现项 1 修复时注意 AddRule 前后组件池变化）。

## 8. 发现项

### Blocker

**B-1 TriggerRule 随 Rebake 重复累积，触发技被多次执行**
- `SkillSystem.cpp:2229-2277`（RebakeSkillProfiles）：每槽无条件调用 `SkillSpecializationBaker::Bake(..., out_triggers)`，幂等 continue 仅保护 profile 写回，不保护副作用。
- `SkillSpecializationBaker.cpp:118-127`：Bake 对每个 Trigger 节点直接 `out_triggers->AddRule(rule)`。
- `TriggerRuleComponent.hpp:50-56`（AddRule）：仅做容量检查（kMaxRules=8），**无 rule_id 去重**。
- 调用链：`AttributePipeline.cpp:788` 属性重算（高频，随 StatsDirty 触发）→ Rebake → AddRule 重复追加 → `src/game/systems/skill/ProcEngine.cpp:58-61` 按 rule_count 逐条执行 → 同一事件触发技多次施放；8 条满后静默丢弃规则。
- Baker 内已有正确的同步实现 `SyncTriggerRules`（`SkillSpecializationBaker.cpp:405-444`，先 RemoveRule 全节点再 Add），但全仓库零调用，为死代码。
- 问题：相对计划 DoD「Rebake 幂等」与工程正确性，属性重算路径每帧级触发会造成触发技行为漂移。同一家族问题：`PhantomFlash.cpp:77-83` 自身实现了 RemoveRule+AddRule 去重，说明该模式已知，但 Baker 漏用。
- 修复：让 Rebake 统一走 `SyncTriggerRules`；或 `TriggerRuleComponent::AddRule` 按 rule_id 去重（存在即更新）；并新增「Rebake 两次后规则数不变」的断言测试。

**B-2 ProjectileSystem 每帧堆分配（硬否决）**
- `src/game/systems/skill/ProjectileSystem.cpp`（新增 DirectStrike / StickyDetonation 消费段）：`std::vector<entt::entity> deadStrikes`、`std::vector<std::pair<entt::entity, StickyDetonationComponent>> explodingStickies` 为 Update 内**非 static 局部变量**，每帧构造/析构堆分配。
- 违反 review.md 硬否决「热路径堆分配」、计划 DoD「零堆分配」、code_standard §7.3。
- 修复：`static thread_local` + 每帧 `clear()`（与系统内既有 static 缓冲模式一致），或复用 `CompactEntitySet`/栈上固定数组。

**B-3 环绕哨兵拦截概率从不掷骰**
- `OrbitingSentinelDeliverySystem.cpp:47-60`：`interception_chance > 0.0f` 即无条件摧毁 32px 内所有敌方弹道。
- `BladeWard.cpp:51` 基础值 0.3 恒大于 0 → 节点 401(+0.25)/471(+0.15)/451(+0.1) 的概率加成全部无效，语义退化为「开/关」。
- `attack_scan_radius`、`attack_interval`、`attack_timer`、`damage_mult` 字段零消费（死字段）。
- 修复：掷骰判定（复用项目内 RNG 工具）；消费或删除死字段；拦截扫描若保留线性遍历需记录上限。

### High

**H-1 技能4 节点 411/431 烘焙后零消费（剑气护体回归）**
- `SkillSpecializationBaker.cpp:267-272` 设置 flag 4（411 五行御守元素抗性）与 flag 16（430 剑意格挡），但 `BladeWard.cpp:42-63` 只读取 1/2/8/32/64/128/256/512。
- 重构前实现明确处理两者（旧 BladeWard 元素抗性 buff、格挡率提升）。回归：两个天赋节点点了无效。
- 修复：行为内消费两个 flag，或 Baker 直接烘焙为结构化字段（resist_bonus / guard_chance）而非位标志。

**H-2 技能1 节点集缺失（流云刺）**
- 设计 5.1 要求节点 100/101/103/110 全部进 `BakedSkillProfile`；Baker 处理 100/101/110，**103（减CD）缺失**。
- 101 的 more_damage_mult(+5%/点) 在行为/交付系统零消费（more_damage_mult 全部消费方仅 `RendingWave.cpp:79`、`BladeFormation.cpp:43`、`BladeBoomerang.cpp:47` 三处）→ 101 纯死节点。
- 节点 133（PrisonSlash，旧实现 FlowingThrust 有处理）与 171（元素身法，契约数据 `assets/data/skill_contracts_compact.json` 中 transmuter_node_ids=[170,171]，Baker 仅处理 170）零处理。
- 修复：Baker 补 103；各交付系统统一消费 `profile->more_damage_mult`；133/171 按旧实现补平价处理或记录为已知缺口。

**H-3 残影功能退化（流云刺 剑意流）**
- `FlowingThrust.cpp:67-68`：剑意≥5/8/10 生成**裸实体**（仅 ShadowComponent，无 Position/视觉组件/生命周期/snapshot.stats）。`ShadowSystem.cpp:9-22` 随后对其 ShadowCast(skill_id=0)，`SkillSystem.cpp:1215-1217` GetSkill(0)=null 直接失败 → 重构前三档残影（0.22/0.18/0.14 伤害）**完全失效**。
- 节点 130 残影（`FlowingThrust.cpp:70-75`）snapshot 未拷贝 stats → `SkillSystem.cpp:1264-1268` exec 继承空快照 → 残影施放伤害为 0（重构前为 owner stats × 0.3）。
- 防递归 guard `!any_of<ShadowCastTag>(owner)`（`FlowingThrust.cpp:70`）永假：ShadowCastTag 仅加在 exec_ent/finisher/proj 实体（`SkillSystem.cpp:1311`、`BeamChannelDeliverySystem.cpp:40/183/386`），从不加到 shadow 实体；当前仅靠空 active_nodes 兜底终止链。
- 另外 `FlowingThrust.cpp:58` 设 `mob.spawn_clone_node=130`，与行为内手动残影形成**双生成机制**并存。
- 修复：残影创建复用 SkillSystem/旧实现的完整装配路径（Position + 视觉 + DelayedDestroy + snapshot.stats 拷贝）；guard 改回 `!any_of<ShadowComponent>(owner)` 或向 shadow 实体补 ShadowCastTag；移除双路径之一。

**H-4 技能2 节点 232 引力陷阱死节点**
- 设计（design.md:380）：230/232 → 切换 Boomerang 原型并注入 `pull_radius=120.0f`。
- `SkillSpecializationBaker.cpp:216-218` 写 `del.range=120 + flag16`，但 `RendingWave.cpp` 不读 flag16/del.range，`BoomerangComponent.pull_radius` 保持默认 0 → 引力陷阱无效。
- 修复：RendingWave 按 flag16 设 `bc.pull_radius = profile->delivery.range`，BoomerangDeliverySystem 在悬停顶点消费 pull_radius 施加牵引。

**H-5 BeamChannel 特例未剔除，DoD 文本不实**
- 旧 ChannelingComponent 管线整体保留（`BeamChannelDeliverySystem.cpp:224` 起 skip-if-BeamChannel），旧路径仍有 5 处 `chan.skill_id == 5/7` 特例（现行行 180/212/230/347/459）→ DoD「5 处特例全部剔除」按字面不成立。
- ChannelingComponent 仍被 15 文件引用（SkillSystem/BladeMasteryService/DamagePipeline/InputSystem/GameplayState/StatsSystem/SkillDefs/3 行为/4 测试）；行为对技能 5/7 **双写**两组件（`InfiniteBlades.cpp:32-36+67-71`、`MindBlade.cpp:25-29+50-54`），legacy 消费方（`BladeMasteryService.cpp:83-87`、`DamagePipeline.cpp:429-431,576-580`、`StatsSystem.cpp:390-394`）继续读取兼容组件工作。
- 定性：双写保持了运行时兼容（可接受），但计划任务 2.4c「迁移全部消费方」与 DoD 文本需修正，旧管线清理必须列入 Phase 4（任务 4.1）实施并给出日程。
- 修复：更新计划 DoD 表述为「新管线接管 + 旧管线冻结 + Phase 4 移除」；在计划中登记清理任务与验收标准。

### Medium

**M-1 BakedDeliveryParams 字段语义重载**
- `del.range` 兼作：弹道射程（技能2 默认 500）、牵引半径（232:120）、护甲穿透（571:6×点数）；`del.speed` 兼作暴击率（552:1.5×点数，`InfiniteBlades.cpp:52` 消费）与位移速度。技能 8 节点 830/832/833 写 100/150/200 会覆写该技能默认射程 300。
- `RendingWave.cpp:74` 生命期魔数 400 与烘焙 speed 500 脱节。
- 修复：为 BakedDeliveryParams 增加显式字段（pull_radius/armor_pen/bonus_crit）或以注释+烘焙端断言固化语义，消费端同步。

**M-2 more_damage_mult 大面积死值**
- 技能5(552)、6(633)、7(750/752) 烘焙值无任何消费方；技能6 的 633 无局部近似（纯失效）；技能5/7 用行为内硬编码近似（`InfiniteBlades.cpp:51` +0.1、`MindBlade.cpp:40/43` +0.25/+0.5，其中 752 与 Baker more×1.5 等值属巧合）。
- 修复：交付系统统一乘 `profile->more_damage_mult`，删除行为内近似。

**M-3 技能6 烘焙值未消费 + 设计偏差**
- Baker case 6（`SkillSpecializationBaker.cpp:76-81`）area_radius=120/duration=6.0/sub_interval=0.3；`SwordArray.cpp:31` 硬编码 radius=75/duration=5.0。
- 设计（design.md:412）要求 630/631/633 转化为 `AreaFieldComponent::payloads` 由通用管线派发；实现改用 SwordArrayComponent + 旧 DamagePipeline 路径。
- 修复：对齐设计（payloads 派发）或将偏差作为计划变更记录，二者取一并同步烘焙值。

**M-4 接剑退款不完整**
- `BoomerangDeliverySystem.cpp:78-87`：catch 退款 `slot.cooldown -= 1.0f` 未按节点 831 flag(8) 门控；无剑意回复（设计 design.md:428：831=回复剑意与冷却）；L80 注释称「返还冷却与法力」与实现不符（无法力）。
- 修复：flag 门控 + 剑意回复 + 注释修正。
- 顺带：`hover_timer`/`pauseTimer` 双份冗余计时器（同帧同步递减），建议合并。

**M-5 天降打击波次与魔数**
- `AreaFieldDeliverySystem.cpp` SkyfallImpact 消费方：`wave_interval` 零消费（所有波同帧连发）；硬编码 added_effectiveness=1.5f/时长5.0f/pulse 0.3f/value_mult 0.5f。
- 修复：按 wave_interval 调度波次；常量进 profile 或具名常量。

**M-6 死代码/死字段清理**
- `SkillSpecializationBaker.cpp:107` 未使用的 `contract` 局部变量。
- `SkillSpecializationBaker.cpp:405-444` SyncTriggerRules 死代码（B-1 修复后启用或删除）。
- Sticky `current_stacks/max_stacks/explode_on_max_stacks`、Sentinel `attack_*`、Boomerang 兼容字段与 `BoomerangPhase::Paused=1` 别名（`DeliveryArchetypes.hpp:79-89`）。
- 修复：消费或删除；兼容字段若保留需在计划登记清理节点。

### Low

**L-1 Baker 元素转换模板占位 hack**
- `SkillSpecializationBaker.cpp:196/229/255/307` 使用 `SkillBehaviorBase<struct BakerDummy>::ResolveElementalConversion` 空类型占位调用静态成员。建议抽为自由函数，消除对行为模板的伪依赖。

## 9. 最佳实践建议

1. **B-1 优先**：`TriggerRuleComponent::AddRule` 按 rule_id 幂等化（存在即覆盖），这是最小改动且同时保护所有调用方；Rebake 侧再改为委托 SyncTriggerRules，双保险。
2. 交付系统统一入口处理 `more_damage_mult` 与 `del.*` 语义字段，烘焙端为每个字段写 unit 断言（Baker 测试已具雏形，扩展字段覆盖即可）。
3. 逐节点「烘焙→消费」映射表纳入仓库（设计 5.1 旁附消费方 `path:line`），使「死节点」在代码评审时一目了然。
4. 残影/召唤类实体统一走工厂（SkillSystem::SpawnShadow 之类），杜绝裸 `registry.create()`。
5. 契约测试补「两次 Rebake 后 TriggerRuleComponent 规则数不变」「interception_chance=0.5 掷骰分布」两条断言。

## 10. 剩余风险

（若本轮修改后通过）仍保留：
- 双管线共存窗口内，legacy ChannelingComponent 消费方与 BeamChannel 管线并行运行的行为面未逐一审计（BladeMasteryService tick 调整、DamagePipeline cast_id/GlobalWhileBuffActive、StatsSystem 同 policy 均读取兼容组件）。
- 技能 6 特例（SwordArray 保留在 DamagePipeline）与技能 9 与 ProcEngine 硬编码 rule 9 的交互。
- 性能基线未复跑（`nmd.tests.perf.baseline` 未纳入本轮验证）；B-2 修复后应复跑确认帧耗时。

## 11. 下一步动作

结论为 **修改**，需完成以下后进入跟进审查：
1. 修复 B-1/B-2/B-3（Blocker 全部）。
2. 修复 H-1～H-5（至少完成 411/430 消费、103 烘焙、232 消费、残影装配、DoD 文本修正；133/171 可登记为已知缺口）。
3. 按 M-1～M-6 尽量清理，最低完成死变量与注释失实项。
4. 补测试：Rebake 幂等规则数断言、掷骰分布、del 字段语义断言；复跑 `ctest -C RelWithDebInfo -R "\.(unit|integration)$"` 与 perf baseline。
5. 更新计划 DoD 表述（特例剔除→管线接管+冻结计划；零堆分配复测）。

---

# 第二轮：跟进审查

## 1. 审查目标

同第一轮。审查实施方针对第一轮发现项（B-1～B-3、H-1～H-5、M-1～M-6）的修复结果。

## 2. 结论

**提交**

## 3. 审查轮次

跟进审查（第二轮，修复后独立复核）。

## 4. 输入

- 第一轮报告：本文档上半部分。
- 修复后代码：20 个修改文件（新增 `TriggerRuleComponent.hpp`、`SkillSystem.hpp`、`SkillBehaviorBase.hpp` 等改动）。
- 独立验证证据：
  - 构建：`build.bat RelWithDebInfo` 通过，零警告零错误（日志 `C:\Users\yuminao\AppData\Local\Temp\opencode\build_review2.log`）
  - 测试：`ctest -C RelWithDebInfo -R "\.(unit|integration)$"` **15/15 通过**（`100% tests passed, 0 tests failed out of 15`，日志 `ctest_review2.log`）
  - 逐文件检视：Baker、Rebake、TriggerRuleComponent、4 个交付系统、SpawnShadowEcho、9 个行为文件（行数重新统计）

## 5. 发现项修复核验

| 发现项 | 状态 | 证据 |
|---|---|---|
| B-1 TriggerRule 重复累积 | ✅ 已修复 | `TriggerRuleComponent.hpp:50-62` AddRule 按 rule_id 去重（存在即覆盖）；`SkillSystem.cpp:2321-2324` Rebake 改走 `SyncTriggerRules`（先按树全节点 RemoveRule 再按契约重建）且 `Bake(..., nullptr)` 不再触碰 triggers；新测试断言多次 Rebake 规则数不变 |
| B-2 热路径堆分配 | ✅ 已修复 | `ProjectileSystem.cpp:72/149` `static thread_local` + `clear()`，与文件内既有模式一致 |
| B-3 拦截概率不掷骰 | ✅ 已修复 | `OrbitingSentinelDeliverySystem.cpp:53-82` `ThreadSafeRandom::GetFloat01()` 掷骰 + `kMaxInterceptionsPerSentinel=8` 上限 + `s_intercepted_projectiles` 静态复用缓冲（:19-20）；新测试覆盖 0%/100%/上限 |
| H-1 节点 411/430 零消费 | ✅ 已修复 | `BladeWard.cpp:48-55` flag4→ResistAll+15、flag16→BlockChance+10，经 ActiveEffectsComponent 生效 |
| H-2 节点 103/101/133/171 | ✅ 已修复 | Baker:185-186（103→more+10%/点，与设计 design.md:369「103=增伤」一致）；more_damage_mult 现被 8 个行为文件真实消费（乘入 damage_multipliers/payload_context/value_mult）；`FlowingThrust.cpp:90` PrisonSlash(133) 概率触发；Baker:198 处理 170/171 |
| H-3 残影退化 | ✅ 已修复 | 新工厂 `SkillSystem::SpawnShadowEcho`（`SkillSystem.cpp:1316-1365`）：Position/ShadowVisualComponent/DelayedDestroy/完整 snapshot（stats×scale + payload_context 含 more_damage）/SummonComponent 完整装配；`FlowingThrust.cpp:55-61` 恢复四档残影（0.3/0.22/0.18/0.14）；guard 修复为 `!any_of<ShadowComponent> && !any_of<ShadowLifetime>`；`spawn_clone_node` 双路径已移除 |
| H-4 节点 232 死节点 | ✅ 已修复 | `BakedDeliveryParams` 新增显式字段 `pull_radius/armor_pen/bonus_crit`（`SkillDefs.hpp:540-543`）；Baker:221/362/367/370 写 pull_radius（232/830/832/833）；`RendingWave.cpp:81` flag16 门控消费；`BoomerangDeliverySystem.cpp:47-48` hover 阶段 grid 牵引 |
| H-5 BeamChannel DoD | ✅ 已修复 | 计划更新（plan L178/L234）：「新管线通用派发，旧 Channeling 路径冻结并双写兼容，排期 Phase 4 彻底清除」；DoD 表述与实际一致 |
| M-1 字段语义重载 | ✅ 已修复 | Baker:310（552→bonus_crit）、:318（571→armor_pen）不再写 speed/range；消费方 `InfiniteBlades.cpp:52/61`、`RendingWave.cpp:68`（maxRange 读 delivery.range）；`RendingWave.cpp:81` 优先读 delivery.pull_radius |
| M-2 more_damage_mult 死值 | ✅ 已修复 | 全部 8 个行为消费（FlowingThrust:69-73、BladeFormation:43、BladeBoomerang:47、RendingWave:74、MindBlade:49、InfiniteBlades:66、SwordArray:64） |
| M-3 技能6 烘焙值/设计偏差 | ✅ 已修复 | `SwordArray.cpp:38-39` 消费 profile duration/area_radius（fallback 与 Baker 一致 6.0/120）；:62-66 走 `AreaFieldComponent::payloads` 通用管线（Damage payload × more_damage_mult + Slow），对齐设计 design.md:412 |
| M-4 接剑退款 | ✅ 已修复 | `BoomerangDeliverySystem.cpp:81-92` flag8 门控 + 冷却退款 + `GainSwordIntent`（SkillSystem.cpp:2215）剑意回复 + 注释修正 |
| M-5 天降打击 | ✅ 已修复 | `AreaFieldDeliverySystem.cpp:118` 按 `wave_interval` 调度波次 |
| M-6 死代码/死字段 | ✅ 已修复 | Sentinel `attack_*`/`damage_mult` 消费（OrbitingSentinelDeliverySystem.cpp:84-100）；Sticky stacks 消费（ProjectileSystem.cpp:158/192）；Baker 未用 contract 变量与 BakerDummy hack 已删除 |

另核验：9 个行为文件行数修复后仍全部 ≤100（100/98/99/88/80/84/67/97/97，合计 810）。

新增测试护栏（`tests/unit/SkillSpecializationBakerTests.cpp`）：「Rebake TriggerRule Idempotency」（:113-166，多次 Rebake 规则数不变 + 洗点清空规则归零 + 无组件实体自动创建）、「BakedDeliveryParams Dedicated Fields」（:168）、「Sentinel Interception Dice Roll & Cap」（:216）。

## 6. 新发现（Low，不阻塞提交，已全部解决）

1. **卸下技能槽时 trigger 规则残留**（`SkillSystem.cpp:2289-2305`）：✅ 已解决。移除空槽对 `specialized_slots[i]` 的错误回退，增加跨槽唯一性检查后对 `old_skill_id` 调用 `SyncTriggerRules(registry, entity, old_skill_id, nullptr)` 彻底清理该技能树规则，并在 `SkillSpecializationBakerTests` 补充仅卸下技能快捷槽（专精树仍存点）场景下的规则清零回归用例。
2. **hover_timer/pauseTimer 双字段冗余仍在**（`DeliveryArchetypes.hpp`、`BoomerangDeliverySystem.cpp`）：✅ 已解决。移除冗余 `pauseTimer` 字段及其同步赋值，统一以 `hover_timer` 管理悬停。
3. **AreaField 消费方魔数未全清**（added_effectiveness=1.5f/时长5.0f 等）：✅ 已解决。已抽取具名常量并优先联动 `BakedSkillProfile`（`more_damage_mult`/`duration`/`sub_interval`/`area_radius`/`effective_tags`）参数。
4. **DeliveryArchetypes.hpp 兼容字段与 `BoomerangPhase::Paused` 别名**：✅ 已解决。移除 `BoomerangPhase::Paused` 别名、`BoomerangComponent` 内嵌常量别名，并将 `ProjectileSystem` 的悬停时间对齐至 `hover_duration`。
5. **节点 100 注释语义**：✅ 已解决。已在 `SkillSpecializationBaker.cpp:181` 注释中明确记录设计草案「攻速」与既有代码实装「突进移动速度（del.speed，基底 400，每点 +40/10%）」的语义对齐。

## 7. 剩余风险

- 旧 ChannelingComponent 管线（含 5 处 `skill_id == 5/7` 特例）按修正后 DoD 冻结至 Phase 4 清除；双写窗口内 legacy 消费方（BladeMasteryService/DamagePipeline/StatsSystem）行为面已确认兼容。
- 性能基线（`nmd.tests.perf.baseline`）本轮未复跑；B-2 修复消除了主要每帧分配，建议提交后跑一次基线记录。

## 8. 下一步动作

**提交**。全部 Blocker/High/Medium 修复项经独立复核通过，测试与构建零告警。Phase 4 清理项（旧管线移除、兼容字段、上述 Low 项）已登记于计划。
