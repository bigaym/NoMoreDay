# 技能抽象化与 B2 结构清理设计 (Skill Abstraction & B2 Structural Cleanup Design)

- 日期：2026-09-13
- 状态：设计修订版 / 评审意见已并入（Rev.2，原状态 Draft for Review；第 1 轮评审结论 `修改`，见 §12）
- 范围：A-01 技能行为层抽象边界重定；B2-01..B2-24 结构清理并入 A 第一批；A-02/A-03 技能 10~12 迁移的目标结构；RD-16/RD-17/RD-13/RD-04 的依赖处理
- 前置文档：`docs/workflows/design.md`；`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（权威活清单）
- 技术栈规范参考：`conductor/tech-stack.md`、`conductor/code_standard.md`（V2.1）、`conductor/code_styleguides/`
- 上游设计：`docs/designs/2026-09-06-modern-skill-system-design.md`（五大支柱）、`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`（DOD 组合 / 12 原型 / AOT 烘焙）
- 上游评审：`docs/reviews/2026-09-07-modular-skill-archetypes-implementation-review.md`（§7/§9/§10 残留项）、`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`（B1 第一波，结论 `提交`）
- 结项计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`（P4 已结项，删除清单为空）
- 本次不做：任何 `src/`、`tests/`、`assets/` 生产改动；不编译、不跑测试

---

## 1. 问题陈述与代码证据 (Problem Statement & Code Evidence)

### 1.1 A-01 DoD 争议：从「≤100 行」到 4547 行的事实演进

模块化重构计划（2026-09-06）与二轮评审（2026-09-07）记录：重写后 9 个技能行为文件为 100/98/99/88/80/84/67/97/97 = 810 行，恰好达标「每个行为文件 ≤100 行」的旧 DoD。

但该状态是**节点语义尚未实现**时的中间态。随后技能 1~9 专精收尾工作（B1 批次）逐节点补全了行为语义，行为文件重新增长。本次复核实际行数（`src/game/systems/skill/behaviors/*.cpp`）：

| 文件 | 行数 | 文件 | 行数 |
|------|------|------|------|
| FlowingThrust.cpp | 955 | InfiniteBlades.cpp | 526 |
| HeavenlySwordDescent.cpp | 833 | BladeFormation.cpp | 517 |
| PhantomTrance.cpp | 747 | SwordArray.cpp | 496 |
| RendingWave.cpp | 700 | BladeBoomerang.cpp | 383 |
| SevenStarSlash.cpp | 681 | BladeWard.cpp | 137 |
| BloodSea.cpp | 560 | MindBlade.cpp | 86 |

- 技能 1~9 合计 4547 行（清单记 4542）；全 12 技能 6621 行（清单记 6615）。
- `SkillSpecializationBaker.cpp` 实测 1087 行（清单记 1117），含 **2 个 switch × 9 个 case = 18 个 `case N:`**：switch#1 交付原型（`:63-113`）、switch#2 节点修正（`:237-914`）。

**结论**：≤100 行/文件是「节点行为缺失」下的产物，不是稳定架构；当前行数反映的是真实节点内聚逻辑。这一点必须在 A-01 的 DoD 上正面裁决，不能靠再次「搬走逻辑」假装达标。

### 1.2 Baker 双源与节点语义外溢

节点数值/存在性目前有两条并行来源，且部分互不消费：

- **Baker 烘焙 flags**：`SkillSpecializationBaker.cpp` 将节点写成 `feature_flags` 位（如 InfiniteBlades 路径消费的 2/4/128/16384/32768/262144/4194304/16777216/67108864）与交付参数；其中多个 flag **无消费点**（写而不读）。
- **行为层 `allocated_points`**：行为文件直接 `spec.allocated_points.find(NODE)` 判定节点是否点亮。`InfiniteBlades.cpp` 单文件重复 9 处（`:168,265,267,269,271,273,275,277,279`）；`BeamChannelDeliverySystem.cpp` 同型循环重复 6+ 处。
- 同一份「节点是否点亮」在交付层读 flags、在效果层读 `allocated_points`，构成**双源语义**；`skill_mechanics.json` 已有数值但被硬编码绕过。

证据链示例（B2-06）：`SkillSystem.cpp:530 void SkillSystem::InitHooks()` 内 451(`:1111-1124`)、455(`:1115,:1144`)、432(`:1171-1184`)、435(`:1175-1192`) 的数值（10f×pt、0.15f×pt、More+20% 等）与 `skill_mechanics.json` 双写，且用 `std::max(...)` 合并，导致数据侧改动无效。

### 1.3 B2 结构债现状核查（逐项结论表）

复核方法：`codebase-memory-mcp` 图工具（定义/调用链）+ 同步 `rg`（引用集合）+ Read（中文注释/上下文）。标注 = **仍然成立 / 已变化 / 已不存在**。

