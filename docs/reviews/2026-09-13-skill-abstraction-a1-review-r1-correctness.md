# Skill 抽象与 B2 清理 · A1 批次审查报告（第一轮·正确性）

## 审查目标

只读审查计划 `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` 的 **A1 批次（A1-0 ~ A1-7）**（工作区未提交变更，baseline = `HEAD`）生产代码改动的**正确性与稳健性**，对照设计 `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md` §4.7 DoD。重点：

1. 行为回归：A1-3 拆除 `ChannelingComponent` 后，技能5/7 引导语义（保活、元素转质、暴击/穿甲、松键收尾、结束清理）是否与拆除前等价；`BeamChannelDeliverySystem.cpp` 删除 184 行旧回滚块有无遗漏语义。
2. 伤害管线：A1-4 `ResolveSkill4Counter` 替换 `ProjectileSystem.cpp` / `DamagePipeline.cpp` 三处是否数值/归因等价；`SourceAttribution` / `ResolveCastIdFromSourceEntity` 顺序变化是否改变归因。
3. 内存/UB/容器：EnTT 视图迭代中增删组件、跨 mutation 指针失效（`conductor/code_standard.md` §5.3）；`BladeFormation.cpp` 新增 `ForEachChainCandidate` + dedup 安全性；`FlowingThrust.cpp` helper 契约。
4. 热路径：是否新增字符串比较/堆分配/临时 `std::string`（§7.2、§2.1）；`BuffIds.hpp` 枚举与 `kBuffIdNames` 一致性、ordinal 序列化隐患。
5. 组件契约：`BeamChannelComponent` 新增 4 字段、删除 `OrbitingSentinelComponent::interception_chance`、删除 `ChannelingComponent`/`ReactiveWardComponent`/`primary_archetype`/`SkillSnapshot::payload_context` 是否破坏 POD/存档/聚合初始化/static_assert。
6. 配置校验：`SkillMechanicsRegistry::LoadFromFile` schema 硬化是否误拒合法配置或漏检。
7. 数据正确性：`skill_mechanics.json` skill4 新增 key、`skill_4_tree.json` max_points、`BuffIds.hpp`。

## 结论

**修改**

理由：核心拆解正确、无 Critical/High（无内存安全/UB/并发/安全缺陷），内存与归因迁移基本等价；但存在 **1 项可复现的引导语义行为差异（Medium）** 与 **1 项死字段/注释失真（Medium）**，与计划自述的「等价迁移」不符，需设计确认或补齐基线证据后方可提交；另有若干 Low 级质量问题。

## 审查轮次

第一轮 · 正确性（静态审查，不含编译/运行；未执行 `clang-tidy`/`cppcheck`，见「剩余风险」）

## 输入

- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`
- 设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（§4.7 DoD / §5.3）
- 工作区 diff：`git diff HEAD -- src/ assets/data/`（baseline `HEAD=dbda488d087f0eae82674ee2446bd1c869c84b00`；源 28 文件 +551/-725；数据 2 文件）
- 独立复核：`git diff`、`rg`、`git show HEAD:<file>`、`ConvertFrom-Json` 比对数据、`Buff.hpp` / `SharedContext.hpp` 契约核对

## 变更文件边界

`git status --porcelain` 显示工作区未提交变更（仅源码/测试/数据/文档，无越界）：

- 源码 28 文件（+551/-725）：`application/input/InputSystem.cpp`、`application/states/GameplayState.cpp`、`contracts/impl/StatsSystem.cpp`、`foundation/components/{DeliveryArchetypes,SkillDefs}.hpp`、`foundation/data/{BuffIds.hpp,SkillMechanicsRegistry.cpp,SkillMechanicsRegistry.hpp}`、`systems/combat/{DamageMitigationService,DamagePipeline}.cpp`、`systems/combat/damage/{DamageConditions.cpp,DamageInterceptors.hpp}`、`systems/skill/{BeamChannelDeliverySystem,BladeMasteryService,OrbitingSentinelDeliverySystem,ProjectileSystem,ShadowDuplicationHook,SkillSpecializationBaker,SkillSystem}.cpp`、`systems/skill/behaviors/{BladeBoomerang,BladeFormation,BladeWard,FlowingThrust,HeavenlySwordDescent,InfiniteBlades,MindBlade,RendingWave}.cpp`、`systems/skill/behaviors/SkillBehaviorBase.hpp`。
- 数据：`assets/data/skill_4_tree.json`（+29 节点 `max_points`）、`assets/data/skill_mechanics.json`（+1 处 `451.dodge_speed_per_point`）。
- 测试 14 文件（+382/-245）、未跟踪 3 测试 + 4 文档。

## 范围对齐

- 改动范围与 A1-0~A1-7 一致；未发现 A1 范围外的生产代码改动，无编译产物/临时文件入库。
- 与完成性报告（`docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-completeness.md`）互补：本报告只判正确性，不重复 DoD#3/#8 未达成的完成性问题。

## 质量与风险评估

### 正面的点（经独立复核确认「无问题」）

- **A1-3 组件拆除彻底**：全仓 `rg` 对 `ChannelingComponent` / `ReactiveWardComponent` / `primary_archetype` / `snapshot.payload_context` 均为 0 命中（源码 + 测试）；`SkillDefs.hpp` / `DeliveryArchetypes.hpp` 中相应结构体/字段删除干净。四者均无存档/POD codec 引用（全仓无按裸内存布局序列化 `SkillSnapshot`/`BakedDeliveryParams`/`BeamChannelComponent`/`OrbitingSentinelComponent` 的代码），删除安全。
- **`BeamChannelComponent` 契约不破**：`DeliveryArchetypes.hpp:185-186` 的 `is_standard_layout` / `is_trivially_destructible` 断言仍成立（新增 `float/Tag/float/float` 均为 POD）；`BakedDeliveryParams` 的断言是 layout/析构属性而非尺寸，删字段不触发。
- **A1-4 数值等价（已证）**：`ResolveSkill4Counter`（`DamageInterceptors.hpp:33-58`）从 `GetMech(4,470,"base_damage",35.0f)` 取基伤；数据 `skill_mechanics.json` 中 `4.470 = {"counter_swords":5.0,"base_damage":35.0}`，即 `35.0 * (1 + ward.counter_damage_more)` 与旧硬编码 `35.0f * ...` **逐位相等**。三处调用点（`DamagePipeline.cpp` 单体/批处理、`ProjectileSystem.cpp`）的 attacker/defender 实参顺序经核对正确（counter_attacker=source，counter_defender=target）。
- **A1-4 归因顺序保留**：`ResolveCastIdFromSourceEntity` 由 `（exec 优先 → projectile → swordarray）` 改为 `（exec 优先 → 新 `ResolveSourceAttribution`（projectile→swordarray，返回 cast_id）→`BeamChannelComponent` 兜底）`，优先级与 `ResolveEventAttackerContext` 一致，常见的 4 类来源归因不变；`ProjectileSystem.cpp` 在 `ResolveDamage` 前缓存 `spin = ward->counter_spin`，规避了伤害回调销毁组件后的悬垂指针，属**正确性改进**。
- **引导语义迁移大体等价**：元素转质（`conversion_tag`）、暴击/穿甲（`bonus_crit_chance`/`bonus_armor_pen`）、`bonus_damage_mult`/`is_empowered` 均由 `BeamChannelComponent` 承接（`HeavenlySwordDescent.cpp:516-519` 技能5 双写点删除后 beam 分支保留）；`ChannelingComponent` 的 `channel_timer` 语义由新字段 `BeamChannelComponent::channel_timer` 承接，技能7 松键收尾 `BeamChannelDeliverySystem.cpp:506-507` 与旧 `:510-516` 等价。
- **`BuffIds.hpp` 一致**：枚举新增 `SpiritCorrosionFire`/`SpiritCorrosionLightning`（Count=30）与 `kBuffIdNames` 30 项逐项对应；字符串与旧中文字面量 `SpiritCorrosion_Fire`/`SpiritCorrosion_Lightning` 逐字节一致；新项追加在 `Count` 之前，无 ordinal 位移；`BuffIdToString`/`BuffIdFromString` 按索引查表，无序列化隐患。
- **配置 schema 不误拒**：`LoadFromFile` 顶部 `m_nodes.clear(); m_loaded=false;`，解析失败保持一致的未加载态；`TryParseUint32Id` 拒绝非数字/溢出，`node id 0` 合法，`skill_id>0`；对 `skill_mechanics.json` 全量静态核对：顶层 `version/comment/1..9`，叶子**全为数值**（无非数值叶子），当前数据可被硬化 schema 完整接受，未见误拒/漏检。
- **数据一致**：`skill_4_tree.json` 与 `skills.json` 内联技能4 树均为 29 节点且 `max_points` 逐项相等；`ailment_contracts.json` Chill `max_stacks=3` 与 `1.173.chill_stacks_to_freeze=3.0` 一致，`FlowingThrust.cpp` chill 阈值改写等价；`AddOrRefresh`（`Buff.hpp:185`）为 `const BuffEffect&` 且只读复制，`SkillSystem.cpp` 函数内 `static const kBladeWardPowerBuff` 模板安全。
- **`AddTalentPoint` 仅增诊断**：`has_reachable_prereq` 不改变功能前置判定，`tree->nodes.at(pre_id)` 已由 `contains` 守卫，无越界。
- **热路径字符串比较清零**：`behaviors/` 与 `DamageConditions.cpp` 已无 `.find("Slow"/"ignite"/"shock")`；`BladeFormation.cpp:521` 改 `b.type==BuffType::Burn || b.kind==BuffKind::Ignite`、`:439/:480` 改 `b.type==BuffType::Shock`，覆盖 AilmentEngine 托管 buff（type=Burn/Shock、kind=Ignite/Shock）与技能自建 `"Ignite"` buff，语义完整。

### 风险

- **行为等价性未运行验证**：计划自述「测试全绿」本轮无法独立复现（禁编译），引导时序、反击伤害实测归因均由静态推演得出。
- **发现 C1 属设计意图未确认的 delta**：可能是「修复了旧死写入」，但仍改变相对 HEAD 的数值行为，须设计裁决 + A1-0 基线对比。

## 发现项

> 严重度：Critical / High / Medium / Low。未发现 Critical / High。

### M1（Medium）引导节奏加成由「死写入」变为「生效」，相对 HEAD 存在行为差异

- 位置：`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:470`、`src/game/systems/skill/BladeMasteryService.cpp:84`、`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:669`
- 问题：旧代码写 `ChannelingComponent::tick_interval` / 读 `ChannelingComponent::tick_interval`；但 HEAD 的 `BeamChannelDeliverySystem.cpp` 交付路径只消费 `BeamChannelComponent::tick_interval`（技能5 在 `InfiniteBlades.cpp:242-243` 于施放时把 `chan.tick_interval` 一次性拷贝到 `beam`，此后不再同步），且旧回滚块的 `if (any_of<BeamChannelComponent>) continue;`（HEAD `:1336`）会跳过技能5，故 HEAD 上「天降剑 + 旋舞节奏」加成（`ApplyHeavenlySwordSpinningBonus` / `CleanupSpecializedFields`）**从未作用到技能5 光束**。A1-3 将写点/读点迁到 `BeamChannelComponent` 后该加成**变为生效**（对未点 501 的构筑；点 501 时该值每 tick 由 `BeamChannelDeliverySystem.cpp:1005` 重算覆盖）。这是一处相对 HEAD 的数值行为变化，而非等价迁移。
- 建议：请设计确认这是否为预期修复；若是，在计划/设计显式记录行为变更并在 A1-0 基线对比中给出证据；若非，需恢复旧语义或补充说明。评审建议：**修改**。

### M2（Medium）技能5 的 `BeamChannelComponent::channel_timer` 为死字段，注释声称「输入保活」失真

- 位置：`src/game/systems/skill/behaviors/InfiniteBlades.cpp:129`、`src/game/systems/skill/SkillSystem.cpp:2104`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp:506-507`
- 问题：`channel_timer` 仅在 `UpdateMindBladeBeam`（技能7）被 `-= dt` 并判定松键；技能5（`BarrageEmitter`）既写 `InfiniteBlades.cpp:129`（5.0f）又由输入保活 `SkillSystem.cpp:2104`（0.25f）刷新，但交付路径从不读取，故该字段对技能5 完全无效。HEAD 同样如此（旧回滚块跳过 beam 实体、技能5 也未被读），**非回归**；但新代码注释（`InfiniteBlades.cpp:129` 等）宣称其为技能5「输入保活窗口」，易误导后续维护者，且新增字段引入不必要状态。
- 建议：为技能5 明确保活/松键的真实机制并同步注释；若确无用途，考虑移除技能5 的写入或在字段注释标注「仅技能7 消费」。评审建议：**修改**（文档/注释级）。

