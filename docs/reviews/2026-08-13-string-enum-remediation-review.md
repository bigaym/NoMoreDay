# 字符串比较枚举化整改最终审查（T11）

**结论：修改 → 修复闭环后建议 提交**（见文末"复审修复闭环"与"更新后结论"）

- 审查日期：2026-08-13
- 审查轮次：首次最终审查（T11）
- 审查目标：逐项核验字符串比较枚举化整改 T1–T9 及后续回归修复 A/B/C，确认未引入玩法、显示文本或持久化格式兼容性漂移。本审查只输出报告，未修改生产代码，未重跑构建或测试。

## 输入、范围与方法

| 项目 | 证据 |
|---|---|
| 实施计划与验收 | `docs/plans/string-comparison-enum-remediation-plan.md:5-12,38-148` |
| 审查流程 | `docs/workflows/review.md:39-60,80-101` |
| 代码标准 | `conductor/code_standard.md:36-40,51-63` |
| 工作区范围 | `git status --short`：112 个已跟踪修改及新增 `docs/plans/string-comparison-enum-remediation-plan.md`、`src/game/foundation/data/BuffIds.hpp`；`git diff --stat`：112 files changed, 1739 insertions(+), 1224 deletions(-) |
| 静态门禁 | `git diff --check`：通过（仅 LF→CRLF informational 提示） |
| 检索方法 | 先使用 `D-PRJ-NoMoreDay` 代码图谱定位定义、调用与影响面；图谱索引存在变更前信息时，以工作树源码、diff 与精确 grep/read 为准。 |
| 构建证据（既有） | `logs/build_fix_final.log:8-21,29`：RelWithDebInfo 预检全过、ALL_BUILD 成功、0 error。 |
| 测试证据（既有） | `logs/ctest_final.log:1-75`：19/21 通过；本轮按用户要求不重跑。 |

计划的全局约束是“枚举放源层、字符串留 JSON/注册表/显示/资产边界”，且不得改变玩法、显示文本、JSON 字段及取值兼容性（`docs/plans/string-comparison-enum-remediation-plan.md:7-12`）。以下结论据此作出。

## 逐任务结论