| 条目 | 本轮核实结论 | 关键证据 |
|------|--------------|----------|
| B2-01 ChannelingComponent legacy 清理 | **仍然成立** | 定义 `SkillDefs.hpp:1320-1339`；src 引用恰为清单所列 **12 文件**，rg 64 匹配；含 `BeamChannelDeliverySystem.cpp:1288-1289` 移除、`:1332` 注释「兼容并存旧版 Channeling（平滑回滚窗口）」 |
| B2-02 双管线共存窗口审计 | **仍然成立** | 旧 Channeling 消费者与 BeamChannel 管线并行；`:1158` 读、`:1288` 移，缺逐点审计 |
| B2-03 技能6 SwordArray 特例 / 技能9 ProcEngine rule 9 | **已变化（大幅收窄）** | `ProcEngine.cpp`（154 行）无硬编码 rule 9，走通用 `required_skill_id`(`:54-55`)；`SkillSystem.cpp:1080-1092` 为通用事件监听注册。技能6 残留仅 `DamagePipeline.cpp:289-295`/`:454-456` 的 `try_get<SwordArrayComponent>` 归因分支 |
| B2-04 技能4 反击 5 处合 helper | **成立（但计数需订正）** | 直查 3 处伤害站点：`ProjectileSystem.cpp:689-710`、`DamagePipeline.cpp:1446-1475`（Calculate）、`DamagePipeline.cpp:2122-2150`（Batch）；均含硬编码 `35.0f` 且未读 `mechanics(4,470,base_damage)` |
| B2-05 TagRegistry 单源 | **仍然成立** | `TagRegistry.hpp` 为生成文件；手写 `GetTagName`/`TagFromString`(`:98-178`) 与生成 `kTagInfoTable`(31) 并存，后者仍被 `SkillDefs.hpp:368,377` 消费 |
| B2-06 InitHooks 读 mechanics + static const | **仍然成立** | `SkillSystem.cpp:530,1111-1192`；见 §1.2 |
| B2-07 OrbitingSentinel 死分支 | **仍然成立** | C6 以 `BladeWard.cpp:110 interception_chance=0.0f` 关闭；系统仍每帧 `Update()`，拦截分支不可达 |
| B2-08 ReactiveWard（依 RD-16） | **仍然成立** | `DeliveryArchetypes.hpp:265-275`；仅 `timer/ward_duration` 被 `SkillSystem.cpp:1251-1265` 消费，`counter_window/triggered/damage_absorb_pool/counter_skill_id` 在技能4 路径零消费；`BladeWard.cpp:113-117` 仍 emplace |
| B2-09 M7/M8/M9 | **仍然成立** | M8：`SkillSystem::AllocateSkillNode`(`:1919-1940`) 只查 required_points 不查 max_points 可达性 → 静默死路；M9：`skill_4_tree.json` 缺 max_points（与 skills.json 双源）；M7 476 双前置为布局锚点 |
| B2-10 技能5 N10 提取 helper / 双源统一 | **仍然成立** | `InfiniteBlades.cpp` 9 处 + `BeamChannelDeliverySystem.cpp` 6+ 处；flags 与 `allocated_points` 语义分裂 |
| B2-11 清理 Channeling 旧注释 | **仍然成立（行号已变化）** | 清单引 `SkillDefs.hpp:1197-1216` 已失效；该区间现为 SwordArrayComponent Branch D 字段 + `struct ExecutedTag{}`(`:1216`)；Channeling 旧注释在 `:1338`（`synergy_lock`：旧 730 废弃，改由 754 承接） |
| B2-12 双组件结构冗余收敛 | **已变化（部分不存在）** | 二轮评审记录 `BoomerangComponent` 纯兼容别名（`BoomerangPhase::Paused`、内嵌常量）已删；`DeliveryArchetypes.hpp:67-109` 余字段为功能字段 → 需逐字段复核后再决定是否仍需收敛 |
| B2-13 373 O(N) 敌遍历空间网格化 | **仍然成立** | `BladeFormation.cpp:437-450` 线性遍历附近敌人；`SpatialGrid.hpp` 已存在且被 AreaField/Boomerang/OrbitingSentinel 使用 → 可直接复用 |
| B2-14 374 debuffId enum 化 | **仍然成立** | `BladeFormation.cpp:500-518` `std::string debuffId = ...` 二选一字符串 |
| B2-15 351 数据消费统一 + ArmorShred 命名冲突 | **仍然成立** | BladeOrbit/RetaliationWeb 双源；ArmorShred id 冲突见 `RendingWave.cpp:537` / `FlowingThrust.cpp:412` |
| B2-16 714 精确化 | **已不存在（建议销项）** | Baker `:1084-1095` 已用通用 `required_skill_id`（714→7）；测试 `MindBladeNodes.cpp:468,721` 已断言 |
| B2-17 primary_archetype 死字段（RD-17） | **仍然成立** | `SkillDefs.hpp:615`；写入共 14 处（Baker 13 处 `:64-364` + **`BladeBoomerang.cpp:109` 默认交付**），读取仅测试 3 处（`SkillSpecializationBakerTests.cpp:44,70,221`），生产零读 → 死字段；删除时影响面必须含 BladeBoomerang.cpp 与 BakerTests |
| B2-18 Slow/Chill legacy 收编 ailment | **仍然成立** | `FlowingThrust.cpp:533-583` 仍造 `"FrostSlow"`/`"FrostChill"` 字符串 buff；`:578-580` 依赖 `Get("FrostChill")->stacks`；注释自承认「Slow 异常未注册 ailment 契约」 |
| B2-19 技能1 第4轮遗留 | **成立（3 项之一）** | `SkillDefs.hpp:777 DamagePayloadContext payload_context{};` 写入 4 处（`SkillSystem.cpp:1537,:1605,:2102`、`ShadowDuplicationHook.cpp:77`）、无读取 → 死数据（删除须连带 4 写点）；另 2 项（UMR merge skip、ShadowDuplicationHook 特例）清单 §6 已注册接受 |
| B2-20 烘焙→消费映射表 | **成立（缺失）** | 仓库无该表；flags 无消费点问题（§1.2）无法系统性发现 |
| B2-21 DeliveryArchetypesTests 技能6 夹具 | **仍然成立** | `tests/integration/DeliveryArchetypesTests.cpp:191,194,207` 用 skill_id=6 / leave_field_skill_id=6 / source_skill_id==6 |
| B2-22 技能12 绝影共噬 | **成立（跨技能不完整）** | `BloodSea.cpp:41 PhantomDevour=1217`、`:365 field.has_linked_synergy=HasBloodSeaNode(...)`；技能9 `PhantomTrance.cpp` 未检出 1217 消费 |
| B2-23 deprecated CalculateBatch 清理 | **建议推迟/降级** | `DamagePipeline.hpp:53-55 [[deprecated]]`；生产入口为 `DamageResolutionHooks::ResolveDamageBatch`(`.hpp:47`)。伤害管线 P4（2026-09-12）已结项且「删除清单为空」→ 该入口有意保留给测试/基准；迁移将大面积改动测试（10+ 文件）而收益低 |
| B2-24 热路径字符串比较 | **仍然成立** | `DamageConditions.cpp:19 id.find("Slow")`；`BladeFormation.cpp:483 b.id.find("ignite")`；测试侧同型 `SwordArrayNodes.cpp:408`、`BladeFormationNodes.cpp:764` |

