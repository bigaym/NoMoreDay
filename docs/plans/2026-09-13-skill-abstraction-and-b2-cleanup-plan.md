# 技能抽象化与 B2 结构清理实施计划 (Skill Abstraction & B2 Cleanup Implementation Plan)

- 日期：2026-09-13
- 状态：待评审 (Draft for Review)
- 设计依据：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（Rev.2，已含评审修订与用户裁决，本文档唯一需求来源）
- 权威清单：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（§1 裁决、§3 B2、§4 A、§7 验收）
- 工作流依据：`docs/workflows/planning.md`、`docs/workflows/testing.md`、`docs/workflows/performance.md`、`docs/workflows/implementation.md`、`docs/workflows/review.md`
- 代码规范：`conductor/code_standard.md`（V2.1）
- 批次前提（清单第 6 行）：B2 全部条目并入 A 第一批改动，避免同一批文件改两遍。
- 已落定裁决（2026-09-13，见设计 §7/§11）：RD-16 删除 ReactiveWardComponent；RD-17 删除 `primary_archetype`；SpecState 采用生成化（`--gen-specstate` 首选）；B2-23 保留现状仅记录。
- 执行边界：本文档仅为计划。本阶段不写生产代码、不编译、不跑测试；所有验证命令为计划项，执行证据待各 wave 实施时按 §9 采集。

---

## 1. 实施理由与原则

### 1.1 为什么这样做

- 行为文件从重构后的 ≤100 行增长到 4547 行（技能 1~9），是**节点语义补全**的结果而非架构退化；故不回到「薄装配」旧 DoD，采用设计 §4.1 裁决 **方案 B**：承认节点逻辑内聚，抽取「触发/效果/交付」三层参数化，并以「数据驱动收敛」为强制约束。
- 真实缺陷是**数据双源与硬编码**：Baker flags 与 `allocated_points` 就「节点是否点亮」各判一次（`InfiniteBlades.cpp` 单文件 9 处直查；`BeamChannelDeliverySystem.cpp` 6+ 处同型循环），`skill_mechanics.json` 数值被 `SkillSystem::InitHooks`（`SkillSystem.cpp:530`，段内 `:1111-1192`）硬编码绕过。本批核心是把数据归位、节点判定单源。
- B2 条目散落 12+ 文件，且与 A-01 抽象 helper 触及同一批文件，故**同批按文件聚类实施**（§6.3 冲突矩阵），杜绝同文件改两遍。

### 1.2 实施原则

1. **先基础后清理**：A1-1 的 helper / SpecState / GetMech 先落地，后续 wave 复用，不重复造轮子。
2. **同文件一次性改完**：`SkillDefs.hpp`、`SkillSystem.cpp`、`InfiniteBlades.cpp`、`DamagePipeline.cpp` 的多个条目按 §6.3 规则在同一 wave 合并提交。
3. **每 wave 独立可 revert**：按 wave 提交，波内文件集合内聚；legacy Channeling 拆除按设计 §9 两步走。
4. **数据即事实**：可调数值一律走 `skill_mechanics.json` / 契约生成物；违反 `conductor/code_standard.md` §7.2（String-Free Core Logic）、§9（无 magic number）者一律清除。
5. **不碰已结项伤害管线语义**：B2-03/B2-04 只做结构收敛（归因、反击 helper）与数值归位，不改伤害数值与结算顺序。
6. **门禁前置**：A1-0（裁决确认 + 性能基线）、A1-1 完成前不进入 A1-2；删除字段前必须过 §3.3 门禁。

---

## 2. 目标结构与关键接口（三 wave 共用基础）

> 本节只描述接口形状与数据流，不写完整可编译代码（planning.md §2 纪律）。

### 2.1 点读 helper（A1-1 核心，设计 §4.4 动作 3）

- 三个 helper 定义为 **`skills` 命名空间自由函数**，声明于 `src/game/systems/skill/behaviors/SkillBehaviorBase.hpp`（现文件 135 行，`skills` 命名空间已在 `:79-82` 存在），**不作为 `SkillBehaviorBase` CRTP 成员**，以便 UI 层与系统层共用同一 API。

```cpp
// 约定：inline / noexcept / 零分配 / 零字符串；仅读 SpecState 或规范化的 spec 视图
namespace skills {
  int   ReadPoints(const SpecLike& spec, uint32_t node);              // 未分配返回 0
  bool  HasNode  (const SpecLike& spec, uint32_t node);              // 唯一节点判定入口
  float GetMech  (uint32_t skill, uint32_t node,
                  std::string_view key, float fallback) noexcept;     // 启动期表驱动查表
}
```

- 共用消费点（设计 §4.4 动作 3 明确）：UI 层 5 文件（`src/game/application/ui/GameUiCommandHandler_SkillAstrolabe.cpp`、`GameUiSnapshotBuilder.cpp`、`GameUiSnapshot.hpp`、`UISkillSpecRenderer.cpp`、`UISkillTalentTree.cpp`）+ `src/game/systems/modifier/SkillSpecModifierAdapter.cpp` + `src/game/systems/skill/BeamChannelDeliverySystem.cpp`。UI 侧现直查点（`GameUiSnapshotBuilder.cpp:423`、`UISkillSpecRenderer.cpp:58/:497/:509/:550`、`UISkillTalentTree.cpp:107/:412/:736`）改为经 helper，消除 DoD#8 所列直查点。
- **nullptr 安全**：现 UI 直查发生在 `specialized` 可能为空的路径（如 `UISkillSpecRenderer.cpp:497` 三目链），helper 必须显式处理空 spec（返回 0/false），不得引入空指针解引用。

### 2.2 SpecState 缓存机制（设计 §4.4 动作 1/2）

```text
DoCast:
  XSpecState st = ResolveSpecState(registry, owner);   // 只在此处解析 allocated_points
  registry.emplace_or_replace<XSpecState>(owner);      // POD 缓存，standard-layout + trivially destructible
DoTick / DoHit / helper:
  const XSpecState* st = registry.try_get<XSpecState>(owner);
  // 只读缓存字段；禁止再次 find/contains；禁止跨 registry 变更持有组件指针
  // （code_standard.md §5.3：需要时先复制 POD 到栈）
```

- 规范为**强制项**而非建议：解析只能一次，tick/hit 只读。
- 生成化优先：扩展 `scripts/gen_skill_contracts.py` 新增 `--gen-specstate`，由树 JSON + 行为声明生成 `XSpecState` 结构与 `ResolveSpecState`；生成物以「node id → 成员偏移表」替代逐字段赋值（现 `SevenStarSlash.cpp:158-201` 为手写样板，内含循环内 7 次 find/contains，即设计点名的 `linear_scan_in_loop=7`）。手写路径仅作生成器未覆盖技能的过渡。
- **命名消歧（重要）**：项目已存在 `SkillMechanicsRegistry::HasNode(uint32_t skill_id, uint32_t node_id)`（`src/game/foundation/data/SkillMechanicsRegistry.hpp:43`、`.cpp:100`），语义是「机制表是否存在该键」；本设计新增的 `skills::HasNode(spec, node)` 语义是「玩家是否点亮该节点」。二者命名空间不同、无编译冲突，但**同名易混**，实施与评审时须以命名空间严格区分，禁止互相调用。

### 2.3 GetMech 数据流与 missing key 策略（设计 §4.4 动作 4）

```text
启动期（`src/app/Game.cpp:259` 已有 SkillMechanicsRegistry::Get().LoadFromFile("assets/data/skill_mechanics.json")）：
  读 assets/data/skill_mechanics.json → 校验 schema
    · comment / version 为保留 key，不参与查表
    · 缺失技能 key（当前实测 keys = 1..9 + comment/version，无 10/11/12）视为配置错误
    · missing key → 加载期断言失败；运行期绝不静默返 0
运行期：
  GetMech(...) → 纯查表，零分配、零字符串比较
```

- 现文件为 `src/game/foundation/data/SkillMechanicsRegistry.hpp/.cpp`；`skill_mechanics.json` 实测顶层 keys = `version, comment, 1..9`。
- 加载期断言与运行期查表的边界须在 A1-1 明确，并新增测试覆盖「缺失 key 触发断言 / 保留 key 不参与查表」。

### 2.4 flags 用途二分准则（设计 §4.4.1，防 helper 误迁移）

现有 52 处 `profile ? ((profile->delivery.feature_flags & N) != 0) : (getPoints(Node) > 0)` 按**结果去向**二分：