| 项目 | 结论 | 审查结论与关键依据 |
|---|---|---|
| T1 渲染 pass / 资源标签 | **失败（必须修改）** | 20 项 `RenderPassType`、名称表、资源单表、Profiler 20 项与预算表已建立；但 stable ID 仍由 `GetName()` 字符串而不是 Type 表生成，且不验证 Type/name 一致性，违反单一身份与 stable-ID 规格。见 H-01。 |
| T1b 图资源标签 | 通过 | `src/engine/render/graph/RenderGraph.hpp:69-160` 的 `(RenderResourceTag,name)` 单表和顺序断言成立，旧字符串 API/RejectLegacyStringAccess 仍保留。 |
| T2 popup 协议 | 通过 | `StatusPopupKind` 的 5 类、glyph 16–25 与默认 fallback 完整；`CombatSystem.cpp` 指定的“不灭剑魂”邻近行为改为 `Immune`，符合本次允许的唯一行为调整。 |
| T3 技能 buff / modifier / behavior | 部分通过 | `BuffId` 表及 call site 已替代目标裸字面量，`TalentNode::behavior_id` 仍是字符串并在注册边界转换，旧 JSON 兼容。`ActiveEffectsComponent`/SevenStar 仍 enum→临时 string→string lookup，见 M-03。 |
| T4 物品系统 | **失败（必须修改）** | 物品实体 JSON 对新 `baseId/catalystKind` 有可选旧档读取、A 回退正确，runeword runtime 已按 ID 比较；但真实存档 DTO 未保存 `baseId/weaponSubtype/catalystKind`，新存档 round-trip 丢失枚举身份。见 H-02。 |
| T5 应用层 | 通过（低风险） | EntranceId 保持旧字符串 JSON；DisplayLineCategory 读取 string/int/缺失默认，并由 UI switch 排序；Unknown helper方向符合计划。重复注释见 L-01。 |
| T6 foundation | **失败（必须修改）** | Tag/Biome 查表与 BladeMastery `ParseMasteryId` 单一实现符合要求；实际 Astrolabe node loader 绕过新的 effect parser，无法填 `behavior_id/ratio`，运行期 enum 条件失效。见 H-03。 |
| T7 VFX 词汇表 | 通过 | `VFXSequenceManager.cpp:43-90` 已使用 `FromStringTierPolicy` / `FromStringEventType`；`VFXSequencerSystem.cpp:646-677` 只消费 enum，无第二份解析 switch。Anchor / 文件扩展名比较是 JSON/资产边界。 |
| T8 GPU fixture | 通过 | `GpuFixtureType` 表保留历史字符串；`GpuGateDriver.cpp:90-121` 按 enum dispatch 且 hash 明确取 `GpuFixtureTypeToString`，旧 hash 输入字节稳定。 |
| T9 Ailment 结构化字段 | 部分通过 | 新创建和刷新都写 structured fields，旧 id 有 fallback；但活动 ailment 序列化后无法恢复 tick damage/interval，且无 round-trip 测试。见 M-02。 |
| A 催化剂回退 / crafting | 通过 | `ItemComponent.hpp:301-307` enum 优先，仅 None 走 “Legendary Core/传奇核心”旧档回退；`CraftingSystem.cpp:363` 使用 helper，未跨实体销毁持 component 指针。 |
| B Profiler 四态测试 | 通过 | `tests/unit/RenderProfilerFourStateTest.cpp:192-225` 将任意未追踪 ID 定义为 Unavailable，并验证 canonical ShadowResolve 使用其自身状态，语义正确。 |
| C GPU gate teardown | 通过 | `tests/integration/GPUHardwareValidationGateTest.cpp:585-591` 释放 `ClusteredLightingState` 与 `LightManager` singleton GPU records。 |

## 序列化与稳定性核验

| 对象 | 写出格式 | 旧格式读取 | 结论 |
|---|---|---|---|
| EntranceId | 字符串 `start` / `rift_resume` / `rift_complete_return` | `MapComponent.hpp:87-105` 读取原字符串，未知默认 Start | 兼容 |
| DisplayLineCategory | 新可选字符串；读取 string、历史整数，缺字段 Default | `SkillDefs.hpp:183-241` | 兼容；整数未做范围夹制是低风险 |
| TalentNode behavior_id | 继续写/读字符串 | `SkillDefs.hpp:243-278,307-392` | 兼容 |
| Astrolabe effect | `to_json` 仍写旧 `type/trait_id/value`；通用 reader 可由旧 `IntToCritMult:ratio` 解析 | 实际 node reader 未调通用 reader | **不兼容目标运行语义，H-03** |
| ItemComponent 单体 JSON | 新写 `baseId`、`weaponSubtype`、`catalystKind`；缺字段默认 0/None | `ItemComponent.hpp:209-295` | 单体 JSON 兼容 |
| 实际 CharacterSaveData Item DTO | 仍不写上述三字段 | `SerializedItem.hpp:16-100` + ItemFactory save/restore | **新数据保存即丢身份，H-02** |
| Buff/Ailment | 新写 structured fields；旧档缺字段 fallback id | `Buff.hpp:88-122` | 字段兼容，但 tick runtime state 未持久化，M-02 |
| Render stable pass ID | 合法既有名称当前仍走原 canonical-name FNV 输入 | Type/name 不一致时可分裂 | 仅偶然稳定，H-01 |
| GPU fixture hash | 使用原 fixture string table | `GpuGateDriver.cpp:90-121` | 兼容 |

## 发现项（按严重度）

### High（提交前必须修改）

#### H-01：RenderGraph 的 stable pass ID 未由 RenderPassType 表生成，Type、名称与 contract 可分裂