---

## 2. 目标与非目标 (Goals & Non-Goals)

### 2.1 目标

1. **裁决 A-01 抽象边界**：给出可执行、可度量、符合 DOD/性能约束的技能行为层结构与 DoD。
2. **B2 全部条目并入 A 第一批**：按文件/系统聚类，杜绝同一批文件改两遍。
3. **消除双源与硬编码**：Baker/behavior/mechanics 三者的数据归属明确、可校验。
4. **消除热路径字符串比较与 legacy 兼容管线**。
5. **定义技能 10~12 的目标结构**，使其与技能 1~9 使用同一抽象信封，不再新增特例。

### 2.2 非目标

- 不引入通用「效果描述符 DSL / 全数据驱动技能引擎」。
- 不追求「行为文件 ≤100 行」这一旧 DoD（见 §4）。
- 不重写已结项的伤害管线（P0~P4），不做 B2-23 强制迁移。
- 不修改既有术语、层级、命名；不迁移动画/VFX/美术资产。
- 本次仅产出设计/计划文档，不含生产代码。

---

## 3. 设计支柱 (Design Pillars)

沿用上游五大支柱，并新增第 6 条作为本次抽象裁决的依据：

1. **Tag-Driven**：标签驱动，注册表在加载期把 JSON 字符串映射为 ID。
2. **Item Skill Modifiers**：装备修正与技能解耦。
3. **Delivery + Payload 分离**：交付形态（原型组件）与伤害载荷解耦。
4. **Unified Proc Engine**：统一触发器，无技能硬编码。
5. **DOD / 无虚函数分派**：POD 组件、线性系统迭代、热路径零分配。
6. **数据单一来源（Single Source of Truth，本次新增）**：任一节点的「是否点亮」只有一处权威判定；数值只来自 `skill_mechanics.json` / 契约生成物。

---

## 4. A-01 核心裁决：抽象边界与接口形态

### 4.1 候选方案评估

| 维度 | 方案 A：恢复薄装配（≤100 行/文件） | 方案 B：承认内聚，抽取「触发/效果/交付」三层参数化 |
|------|-----------------------------------|---------------------------------------------------|
| 实现方式 | 把节点逻辑下沉到数据/烘焙（DSL 或描述符表），行为文件仅装配 | 行为文件保留节点逻辑作为「效果层」，统一 SpecState 与共享 helper，数据只承载数值/交付参数 |
| 对 4547 行事实 | 需把数百节点语义编码为数据，等于新建 DSL | 与现状兼容，增量收敛 |
| 双源风险 | **加重**：复杂度迁移到 Baker 层，正是已 18 case、易漂移之处 | **降低**：SpecState 成为唯一解析点 |
| 性能 | 通用描述符引擎可能引入间接跳转/分配 | 保持零分配、零字符串、零虚函数，链路更短 |
| 可测性 | 节点语义散在数据中，单测须经数据加载 | 行为层可独立单测；数据由 `gen_skill_contracts.py --check` 守 |
| 风险/可回滚 | 高风险，无中间可交付状态 | 低风险，按文件/技能增量提交 |
| 与既有实现一致性 | 偏离「行为即效果」，需重写全部行为文件 | 与 `SkillBehaviorBase` CRTP + `DeliveryArchetypes` 完全一致 |

**裁决：采用方案 B**，但以「数据驱动的收敛」为强制约束——方案 B 不等于放任硬编码，而是把数值/交付参数强制归位到数据侧、把节点逻辑收敛为可测的效果层。

### 4.2 推荐方案的抽象边界

```text
+-------------------------------------------------------------+
| 触发层 Trigger                                               |
|  - SkillContract / TriggerRuleComponent（数据/契约驱动）       |
|  - ProcEngine 通用过滤（required_skill_id / node_id / crit）   |
|  - SkillBehaviorBase::OnCast / OnHit（CRTP 入口）             |
+-------------------------------------------------------------+
| 效果层 Effect（行为文件保留，内聚但受约束）                    |
|  - 每技能一个 POD XSpecState（DoCast 一次解析，向下传递）      |
|  - 共享 helper：节点点读、异常施加、buff 施加、反击、AoE 查询   |
|  - 所有数值 <- skill_mechanics.json（经统一读取 helper）       |
|  - 节点存在性 <- 唯一 HasNode(spec,id)（allocated_points）     |
+-------------------------------------------------------------+
| 交付层 Delivery                                              |
|  - DeliveryArchetype POD 组件 + 对应 DeliverySystem           |
|  - 参数 <- Baker 烘焙的交付参数（仅交付参数，不含节点语义）      |
+-------------------------------------------------------------+
```

边界规则：

- **触发层不得出现技能专有分支**；凡技能专有触发条件一律以 `TriggerRule` 字段表达。
- **效果层允许技能专有逻辑**，但禁止：字符串比较、热路径堆分配、`dynamic_cast`、magic number、直接读 Baker flags 判节点。
- **交付层只消费交付参数**，不再作为「节点是否点亮」的第二判定源（flags 用途二分见 §4.4.1）。
- **读点 API 单一**：任何层级读取 `allocated_points` 只允许经 §4.4 的共享 helper；UI/适配层与行为层共用同一 API，不允许各自封装。

### 4.3 接口形态取舍（枚举 / 契约 / 注册表 / pipeline hook）

| 机制 | 用途 | 采纳 | 理由 |
|------|------|------|------|
| 枚举（`BuffKind`/`BuffId`/`DeliveryArchetype`/`DamageOrigin`） | 数值类别与离散身份 | **采纳** | 编译期、零字符串、可 `static_assert` 同步；已有成功先例（`DamageConditions.hpp:53`） |
| 契约（`SkillContract` / `skill_mechanics.json` / `GetNodeContract`） | 节点数值与修正 | **采纳** | 加载期解析、生成物可校验，符合支柱 1/6 |
| 注册表（`AilmentEngine` / `BuffEffect` / 系统注册） | 跨技能共享「效果类别」 | **采纳** | 已存在；只注册「类别」，不注册「技能专有效果」 |
| pipeline hook（伤害管线前后挂点） | 全局伤害修正 | **不新增** | 伤害管线已结项；新增挂点会破坏性能基线与稳定性 |
| 通用效果描述符枚举（node→effect 大表） | 全技能数据驱动效果 | **不采纳** | 节点效果异质，粗粒度丢语义、细粒度即 DSL；风险最高 |