### L1（Low）A1-1 热路径新增临时 `std::string` 堆分配

- 位置：`src/game/systems/skill/behaviors/BladeFormation.cpp:542`、`:558`（对照 `src/game/foundation/components/Buff.hpp:247-248`）
- 问题：原 `std::string debuffId="SpiritCorrosion_Fire"` 改为 `BuffId` 枚举后，`fx.Get(debuffId)` 经 `Get(BuffId)` 内部 `Get(std::string(BuffIdToString(id)))` 仍构造临时字符串；同处 `existing->id = std::string(BuffIdToString(debuffId))` 再分配一次。相对原实现未减少分配（甚至按命中一次 +1），与 §7.2「零分配」取向不符。正确性无影响（SpiritCorrosion 无对应 `BuffKind`）。
- 建议：为 `ActiveEffectsComponent` 增加 `Get/Find(BuffId)` 的重载（内部可直接比对枚举映射，或缓存 `BuffId→id` 视图），消除逐次分配。

### L2（Low）`ResolveSkill4Counter` 仍硬编码 `35.0f` 作为 GetMech 兜底

- 位置：`src/game/systems/combat/damage/DamageInterceptors.hpp`（`ResolveSkill4Counter`，`GetMech(4,470,"base_damage",35.0f)`）
- 问题：DoD#1「数值零硬编码」仅靠兜底默认值满足；本次未在数据中显式确认该 key 的单一权威性（虽当前 `4.470.base_damage=35.0` 与之相等，数值等价成立）。属声明与实现的措辞差距。
- 建议：在设计/映射表登记 `4.470.base_damage` 为权威来源，或统一「无数据即用兜底」策略并注明。