- **位置**：`src/engine/render/graph/RenderGraph.cpp:662-721,742-749`；`src/engine/render/graph/RenderGraph.hpp:300-351`；`src/engine/render/debug/RenderProfiler.cpp:79-97`；计划 `docs/plans/string-comparison-enum-remediation-plan.md:42`。
- **问题**：`ValidatePassIdentityContract()` 从 `node.passName`（`GetName()`）计算 `canonicalPassName` 和 `stablePassId`，只检查空、重名和 hash collision；不检查 `RenderPassType` 的范围，也不要求名称等于 `kRenderPassNames[Type()].full`。Profiler 却按 Type 归档；`ResolvePassContractStage(type,name)` 在名称不匹配时直接跳过 contract。一个生产 pass 的 Type/name 配错会导致 profiler、trace/hash 与 graph contract 使用不同身份而不报错。
- **影响**：未满足 T1 “pass 身份单一枚举、stable ID 由 Type 索引表生成”的验收。现有标准 20 pass 只是因名称恰好未改而保持历史 FNV 值，不能保证后续稳定性/契约正确性。7 个测试 generic pass 以 `Type::Scene` 加非 canonical name 作为豁免，也证明当前 contract 没有硬保护。
- **必须修改**：以 `kRenderPassNames[Type()].full`（同一旧字节串）生成 canonical name/FNV stable ID；验证 Type 有效和 canonical name 一致。VFX snapshot 等需要不同显示名的 pass 应有独立 Type 或受控、测试覆盖的例外，不能使任意 mismatch 绕过 contract。增加 20 项历史 stable-ID golden regression，并更新测试 generic pass 的明确 fixture 类型/白名单。

#### H-02：新引入的 Item 身份字段没有进入实际 SaveManager 持久化 DTO

- **位置**：`src/game/foundation/data/SerializedItem.hpp:16-100`；`src/game/systems/item/ItemFactory.cpp:225-280,934-1004`；`src/game/application/persistence/SaveManager.cpp:40-75,172-287`；`src/game/foundation/components/ItemComponent.hpp:151-170,209-295`。
- **问题**：`ItemComponent` 已有 `baseId`、`weaponSubtype`、`catalystKind`，但 `SerializedItem`、`serializeItem` 与 `restoreItem` 均未包含它们。SaveManager 对背包、装备、stash 都走该 DTO，因此新物品一经保存再加载就变为 `baseId=0`、`weaponSubtype=None`、`catalystKind=None`。
- **影响**：破坏 T4 身份枚举和“JSON/存档兼容不变”约束；Greatsword 的 subtype 限制和以 baseId 取基础属性可能失效，LegendaryCore 仅靠名称回退幸存。`ItemFactory.cpp` 中“new save carries baseId”的注释与事实相反。
- **必须修改**：把三字段加入 `SerializedItem` 与 JSON，读取旧档时缺字段默认 `0/None`；在 ItemFactory save/restore 双向复制；补实际 SaveManager/DTO round-trip（weapon Greatsword、LegendaryCore、baseId、socket/stash）测试，断言旧 DTO 仍可读。

#### H-03：Astrolabe 的实际 node JSON loader 绕过新的行为枚举解析，IntToCritMult 不会进入运行时条件

- **位置**：`src/game/foundation/data/TalentData.hpp:239-268,286-311,362-387`；`src/game/foundation/stats/AttributePipeline.cpp:614`；计划 `docs/plans/string-comparison-enum-remediation-plan.md:101`。
- **问题**：通用 `AstrolabeNodeEffect::from_json` 可读新 `behavior_id` 或旧 `"IntToCritMult:0.3"` 并填 enum/ratio；但 `AstrolabeTalentNode` 与 `StarNode` 的手写 effects 循环只复制 `type/trait_id/value`，未调用它。实际 loader 由这些 node parser 建造 effects，而 runtime 已只判断 `behavior_id == AstrolabeBehaviorId::IntToCritMult && ratio > 0.0f`。
- **影响**：主加载路径得到 None/0，无法满足 T6 的旧 JSON 兼容和行为枚举化验收。旧手写 parser 已可能使 ratio 路径无效，不能断言本次首次引入数值回归；但本次实现没有接通计划要求的链路，也没有 loader 回归测试。
- **必须修改**：两个循环改为 `eff_json.get_to(eff)` 或唯一 helper；分别测试旧 value 字符串、新 explicit behavior_id、TalentLoader 实载以及 AttributePipeline 的 crit multiplier 结果。