**结论**：效果层用「显式 helper + SpecState POD」而非注册表分派——零间接、编译期、可单测。仅在「跨技能共享的效果类别」（异常、buff、交付）使用注册表/枚举。

### 4.4 SpecState 模式规范化

`SevenStarSlash.cpp` 已存在可复用样板：`:67 struct SevenStarSlashSpecState`、`:158 SevenStarSlashSpecState ResolveSpecState(registry, owner)`，内部用 `readPoints` / `allocated_points.contains`。已知缺陷：现样板约 25 字段逐行赋值，且 `ResolveSpecState` 在循环内 7 次 `find`/`contains`（结构指标 linear_scan_in_loop=7）；照此逐字段复制到 12 技能将产生数百行纯赋值样板。

设计动作：

1. 抽出通用、POD、零分配的约定：每技能定义 `XSpecState`（已解析点数、effectiveness、关键 flags），在 `DoCast` 解析一次并存入组件（POD cache），`DoTick`/`DoHit`/helper **只读缓存**，禁止在循环/每帧内重复 `find`。该「一次解析、向下只读」条款为规范要求，非建议。
2. SpecState 解析代码**生成化（优先）**：扩展 `scripts/gen_skill_contracts.py`，由树 JSON + 行为声明生成 `XSpecState` 结构与 `ResolveSpecState` 函数（`--gen-specstate`），与 DoD#6 幂等校验共用工具链；手写路径仅作为生成器未覆盖技能的过渡方案，且须用生成表（node id → 成员偏移表）替代逐字段赋值。
3. 点读 helper 的**挂载层**：`ReadPoints` / `HasNode` / `GetMech` 定义为 `skills` 命名空间自由函数，**声明于 foundation 头 `game/foundation/components/SkillPointAccess.hpp`**（由 `SkillBehaviorBase.hpp` include 复用，不入 `SkillBehaviorBase` 成员），使行为层与以下非行为层消费点共用同一 API，避免出现第二套读点写法：UI 层（`GameUiCommandHandler_SkillAstrolabe.cpp`、`GameUiSnapshotBuilder.cpp`、`GameUiSnapshot.hpp`、`UISkillSpecRenderer.cpp`、`UISkillTalentTree.cpp`）、`SkillSpecModifierAdapter.cpp`、`BeamChannelDeliverySystem.cpp`。B2-10 的 9 处 + 6 处重复由此消除。〔实施订正：原定声明于 `SkillBehaviorBase.hpp`，但该头 include `SkillSystem.hpp`，UI/战斗层引用会造成反向依赖；落于 foundation 层后 A1 已将上述全部消费点（含 UI 层）迁移至该 helper，`allocated_points.(find|contains|count)` 直查全仓归零。〕
4. `GetMech(skill_id, node, key, default)` 形态裁决：实现为 `SkillSystem` 上的启动期一次性加载查表（`skill_mechanics.json` 位于 `assets/data/`），运行期纯 `constexpr` 查表零分配；加载期校验 schema（`comment`/`version` 为保留 key，不参与查表；缺失技能 key 视为配置错误）。**missing key 策略：加载期断言失败（配置错误），运行期不静默返 0**——静默 0 会把调平错误变成隐性 nerf。〔实施订正：`SkillMechanicsRegistry::LoadFromFile` 已做加载期 schema 完整性校验（必需技能 1~9、节点段与数值叶子、保留 key），结构损坏即失败；运行期 `GetMech` 对缺失键返回**调用点声明的语义默认值**（如 `base_damage` 默认 35.0f，等于设计值，非裸 0）。逐键必填清单断言列为 A2 增强项。〕
5. `ResolveElementalConversion`（`SkillBehaviorBase.hpp:22-77` 硬编码 170/370/570、172/270/474/572、272/372/472）改为契约/数据驱动，保留常量作为 fallback 并加测试。

#### 4.4.1 flags 用途二分准则（防止 helper 误迁移）

现有 52 处 `profile ? ((profile->delivery.feature_flags & N) != 0) : (getPoints(Node) > 0)` 分两类，迁移方式不同：

| 类别 | 判定特征 | 示例 | 迁移方式 |
|------|----------|------|----------|
| 节点语义（是否点亮某能力） | 判定结果改变行为分支/开启效果 | SwordArray `has_execute`、`is_fire_field`、`is_mobile_aura`、RendingWave `IntentBurst` | 迁 `HasNode(spec, id)`（A1-1） |
| 交付参数消费（回填交付结构体字段） | 结果写入 DeliveryArchetype/交付 POD 的参数位 | SwordArray.cpp:181-251 的 `array.has_slow/has_armor_shred/has_cage/...` 回填 | 不迁 helper；归 A1-3 交付参数化，由 Baker 烘焙单源供给 |

判定准则：**迁移后消费点是否仍需知道「节点」概念**——不需要（只消费布尔参数）则属交付参数，留在 Baker 侧；需要（效果层分支逻辑）则属节点语义，用 helper。实施时按此准则逐处归类，禁止把交付参数判断改成节点查询（那会加深 Baker/行为耦合，与边界规则相悖）。

### 4.5 数据归位规则（哪些进 `skill_mechanics.json`）

- **进数据**：一切可调数值（伤害、百分比、持续时间、层数、范围、冷却）、节点修正、元素转换映射。
- **留代码**：结构/控制流（顺序、条件组合）、组件生命周期、系统迭代。
- **判定规则**：若一个数值在调平时可能需要改动，它必须能被非程序员在不改 C++ 的前提下修改；否则视为 magic number（违反 `conductor/code_standard.md` §9）。
- **单一来源**：行为层只经统一 helper 读数据；Baker 只烘焙交付参数。B2-20 映射表登记每个 Baker flag 的消费点。