| 类别 | 判定 | 处置 | 归属 |
|---|---|---|---|
| ① 节点语义 | 判定结果改变效果层行为分支/开启效果（如 `SwordArray.cpp` has_execute/is_fire_field/is_mobile_aura、`RendingWave` IntentBurst） | 迁 `skills::HasNode(spec, id)` | A1-1 |
| ② 交付参数消费 | 结果写入 DeliveryArchetype / 交付 POD 参数位（如 `SwordArray.cpp:181-251` 的 `array.has_slow/has_armor_shred/has_cage` 回填） | 不迁 helper，归交付参数化，由 Baker 烘焙单源供给 | A1-3 |

判定准则：迁移后消费点是否仍需知道「节点」概念——不需要（只消费布尔参数）属②；需要（效果层分支逻辑）属①。**禁止把交付参数判断改成节点查询**。

### 2.5 串行/并行冲突矩阵（设计 §6.3，全局约束）

| 文件 | 触及 wave | 串行规则 |
|---|---|---|
| `SkillDefs.hpp` | A1-2（字段删除）、A1-3（引用拆除） | A1-2 先冻结字段删除范围，A1-3 只做引用拆除；二者在 A1-2 完成后**串行**，不并行 |
| `SkillSystem.cpp` | A1-1（InitHooks）、A1-2（T2.3 `:1537,:1605,:2102` payload_context 写点、T2.4 `:1373-1383/:1409-1419` ReactiveWard 分支）、A1-3（Channeling）、A2-2（监听器 `:1080-1092`） | 按 wave **串行**：A1-1 → A1-2 → A1-3 → A2-2（A1-2 新增触及点后仍维持全串行） |
| `InfiniteBlades.cpp` | A1-1（`allocated_points` helper）、A1-5（效果清理） | A1-1 先落地 helper，A1-5 复用；不重复改 |
| `DamagePipeline.cpp` | A1-3（Channeling `:457,:799`）、A1-4（反击/SwordArray 归因） | 同一 wave 内**合并为一次改动**（A1-3 与 A1-4 合并评审点） |

- wave 内可并行判定：凡「涉及文件集合不相交」的原子任务可并行；§3 各 wave 任务条目标注「可并行 / 串行」。

---

## 3. 原子任务分解（按 wave）

状态：`[ ]` 未开始 / `[~]` 进行中 / `[x]` 完成。每个 wave 含：任务分解、实现要点/伪代码、测试设计、完成标准（DoD，引用设计 §4.7）。证据格式见 §9。

### A1-0 裁决确认与性能基线门禁（无生产代码）

**任务分解**

- [x] T0.1 裁决确认（记录即可，不再决议）：RD-16 删除 ReactiveWardComponent；RD-17 删除 `primary_archetype`；SpecState 采用生成化；B2-23 保留现状仅记录。产出：清单 `docs/plans/2026-09-12-skill1-9-followup-backlog.md` §1 表对应行标注「已裁决 + 依据设计 §7」，本计划 §7 记录。
- [x] T0.2 Tracy 性能基线采集（技能 1~9 cast/tick 路径），按 `docs/workflows/performance.md`，工具 `%NMD_TRACY%`。**〔未验证/延后：无可用运行环境；结构指标初始观测与补采待办见 `docs/performance/2026-09-13-skill-abstraction-baseline.md`〕**

**实现要点**：T0.1 纯记录。T0.2 采集前确认构建为 `RelWithDebInfo`（`build.bat`，禁用 debug），记录构建版本/哈希、场景与输入、样本帧数、捕获文件路径、帧时间 p95 与上述路径的关键 zone 耗时占比。

**测试设计（性能基线，非功能断言）**

- 采样点：技能 1~9 的 cast/tick 路径（`SkillSystem::Update` → 各 `behaviors/*` 的 `DoCast`/`DoTick`/`DoHit`），以及改动热点 `DamageConditions`、`BladeFormation`、反击 helper、`BeamChannelDeliverySystem`。
- 断言点（基线，无阈值门槛）：记录各采样点自耗时/总耗时占比，作为 A1-3 拆除 legacy 后对比依据。

**完成标准**

- [x] 四项裁决结论在清单 §1 标记「已裁决」并引用设计 §7。
- [x] 基线数据落 `docs/`（`docs/performance/2026-09-13-skill-abstraction-baseline.md`），含可复现命令与关键数字；原始 `.tracy` 不入库、不入记忆。**〔标记未验证：无运行环境，待补采〕**
- [x] 设计 §4.7 DoD#9（量化结构指标仅记录观测值）所需的初始观测值一并登记。

---

### A1-1 共享基础（串行前置，之后所有 wave 复用）

**任务分解**

- [x] T1.1 在 `src/game/systems/skill/behaviors/SkillBehaviorBase.hpp` 增加 `skills::ReadPoints` / `skills::HasNode` / `skills::GetMech`（inline、noexcept、零分配、零字符串；非 CRTP 成员）。**与 T1.6 串行**（同文件 `SkillBehaviorBase.hpp`）。
  - 验证：编译 + `ctest -R nmd.tests.skill`。
- [x] T1.2 `ResolveElementalConversion`（`SkillBehaviorBase.hpp:22-77`，硬编码 170/370/570、172/270/474/572、272/372/472）改契约/数据驱动，保留常量作 fallback。**与 T1.1 串行**（同文件）。
  - 验证：元素转换既有测试（流云刺/裂地/剑阵路径）通过。
- [x] T1.3 `GetMech` 形态落地：接 `SkillMechanicsRegistry`（`Game.cpp:259` 启动期加载），实现运行期查表；补齐加载期 schema 校验与 missing key 断言（保留 key `comment`/`version` 不参与查表）。**可与 T1.1/T1.2 并行**（独立文件 `SkillMechanicsRegistry.*` + `Game.cpp`；若需改 `SkillBehaviorBase.hpp` 声明则与 T1.1 串行）。
  - 验证：新增单测覆盖「缺失 key 断言」「保留 key 跳过」「命中数值等价」。
- [x] T1.4 B2-05：TagRegistry 单源收敛（实施核验：`TagRegistry.hpp` 已由 `scripts/gen_tags.py` 全量生成、无手写并存，单源已成立；计划文中的 `scripts/gen_asset_registries.py` 为过期引用）（生成器 `scripts/gen_asset_registries.py` 与手写 header 二选一；现 `TagRegistry.hpp` 手写 `GetTagName/TagFromString` `:98-178` 与生成 `kTagInfoTable` 并存，后者被 `SkillDefs.hpp:368,377` 消费）。**可与 T1.3 并行**（`TagRegistry.hpp` + 生成器，独立于 helper 文件）。
  - 验证：`python scripts/gen_asset_registries.py` 后无预期外 diff；`SkillRegistry::StringToTag` 的 `kLegacyTags` 回归。
- [x] T1.5 B2-06：`SkillSystem.cpp` InitHooks（`:530`，段内 `:1111-1192`）中 451/455/432/435 改读 `GetMech`；`BuffEffect` 改 `static const` / 复用 POD，消除事件期 `std::string` 临时。**依赖 T1.3；与 T1.1/T1.4 可并行（文件不相交）**。
  - 验证：数值等价断言（新增）+ 代码评审确认事件期无字符串临时。
- [x] T1.6 B2-10（前半）+ B2-24（前半）：`InfiniteBlades.cpp` 9 处直查（`:168,265,267,269,271,273,275,277,279`）改 `HasNode`/`ReadPoints`；`BeamChannelDeliverySystem.cpp` 6+ 处同型循环（`:43` 等）收敛为一次 `resolve + 传参`；`DamageConditions.cpp:19` `id.find("Slow")` 改 `BuffKind` 判定。**依赖 T1.1；与 T1.5 可并行（文件不相交，但同属 A1-1，评审合并）**。
  - 验证：`ctest -R nmd.tests.skill`、`ctest -R nmd.tests.combat`。

**实现要点/伪代码**

```cpp
// T1.1 目标形状（声明于 SkillBehaviorBase.hpp 的 skills 命名空间）
namespace skills {
  inline int ReadPoints(const SpecLike& spec, uint32_t node) {
    // 仅读 SpecState 内已解析点数数组 / 规范化视图；未分配返回 0
  }
  inline bool HasNode(const SpecLike& spec, uint32_t node) {
    return ReadPoints(spec, node) > 0;   // 唯一节点判定入口（Single Source of Truth）
  }
  inline float GetMech(uint32_t skill, uint32_t node,
                       std::string_view key, float fallback) noexcept {
    // 启动期已加载的查表；命中返回数值，未命中返回 fallback（配置错误已在加载期断言）
  }
}
// T1.5 形状：编译期常量替代事件期构造
static const BuffEffect kSwordStepDrainEffect = { /* POD 字面量，字段走 GetMech(...) */ };
```

- 禁止在 helper 内做字符串比较或堆分配（`code_standard.md` §7.2、§2.1）。
- `SpecLike` 的精确形态（模板约束 `concept` vs 基类）见 §8 待确认项。

