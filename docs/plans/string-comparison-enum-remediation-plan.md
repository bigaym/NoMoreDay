# 字符串比较枚举化整改计划

## 范围与输入

基于 2026-08-13 全库字符串比较滥用审查（7 个并行子代理，覆盖 src/ 约 640 文件）产出。目标：消除"本可用枚举/类型标识却用魔法字符串比较"的代码，全部问题修复（含中/低严重度），保留序列化/JSON/资产路径/显示文本等合法字符串用法。

## 设计决策（全局）

1. **枚举放源层、字符串留边界**：所有枚举定义放在使用方所在模块的头文件（如 `BuffId` 在 foundation/data，`RenderPassId` 在 render/debug）；JSON 与显示文本转换只在序列化/注册表边界发生。
2. **不变更产品行为**：本次为重构，不改变任何玩法数值、显示文本、序列化格式兼容性（JSON 字段名与取值保持兼容）。
3. **防止并发冲突**：任务按"文件不重叠"原则分组；共享文件（Buff.hpp、GPUHardwareValidationGate.cpp、SkillSystem.cpp）只归一个任务所有，跨波次顺序执行。
4. **验证策略**：实现子代理不单独跑全量构建（耗时），负责静态自查 + grep 验证零字面量残留；主线程在全部任务完成后统一跑 `build.bat`（RelWithDebInfo）+ 相关 ctest，再做最终 review。

## 任务拆分

### 波次 1（并行，文件互不重叠）

- **T1 渲染 pass 身份与预算/图资源标签统一**（render H1/H2/M1/M2/M3、validation L4）
- **T2 Popup 状态协议枚举化**（render H3）
- **T3 技能 buff id 与 modifier key 枚举化**（skill H1/H2/M1/M2/L1/L2、ai L2、foundation M-1 behavior_id）
- **T4 物品系统身份枚举化**（item H1/H2/H3/M2/M3/L1/L2/L3/L4）
- **T5 应用层入口/UI 身份枚举化**（application H9、application M1/M2、render M4、app M5）
- **T6 foundation 行为 id 与注册表查表统一**（foundation H1、M2/M3/L1/L2、combat 无关部分）
- **T7 VFX 词汇表单源化**（vfx M1/M2/M3）

### 波次 2（并行，依赖波次 1 的文件所有权）

- **T8 GPU fixture 类型枚举化**（依赖 T1 完成后 GPUHardwareValidationGate 稳定）
- **T9 Ailment 结构化实例身份**（依赖 T3 完成后 Buff.hpp 稳定）

### 波次 3

- **T10 全量构建 + ctest**
- **T11 最终 review**

## 各任务规格

### T1 渲染 pass 身份（T1a）+ 预算表 + 图资源标签（T1b）

文件所有权：`src/engine/render/graph/RenderPass.hpp`、`src/engine/render/passes/*.hpp/*.cpp`（20 个 pass）、`src/engine/render/debug/RenderProfiler.hpp/.cpp`、`src/engine/render/graph/RenderGraph.hpp/.cpp`、`src/engine/render/validation/GPUHardwareValidationGate.cpp`（仅 pass 相关区域）、`src/engine/render/core/RenderResourceDescriptor.hpp`。

T1a 原理：pass 身份单一枚举。在 `RenderPass.hpp` 定义 `enum class RenderPassType : uint8_t { Scene, Lighting, HeightShadow, OccluderExtract, JFA, RadianceCascades, GIComposite, FluidSimulation, Volumetric, VFX, GPUText, GPULoot, UIWorld, PostProcess, Distortion, Composite, LightCulling, ShadowPrepare, ShadowBuild, ShadowResolve, Count }`；RenderPass 基类加 `[[nodiscard]] virtual RenderPassType Type() const = 0;`（或纯虚改虚返回），GetName() 保留仅用于调试输出/日志。RenderProfiler 的 `RenderPassId` 改为 `using RenderPassId = RenderPassType;`（或直接类型替换），补 4 个缺失成员后 `FromPassName` 完整覆盖 20 项；合并 `ToString`/`FullPassName` 为单一张表；RenderGraph.cpp `ResolvePassContractStage` 改查 `constexpr std::array<std::string_view, Count> kPassContractStages` 按 Type() 索引；RenderGraph.cpp:617 `BeginCpuPass` 调用改 `BeginPass(node.pass->Type())`（BeginCpuPass 删除或保留但不再被图调用）；StablePassId 由 `Type()` 索引的 constexpr 数组生成（数值与旧哈希一致：先验证旧值 hash 生成逻辑，若旧值为 name hash 则保留 hash(name) 实现但 name 来自表，保证 stable id 不变）；`canonicalToPassIndex`/`idToCanonicalName` 以表为源。GPUHardwareValidationGate `GetPassBudgets` 改为 `std::array<float, Count>` constexpr 预算表按枚举索引；`BuildGiPassTrace` 遍历枚举表输出 pass 名（名字从同一张表取）；:1443-1444/:1725-1726 的 StablePassId(CanonicalizePassName(...)) 改按枚举。`\"Ultra\"` 参数（:1017）改 `ForceTier(QualityTier)` 直接传枚举。