### 4.6 性能约束映射（热路径）

| 约束 | 现状 | 设计动作 |
|------|------|----------|
| 零堆分配 | 大体满足；`SkillSystem.cpp:1111-1192` 构造 `std::string` 临时 | B2-06 改 `static const` / 复用 POD，消除事件期分配 |
| 零字符串比较 | `DamageConditions.cpp:19`、`BladeFormation.cpp:483` 违反 | B2-24 改 `BuffKind`/`BuffId` |
| 无虚函数/dynamic_cast | 满足（CRTP + POD） | 方案 B 不得引入间接分派 |
| POD/standard-layout 组件 | `DeliveryArchetypes.hpp` 已 `static_assert` | 保留并扩展到新 `XSpecState`（standard-layout、trivially destructible） |
| EnTT 指针失效 | `code_standard.md` §5.3 | helper 内先复制 POD 再改 registry，禁止跨变更持指针 |

### 4.7 可度量 DoD（替代「≤100 行/文件」）

A-01 完成的判定（全部可脚本/静态校验）：

1. **数值零硬编码**：行为文件与 `SkillSystem::InitHooks` 中不存在未走 helper 的调平数值。
2. **热路径零字符串比较**：`rg` 检索 `behaviors/` + `DamageConditions.cpp` 无 `.find("` / `strcmp` / `std::string ==` 判定分支。
3. **节点判定单源**：不存在「同一节点同时用 flags 与 `allocated_points` 两处判定」；由 B2-20 映射表 + 断言守。
4. **SpecState 全覆盖**：12 个技能均在 `DoCast` 一次性解析 POD SpecState。
5. **无 legacy 兼容管线**：`ChannelingComponent` 不再作为交付路径；`BeamChannelDeliverySystem` 回滚窗口关闭（B2-01/02）。
6. **契约/数据校验通过**：`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`。
7. **SpecState 解析测试**：每技能 `allocated_points → XSpecState` 字段映射有表驱动单测（A1-6 交付；若解析代码为生成器产出，则断言生成物与手写基线等价一次）。
8. **读点 API 单一**：`rg` 检索全仓 `allocated_points\.(find|contains|count)` 直查点为 0（仅 helper 内部与生成代码允许）。
9. **量化结构指标**：行为文件行数仅作为**观测值**记录，不作为门槛；门槛改为「共享 helper 外无重复模式」。

> 说明：明确保留「行长」作为信息项而非 DoD，避免再次出现「靠搬移逻辑达标、节点语义缺位」的假达标。

---

## 5. B2 处置矩阵

处置 = **重构 / 删除 / 推迟 / 销项**。批次见 §6。

| 条目 | 处置 | 归属批次 | 依赖/备注 |
|------|------|----------|-----------|
| B2-01 ChannelingComponent legacy 清理 | 重构+删除 | A1-3 | 12 文件；与 B2-02/11/12 同批 |
| B2-02 双管线共存窗口审计 | 重构 | A1-3 | 逐点审计 BeamChannel 旧窗口，关闭回滚 |
| B2-03 技能6 特例 / 技能9 rule9 | 重构（rule9 已不存在，仅剩技能6 归因） | A1-4 | ProcEngine 无需改；仅 SwordArray 归因分支 |
| B2-04 反击合 helper | 重构 | A1-4 / A1-5 | 计数订正为 3 站点（见 §1.3 注） |
| B2-05 TagRegistry 单源 | 重构 | A1-1 | 生成器 + header 收敛 |
| B2-06 InitHooks 读 mechanics | 重构 | A1-1 | 451/455/432/435 |
| B2-07 OrbitingSentinel 死分支 | 删除 | A1-5 | |
| B2-08 ReactiveWard | 删除（依 RD-16 推荐） | A1-2 | RD-16 裁决门 |
| B2-09 M7/M8/M9 | 重构（M8 加诊断；M9 数据补齐；M7 记录） | A1-5 | M6/251 依 RD-13 |
| B2-10 allocated_points helper / 双源 | 重构 | A1-1 | 与 A-01 SpecState 同源 |
| B2-11 Channeling 旧注释 | 删除注释 | A1-2 | 行号已移至 `:1338` |
| B2-12 双组件冗余 | 复核后删除（大部分已不存在） | A1-2 | 逐字段复核 |
| B2-13 373 空间网格化 | 重构 | A1-5 | 复用 `SpatialGrid`；需扩 `DoHit` 签名 |
| B2-14 374 debuffId enum | 重构 | A1-5 | |
| B2-15 351 统一 + ArmorShred 命名 | 重构 | A1-5 | |
| B2-16 714 精确化 | **销项（已不存在）** | — | 已通用化 |
| B2-17 primary_archetype 死字段 | 删除（依 RD-17） | A1-2 | 存档兼容检查后删；写入 14 处含 `BladeBoomerang.cpp:109`，测试 3 处 CHECK 同步 |
| B2-18 Slow/Chill 收编 ailment | 重构 | A1-5 | 契约注册同批 |
| B2-19 技能1 第4轮遗留 | 删除死数据 | A1-5 | `payload_context` |
| B2-20 烘焙→消费映射表 | 新建文档+断言 | A1-6 | 支撑 DoD#3 |
| B2-21 技能6 夹具 | 重构（测试） | A1-6 | |
| B2-22 技能12 绝影共噬 | 重构 | A2-2 | 与 A-03 合并 |
| B2-23 deprecated batch 清理 | **推迟/降级（已裁决：后续再说）** | 待定 | 伤害 P4 已结项、删除清单为空；保留至有明确替代测试入口 |
| B2-24 热路径字符串比较 | 重构 | A1-1 / A1-5 | 复用 `BuffKind` |

---

## 6. 批次划分与文件聚类

### 6.1 A 第一批（A1）：抽象基线 + B2 全量结构清理 + A-04