### Medium（应在复审前修复或以明确产品决定豁免）

#### M-01：无效符文会将符文之语缩短后继续启用

- **位置**：`src/game/systems/item/RunewordSystem.cpp:98-109,128`。
- **问题/影响**：未知 rune 只警告后 `continue`，仍将删除该元素后的 `runeIds` 保存。配置中 N 个 rune 的错误配方可能变成可匹配 N-1 rune 配方，改变 crafting 结果。
- **建议**：任一名称解析失败即拒绝该 definition（或将其标 invalid），并加 malformed runeword JSON 测试；`parseWeaponSubtype` 也补 `Greatsword`（当前 shipped `runewords.json` 未使用，低即时性）。

#### M-02：活动 ailment 保存/加载后不再造成 tick 伤害，T9 缺 round-trip 覆盖

- **位置**：`src/game/foundation/components/Buff.hpp:55-122`；`src/game/systems/combat/AilmentEngine.cpp:178-201,598-650`；`tests/unit/AilmentEngineTests.cpp:54-216`。
- **问题/影响**：Buff serializer 写 structured identity/power，却不写 `tick_interval/tick_damage/tick_timer/tick_damage_tag`，读入也不将 `ailment_power` 回填 `tick_damage`；Tick 在 damage <= 0 时跳过。已有 serializer 看似原本也遗漏这些字段，故尚不能归因为本次新回归，但 T9 以旧存档回退为验收且新增 `ailment_power` 未闭合该保存语义。
- **建议**：持久化完整 tick state，或对 structured managed ailment 从 `ailment_power`、contract 恢复 tick damage/interval/tag；增加新格式和旧 id-only 格式 load→tick 回归测试，含非法 `ailment_type`。

#### M-03：BuffId helper 仍在技能路径进行 string 分配与比较

- **位置**：`src/game/foundation/components/Buff.hpp:151-197`；`src/game/foundation/components/SevenStarSlashShared.hpp:54-81`；`src/game/systems/skill/behaviors/SevenStarSlash.cpp:539-542`。
- **问题/影响**：`Has/Get/Remove(BuffId)` 每次建 `std::string` 后遍历 `effect.id`；SevenStar 还把 enum转 string_view给旧 helper。虽无裸字面量，仍未完全实现“核心逻辑 enum”/热路径无字符串目的。
- **建议**：至少使用 `std::string_view` 避免临时堆分配，并为 SevenStar 添加 `BuffId` overload；长期在 BuffEffect 缓存/存储 parsed ID，以 string 仅作存档边界。

### Low

#### L-01：维护性瑕疵

- `src/game/foundation/components/MapComponent.hpp:139-141`：`PendingMosaicEditorTag` 注释重复。
- `src/game/systems/item/CraftingSystem.cpp:339-340`：`// 1. Validation` 注释重复。
- `SkillDefs.hpp:202-205`：历史整数 `display_category` 直接 static_cast，未知值不归一为 Default；建议检查范围。
- `ItemComponent.hpp:99-142`：Rarity conversion 是 JSON/资产边界可接受，但每次解析都会分配/upper；保持只在加载时调用。

## 残余字符串比较审查

生产代码的精确搜索命中已逐项归类。未发现 T1–T9 指定的裸字面量仍在已转换的 combat/skill/AI 热分支中；命中的合理边界包括：