**测试设计（单元）**

| 断言点 | 位置（新增/调整） |
|---|---|
| `GetMech` 命中数值与 `SkillMechanicsRegistry` 原值一致 | `tests/unit/SkillMechanicsRegistryTests.cpp`（若不存在则新建同目录单测） |
| 缺失技能 key 触发加载期断言、保留 key 不参与查表 | 同上 |
| B2-05 TagRegistry 生成物与手写入口一致 | `tests/unit/`（TagRegistry 相关套件）+ `gen_asset_registries.py` 幂等 |
| B2-06 451/455/432/435 数值等价 | `tests/unit/SkillSpecializationBakerTests.cpp` / 技能4 相关单测 |
| B2-24 `DamageConditions` Slow 判定改 `BuffKind` | `tests/unit/` combat 套件 |

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#1（数值零硬编码，InitHooks 部分）、DoD#2（Heat 路径零字符串比较，`DamageConditions.cpp` 部分）、DoD#8（读点 API 单一，helper 落地）。
- [x] A1-2 的门禁前置（§3.3）在 A1-1 末冻结。

---

### A1-2 组件与数据契约清理（依赖 A1-0 门禁 + A1-1）

**任务分解**

- [x] T2.1 B2-11：删除 `SkillDefs.hpp` 中 Channeling 旧语义注释（`synergy_lock` 处 `:1338` 附近；清单旧引 `:1197-1216` 已失效，现该区为 `SwordArrayComponent` Branch D + `struct ExecutedTag{}` `:1216`）。**与 T2.2/T2.3/T2.4 串行**（同文件 `SkillDefs.hpp` / `DeliveryArchetypes.hpp` 字段集互相关联，按 §6.3「先冻结字段删除范围」）。
- [x] T2.2 B2-17 / RD-17：删除 `SkillDefs.hpp:615 primary_archetype` 及 `BakedDeliveryParams` 内相关字段；删同步：
  - Baker 写入 13 处：`src/game/systems/skill/SkillSpecializationBaker.cpp:64,70,78,82,86,91,97,106,114,345,352,359,364`（另 `:83` 为 `secondary_archetype`，一并确认是否同属删除范围）；
  - `src/game/systems/skill/behaviors/BladeBoomerang.cpp:109` 默认交付 `defaultDelivery.primary_archetype = ...`；
  - 测试读取 3 处 CHECK：`tests/unit/SkillSpecializationBakerTests.cpp:44,70,221`。
  - **串行**于 T2.1（`SkillDefs.hpp` 字段集）。
- [x] T2.3 B2-19：删除 `SkillDefs.hpp:777 SkillSnapshot::payload_context`。**删除范围必须含 4 处写入点**（rg 复核：`SkillSystem.cpp:1537`、`SkillSystem.cpp:1605`、`SkillSystem.cpp:2102`、`ShadowDuplicationHook.cpp:77`，均为 `snapshot.payload_context = ctx` 赋值，无读取 → 死数据）；活跃的 `payload_context` 属 `DamagePayloadContext` 与 `std::optional` 字段 `Projectile.hpp:21`、`DamagePipelineTypes.hpp:49`、`DeliveryArchetypes.hpp:53`，**不在删除范围**。注意 `SkillSystem.cpp` 新增本触及点，纳入 §2.5 串行矩阵。**串行**于 T2.1。
- [x] T2.4 B2-08 / RD-16：删除 `DeliveryArchetypes.hpp:265-275 ReactiveWardComponent` 结构与 `:274/:275` static_assert；删除 `BladeWard.cpp:113-117` emplace/赋值块；清理 `SkillSystem.cpp:1373-1383`、`:1409-1419` 组件驻留分支；同步 `tests/integration/SkillSystemTests.cpp:1393,1400`、`tests/integration/DeliveryArchetypesTests.cpp:211,216,218,223,226`。**保留 `DeliveryArchetypes.hpp:25 ReactiveWard = 11` 原型枚举位**（不破坏原型编号）。**串行**于 T2.1。
- [x] T2.5 B2-12：逐字段复核 `DeliveryArchetypes.hpp:67-109 BoomerangComponent`，删除确认无消费者的兼容字段（备注：纯兼容别名已删，余为功能字段，须逐字段取证）。**串行**于 T2.4（同处 `DeliveryArchetypes.hpp` 字段集，避免同一提交内字段交叉删除；§6.3 将 `SkillDefs.hpp` 列为串行，`DeliveryArchetypes.hpp` 同理保守处理）。
- [x] **门禁（A1-2 前置，硬性）**：
  1. `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` 全绿；
  2. `ctest -L skill` 通过；
  3. **存档布局前置检查**：扫描 `src/game/systems/item/storage/ItemPersistenceCodec.cpp` 及存档 POD 路径，确认 `primary_archetype`、`SkillSnapshot::payload_context`、`ReactiveWardComponent` 均**不参与裸内存 POD 存档**；若参与则改为保留占位 + `[[deprecated]]`，推迟物理删除（设计 §9、§11.2）。
   - **前置检查结论（2026-09-13，通过）**：`ItemPersistenceCodec` 仅序列化 `ItemInstance` 与进度载荷，不含技能组件；`SaveManager` 以 JSON 序列化 `ActiveSkillsComponent`，其 codec（`SkillDefs.hpp:708-720`）只写 `slots`/`specialized_slots`/`available_talent_points`，而 `BakedDeliveryParams`/`BakedSkillProfile`/`SkillSnapshot` 均无 JSON codec 且不被持久化。三项目标字段**均不参与任何存档路径**，可直接物理删除。

**实现要点/伪代码**

```cpp
// T2.2 删除前（Baker 当前形状）
del.primary_archetype = static_cast<uint8_t>(DeliveryArchetype::BoomerangProjectile);
// T2.2 删除后：交付原型由 secondary_archetype / 组件类型本身承载，或改为仅注释保留语义
// （最终承载方式由 A1-3 交付参数化统一决定，本 wave 只做删除与测试同步）

// T2.4 保留枚举、删结构
enum class DeliveryArchetype : uint8_t {
  // ...
  ReactiveWard = 11,   // 保留编号，不再有对应组件
  // ...
};
// 删除 struct ReactiveWardComponent { ... } 及其 static_assert
```

**测试设计（单元/集成/数据）**

| 断言点 | 位置 |
|---|---|
| Baker 不再写 / 测试不再读 `primary_archetype` | `tests/unit/SkillSpecializationBakerTests.cpp:44,70,221` 删除或改断其它交付字段 |
| `BladeBoomerang` 默认交付仍正确（改断组件/交付参数） | `tests/functional/BladeBoomerangNodes.cpp` |
| ReactiveWard 组件移除后清理分支无残留 | `tests/integration/SkillSystemTests.cpp:1393,1400`、`tests/integration/DeliveryArchetypesTests.cpp:211-226` 删除 |
| 存档/序列化兼容 | 存档加载相关 `ctest` 用例 + `ItemPersistenceCodec` 扫描证据 |
| 契约生成物无 diff | `gen_skill_contracts.py --check` |

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#6（契约/数据校验通过）。
- [x] `rg -n "primary_archetype" src/ tests/` 仅剩移除后允许的注释/枚举，且 `rg -n "ReactiveWardComponent" src/ tests/` 为 0（枚举位 `ReactiveWard = 11` 除外）。
- [x] 存档布局前置检查结论落档（通过或改占位）。

---

### A1-3 Legacy Channeling 管线拆除 + 双管线审计（依赖 A1-2 字段冻结）

**任务分解**