原则：**先基础（helper/SpecState）→ 再组件/契约 → 再 legacy 管线 → 再伤害协同 → 再逐技能 → 最后映射表/测试**。同批交付一次 review，但内部按 wave 串行，wave 内可并行（任务边界见计划文档）。

| Wave | 内容 | 涉及 B2 | 主要文件聚类 |
|------|------|---------|--------------|
| A1-0 | 裁决确认（无代码）+ Tracy 性能基线采集（技能 1~9 cast/tick 路径，按 `docs/workflows/performance.md`） | RD-16/17/13/04 | 设计/计划文档；基线记录入 `docs/`（A1-3 拆除 legacy 管线后需有对比依据） |
| A1-1 | 共享基础：SpecState 模板、点读 helper、TagRegistry 单源、string-free helper、InitHooks 数据化 | B2-05/06/10/24(前半) | `SkillBehaviorBase.hpp`、`TagRegistry.hpp`、`SkillSystem.cpp`、`DamageConditions.*`；`gen_skill_contracts.py --gen-specstate`（生成化已裁决为首选，见 §11.7） |
| A1-2 | 组件与数据契约清理 | B2-01(字段)/08/11/12/17 | `SkillDefs.hpp`、`DeliveryArchetypes.hpp`、`BladeBoomerang.cpp:109`（primary_archetype 默认交付写入）、`SkillSpecializationBakerTests.cpp:44,70,221`；门禁：删除前合约测试套件全绿 |
| A1-3 | Legacy Channeling 管线拆除 + 双管线审计 | B2-01/02 | 12 文件：`BeamChannelDeliverySystem.cpp`、`DamagePipeline.cpp`、`DamageMitigationService.cpp`、`SkillSystem.cpp`、`StatsSystem.cpp`、`InputSystem.cpp`、`GameplayState.cpp`、`BladeMasteryService.cpp`、`MindBlade.cpp`、`InfiniteBlades.cpp`、`HeavenlySwordDescent.cpp`、`SkillDefs.hpp` |
| A1-4 | 伤害管线协同：反击 helper、SwordArray 归因 | B2-03/04 | `DamagePipeline.cpp`、`ProjectileSystem.cpp`、`CombatSystem.cpp` |
| A1-5 | 逐技能效果层清理 | B2-07/09/13/14/15/18/19/24(后半) | `FlowingThrust.cpp`、`BladeFormation.cpp`、`RendingWave.cpp`、`InfiniteBlades.cpp`、`BladeWard.cpp`、`OrbitingSentinelDeliverySystem.cpp`、`ailment_contracts.json`、`skill_4_tree.json` |
| A1-6 | 映射表与测试夹具 + SpecState 表驱动单测 + 常量清点 | B2-20/21 + B1-15/19 断言 | 新增映射表文档；`DeliveryArchetypesTests.cpp`、`SkillSpecializationBakerTests.cpp`；`gen_skill_contracts.py --skills` 范围过滤（便于局部验证）；清点 `35.0f` 在行为文件中的其余出现（`FlowingThrust.cpp`、`RendingWave.cpp` 等），确认语义归属后决定是否纳入 B2-04 helper 迁移 |
| A1-7 | A-04 技能10 illegal tags 收尾 | A-04（B1-10 余量） | 技能10 数据 + 契约 |

### 6.2 A 第二批（A2）：技能 10~12 迁移 + 收口

| Wave | 内容 | 涉及 | 主要文件 |
|------|------|------|----------|
| A2-1 | 技能10 SevenStarSlash 迁移（已具 SpecState，最小） | A-02 | `SevenStarSlash.cpp`、`skill_mechanics.json`、契约 |
| A2-2 | 技能11 HeavenlySwordDescent 迁移 + 绝影共噬 | A-02/A-03/B2-22 | `HeavenlySwordDescent.cpp`、`BladeMasteryService.cpp`、`SkillSystem.cpp:1080-1092`、`BloodSea.cpp`、`PhantomTrance.cpp` |
| A2-3 | 技能12 BloodSea 迁移 | A-02 | `BloodSea.cpp`、`HeavenlySwordField/BloodSeaField` 组件 |
| A2-4 | B2-23 处置（已裁决：保留现状，仅记录不迁移） | B2-23 | 无生产改动；在清单登记"保留至有替代测试入口" |

### 6.3 文件改动冲突规避

- `SkillDefs.hpp` 被 A1-2 与 A1-3 同时触及：A1-2 先冻结字段删除范围，A1-3 只做引用拆除，二者在 A1-2 完成后串行，不并行。
- `SkillSystem.cpp` 被 A1-1（InitHooks）、A1-3（Channeling）、A2-2（监听器）触及：按 wave 串行，A1-1 → A1-3 → A2-2。
- `InfiniteBlades.cpp` 被 A1-1（点读 helper）与 A1-5（效果清理）触及：A1-1 先落地 helper，A1-5 复用，不重复改。
- `DamagePipeline.cpp` 被 A1-3（Channeling `:457,:799`）与 A1-4（反击/SwordArray）触及：同一 wave 内合并为一次改动（A1-3 与 A1-4 合并评审点）。

---

## 7. RD 裁决依赖处理

B2/A 的实施门禁是若干 RD 裁决。本设计给出**推荐结论**，实施前需在清单 §1 表标记「已裁决」。