- JSON/配置/资产解析：`TalentData.hpp:258` legacy `IntToCritMult`、`MapComponent.hpp:87-105` EntranceId、`ItemStats.hpp:199-209` item slot tags、`BladeMasteryRegistry.cpp:11-61`、`VFXSequenceManager.cpp:43-90`、`AilmentEngine.cpp` contract token parser、QualityTier/Material/Condition compiler。
- 显示、日志、路径或命令行：`PopupRenderer.hpp:33-39`、`UISkillTalentTree.cpp:440`、日志 / file extension / CLI flag comparisons。
- 明确旧存档回退：`ItemComponent.hpp:301-307` 的 LegendaryCore 名称判断。

`M-03` 是保留的可枚举化技能路径；除它以外没有发现需要在本整改范围内继续替换的直接裸字符串身份比较。

## 验证状态与环境因素遗留

- 构建：`logs/build_fix_final.log` 记录 0 error；本轮未重跑。
- ctest：`logs/ctest_final.log` 为 19/21 通过。以下两项按用户说明和历史 artifact **不作为本次代码缺陷**：
  1. `nmd.tests.performance`：`RenderingBenchmark.cpp:75` 的 `GPUParticleSystem` mean **0.552 > 0.5**，以及 `RadianceCascadesBenchmark.cpp:327` holographic ratio；测量时主机高负载。
  2. `nmd.tests.gpu.hardware`：RTX 4070 SUPER 的 JFAPass p95 **1.4–1.9ms > 0.8ms**；`artifacts/gpu-gate/` 自 2026-08-01 以来 6 份 artifact 均为 NO_GO。
- T1 profiler HUD、popup glyph、技能 buff、runeword/catalyst、entrance、IntToCritMult 等计划列出的手测并没有可核验证据；特别是 H-01/H-02/H-03 修复后必须补齐相关 focused test 与手测，才可重新判断完成定义 `docs/plans/string-comparison-enum-remediation-plan.md:142-148`。

## 复审出口

1. 先修 H-01、H-02、H-03，并加入相应回归测试；H-02 必须保证新保存档和旧保存档双向预期。
2. 处理 M-01；同时修/明确豁免 M-02 与 M-03，补老/新 ailment 持久化和 BuffId path 测试。
3. 不要求在当前高负载环境下重跑性能/GPU 硬件测试；保留其环境说明。修复后至少运行不依赖性能/GPU 的 focused unit/integration 与正常构建，归档结果。
4. 复核全库字符串比较分类、`git diff --check`，并完成计划手测清单的可核验证据。
5. 只有 High 关闭、Medium 有明确决定、计划完成定义具备证据后，才可复审并结论为 **提交**。

---

## 复审修复闭环（2026-08-13）

### H-01（stable pass ID 身份统一）已修复

- `src/engine/render/graph/RenderGraph.cpp` `ValidatePassIdentityContract()` 新增名称/类型一致性硬检查：`Type()` 映射到 `kRenderPassNames` 表项的 pass，其 canonical name 必须等于该表项（否则 Error + failed）；名字不匹配任何表项的 pass（受控例外，如 `VFXEmissionSnapshotPass`，Type 别名 VFX）保持豁免。
- stable-ID 派生仍按原 FNV 字节流（salt + canonical name），20 个标准 pass 的数值与整改前逐字节一致，因此 profiler/门禁历史数据不变；表与名字分歧现在会 fail-closed 而不是静默分裂。
- 回归测试：`tests/unit/RenderGraphValidationTest.cpp` 新增 `[Unit] RenderGraph - stable pass ids are frozen for the canonical name table`，断言 20 个表名派生 ID 等于冻结的硬编码期望值（如 ScenePass=0x1FF39E00、LightingPass=0xD6250DDA、LightCullingPass=0xF8B11E72、ShadowResolvePass=0x5F9C03D4），并断言互不相同。
- 测试通用 pass 修正：`tests/unit/RenderGraphValidationTest.cpp`、`tests/integration/RenderGraphV3ContractsIntegrationTest.cpp`、`tests/integration/RenderGraphV5ContractsIntegrationTest.cpp` 的 `TestRenderPass::Type()` 改为按名称查表派生类型（非表名仍回退 Scene），消除"名字是表名但类型恒为 Scene"的伪造分裂。
- 该检查的实战价值已被验证：本轮引入时立即捕获了 ai.unit/unit 中 3 类 TestRenderPass 名称-类型不一致。