### L3（Low）`DamageInterceptors.hpp` 引入技能行为头，分层耦合

- 位置：`src/game/systems/combat/damage/DamageInterceptors.hpp:3`（`#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"`）
- 问题：底层战斗伤害头经由 `SkillBehaviorBase.hpp → SkillSystem.hpp + SkillMechanicsRegistry.hpp + SkillDefs.hpp` 反向拉入技能行为/系统层，虽 `#pragma once` 保证不产生编译失败，但破坏层次依赖方向并放大编译耦合/重编范围。
- 建议：将 `ResolveSkill4Counter` 下沉到仅依赖 `SkillDefs.hpp` + `SkillMechanicsRegistry.hpp` 的轻量头（或改为自由函数声明 + 定义在技能侧 `.cpp`），避免战斗头依赖行为层。

### L4（Low）`ResolveCastIdFromSourceEntity` 兜底顺序存在组合边界

- 位置：`src/game/systems/skill/DamagePipeline.cpp`（`ResolveCastIdFromSourceEntity`，`SourceAttribution` 重写后）
- 问题：新逻辑在 projectile/swordarray 命中但 `cast_id==0` 时会继续落到 `BeamChannelComponent` 兜底；若同一实体同时具备「cast_id 为 0 的 projectile/swordarray」与 beam，则返回 beam cast_id，HEAD 在此情形返回 0。该组合极罕见，且仅影响归因（触发源）而非伤害数值。
- 建议：如需严格保持 HEAD 语义，可在 projectile/swordarray **命中即返回**（即使 0），或补充注释说明该放宽为有意为之。

### L5（Low）`ForEachChainCandidate` 网格路径忽略 `onCandidate` 的停止信号

- 位置：`src/game/systems/skill/behaviors/BladeFormation.cpp:334-359`
- 问题：线性回退路径在 `onCandidate` 返回 false 时 `break`；网格路径 `(void)onCandidate(e)` 忽略返回值，`chainsLeft` 用尽后仍遍历全部候选（仅性能，无正确性影响；dedup 数组长度 3 == 最大链数，无越界）。函数注释已声明此限制。
- 建议：可接受；如需一致语义，网格查询可先收集再用可中断循环处理，或在使用处对已用尽情形提前短路。

## 最佳实践建议

- 为「迁移是否等价」建立可复核证据：对 M1/M2 这类写点/读点迁移，建议在设计或映射表中以「旧读点 → 新读点」表格显式登记，并对每个发生「死写入变生效」的位置给出基线/实测对比。
- `BeamChannelComponent` 现同时承载技能5/7 两种语义（`mode` 分流），建议为字段按技能标注消费方（消费矩阵），避免再次出现仅单一技能消费却被双技能写入的死字段。
- `ActiveEffectsComponent` 增加 `BuffId` 直查重载，落实 DoD#2/§7.2 的零分配目标，同时简化调用点。

## 剩余风险

- 未编译、未运行：`clang-tidy`/`cppcheck`/ctest 未执行；M1/M2 的运行期表现与测试全绿结论均为静态推演或计划自述。
- 反击伤害的**归因**（非数值）在批处理/多来源并发场景未做运行时验证。
- A1-0 性能基线文档自述「无可用运行环境」，热路径开销无法比对。

## 下一步

1. 就 M1（引导节奏加成的行为差异）与设计确认是否为预期修复；若预期，更新设计/计划并补 A1-0 基线对比证据；若非，恢复等价语义。
2. 修正 M2 的注释/字段消费标注（或移除技能5 死写入）。
3. 按需处理 L1~L5（建议至少落实 L1 零分配重载与 L3 分层解耦）。
4. 在与完成性报告的整改合并后，于具备运行环境时执行编译 + `clang-tidy`/`cppcheck` + ctest + 基线复跑，再进入第二轮（运行时行为/性能）审查。