| RD | 关联 B2 | 本设计推荐 | 依据 |
|----|---------|-----------|------|
| RD-16 | B2-08 ReactiveWardComponent | **删除（已裁决，2026-09-13）**。字段在技能4 路径零消费，仅 `timer/ward_duration` 被清理逻辑使用；复活需新增拦截/吸收实现，超出结构清理范围。保留 `ReactiveWard` 原型枚举位（`DeliveryArchetypes.hpp`）以免破坏原型编号，仅删组件结构与 emplace 点 | `DeliveryArchetypes.hpp:265-275`、`SkillSystem.cpp:1251-1265`、`BladeWard.cpp:113-117` |
| RD-17 | B2-17 primary_archetype | **删除（已裁决，2026-09-13）**。生产零读、仅测试读（写入 14 处：Baker `:64-364` 13 处 + `BladeBoomerang.cpp:109` 默认交付；读取仅 `SkillSpecializationBakerTests.cpp:44,70,221`）；删除前确认 `SkillDefs.hpp:615` 字段不参与任何 POD 序列化/存档布局（若存档按裸内存布局则改为保留占位并注释） | `SkillSpecializationBakerTests.cpp:44,70,221`、`BladeBoomerang.cpp:109` |
| RD-13 | B2-09 M6/251 | **保持现状并记录**。M6 语义与 251 绑定不在结构清理范围；B2-09 只做 M8 诊断与 M9 数据补齐、M7 记录 | 清单 RD-13 |
| RD-04 | 技能7 数值组 | 不在本批实现；B2-16 若涉及数值须等 RD-04。本设计已建议 B2-16 销项（714 已通用化） | 清单 RD-04 |
| 其他 RD（01/02/03/05~12/14/15/18） | 无 | **不阻塞本批**；本批不改动其数值/机制语义，仅结构收敛 | 清单 §1 |

**门禁规则**：~~A1-0 前必须确认 RD-16、RD-17 的处置~~ **已裁决（2026-09-13，均删除）**，A1-2 门禁解除，仅需执行 RD-17 的存档布局前置检查。A1-2 执行字段删除前须合约测试套件全绿（`gen_skill_contracts.py --check`）+ 技能相关 `ctest -L skill` 通过，与 RD 门禁并列。

---

## 8. A-02/A-03 技能 10~12 迁移目标结构

### 8.1 现状

- 技能10 `SevenStarSlash.cpp`（681 行）：已具备 `SevenStarSlashSpecState` + `ResolveSpecState` 样板；`SkillBehaviorRegistry.cpp:49-71` 已注册。
- 技能11 `HeavenlySwordDescent.cpp`（833 行）：自带 `HeavenlySwordFieldComponent`（`SkillDefs.hpp:1246-1287`，40+ 字段）与 `AreaFieldDeliverySystem.cpp:46-48` 的特例跳过逻辑（自管理脉冲）；依赖 `BladeMasteryService.cpp:51-88`、`SkillSystem.cpp:1080-1092` 监听器。
- 技能12 `BloodSea.cpp`（560 行）：自带 `BloodSeaFieldComponent`（`SkillDefs.hpp:1289-1318`，30 字段）+ 同类特例跳过；节点 1200-1224；`PlayerHUD.cpp:71-170 FindActiveBloodSeaField` 读组件。
- 数据侧：`mastery_skill_trees.json` 有技能 10/11/12（skill_id 10 于 `:8,:233`、heavenly_sword 11 于 `:858-861,:1035`、12 于 `:1612,:1788`）；`assets/data/skill_mechanics.json` **无 10/11/12 key**（实测 keys = 1..9 + `comment`/`version` 保留 key）；独立 `skill_N_tree.json` 只到 9。

### 8.2 目标结构

1. **统一抽象信封**：技能 10~12 采用与技能 1~9 相同的「触发/效果/交付」三层与 SpecState 模式，不新增特例。
2. **持久场交付原型化**：`HeavenlySwordFieldComponent` / `BloodSeaFieldComponent` 收敛为一个共享的「持久场交付」原型（POD 公共头 + 技能扩展），并让 `AreaFieldDeliverySystem` 不再硬编码跳过这两个类型，而是按原型统一调度。若字段差异过大，则至少抽公共 `PersistentFieldHeader`（duration/tick/owner/source_skill）并以组合方式持有技能专有字段。
3. **数据归位**：新增 `skill_mechanics.json` 的 `10`/`11`/`12` key；技能树数据统一到 `mastery_skill_trees.json`（保持现状），或补齐独立 `skill_10..12_tree.json` 使与 1~9 一致（需数据校验脚本支持）。
4. **绝影共噬（B2-22/A-03）**：裁决采用「路径 A：忠于设计」。设计 §5.3:1252 定义 1217 为：在技能9 [绝影绝剑] 的逆脉/免死窗口中释放血海时，血海前 2 秒伤害与治疗效率各 +20%。实现为「窗口门控 + 数据驱动」——`BloodSea::DoCast` 在 1217 已点亮且 `IsDeathSealActive(*PhantomTranceComponent)` 成立时，向新建场写入 2 秒增伤/增疗倍率（`empower_duration/empower_damage_mult/empower_heal_mult` 经 `GetMech(12,1217,*)` 读取），窗口归零自动失效。联动脉冲机制的设计归属为节点 1207 无间血狱（§5.3:1236），故其门控改挂 1207，与 1217 解耦。

### 8.3 顺序

A2-1（技能10，最小，验证抽象信封）→ A2-2（技能11 + 绝影共噬，跨系统监听器与掌握服务）→ A2-3（技能12，持久场原型收敛）→ A2-4（B2-23 视裁决）。

### 8.4 与 B2 的交叉点

- B2-22 与 A-03 同一改动（技能9/12 跨技能）。
- B2-23（若迁移）落在 A2-4，因涉及伤害入口且伤害管线已结项。
- B2-01 的 12 文件含 `HeavenlySwordDescent.cpp`（Channeling 引用 `:470,:516,:673-675`）：必须在 A1-3 先拆除，A2-2 不再重复处理该文件的 Channeling。
- `SkillSystem.cpp:1080-1092` 属 A1-1/A1-3 的 `SkillSystem.cpp` 改动；A2-2 复用其结果。

---

## 9. 兼容与回滚策略

- **legacy Channeling 拆除（B2-01/02/03）**：分两步——先关闭回滚窗口（`BeamChannelDeliverySystem.cpp:1332` 双写），观察一拍；再删组件与引用。保留 `BeamChannelDeliverySystem` 的旧路径代码至确认后删除，确保可 revert 单文件。
- **字段删除（B2-17/RD-17、B2-08/RD-16）**：先加 `[[deprecated]]` 或注释占位，确认无存档/序列化依赖后再物理删除。
- **数据归位（B2-06/18）**：数值迁移采用「读数据 + 保留旧默认值作为 fallback」，保证数据缺失时不崩；契约/数据校验通过后移除 fallback。
- **抽象 helper（A-01）**：新增 helper 与旧写法共存一拍，行为文件逐个迁移；全部迁移完成后删除旧路径。
- **整体回滚**：本批按 wave 提交，每个 wave 可独立 revert；B2-01 的 12 文件改动集中在一个提交内，便于一次性回退。