### H-02（Item DTO 持久化枚举身份）已修复

- `src/game/foundation/data/SerializedItem.hpp`：`SerializedItem` 新增 `baseId`/`weaponSubtype`/`catalystKind` 三字段；替换 `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 为显式 `to_json`/`from_json`——写 13 键，读时新 3 键 `contains` 守卫、缺省 `0/None/None`（旧档兼容，不抛异常）。
- `src/game/systems/item/ItemFactory.cpp` `serializeItem`/`restoreItem` 双向复制三字段；SaveManager 的三条持久化路径（背包/装备/stash）自动获得身份。
- 旧档行为不变：旧 JSON 无新键 → 默认值 → `IsLegendaryCoreCatalyst` 名称回退等既有兼容路径照常。

### H-03（Astrolabe 预解析旁路）已修复

- `src/game/foundation/data/TalentData.hpp` 新增 `PreparseAstrolabeEffect(AstrolabeNodeEffect&)`，集中 numeric_value/ratio/behavior_id（`IntToCritMult` 前缀）计算；`AstrolabeNodeEffect::from_json` 与 `AstrolabeTalentNode::from_json`、`StarNode::from_json` 两个手动 effects 循环三处调用；手动循环另补 `behavior_id` 键读取。
- 旧 `"IntToCritMult:0.3"` 字符串与新显式 `behavior_id` 两条路径现在都在加载期预解析，`AttributePipeline.cpp:614` 的运行时枚举条件得以生效。

### M-01（无效符文缩短配方）已修复

- `src/game/systems/item/RunewordSystem.cpp` `loadRunewords`：未知符文名不再 `continue` 缩短序列，改为 `LOG_ERROR` + 拒绝整个 definition（`continue` 跳过该 runeword）。

### M-02 / M-03 豁免决定（产品决定）

- **M-02（ailment tick state 不持久化）**：审查已确认该缺口为本次整改前既有行为（旧 serializer 同样遗漏 tick 字段），T9 仅新增结构化 identity/power 且旧档回退正确。将完整 tick 状态持久化涉及存档 schema 扩展与契约评审，超出本整改范围。**决定：豁免，另立跟踪项**（建议在存档/契约专项处理，含非法 `ailment_type` 测试）。
- **M-03（BuffId helper 仍做 string 分配）**：`Has/Get/Remove(BuffId)` 是枚举化整改的正确方向，残余的 enum→string 转换仅在技能热路径产生一次小字符串分配，且已无裸字面量。改为 `string_view`/缓存 parsed id 属性能微优化，非身份正确性问题。**决定：豁免，列入后续性能专项**。

### 验证证据（本轮实测）

- 构建：`logs/build_t11fix3.log`——RelWithDebInfo 预检全过、ALL_BUILD 0 error。
- 测试：`logs/ctest_t11fix3.log`——**15/15 通过**（排除 perf/gpu 的 ci.nonperf/unit/integration/item/progression/combat/skill/world/ui/ai/parity；其中 4 个测试 exe 合计 152+ 用例 0 失败）。此前本次整改引入的回归（json 302、催化剂融合、Profiler 四态、跨用例污染、identity contract）均已在环内修复并回归通过。
- 未重跑 `nmd.tests.performance`/`nmd.tests.gpu.*`：用户正在游玩，主机高负载；按用户指示与复审出口第 3 条不作为本次缺陷。
- `git diff --check` 通过（无空白错误）。

### 更新后结论

**提交（建议）**：High 全部关闭、M-01 已修、M-02/M-03 已有明确豁免决定及去向；修复后构建与 15/15 非性能测试通过；性能/GPU 门禁的既有失败已有环境归因与历史 artifact 佐证。剩余风险：M-02/M-03 豁免项的跟踪落实，以及计划手测清单尚未人工执行（建议在低负载时段完成，或作为后续验证项）。