- [x] T3.1 B2-02：审计 `BeamChannelDeliverySystem.cpp` 双写/共存点（`:79,:151,:159,:509,:1158,:1288-1289,:1332-1345,:1377`），确认旧 Channeling 路径与 BeamChannel 的对应关系，输出审计结论（设计 §9 第一步：先关回滚窗口观察一拍）。**可先于 T3.2 单独进行**（只读审计）。〔审计结论 2026-09-13：旧管线块 `:1329-1512` 仅在实体**无** `BeamChannelComponent` 时运行，而生产 `MindBlade`/`InfiniteBlades` 对技能5/7 均同时创建两组件 → 回滚窗口为死代码；`BeamChannelComponent` 缺 `channel_timer`/`conversion_tag`/`bonus_crit_chance`/`bonus_armor_pen` 需新增；`extra_projectiles`/`full_screen_lock`/`consume_intent`/`synergy_lock`/`burst_finisher` 为死字段随结构删除。运行时「观察一拍」因无运行环境标注未验证。〕
- [x] T3.2 B2-01：按 12 文件清单逐一拆除 `ChannelingComponent` 引用，最后删除 `SkillDefs.hpp:1320-1339` 定义。引用集合（rg 实测）：
  - `src/game/contracts/impl/StatsSystem.cpp:390`；
  - `src/game/systems/combat/DamagePipeline.cpp:457,:799`；
  - `src/game/systems/combat/DamageMitigationService.cpp:315`；
  - `src/game/systems/skill/BladeMasteryService.cpp:83`；
  - `src/game/systems/skill/SkillSystem.cpp:1774,:2138,:2503`；
  - `src/game/application/states/GameplayState.cpp:474,:868-870`；
  - `src/game/application/input/InputSystem.cpp:23,:181`；
  - `src/game/systems/skill/behaviors/MindBlade.cpp:23-24,:77`；
  - `src/game/systems/skill/behaviors/InfiniteBlades.cpp:123`；
  - `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:470,:516,:673-675`；
  - `src/game/systems/skill/BeamChannelDeliverySystem.cpp` 各点（随 T3.1 结论）；
  - `src/game/foundation/components/SkillDefs.hpp:1320` 定义。
  - **两步走（设计 §9）**：先关闭 `BeamChannelDeliverySystem.cpp:1332` 兼容并存回滚窗口并观察一拍；确认后物理删除组件与引用。**与 T3.3 在同一提交**。
- [x] T3.3 更新测试侧 Channeling 构造：`tests/functional/{InfiniteBladesNodes,MindBladeNodes,SkillBehaviors}.cpp`、`tests/integration/{SkillKeyNodeMatrixIntegrationTests,SkillSystemTests}.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp`。**与 T3.2 同提交**（保证整体 revert）。
- [x] T3.4 B2-04（与 A1-4 合并评审）：`ChannelingComponent` 拆除后，`DamagePipeline.cpp` 反击/SwordArray 归因改动在**同一 wave 合并提交**（§6.3：`DamagePipeline.cpp` 被 A1-3 与 A1-4 同时触及）。〔2026-09-13：A1-3 与 A1-4 的 `DamagePipeline.cpp` 改动已同 wave 一并进入评审边界（详见 A1-4 DoD 合并评审记录）；实际 git 提交由主控执行。〕

**实现要点/伪代码**

```text
拆除顺序（每步可编译）：
  1. BeamChannelDeliverySystem.cpp：关闭 :1332 回滚窗口双写，观察一拍（B2-02）
  2. 逐文件删除 try_get/emplace/remove<ChannelingComponent>，迁移保活/计时语义到
     BeamChannel 交付组件（channel_timer/tick_interval/tick_timer → 对应交付 POD）
  3. 删除 SkillDefs.hpp:1320-1339 定义
  4. 同提交更新测试构造
回滚：T3.2+T3.3 必须同一 commit，可整体 revert。
```

**测试设计（功能/集成）**

| 断言点 | 位置 |
|---|---|
| 引导技（技能3/4/5 相关）施放/松开/计时结束行为不变 | `tests/functional/{InfiniteBladesNodes,MindBladeNodes,SkillBehaviors}.cpp` |
| 交付系统不再依赖 Channeling | `tests/integration/{SkillKeyNodeMatrixIntegrationTests,SkillSystemTests}.cpp` |
| 行为护栏 | `tests/unit/SkillBehaviorGuardTests.cpp` |
| 组件零残留 | `rg -n "ChannelingComponent" src/` 仅剩历史占位（无） |

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#5（无 legacy 兼容管线：`ChannelingComponent` 不再作为交付路径；回滚窗口关闭）。
- [x] 编译 0 警告；`rg ChannelingComponent src/` 为 0；`ctest -L skill` 全绿。
- [x] A1-0 基线在拆除后复跑一次，与基线对比记录（设计 §11.6）。〔未验证：无运行环境，与 A1-0 基线同延后；结构指标已记录〕

---

### A1-4 伤害管线协同（与 A1-3 合并评审；触及 `DamagePipeline.cpp`）

**任务分解**

- [x] T4.1 B2-04：抽取 `ResolveSkill4Counter` 唯一实现，替换 3 处伤害站点（设计 §1.3 订正：原清单「5 处」实为 3 处）：
  - `src/game/systems/skill/ProjectileSystem.cpp:689-710`；
  - `src/game/systems/combat/DamagePipeline.cpp:1446-1475`（Calculate）；
  - `src/game/systems/combat/DamagePipeline.cpp:2122-2150`（Batch）。
  数值改 `GetMech(4, 470, "base_damage", 35.0f)`（现三处硬编码 35.0f 且未读 mechanics(4,470,base_damage)）。**与 T3.2/T3.4 合并提交**（同文件 `DamagePipeline.cpp`）。〔实施结论 2026-09-13：唯一实现 `damage::ResolveSkill4Counter(entt::entity counter_attacker, entt::entity counter_defender, const BladeWardComponent &ward) noexcept`，置于 `src/game/systems/combat/damage/DamageInterceptors.hpp`（inline 自由函数，零分配/零字符串/内联；两系统均已包含该头）；三站点改为调用。基伤 `GetMech(4,470,"base_damage",35.0f)`（`skill_mechanics.json` 节点 470 实测 base_damage=35.0，等价）。元素/修饰沿用守方 `BladeWardComponent`，`spin` VFX 与延迟动作 `show_vfx`/`apply_attacker` 差异保留在各调用点。〕
- [x] T4.2 B2-03：`DamagePipeline.cpp:289-295`/`:454-456` 的 `try_get<SwordArrayComponent>` 归因归属统一（技能6 残留）。ProcEngine rule9 已不存在（`ProcEngine.cpp:54-55` 通用 `required_skill_id`），仅记录销项依据。**串行**于 T4.1（同文件 `DamagePipeline.cpp`，§6.3 要求与 A1-3 合并为一次改动，本 wave 内 T4.1/T4.2 一并提交）。〔实施结论 2026-09-13：新增 `ResolveSourceAttribution(registry, source_entity) -> {uint64_t cast_id, entt::entity owner}`，按 Projectile→SwordArrayComponent→SkillExecution 取首个命中；`ResolveEventAttackerContext`（原事件归属）与 `ResolveCastIdFromSourceEntity`（原 cast_id 解析）均改为复用该函数，两处 `try_get<SwordArrayComponent>` 归因分支归并为单一判定。`BeamChannelComponent` 仅保留在 cast_id 兜底、不进入事件归属（技能7，越 A1-4 范围，避免行为变更）。ProcEngine 销项：`:54-55` 为通用 `required_skill_id`，无硬编码 rule9，无需改动。〕〔A2-2 订正 2026-09-13：绝影共噬落地时将 `ProcEngine.cpp:59-65` 的 `TriggerWindow::DeathSeal` 判定由 `params.death_seal` 统一为 `IsDeathSealActive(*pt)`，与 `BloodSea.cpp` 的禁疗/窗口判定同源；唯一差异为额外排除 `ending` 终结帧。〕
- [x] T4.3 B2-24（后半）：`BladeFormation.cpp:483` `b.id.find("ignite")` 改 `BuffKind`；同步测试侧 `tests/functional/SwordArrayNodes.cpp:408`、`tests/functional/BladeFormationNodes.cpp:764`。**可并行**（文件与 T4.1/T4.2 不相交，但同属 A1-4/后续 A1-5 交界，评审合并）。〔实施结论 2026-09-13：`BladeFormation.cpp:483` 改 `b.kind == BuffKind::Ignite`；`SwordArrayNodes.cpp:408` 改 `b.kind == BuffKind::Slow`（弃用 `managed_ailment || id.find("Slow"/"slow")`）；`BladeFormationNodes.cpp:764` 同步 Ignite。残余 `BladeFormation.cpp:442/:488` 的 `b.id.find("shock")` 无对应 `BuffKind`（枚举仅 Bleed/Ignite/Chill/Freeze/Slow），且托管 Shock 的 id 为大写 `"Shock"`、该小写比较实为死分支，删除虽等价但越 T4.3 明确范围，留待 A1-5（B2-24 整体清零）处置。〕
- [x] T4.4 A1-6 常量清点前置：清点行为文件中其余 `35.0f` 出现（如 `FlowingThrust.cpp`、`RendingWave.cpp` 等），确认语义归属后再决定是否纳入 T4.1 helper 迁移。**只读清点，可在 A1-1 后任意时点并行**。〔实施结论 2026-09-13：只读清点，主控已完成。〕

**实现要点/伪代码**

```cpp
// 唯一实现（声明位置待实现时定，倾向 combat 侧 helper）
void ResolveSkill4Counter(entt::registry& reg,
                          entt::entity attacker, entt::entity defender,
                          float ward_counter_more, bool spin) {
  const float base = GetMech(4, 470, "base_damage", 35.0f);
  DamageRequest req{ /* origin=ThornsReflect, skill_id=4, tags=Hit|Melee|SecondaryHit */ };
  // 三处调用点仅传上下文差异（ward_counter_more / spin）
}
// 注意：不再持有组件指针跨 registry 变更（code_standard.md §5.3）
```