T1b 原理：`ToResourceName`/`ToResourceTag` 双表合一。定义 `constexpr std::array<std::pair<RenderResourceTag, std::string_view>, kResourceTagCount> kResourceTagTable`，ToResourceName 用线性查找或按枚举序索引（保持枚举值与数组序一致），ToResourceTag 反向线性查找。`descriptorIdsByName` 的 key 用表名生成，行为不变。**不删** RejectLegacyStringAccess 与旧字符串 API（保持契约）。

验收：`grep -rn "GetName()"` 仅剩调试用途；RenderProfiler 16→20 项；编译零警告；ctest 渲染相关用例通过；手测 profiler HUD 上 LightCulling/ShadowPrepare/ShadowBuild/ShadowResolve 4 项出现 CPU 计时（回归验证：此前静默丢失）。

### T2 Popup 状态协议

文件：`src/engine/render/.../DamagePopupManager.hpp`（定义 `enum class StatusPopupKind : uint8_t { Crit, Dodge, Block, Immune, Absorb }` + `constexpr std::string_view DisplayName(StatusPopupKind)` 表）、`PopupRenderer.cpp`、`game/systems/combat/EffectSystem.cpp/.hpp`、`game/systems/combat/CombatSystem.cpp`。

原理：`EmitStatus`/`EmitStatusPopup` 签名加 kind 参数（或改传 kind），PopupRenderer 内 switch(kind) 选 glyph 对（16/17 暴击、18/19 闪避、20/21 格挡、22/23 免疫、24/25 吸收），显示文本走 DisplayName 表。CombatSystem.cpp:274-281/457/465/638 传 `StatusPopupKind::Dodge/Block`。免疫/吸收分支改为枚举成员保留 glyph 能力（死分支不删、行为不变，后续有调用者即用）。

验收：grep 无 `\"闪避\"` 等比较；编译通过。

### T3 技能 buff id / modifier key / behavior_id