---

## 10. 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| B2-01 12 文件引用拆除遗漏导致编译失败/运行时行为变化 | 高 | 以 `rg` 引用集合为清单逐文件销项；编译 + `ctest -L skill`；保留回滚提交 |
| `SkillDefs.hpp` 字段删除破坏存档/序列化布局 | 高 | 删除前审计 `ItemPersistenceCodec`/存档 POD 布局；不确定则保留占位 |
| 抽象 helper 引入间接分派或堆分配，破坏性能基线 | 中 | helper 全部 `constexpr`/`inline` POD；性能工作流复跑基线 |
| B2-18 收编 ailment 契约改变冰冻/减速手感 | 中 | 契约数值先等价映射（30%/-12% 等），加回归测试 |
| B2-13 扩 `DoHit` 签名波及全部技能 | 中 | 仅对 373 路径引入可选 `SpatialGrid` 查询 helper，不改公共签名；或用重载 |
| 技能10~12 持久场原型收敛范围失控 | 中 | 先做公共头组合，不做全量合并；数据差异大时保留专有组件 |
| 映射表（B2-20）流于形式、与实际不同步 | 低 | 加测试断言每个 Baker flag 有消费点 |

---

## 11. 开放问题与待确认

1. ~~**RD-16/RD-17 最终裁决**~~ **已裁决（2026-09-13）：均删除**。A1-2 门禁解除（见 §7）。
2. **存档布局**：`SkillDefs.hpp` 中待删字段（`primary_archetype`、`payload_context`、ReactiveWard）是否参与任何裸内存 POD 存档，需在 A1-2 前确认（`ItemPersistenceCodec` 扫描）。
3. **技能树数据形态**：技能 10~12 是否补齐独立 `skill_N_tree.json`，取决于 `gen_skill_contracts.py` 与数据校验脚本的现有约定（待确认脚本支持范围）。
4. ~~**B2-23**~~ **裁决（2026-09-13）：后续再说**——保留 deprecated `CalculateBatch` 现状，待有明确替代测试入口再议；A2-4 仅做记录，不实施迁移。
5. ~~**B2-04 计数**：清单称 5 处，实查 3 处伤害站点~~ **已核销**：3 处站点已确认（`DamagePipeline.cpp:1450-1471`/`:2126-2141` + `skill/ProjectileSystem.cpp`）；行为文件中其余 `35.0f` 出现（`FlowingThrust.cpp`、`RendingWave.cpp` 等）是否同语义并入迁移，由 A1-6 常量清点裁决（见 §6.1）。
6. **性能基线**：已并入 A1-0 交付（见 §6.1），A1-3 拆除 legacy 管线前后各复跑一次作对比证据。
7. ~~**SpecState 生成化确认**~~ **已裁决（2026-09-13）：采用生成化**——`--gen-specstate` 为首选路径，手写路径仅作生成器未覆盖技能的过渡；A1-1 开工前完成工具链现状比对。
8. **ProcEngine `DeathSeal` 口径（A2-2 残余）**：`TriggerWindow::DeathSeal` 判定统一为 `IsDeathSealActive(*PhantomTranceComponent)`（`ProcEngine.cpp:59-65`），与 `BloodSea.cpp` 的禁疗/窗口判定同源，排除 `ending` 终结帧。**需设计确认是否符合预期时序**（即终结帧不应再触发 DeathSeal 规则）。
9. **绝影共噬 1217 增疗与逆脉禁疗的交互（A2-2 残余）**：逆脉/免死窗口内 `ApplyHealing` 禁疗，默认 3s 窗口下 1217 的 +20% 治疗增益不产生实际治疗量，仅当窗口在 2s 内结束才部分生效；伤害 +20% 不受禁疗影响。需确认该交互是否符合设计意图。
10. **DoD#4 残余**：技能 1-9 尚无 POD SpecState 信封（仅技能 10/11/12 交付完整 CastSpec 信封）；`scripts/gen_skill_contracts.py --gen-specstate` 生成器未实现，技能 1-9 信封化属后续范围。
11. **DoD#9 残余（观测项，非门禁）**：`SevenStarSlashShared.hpp` / `HeavenlySwordDescent.hpp` / `BloodSea.hpp` 的 `Resolve*` 绑定填充为同型形状，可抽模板 helper 统一（并统一 7star 缺 clamp 的差异）；当前按 DoD#9「共享 helper 外无重复模式」为观测值处理，未强制收敛。
12. **技能10 双源残余**：`SevenStarSlash.cpp` 仍并存 `GetMech` 与 `getModifier`（`SkillRegistry::GetNodeContract(...)->trigger`）；已在计划 §5.1 记录边界，待后续统一。另 `skills.json` 技能10 `params.slash_count` 无消费者（非 A2 引入）。

---

## 12. 评审记录

- 2026-09-13 第 1 轮设计评审：结论 **修改**，报告见 `docs/reviews/2026-09-13-skill-abstraction-b2-cleanup-design-review.md`。按报告 H1/H2/M1-M4 修订：补 B2-17 写入点影响面（BladeBoomerang.cpp:109）、明确 helper 挂载层与读点 API 单一规则、新增 §4.4.1 flags 二分准则、SpecState 生成化与缓存规范、GetMech 形态与 missing key 策略、A1-0 性能基线、A1-6 测试与常量清点、DoD#7/#8。修订后无未决 HIGH 项，可进入 A1-0。
- 2026-09-13 用户裁决落定：RD-16 删除、RD-17 删除、SpecState 采用生成化（`--gen-specstate` 首选）、B2-23 后续再说（保留现状）。§7 门禁与 §11 开放问题已同步更新；A1-0 启动条件齐备。