**测试设计（单元/集成）**

| 断言点 | 位置 |
|---|---|
| 三站点反击数值与旧硬编码路径等价 | `tests/unit/DamagePipelineP1Tests.cpp`（含生产批量链路一致性，参考 B1-14） |
| `DamagePipeline.cpp` 归因分支统一后剑阵伤害不变 | `tests/functional/SwordArrayNodes.cpp` |
| `BladeFormation` ignite 判定改 `BuffKind` | `tests/functional/BladeFormationNodes.cpp` |

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#1（反击基伤不再硬编码 35.0f）、DoD#2（`BladeFormation.cpp` 字符串比较清零）。〔DoD#1 达成：三站点统一走 helper，裸 `35.0f` 仅作 `GetMech` 缺失回退。DoD#2 的 ignite 项达成；`behaviors/` 仍余 `BladeFormation.cpp:442/:488` 的 `id.find("shock")` 死比较，见 T4.3 备注，留 A1-5 清零。〕
- [x] `ctest -R nmd.tests.integration`、`ctest -R nmd.tests.unit` 通过。〔2026-09-13：`ctest -L "ci|skill|unit|combat"` 11/11、`ctest -LE "performance|gpu-hardware"` 17/17（首轮 integration 出现一次 `SkillSystemTests.cpp:1269` 跨用例污染型偶发失败，隔离及后续 8 次复跑全绿）。〕
- [x] A1-3+A1-4 合并评审记录（§6.3 要求）。〔2026-09-13：`DamagePipeline.cpp` 被 A1-3（Channeling→BeamChannel）与 A1-4（反击/SwordArray 归因）同时触及，本 wave 两段改动一并通过 A1-3+A1-4 合并取证：build 0 warning 0 error，integration/unit/functional 全绿；提交由主控执行，本次任务禁止提交。〕

---

### A1-5 逐技能效果层清理（依赖 A1-1 helper；按技能文件并行）

**任务分解**

- [~] T5.1 B2-18（部分完成）：`FlowingThrust.cpp:533-583` Slow/Chill legacy buff 收编 `ailment_contracts.json` / `AilmentEngine`；`:578-580` `Get("FrostChill")->stacks` 改按契约栈数；spread `:735-753` 同源。契约数值先等价映射（30% / -12%）后回归。已完成 BuffKind 标签 + Chill 栈上限读契约；Apply 路径收编留 B2-18。**可并行**（`FlowingThrust.cpp`）。
- [x] T5.2 B2-13：`BladeFormation.cpp:437-450` 373 敌遍历改 `SpatialGrid` 查询（`SpatialGrid.hpp` 已存在，被 AreaField/Boomerang/OrbitingSentinel 使用）；**不改公共 `DoHit` 签名**，新增可选查询 helper 或重载。**与 T5.3/T5.4 串行**（同文件 `BladeFormation.cpp`）。
- [x] T5.3 B2-14：`BladeFormation.cpp:500-518` `std::string debuffId` 改 `BuffId` 枚举。**串行**于 T5.2。
- [~] T5.4 B2-15（部分完成）：BladeOrbit/RetaliationWeb(351/352) 数据消费统一 + ArmorShred id 命名冲突消解（`RendingWave.cpp:537` / `FlowingThrust.cpp:412`）。已完成 351/352 数据消费统一（由 BladeFormation feature_flags 承载）；ArmorShred id 重命名未做（仅登记「未决见 B2-15」注释），残留留 B2-15。**注意 `RendingWave.cpp` 与 T5.1 的 `FlowingThrust.cpp` 不相交；`FlowingThrust.cpp:412` 与 T5.1 同文件 → 与 T5.1 串行**。
- [x] T5.5 B2-07：`OrbitingSentinelDeliverySystem` 拦截死分支（`BladeWard.cpp:110` `interception_chance = 0.0f` 置零、系统仍每帧 Update、拦截分支不可达）真删除。**可并行**（独立文件，`BladeWard.cpp` 的 ReactiveWard 部分已在 A1-2 处理，此处仅剩拦截分支）。
- [x] T5.6 B2-09：M8 `SkillSystem::AddTalentPoint`（`SkillSystem.cpp:2128`）增加 max_points 不可达诊断；M9 `skill_4_tree.json` 补 max_points（与 `skills.json` 单源）；M7 476 双前置记录为已知布局锚点。**与 A1-1 的 `SkillSystem.cpp` 改动串行**（同文件，§6.3：A1-1 → A1-3 → A2-2；本项插在 A1-3 之后的 A1-5 内，须核对不与 A1-3 冲突——A1-3 触及 `:1774,:2138,:2503`，本项 `SkillSystem::AddTalentPoint`（计划时点 `:1919-1940`，现行 `SkillSystem.cpp:2128` 起）不相交，但同文件仍需 wave 串行）。
- [x] T5.7 B2-19（技能1 剩余）：确认 `SkillDefs.hpp:777` 已在 A1-2/T2.3 删除，本项仅销项登记，无重复代码改动。**只读登记**。

**实现要点/伪代码**

```cpp
// T5.1 形状：契约驱动
const AilmentContract* slow = AilmentEngine::Get().GetContract(AilmentId::Slow);
effects.AddOrRefresh(BuffEffect::FromContract(slow, /*stacks=*/1));  // 替代 "FrostSlow"/"FrostChill" 字符串
// T5.2 形状：SpatialGrid 查询，不动 DoHit 签名
for (const auto e : SpatialGrid::QueryRadius(center, radius)) { /* 373 命中候选 */ }
// T5.5：删除不可达拦截分支，保留 OrbitingSentinel 本体
```

**测试设计（功能/单元/数据）**

- 冰冻/减速回归（T5.1）：`tests/functional/FlowingThrustNodes.cpp`、`tests/unit/` ailment 套件。
- 373 空间查询等价（T5.2）：`tests/functional/BladeFormationNodes.cpp`（命中集合与旧线性遍历一致）。
- `BuffId` 枚举化（T5.3）：同文件功能测试。
- 351 命名冲突（T5.4）：`tests/functional/RendingWaveNodes.cpp`、`FlowingThrustNodes.cpp`。
- 死分支删除（T5.5）：`tests/functional/BladeWardNodes.cpp`（若存在）或 `tests/integration/`。
- M8/M9（T5.6）：`tests/unit/SkillSystem*` 不可达诊断断言 + `skill_4_tree.json` 契约校验。
- 数据：`gen_skill_contracts.py --check`。

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#2（behaviors 零字符串比较）、DoD#3（节点判定单源）、DoD#6（契约校验）。
- [x] `ctest -L skill`、`ctest -R nmd.tests.integration` 通过。

---

### A1-6 映射表、测试夹具与 SpecState 表驱动单测

**任务分解**

- [x] T6.1 B2-20：产出「节点烘焙→消费映射表」文档（`docs/` 下，建议 `docs/designs/` 或 `docs/plans/` 旁挂，按仓库约定），覆盖 `SkillSpecializationBaker.cpp` 18 个 Baker case（switch#1 交付原型 `:63-113`、switch#2 节点修正 `:237-914`）的 flags / 交付参数与唯一消费点。
- [x] T6.2 新增断言测试：每个 Baker flag 至少一个消费点（防「写而不读」回归）。**可与 T6.1 并行**（测试文件独立）。〔落地为 `tests/unit/SkillBakerFlagConsumerTests.cpp`，29 位掩码 `0x1FFFFFFF` 登记表 + 全树节点 bake 覆盖守护；消费点存在性由 T6.1 映射表 + rg 复核保证。〕
- [x] T6.3 B2-21：`tests/integration/DeliveryArchetypesTests.cpp:191,194,207` 技能6 夹具（`skill_id=6`/`leave_field_skill_id=6`/`source_skill_id==6`）改独立 ID。**可并行**。〔已改 106，并加 `source_skill_id == leave_field_skill_id` 透传断言。〕
- [x] T6.4 B1-15 / B1-19 断言补强：技能4 435 剑意回复断言；技能2 `trigger_skill_id=2` 数据检查（`skills.json` 技能3 373 / 技能6 633）。**可并行**（测试与数据，文件不相交）。〔实施订正：实际承载 `trigger_skill_id=2` 的契约节点为技能3 node 335（`skills.json:1999`）与技能6 node 635（`skills.json:4128`），373/633 为天赋树条目、无 `trigger` 字段；已在 `SkillContractRegistryTests.cpp` 增断言固定真实链接，数据零改动。435 断言在 `SkillSystemTests.cpp`。〕
- [x] T6.5 SpecState 表驱动单测（DoD#7）：每技能 `allocated_points → XSpecState` 字段映射有表驱动单测；若 A1-1 生成器已产出，则断言生成物与手写基线等价一次。〔落地为 `tests/unit/SpecStateMappingTests.cpp`（9 用例/1188 断言），覆盖技能10 SevenStarSlashSpecState 18 数值 + 6 存在判定 + 2 转质映射。已知限制：`ResolveSpecState` 在匿名命名空间，测试只能用手写镜像，无法检出生产漂移；A2-1 迁移后应改为直调生产符号。〕
- [x] T6.6 常量清点裁决（承接 T4.4）：确认 35.0f 在行为文件中其余出现是否同语义并入 B2-04 helper，销项登记。〔裁决：`FlowingThrust.cpp:177,292`、`RendingWave.cpp:149` 的 35.0f 为几何半径/弹体半径语义，与技能4 反击伤害无关，**不纳入** `ResolveSkill4Counter`。〕