文件：`src/game/foundation/components/Buff.hpp`、新增 `src/game/foundation/data/BuffIds.hpp`、`src/game/systems/skill/behaviors/SkillBehaviorBase.hpp`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/skill/behaviors/*.cpp`（FlowingThrust/BladeWard/BladeBoomerang/BloodSea/PhantomFlash/SwordArray/HeavenlySwordDescent/SevenStarSlash）、`src/game/systems/combat/SwordIntentVisualSystem.cpp`、`src/game/systems/ai/EnemyAIBehaviors.cpp`、`src/game/foundation/components/SevenStarSlashShared.hpp`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/systems/skill/BehaviorInjectionRegistry.cpp/.hpp`。

原理：
- BuffIds.hpp：`enum class BuffId : uint8_t { SwordStep, BladeWard, BladeBoomerangBleed, BladeBoomerangGuardQi, BloodSeaMiasma, PhantomFlashShadowHide, SwordArraySlow, SwordArrayArmorShred, HeavenlySwordFieldResist, HeavenlySwordMeteorCore, FlowingThrustElementBody, SevenStarSwordStepMirage, LingJianHuTi, SupportShield, AssassinBackstabBoost, Count }` + `constexpr std::array<std::string_view, Count> kBuffIdNames`（名字与原字面量逐字一致）+ `ToString(BuffId)`/`FromString(std::string_view)`。
- Buff.hpp：`ActiveEffectsComponent` 增加 `Get(BuffId)`/`Remove(BuffId)`/`Has(BuffId)` 重载（内部转 ToString 调 string 版），`AddOrRefresh` 不动。`BuffEffect.id` 保持 std::string（序列化兼容）。
- 所有创建点 `effect.id = "xxx"` → `std::string(ToString(BuffId::Xxx))`；比较点 → `Has(BuffId::Xxx)` 或 `effect.type` 语义不变时用 `FromString(effect.id) == BuffId::Xxx`（统一用 Has/Get 重载优先）。
- SkillSystem.cpp:391 的 HasSwordStepBuff 与 SwordIntentVisualSystem.cpp:41 重复实现合并为 `ActiveEffectsComponent::Has(BuffId::SwordStep)`（定义于 Buff.hpp，两处调用）。
- getModifier：`enum class ModifierParam : uint8_t { Effectiveness, RangeMultiplier }`，签名改 `getModifier(uint32_t, ModifierParam, float)`，switch 取 trigger.effectiveness/range_mult。**修复 bug**：HeavenlySwordDescent.cpp:564 `\"field_effectiveness\"` —— 查 SkillContract.hpp 确认是否存在 field 语义字段；无则映射 ModifierParam::Effectiveness（同时修天赋失效 bug，数值 0.04 为 default 需核对节点 JSON 是否有 effectiveness 覆盖，若无则在任务报告标注风险）。
- behavior_id：`SkillDefs.hpp` 定义 `enum class SkillBehaviorId : uint8_t { None, ShadowCaster, Count }` + ToString/FromString 表；BehaviorInjectionRegistry 改 `std::array` 或 switch 索引；SkillSystem.cpp:562-563 改枚举比较；`behavior_id` JSON 字段解析在注册边界做 FromString。

验收：grep 全库无 `\"flowing_thrust_swift\"` 等裸字面量（除 BuffIds.hpp 表与 JSON 边界）；编译通过；SevenStarSlashShared.hpp 的 kSwordStepBuffId 迁移后删除旧常量并确认无剩余引用。

### T4 物品系统身份

文件：`src/game/foundation/components/ItemComponent.hpp`、`src/game/foundation/components/ItemStats.hpp`、`src/game/systems/item/ItemFactory.hpp/.cpp`、`src/game/systems/item/CraftingSystem.cpp/.hpp`、`src/game/systems/item/RunewordSystem.hpp/.cpp`、`src/game/systems/item/MaterialRegistry.cpp`、`src/game/systems/item/LootFilter.cpp`、`src/game/application/ui/UICraftingController.cpp`。

原理：
- CatalystKind：`ItemComponent.hpp` 加 `enum class CatalystKind : uint8_t { None, LegendaryCore }`，ItemComponent 加 `catalystKind` 字段（JSON 兼容：若 ItemComponent 有 to_json/from_json 则加可选字段，默认 None）；UICraftingController.cpp:897 创建时设 `catalystKind = LegendaryCore`；CraftingSystem.cpp:363 改 `catalyst->catalystKind != CatalystKind::LegendaryCore`。显示名"传奇核心"仍为数据。
- Runeword：`RunewordDefinition.runes` 改 `std::vector<uint32_t> runeIds`；from_json 在解析时用 name→id（LoadRunewords 内建 map，或复用 getRuneByName 后删除）；`checkForRuneword` 直接按 id 比较 socketedRunes 的 item id；删除 `getRuneByName`（确认无其他调用者）。
- Greatsword：`WeaponSubtype` 加 `Greatsword`；ItemFactory.cpp:76-77 name.find 判定删除，`getRandomTextureForType(WeaponSubtype)` 纯 switch 加 Greatsword case；BaseItemDef 表（:259-265）补 `WeaponSubtype subtype` 字段，大剑条目设 Greatsword，其余按原名推导；若表数据来自 JSON 则在解析时判定。
- 槽位标签：`enum class ItemSlotTag : uint8_t { Weapon, Armor, Gloves, Boots, Jewelry, Misc, Count }` + `slotTagsFor(EquipmentSlot)` 单一函数（ItemFactory.cpp 两处 switch 合并调它）；AffixDefinition.allowedTags 改 `std::bitset<Count>` 或 uint8_t 位掩码（对齐 requiredSkillTags 的 ParseTagList→Tag 模式），JSON 解析处转换。
- BaseItemDef：加 `uint32_t baseId`（或 `BaseItemId` 枚举）；`findBaseByName` 改 `findBaseById`，调用点（含 CraftingSystem.cpp:210）改传 id；`getBaseStatRange` 按 id。
- Rarity 单源：MaterialRegistry.cpp 与 LootFilter.cpp 的 rarity 表统一为一个 `RarityFromString/ToString`（大小写归一），LootFilter.cpp:227-245 序列化/反序列化不对称修复（统一按同一枚举 int 读写或统一字符串读写，以现有存档格式为准——若存档已有字符串格式则写入也改字符串）。

验收：grep 无 `\"大剑\"`/`\"Common\"` 比较、无 getRuneByName；ItemFactory 两处 slotTags switch 只剩一处；编译通过。

### T5 应用层入口/UI

文件：`src/game/foundation/components/MapComponent.hpp`、`src/game/application/scene/SceneManager.hpp/.cpp`、`src/game/application/GameplayState.cpp`、`src/game/systems/world/PortalSystem.cpp`、`src/game/application/ui/UISkillTalentTree.cpp`、`src/game/foundation/data/BuffRegistry.hpp/.cpp`、`src/game/application/ui/UIRenderer.cpp`、`src/game/application/MosaicEditorState.cpp`、DisplayLine 所在头文件。

原理：
- `enum class EntranceId : uint8_t { Start, RiftResume, RiftCompleteReturn, Count }`（MapComponent.hpp）；MapComponent 字段改 EntranceId（JSON 边界 FromString）；SceneManager 重载默认参 `= EntranceId::Start`，内部比较改枚举（:348/:414）；GameplayState.cpp:1275/1333 传枚举；PortalSystem 透传枚举。
- DisplayLine：结构加 `uint8_t displayCategory`（或枚举 DisplayLineCategory{Label, Damage, Duration, Frequency, Speed, Range, Cost, Cooldown, Default}）；数据填充处（构造/JSON 解析）写类别；UISkillTalentTree.cpp:471-475 删除中文子串判定，改 switch(displayCategory) 定 priority；stable_sort 不变。
- BuffRegistry：`IsUnknownVisual(const BuffVisualData&)` 或 `IsUnknown(BuffType)` helper；UIRenderer.cpp:1546 改调用。默认条目 name=\"Unknown\" 保留（显示用）。
- MosaicEditorState.cpp:393：改 `name.empty()` 单独判定，或加 `IsUnknown` 常量复用；保持既有显示行为。

验收：grep 无 `\"rift_resume\"`/`\"伤害\"` 等比较；编译通过。

### T6 foundation 行为 id 与注册表

文件：`src/game/foundation/data/TalentData.hpp`、`src/game/systems/modifier/AttributePipeline.cpp`、`src/game/foundation/data/BladeMasteryRegistry.hpp/.cpp`、`src/game/foundation/data/SkillRegistry.cpp`、`src/game/foundation/components/TagRegistry.hpp`、`src/game/foundation/data/BiomeRegistry.cpp`、`src/game/systems/skill/...`（无，behavior_id 归 T3）。

原理：
- Astrolabe 行为：`enum class AstrolabeBehaviorId : uint8_t { None, IntToCritMult, Count }`；TalentData.hpp:241 from_json 边界解析 `\"IntToCritMult:0.3\"` → behavior_id + ratio（兼容旧 JSON：值字符串前缀解析保留）；AstrolabeNodeEffect 加 `behavior_id` 字段（JSON 可选）；AttributePipeline.cpp:613-617 改 `eff.behavior_id == AstrolabeBehaviorId::IntToCritMult`。
- ParseMasteryId：BladeMasteryRegistry.cpp:13-45 与 SkillRegistry.cpp:20-31 去重——由 BladeMasteryRegistry 导出单实现，SkillRegistry 调用；死分支 ParseProfessionId（:27-30 if 两侧同值）修正或删除。
- TagRegistry：kTagInfoTable/GetTagName/TagFromString 三份映射合一——以 kTagInfoTable 为唯一源，GetTagName/TagFromString 改为表查找（constexpr 线性查找或二分）。
- BiomeRegistry.cpp:37-46 style if 链改查表（对齐 :55-63 kFeatureMap 模式）。

验收：grep 无 `\"IntToCritMult\"` 运行时比较（仅 JSON 解析边界）；编译通过。

### T7 VFX 词汇表

文件：`src/engine/vfx/VFXTypes.hpp`、`src/engine/vfx/VFXSequenceManager.cpp`、`src/game/systems/vfx/VFXSequencerSystem.cpp`、`src/engine/render/core/QualityTierManager.cpp`、QualityTier 定义头（RenderConstants.hpp 或 render/core）。

原理：VFXTypes.hpp 增加 constexpr `ToString(TierPolicy)`/`FromStringTierPolicy`（接受 \"strict\"/\"degrade\"/\"skip\" 归一）、`ToString(EventType)`/`FromStringEventType`（小写 + 别名 materialswap→MaterialSwap 归一）；QualityTier 的 FromString 单点（选 QualityTierManager.cpp:269-281 或 VFXSequenceManager 之一为源，另一处调用）；VFXSequenceManager.cpp:102-145 与 VFXSequencerSystem.cpp:103-129 两套大小写解析统一调用同一 FromStringEventType。soundId 等动态资产名不动。

验收：grep 无第二份 tier/event 解析 switch；编译通过。

### T8（波次 2）GPU fixture

文件：`src/engine/render/validation/GPUHardwareValidationGate.hpp/.cpp`（fixture 区域）、`src/app/.../GpuGateDriver.hpp/.cpp`。

原理：`enum class GpuFixtureType : uint8_t { None, CaveColorBleed, DynamicCombatEmissive, OutdoorLightPressure }` + ToString 表；FixtureConfig 加 `GpuFixtureType type` 字段（name 保留用于报告标题/哈希）；GetStandardFixtures 三处 cfg.name=... 处同步设 type；GpuGateDriver BuildSceneOnly/驱动改按 type switch 分派（else→None return false 保持）。

### T9（波次 2）Ailment 结构化身份

文件：`src/game/foundation/components/Buff.hpp`（仅波次 1 完成后）、`src/game/systems/combat/AilmentEngine.cpp`。

原理：BuffEffect 加 `std::optional<AilmentInstance>` 或 `bool managed_ailment` + `AilmentType ailment_type` + `float ailment_power` 字段（to_json/from_json 加可选键，旧存档无字段时默认 false 回退字符串解析路径）；AilmentEngine 热路径（:149/:353-359/:575-580）改字段判定，id 字符串仅保存兼容。若实现中发现字段拆分风险过大，允许降级方案：解析/编码收敛为两个函数（parse 一次缓存到并行结构），并在任务报告标注偏离原因。

### T10 构建验证

主线程执行：`build.bat`（RelWithDebInfo）全量构建，零错误；跑渲染/物品/技能相关 ctest（用 `ctest --test-dir build -N` 找到相关用例集再跑，输出重定向日志文件）；证据写入 `docs/reports/string-enum-remediation/evidence.md`。

### T11 最终 review

reviewer 子代理：逐文件 diff 审查（对照本计划验收标准），检查：零行为漂移（数值/显示文本/JSON 兼容）、枚举边界转换完整性、并发合并冲突（git status 确认无未合并）、裸字面量残留 grep。产出 `docs/reviews/2026-08-13-string-enum-remediation-review.md`，结论 提交/修改。

## 测试方法

- 单元：ctest 全量（重点渲染 graph、profiler、物品工厂、技能系统、buff、天赋、vfx 序列相关用例）。
- 功能手测（构建后）：profiler HUD 显示 20 个 pass 计时；技能施放（七星剑/天剑/血流）buff 表现不变；符文之语合成；催化锻造；城镇传送门/裂缝续战入口出生点；暴击/闪避/格挡浮字字形；六扇区天赋 IntToCritMult 节点。
- 回归证据：grep 输出存 evidence.md。

## 验证任务完成（完成定义）

1. 构建零错误零新增警告。
2. 全部 grep 验收项通过（各任务列出的裸字面量全库仅剩枚举表/JSON 边界/显示文本数据）。
3. ctest 通过。
4. 手测清单关键项通过（至少 pass HUD、popup 字形、技能 buff）。
5. review 报告结论为 提交。