**测试设计（单元/集成/数据）**

| 断言点 | 位置 |
|---|---|
| 每个 Baker flag 有消费点 | 新增 `tests/unit/SkillSpecializationBakerTests.cpp` 用例 |
| 技能6 夹具独立 ID | `tests/integration/DeliveryArchetypesTests.cpp` |
| 技能4 435 / 技能2 数据 | `tests/functional/` skill4/skill2 套件 |
| 每技能 SpecState 映射 | 新增表驱动单测（可生成） |
| 生成物与手写基线等价 | 若走 `--gen-specstate` |

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#3（映射表 + 断言守）、DoD#7（SpecState 解析测试）。
- [x] `ctest -R nmd.tests.integration`、`ctest -R nmd.tests.unit` 通过。

---

### A1-7 A-04 技能10 illegal tags 收尾（B1-10 余量）

**任务分解**

- [x] T7.1 技能10 剩余 illegal tags 清理 + 契约注册（承接 B1-10 已完成的三处 `sword_skill`→`SwordSkill`，`assets/data/skills.json` 技能2/3/10；技能6 `Duration` 已判定为 legacy 别名映射，保留）。复核结果：技能10 无剩余 illegal tags（tags 全为注册项），契约已在 `skill_contracts_compact.json` 注册，无需改动数据。
- [x] T7.2 数据校验与销项登记。`gen_skill_contracts.py --check --check-idempotency --check-determinism` 与 `sync_skill_node_icon_ids.py --check` 均 PASS（exit 0）；清单 A-04/B1-10 已勾选并附证据（`docs/plans/2026-09-12-skill1-9-followup-backlog.md` §8）。

**测试设计（数据）**

- `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS。
- 如涉及图标 id：`python scripts/sync_skill_node_icon_ids.py --check`。

**完成标准（DoD）**

- [x] 设计 §4.7 DoD#6（契约/数据校验通过）。
- [x] 清单 A-04 / B1-10 勾选并附证据。

---

### A2-1 技能10 SevenStarSlash 迁移（最小改动，验证抽象信封）

**任务分解**

- [x] T8.1 将 `SevenStarSlashSpecState`/`ResolveSpecState` 迁至 `SevenStarSlashShared.hpp` 的 `NoMoreDay::skills` 命名空间并导出；`ResolveSpecState` 改为「node id → 成员指针」双静态表（18 点读 + 6 点亮）驱动，循环内无 find/contains；每点系数经 `GetMech` 外置。证据：`rg "allocated_points\.(find|contains|count)" SevenStarSlash.cpp` = 0；行为数值与迁移前等价。
- [x] T8.2 `assets/data/skill_mechanics.json` 增顶层 `"10"`（13 节点、17 键，值等于原硬编码）；`SkillMechanicsRegistry.cpp` `kRequiredSkillIds` 扩为 `{1..10}` 并更新加载期断言测试（新增 missing skill 10 子例）。`gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS；`sync_skill_node_icon_ids.py --check` 无 diff。
- [x] T8.3 `SkillBehaviorRegistry.cpp:54-74` 核对：`RegisterSevenStarSlash()` 与其余技能走同一注册序列，无新增特例分支，无需改动。

**测试设计**

- `tests/functional/SevenStarSlashNodes.cpp`（新建，2 用例）：SpecState 点读/点亮/转质映射 + VoidTread 节点经机制数据缩放施法无敌时长；`ctest -L skill` PASS。
- `tests/unit/SpecStateMappingTests.cpp` 重写：删除手写镜像，直接 include 生产头并调用 `skills::ResolveSpecState`，以独立期望表与生产绑定表互校（9 用例）。
- `tests/integration/GameplaySystems.cpp` 节点常量守卫改为扫描 `SevenStarSlashShared.hpp`（常量随 A2-1 迁移）。
- `skill_mechanics.json` 契约校验：`gen_skill_contracts.py --check` PASS。

**完成标准（DoD）**

- [x] 设计 §5.3 A2 DoD：技能10 使用统一抽象信封，无新增技能特例分支；`skill_mechanics.json` 含 `10` key 并通过校验。
- [x] 设计 §4.7 DoD#4（技能10 计入一次性解析 POD SpecState；余下技能 11/12 由 A2-2/A2-3 收口）。

---

### A2-2 技能11 HeavenlySwordDescent 迁移 + 绝影共噬（A-02/A-03/B2-22）

**任务分解**

- [x] T9.1 迁移 `HeavenlySwordDescent.cpp`（现 833 行，自带 `HeavenlySwordFieldComponent` `SkillDefs.hpp:1246-1287` 40+ 字段、`AreaFieldDeliverySystem.cpp:46-48` 特例跳过）到统一抽象信封；接入 `BladeMasteryService.cpp:51-88` 与 `SkillSystem.cpp:1080-1092` 监听器（复用 A1-1/A1-3 已改的 `SkillSystem.cpp`，§6.3 串行：A1-1 → A1-3 → A2-2）。
- [x] T9.2 抽取 `PersistentFieldHeader` 公共头（实现为 `SkillDefs.hpp:1256-1265` 的 `owner / duration / radius / tick_interval / tick_timer / linked_hit_count / has_linked_synergy`），`HeavenlySwordFieldComponent` 组合持有专有字段。计划原拟的 `source_skill/remaining/center` 不纳入公共头：归属技能由既有 `SkillComponent/AreaFieldComponent.source_skill_id`（`SkillDefs.hpp:771/1074/1237`）承载，剩余时间由 `header.duration`（倒计时语义）承载，作用中心由实体 `Position` 承载。**R2 订正**：原拟纳入的 `cast_id` 经死代码审查为生产只写不读（归因单源在 `AreaFieldComponent::cast_id`，`AreaFieldDeliverySystem.cpp:100`），已删除；`linked_hit_count` 生产只写但为测试/调试可观测计数，保留并加注释。
- [x] T9.3 绝影共噬（B2-22/A-03）：技能12 新建血海场时按「窗口门控 + 数据驱动」实现——已点 1217 且技能9 逆脉/免死窗口激活（统一经 `IsDeathSealActive(*PhantomTranceComponent)` 判定）时，前 2 秒伤害与治疗效率各 +20%，2 秒后自动恢复；数值经 `GetMech(12,1217,empower_duration/empower_damage_mult/empower_heal_mult)` 读取。裁决依据：设计 §5.3:1252 明确 1217 语义为窗口增益；联动脉冲机制的设计归属为节点 1207 无间血狱（§5.3:1236），故 1217 与联动脉冲解耦，联动脉冲门控改挂 1207（保留既有行为、不无条件放大）。
- **交互记录（禁疗×增疗，T9.3 关联）**：逆脉/免死窗口内 `ApplyHealing` 处于禁疗状态，默认 3s 窗口下节点 1217 的 +20% 治疗增益不产生实际治疗量，仅当窗口在 2s 内结束时才部分生效（伤害 +20% 不受禁疗影响，正常生效）。
- [x] T9.4 `skill_mechanics.json` 增 `11` key。

**测试设计**

- `tests/functional/HeavenlySwordDescent*`（相关技能11 套件）；`ctest -L skill`、`ctest -R nmd.tests.integration`。
- 绝影共噬窗口门控测试：窗口激活 → 前 2 秒增伤/增益；窗口未激活或未点 1217 → 无增益；2 秒后恢复（`tests/functional/BloodSeaNodes.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp`）。既有联动脉冲覆盖改挂 1207 门控，不弱化。

**完成标准（DoD）**

- [x] 设计 §5.3 A2 DoD：技能11 使用统一抽象信封；`skill_mechanics.json` 含 `11` key；绝影共噬以「窗口门控 + 数据驱动」表达（1217 经 `GetMech(12,1217,empower_*)` 读取，门控来源 `IsDeathSealActive`）。
- [x] `HeavenlySwordFieldComponent` 以公共头组合方式持有专有字段（不做全量字段合并）。实现：`SkillDefs.hpp:1272-1312` 以 `PersistentFieldHeader header` 成员嵌套组合专有字段（禁止继承）。

---

### A2-3 技能12 BloodSea 迁移 + 持久场原型收敛

**任务分解**

- [x] T10.1 迁移 `BloodSea.cpp`（现 560 行，自带 `BloodSeaFieldComponent` `SkillDefs.hpp:1289-1318` 30 字段）；节点 `1200-1224`；`PlayerHUD.cpp:71-170 FindActiveBloodSeaField` 读取路径核对。
- [x] T10.2 `HeavenlySwordField/BloodSeaField` 收敛为公共 `PersistentFieldHeader` + 专有字段；`AreaFieldDeliverySystem.cpp:46-48` 去 HeavenlySword/BloodSea 类型特判，按原型统一调度。
- [x] T10.3 `skill_mechanics.json` 增 `12` key。

**测试设计**

- `tests/functional/BloodSea*`；`ctest -L skill`、`ctest -R nmd.tests.integration`；`AreaFieldDeliverySystem` 类型特判清零检索。

**完成标准（DoD）**

- [x] 设计 §5.3 A2 DoD：技能12 使用统一抽象信封；`AreaFieldDeliverySystem` 无 HeavenlySword/BloodSea 类型特判。实现：`BloodSeaFieldComponent`（`SkillDefs.hpp:1316`）与 `HeavenlySwordFieldComponent`（`SkillDefs.hpp:1272`）均以 `PersistentFieldHeader` 嵌套组合。
- [x] `rg -n "HeavenlySword|BloodSea" src/game/systems/skill/AreaFieldDeliverySystem.cpp` 无类型特判分支。实现：`:48` 改 `registry.any_of<PersistentFieldTag>(entity)` 统一判定，无具体类型分支。

---

### A2-4 B2-23 处置（保留现状，仅记录）

**任务分解**

- [x] T11.1 按已裁决（2026-09-13）**保留 deprecated `CalculateBatch` / `ResolveDamageBatch` 现状**，不迁移测试/基准调用点（`tests/unit/DamageElementIndexTests.cpp:109,:167`、`tests/unit/EventConsistencyTests.cpp`、`tests/performance/DamagePipelineBenchmark.cpp`）。无生产代码改动。
- [x] T11.2 清单 B2-23 销项登记「保留至有替代测试入口」，附裁决依据（设计 §7/§11.4）。

**测试设计**：无生产改动；仅回归现有 deprecated 入口相关用例确认仍绿。

**完成标准（DoD）**

- [x] 清单 B2-23 标注「保留/降级」并附理由；无代码改动。

---

## 4. 测试策略

### 4.1 受影响测试层

| 层 | 覆盖点 | 用例/位置 |
|---|---|---|
| 单元 | helper 语义、`GetMech` 数据等价与断言、`BuffKind` 判定、Baker 字段、SpecState 映射 | `tests/unit/{SkillSpecializationBakerTests,DamageElementIndexTests,DamagePipelineP1Tests,SkillMechanicsRegistryTests}.cpp` |
| 功能 | 技能 1~12 节点行为 | `tests/functional/{FlowingThrust,BladeFormation,InfiniteBlades,MindBlade,PhantomTrance,BladeBoomerang,SwordArray,SevenStarSlash,HeavenlySwordDescent,BloodSea}Nodes.cpp`、`SkillBehaviors.cpp` |
| 集成 | 触发/交付跨系统、契约映射、场景字段 | `tests/integration/{DeliveryArchetypesTests,SkillKeyNodeMatrixIntegrationTests,SkillSystemTests}.cpp` |
| 数据 | 契约/节点/图标 id 一致性 | `scripts/gen_skill_contracts.py --check`、`sync_skill_node_icon_ids.py --check`、`gen_asset_registries.py` |
| 性能 | 热路径无分配/无字符串、伤害基线 | `tests/performance/DamagePipelineBenchmark.cpp`，按 `docs/workflows/performance.md` |
| 存档 | 待删字段不参与裸内存 POD 存档 | `ItemPersistenceCodec` 扫描 + 存档加载用例 |

### 4.2 新增/调整回归点

- 新增：`GetMech` 数值等价单测 + missing key 加载期断言（A1-1）。
- 新增：Baker flag 消费点覆盖断言（A1-6，B2-20）。
- 新增：每技能 `allocated_points → XSpecState` 表驱动单测（A1-6，DoD#7）。
- 新增：绝影共噬窗口门控测试（A2-2，窗口激活 / 未激活 / 未点三态 + 2 秒恢复）。
- 调整：技能6 夹具改独立 ID（`DeliveryArchetypesTests.cpp`，B2-21）。
- 调整：测试侧字符串比较（`SwordArrayNodes.cpp:408`、`BladeFormationNodes.cpp:764`）改 `BuffKind`。
- 删除/更新：`primary_archetype` CHECK（`SkillSpecializationBakerTests.cpp:44,70,221`）、Channeling 构造、`ReactiveWardComponent` 用例（随 A1-2/A1-3）。

### 4.3 测试方法

- 每个 wave 完成后跑该 wave 相关标签；批次出口跑全量（§5）。
- 数值迁移（B2-06 / B2-18）采用「先等价断言、后删 fallback」两步（设计 §9）。
- 热路径改动（`DamageConditions`、`BladeFormation`、反击 helper、`BeamChannelDeliverySystem`、SpecState 解析）必须复跑基准并记录基线对比。
- 无交互运行环境时的运行时观察项（清单 B1-21/22/23）标注「未验证」，不计入本批出口阻断，但须在复审报告列出。

---

## 5. 完成标准（DoD）与出口标准

### 5.1 设计 §4.7 的 9 条 DoD（批次总 DoD）

1. **数值零硬编码**：行为文件与 `SkillSystem::InitHooks` 中不存在未走 helper 的调平数值。
2. **热路径零字符串比较**：`rg` 检索 `behaviors/` + `DamageConditions.cpp` 无 `.find("` / `strcmp` / `std::string ==` 判定分支。
3. **节点判定单源**：不存在「同一节点同时用 flags 与 `allocated_points` 两处判定」；由 B2-20 映射表 + 断言守。
4. **SpecState 全覆盖**：12 个技能均在 `DoCast` 一次性解析 POD SpecState。**残余项记录（2026-09-13）**：当前仅技能 10/11/12 交付完整 POD CastSpec 信封（`SevenStarSlashShared.hpp`、`HeavenlySwordDescent`、`BloodSea` 的行为层 `Resolve*SpecState`）；技能 1-9 经 A1 交付 `skills::ReadPoints/HasNode/GetMech` 就地读点与数据外置，未建立每技能 SpecState 结构；`scripts/gen_skill_contracts.py --gen-specstate` 生成器未实现（见 §8.3）。故本条**部分达成**，技能 1-9 信封化属后续范围。
5. **无 legacy 兼容管线**：`ChannelingComponent` 不再作为交付路径；`BeamChannelDeliverySystem` 回滚窗口关闭。
6. **契约/数据校验通过**：`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`。
7. **SpecState 解析测试**：每技能 `allocated_points → XSpecState` 映射有表驱动单测（若生成器产出则断言生成物与手写基线等价一次）。
8. **读点 API 单一**：全仓 `allocated_points\.(find|contains|count)` 直查点为 0（仅 helper 内部与生成代码允许）。
9. **量化结构指标**：行为文件行数仅作观测值记录不作门槛；门槛改为「共享 helper 外无重复模式」。

**补充限制/交互记录（2026-09-13）**：

- **限制①（加载期校验范围）**：`SkillMechanicsRegistry` 加载期 schema 校验只保证技能节 / 节点整数键 / 叶值数字，**不校验具体键名拼写**；键名拼错时静默回退默认值，需靠单测与 `--check` 覆盖。
- **限制②（技能10 系数双源）**：技能10 系数同时存在 `GetMech` 与 `SkillRegistry::GetNodeContract(...)->trigger` 的 `getModifier` 两条读取路径（`SevenStarSlash.cpp:440,549,559,564,568,576,616,647`），需冻结边界、明确以哪一路为权威来源。

### 5.2 批次出口（清单 §7）

1. `build.bat`（`RelWithDebInfo`）**0 警告**。
2. `ctest -L ci` 全绿；`ctest -L skill` 全绿。
3. 数据改动跑 `gen_skill_contracts.py --check` / `sync_skill_node_icon_ids.py --check` 无 diff；`gen_asset_registries.py` 运行后生成物无预期外 diff。
4. 复审按 `docs/workflows/review.md` 产出报告，结论 `提交`。
5. 每个销项条目在清单 §1/§3/§4 勾选并附证据（file:line / 报告 / 测试名）。

### 5.3 wave 级 DoD 映射

| wave | 主要贡献 DoD |
|---|---|
| A1-0 | 基线 + DoD#9 初始观测值 |
| A1-1 | #1（部分）、#2（部分）、#8 |
| A1-2 | #6、存档兼容 |
| A1-3 | #5 |
| A1-4 | #1（反击基伤）、#2（`BladeFormation`） |
| A1-5 | #2、#3、#6 |
| A1-6 | #3、#7 |
| A1-7 | #6 |
| A2-1..A2-4 | #4、§5.3 A2 专项 DoD |

---

## 6. 验证命令（计划项，本次不执行）

```powershell
# 构建（RelWithDebInfo，禁止 debug）
.\build.bat

# 全量 CI（非性能）
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure

# 技能 / 战斗 / 单元 / 集成专项
ctest --test-dir build -C RelWithDebInfo -L skill --output-on-failure
ctest --test-dir build -C RelWithDebInfo -R nmd.tests.skill --output-on-failure
ctest --test-dir build -C RelWithDebInfo -R nmd.tests.combat --output-on-failure
ctest --test-dir build -C RelWithDebInfo -R nmd.tests.unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -R nmd.tests.integration --output-on-failure

# 数据校验
python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
python scripts/sync_skill_node_icon_ids.py --check

# 静态核查（DoD）
rg -n "ChannelingComponent" src/
rg -n "allocated_points\.(find|contains|count)" src/
rg -n "primary_archetype|ReactiveWardComponent" src/ tests/
rg -n "\.find\(\"" src/game/systems/skill/behaviors/ src/game/systems/combat/DamageConditions.cpp

# 性能基线（按 docs/workflows/performance.md，工具 %NMD_TRACY%）
%NMD_TRACY%\tracy-capture.exe -o <输出>.tracy -a 127.0.0.1 -s <秒数>
%NMD_TRACY%\tracy-csvexport.exe <输入>.tracy > <输出>.csv
```

---

## 7. 关键裁决落点（记录，供实施引用）

| 裁决 | 结论 | 影响面 | 落点 |
|---|---|---|---|
| RD-16 | 删 `ReactiveWardComponent`；**保留 `DeliveryArchetypes.hpp:25 ReactiveWard = 11` 枚举位** | `DeliveryArchetypes.hpp:265-275`、`BladeWard.cpp:113-117`、`SkillSystem.cpp:1373-1383/:1409-1419`、测试 | A1-2 / T2.4 |
| RD-17 | 删 `primary_archetype` | `SkillDefs.hpp:615`；写入：`SkillSpecializationBaker.cpp:64-364` 共 13 处 + `BladeBoomerang.cpp:109`（合计 14）；读取：`SkillSpecializationBakerTests.cpp:44,70,221` | A1-2 / T2.2 |
| SpecState | 采用生成化（`gen_skill_contracts.py --gen-specstate` 首选；手写仅过渡） | A1-1 生成器改造；A1-6 等价断言 | A1-1 / A1-6 〔实施订正：`--gen-specstate` 生成器未实现，延后 A2-1；A1-6 T6.5 以手写镜像基线交付，A2-1 迁移后改为直调生产符号〕 |
| B2-23 | 保留现状，仅记录（`CalculateBatch` + `ResolveDamageBatch` 不迁移） | 无生产改动 | A2-4 / T11 |
| RD-13 | 保持现状并记录（M6/251 不在范围） | 仅 M8/M9/M7 | A1-5 / T5.6 |
| RD-04 | 不在本批；B2-16 已建议销项 | — | 记录 |

---

## 8. 未做与待确认

1. 本次未执行任何构建/测试/数据校验，所有验证命令为计划项。
2. **`SpecLike` 形态待确认**：设计 §2.1/§4.4 以占位符 `SpecLike` 描述 `ReadPoints/HasNode` 入参，尚未裁决是单一 `concept` 约束、公共基类，还是生成物直接按具体 `XSpecState` 模板实例化。此确定义影响 A1-1 与全部消费点签名，实施前须与设计确认。
3. **`--gen-specstate` 生成器接口待确认**：`scripts/gen_skill_contracts.py` 现无该选项（现有：`--skills/--mastery-skills/--compact/--check/--verbose/--check-idempotency/--check-determinism`，`:735-743`）。生成物落点（头文件路径）、生成范围（全部 12 技能 vs 逐技能）与「行为声明」的输入形态待设计补充。
4. **技能树数据形态待确认**（设计 §11.3）：技能 10~12 是否补齐独立 `skill_10..12_tree.json`，取决于生成器与数据校验脚本约定；当前独立 tree 仅到 9，`mastery_skill_trees.json` 已含技能 10/11/12。
5. **存档 POD 布局审计未执行**：A1-2 前置门禁项（`ItemPersistenceCodec` 扫描）；若字段参与裸内存存档则改保留占位 + `[[deprecated]]`。
6. **T5.6 与 A1-3 同文件排期**：`SkillSystem.cpp` 多处改动按 §6.3 串行；T5.6（`:1919-1940`）与 A1-3（`:1774/:2138/:2503`）行号不相交，但须在 wave 串行内核对不冲突。
7. **`primary_archetype` 删除后语义承载**：`secondary_archetype`（`SkillSpecializationBaker.cpp:83` 仍写 `ReactiveWard`）与交付原型字段的最终归属由 A1-3 交付参数化统一决定，本计划只做删除与测试同步。
8. **B2-04 计数**：设计已订正为 3 处（原清单「5 处」）；行为文件中其余 `35.0f` 是否同语义由 A1-6 常量清点裁决（T4.4/T6.6）。
9. **命名消歧**：`skills::HasNode(spec,node)`（节点点亮）与既有 `SkillMechanicsRegistry::HasNode(skill,node)`（机制表存在）同名不同义，实施/评审须严格区分。

---

## 9. 证据与回滚

### 9.1 证据要求（每 wave 完成时记录）

- **静态证据**：关键 `rg` 检索结果（如 `ChannelingComponent`、`allocated_points\.(find|contains|count)`、`primary_archetype`、`\.find("`）前后对比，含命令与摘要。
- **测试证据**：wave 相关标签的 `ctest` 输出摘要（用例名 / 通过数），批次出口全量结果。
- **数据证据**：`gen_skill_contracts.py --check*`、`sync_skill_node_icon_ids.py --check`、`gen_asset_registries.py` 的运行结论。
- **性能证据**：A1-0 基线与 A1-3/A1-4/A1-5 后复跑的对比（构建哈希、场景、样本帧数、关键数字、捕获文件路径）；原始抓取文件不入库、不入记忆，只写结论。
- 每 wave 销项条目在清单对应节勾选并附证据（file:line / 报告链接 / 测试名）。

### 9.2 回滚策略（设计 §9）

- 按 wave 提交，每个 wave 独立可 revert；A1-1 为其他 wave 前置，回滚 A1-1 须连带回滚依赖 wave（依赖见 §3）。
- legacy Channeling 拆除**两步走**：先关闭 `BeamChannelDeliverySystem.cpp:1332` 回滚窗口并观察一拍，确认后物理删除；T3.2 的 12 文件改动集中一个提交 + T3.3 测试同步同提交，便于一次性回退。
- 字段删除（RD-16/RD-17）：先确认无存档/序列化依赖；不确定则 `[[deprecated]]` / 占位保留，推迟物理删除。
- 数据归位（B2-06/18）：采用「读数据 + 旧默认值 fallback」，校验通过后移除 fallback。
- 抽象 helper：新增 helper 与旧写法共存一拍，行为文件逐个迁移，全部迁移后删旧路径。
- A2 独立于 A1 之外，可单独回滚（§8）。

---

## 10. 自查

- **原子性**：每个 T 编号任务对应单一可验证动作（一次删除 / 一次迁移 / 一组同文件改动），跨文件批量拆除（T3.2）以「单提交整体 revert」为原子单位。
- **完成标准可验证**：所有 DoD 均映射到 `ctest` 标签、`rg` 零点、生成器 `--check` 或性能数字，见 §5.3 与 §6。
- **串行约束与设计 §6.3 一致**：`SkillDefs.hpp`（A1-2→A1-3）、`SkillSystem.cpp`（A1-1→A1-3→A2-2）、`InfiniteBlades.cpp`（A1-1→A1-5）、`DamagePipeline.cpp`（A1-3+A1-4 合并）逐条落实于 §2.5 与各 wave 任务注记。
- **未自行发明需求**：设计未覆盖处均列入 §8「待确认」，不擅自决策。
