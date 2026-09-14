# 设计文档 ↔ 实现 偏差审计（2026-09-15）

> **后续处理决策（2026-09-15）**
> - 4.6 装备/符文/传说、4.8 天赋/局外成长：**跳过**（重构未到，文档与实现均暂不动）。
> - 4.2 UMR：**下一个待办事项**（需修实现，见 §3.1 与 §4.2），本次不改 UMR 文档。
> - 其余簇：**仅同步已落地机制到文档**（修正「文档过时」「文档未覆盖」）；「实现偏差」「文档缺失」等缺口与疑似缺陷条目原样保留，不改动、不写入文档。
> - 文档更新禁止携带过程信息、禁止新建实现文档，按算法/实现思路/数据模型要素更新。

## 1. 范围与方法

- 目标：找出 `设计文档/` 与 `src/` 实现之间的偏差点，供决策「改实现」或「改文档」。
- 方法：11 个并行只读子代理，逐文档对照代码；代码检索优先 codebase-memory 图（project `D-PRJ-NoMoreDay`），辅以 `rg`/Read；每条结论附 `file:line` 或文档行号。
- 未修改任何代码/文档，未编译、未运行测试。
- 类型口径：
  - **实现偏差** = 实现与文档不符，且实现疑似缺失/缺陷（倾向改实现）。
  - **文档过时** = 实现已演进且自洽，文档未同步（倾向改文档）。
  - **文档未覆盖** = 实现有、文档缺（倾向改文档）。
  - **文档缺失** = 文档描述的功能整体未实现（需决策）。

## 2. 总览

| 文档簇 | 实现偏差 | 文档过时 | 文档未覆盖 | 文档缺失 | 合计 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 战斗核心（战斗系统与属性设计 / 核心战斗与角色设计 / CombatV2开关） | 3 | 7 | 3 | 0 | 13 |
| UMR 统一修饰器运行时 | 5 | 1 | 2 | 1 | 9 |
| 职业被动和技能设置 | 5 | 13 | 5 | 1 | 24 |
| 职业设计草案_剑修 | 12 | 3 | 5 | 0 | 20 |
| 怪物和AI设计 / 怪物词缀设计 | 9 | 6 | 3 | 2 | 20 |
| 装备和存储 / 符文之语 / legendary_affixes | 11 | 4 | 4 | 0 | 19 |
| 地图生物群落与城镇皮肤 / 地图和敌人刷新机制 | 9 | 3 | 4 | 2 | 18 |
| 星系天赋系统 / 局外成长与终局玩法 | 9 | 2 | 2 | 1 | 14 |
| 游戏流程与状态管理 / 存档与持久化系统 | 5 | 9 | 5 | 2 | 21 |
| GPU 渲染（V2/V3/V4/V5/QuickRef/PBR） | 3 | 13 | 1 | 1 | 18 |
| VFX / UI | 8 | 2 | 3 | 1 | 14 |
| **合计** | **79** | **63** | **37** | **11** | **190** |

一句话结论：**文档整体滞后于 2026-03~09 的多轮重构**（damage-pipeline-modernization、skill-abstraction、item-storage、ui-system-rearchitecture、gpu-pipeline-correction、string-enum-remediation）。多数是「文档过时/未覆盖」（改文档即可）；但仍有 79 条「实现偏差」与 11 条「文档缺失」，其中若干是真实功能缺口或疑似缺陷，需优先决策。

## 3. 需优先决策（实现缺失 / 疑似缺陷）

### 3.1 真实功能缺口（文档承诺、代码缺失）
- **UMR 门禁全链路未通电**：`build.bat:268-287` precheck 不含 modifier 校验；`scripts/gen_map_monster_modifier_v2.py:10-14` 三个源路径已失效（`src/game/data/*`、`src/game/components/Stats.hpp` → 实际在 `foundation/` 下），一旦执行 `--check` 即失败；`scripts/check_monster_behavior_dispatch.py` 无任何调用点。→ 文档承诺的漂移门禁形同虚设。
- **UMR map/monster 产物为死数据**：`MapModifierAdapter.cpp:75`、`MonsterModifierAdapter.cpp:200` 绕过 `ModifierRuntimeRegistry` 自建内存 record；编入 `modifier_runtime_v2.bin` 的 map/monster 记录运行时无人按 id 读取。`MANA_COST_MULT` 在 `ModifierEvaluator.cpp` 无 case（静默 no-op）。
- **剑修核心被动空壳**：`SwordHeartComponent`（`Progression.hpp:85`）仅被写入、无读取方 → 剑心通明三条效果全缺；`AttributePipeline.cpp:657-679` 无 `SwordIntent` 分支 → 基础剑意无攻速/移速收益。
- **技能专精 UMR 化仅 2/382**：`skill_spec_modifiers.json` 1 条、`talent_modifiers.json` 1 条；数值主载体实为 `assets/data/skill_mechanics.json` + generated SpecState，文档 §8「已并轨 UMR」名不副实。且 `talent_modifiers.json` 的 `node_id_whitelist:[1100]` 指向不存在的节点（实际 id 区间 1000-1044）。
- **终局/Nemesis 未接入运行时**：`NightmareFloorState` 无任何 `PushState` 调用 → 层数循环不可达；`LeaderboardSystem::load()` 无调用者（只写不读）；`FactionAggroSystem::Init/Update`、`NemesisGenerator::SpawnNemesisIfReady` 无生产调用者；`EndgameModifierRuntimeComponent` 仅测试中 emplace → 终局合同 5 条永不生效。
- **装备传说词缀从未实例化**：1001-1120 区间被 `IsRandomRollableAffix`（仅允许 0-999）排除；`legendary_affixes.json` 仅用于 UI 名称查找，数值全为占位 `1.0`；`allowedTags` 中 `head/chest/belt/amulet/ring/offhand` 解析后掩码为 0（永不匹配）。
- **物品存储轨丢失 UMR record ids**：`ItemStorageTypes.hpp:93-102` 的 `CompactAffix` 无 `modifier_record_ids`，但 `EquipmentModifierAdapter.cpp:16-21` 依赖它 → 经存储轨/读档的物品装备词缀静默失效（`ItemStorageService.hpp:21-25` 自认双轨待统一）。
- **地图词缀源不统一**：`MapAffixRegistry.hpp:38-99` 硬编码 31 个词缀，`MapAffixRegistry.cpp:9-11` 明示不读 JSON；仅 3 个有 `map_modifiers.json` 挂钩。

### 3.2 疑似数值/逻辑缺陷（实现与设计意图不符）
- 剑修：剑雨回响效力 `0.1` vs 文档 `25%`（`skill_contracts_compact.json` 节点 1111）；血海鲜血回灌 CD `3.5s` vs `4s`（节点 1211）；满层释放增幅各行为 1.15–1.5 倍不统一（文档 100% More）；御剑步触发源/数值与文档不符。
- 怪物：`Fast` 攻速 `+30%` vs 文档 `+50%`；`Vortex` 周期 `8s`/半径 `300` vs 文档 `5s`/`400`；`PhaseShield` 无敌 `3s` vs `2s`；`VoidZone` 走 `DamageType::Shadow` 而非真实伤害；`Berserker` 无免控；`Molten` 无死亡延迟爆炸、`Toxic` 无攻击叠毒。
- 天赋：`NemesisGenerator.cpp:81` 与 `:288` 两次 `GenerateNemesisId()`，实体 id 与存档记录 id 不一致（一致性缺陷）。
- 战斗：事件载荷字段名 `tags`（文档 `damage_tags`），且 `Calculate` 直调路径 `final_applied_damage` 为计算值、`Execute` 路径不含屏障吸收，两路径语义不一致。
- 渲染：PBR roughness 在 `MaterialManager.cpp:974,983-984` 上传时与 `assets/shaders/entity_mdi.frag:115` 各加一次 `roughnessBias`（疑似双重偏置）；`assets/shaders/lighting/volumetric_light.frag:19` 手写 GLSL struct `GPULight`，违反 ABI 生成规则，`abi_manifest.json` 缺 9 个结构体。
- 物品：符文语不校验底材稀有度（`RunewordSystem.cpp:153-210` 从不检查 `item.rarity`）；单符文镶嵌基础属性未应用；`stringToAffixType`（`RunewordSystem.cpp:213-263`）覆盖不足导致大量词缀被 `LOG_WARN` 丢弃；无 `baseName` 导致拆除符文语后名称不可逆。
- 存档：`HeirloomVault` 归属角色档（`SaveManager.cpp:404`），与文档「全局档、跨局继承」相反。

### 3.3 文档结构性问题（建议直接改文档）
- 战斗：`CombatV2运行开关与收口验证说明_2026-03-04.md` 整篇描述已被删除的 `CandidateOnly`/双跑 parity；伤害公式 `FinalDamage = ((Base + Added*AddedEff) * ...)` 与实现分段式不符；权威来源指向已停用的 `conductor/`（AGENTS.md 明示归档不再使用）。
- 职业/技能：`职业被动和技能设置.md`（2026-02-25）整体过时——三职业→六扇区、示例技能全部不存在、星盘三扇区→六扇区/Tier、触发深度 1→2、节点数 15-20→25-29；`职业设计草案_剑修.md` §4 星盘表与 `profession_talents.json` 整体不符。
- 存档：文档描述 JSON（`slot_N.json`/`global.json`）与 `SerializedItem` DTO，实现已改为二进制单轨 `.nmd`（Magic/Version/CRC32，版本不等即拒载 + `.bak` 回退，无加密）。
- 渲染：`RENDERGRAPH_CONTRACT_VERSION` 文档 3 / 实现 4；`GPUVisualStats` 16B→64B；`GPUMaterialDataV3` 64B→128B；binding 8 拆分并新增 binding 16；`GPULightV2`→`GPULight`；`GPU_Rendering_Quick_Reference.md` 的 pass 顺序（缺 Fluid、误含 GPULoot、Volumetric 位置错）与上限常量（200k/1024 vs 100k/10000）均过时；`pbr_material_guidelines_v4.md` 仍写 ABI=4。
- VFX：`Asset_Regeneration_List.md` 的 29 个资产名全部未采用（实际为目录+数字占位）；V3 元素贴图已生成但运行时改用 GLSL 调色板；`vfx_dissolve.frag`、`vfx_afterimage.frag` 不存在。

## 4. 各簇偏差明细

### 4.1 战斗核心（13）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | CombatV2开关 §1 行7-8 | `DamagePipeline.cpp:1555-1621`、`DamagePipelineTypes.hpp:35-54` | `CandidateOnly`/候选路径不存在 | 文档过时 | 改文档 |
| D2 | CombatV2开关 §3 行24-27 | `docs/reports/damage-pipeline-modernization/phase-P4/P4-post-cleanup-report.md:26` | 候选 stub 已清理 | 文档过时 | 改文档 |
| D3 | CombatV2开关 §4.2 行33-34 | `tests/integration/CombatDamagePipelineCutoverTests.cpp:57,72,88,108,122,153,180` | 用例名 `CombatV2Cutover*` 不存在 | 文档过时 | 改文档 |
| D4 | 战斗系统 §4 行49 | `DamagePipeline.cpp:786-790,1047-1050,1362-1365` | 公式分段与暴击乘区位置不同 | 文档过时 | 改文档 |
| D5 | 战斗系统 §4.2 行55 | `DamagePipeline.cpp:791-801`、`DamagePipelineTypes.hpp:42` | `trigger_effectiveness` 来源字段不符 | 文档过时 | 改文档 |
| D6 | 战斗系统 §3.4 行42 | `foundation/components/Stats.hpp:64-198` | 机制层字段不在面板 | 文档过时 | 改文档 |
| D7 | 战斗系统 §7 行90-97 | `contracts/CombatEvents.hpp:91,97-98`、`DamagePipeline.cpp:1546-1549,1614-1618,1582-1584` | 字段 `tags`；两路径 final 语义不一致 | 实现偏差 | 待判断 |
| D8 | 战斗系统 §9.2 行118-119 | `contracts/impl/ProcBudgetManager.cpp:228-250,183-226` | 超预算硬丢弃，非降采样 | 实现偏差 | 改文档 |
| D9 | 战斗系统 §8.4 行104 | `systems/skill/SummonCombatBridge.cpp:94-109`、`SummonLifecycleSystem.cpp:36-39`、`SkillDefs.hpp:1075-1077,1095` | 召唤走本地 token bucket，未接 ProcBudget | 实现偏差 | 待判断 |
| D10 | 战斗系统 §5.1 行61-63 | 14 处投递方自加 `Tag::Hit` | 无集中强制校验 | 文档未覆盖 | 改文档 |
| D11 | 战斗系统 §2 行30-33 | `DamagePipeline.cpp:1540-1550` | `Calculate` 可先发事件，非唯一四步序 | 文档未覆盖 | 改文档 |
| D12 | 战斗系统 §6 行83-85 | `DamageMitigationService.cpp:97-338`、`CombatSystem.cpp:489-515,547-615` | 防御实现分散两文件 | 文档未覆盖 | 改文档 |
| D13 | 战斗系统 行5-11 | `AGENTS.md`（conductor 已归档） | 权威来源失效引用 | 文档过时 | 改文档 |

一致：防御常量 `CombatConstants.hpp:36-38`；DoT 闭环 `AilmentEngine.cpp:861-871`；Ailment v1 合同；召唤三元归因 `SkillDefs.hpp:1100-1104`；ProcBudget 默认值 `ProcBudgetManager.hpp:22-26`；合同门禁 `scripts/gen_skill_contracts.py`。

### 4.2 UMR（9）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | §3.2 行67 / §5.1 行114 | `build.bat:268-287` | precheck 无 modifier 校验 | 实现偏差 | 改实现 |
| D2 | §3.2 行56 | `scripts/gen_map_monster_modifier_v2.py:10-14,115-119,833-835` | 三个源路径失效 | 实现偏差 | 改实现 |
| D3 | §4 行92-98 | `ModifierContext.hpp:10-40` | 实际 29 个 OpCode（含 24 行为/事件） | 文档过时 | 改文档 |
| D4 | §4 行98 | `ModifierEvaluator.cpp`（两 switch 无 case）；数据 `equipment_modifiers.json:32` | `MANA_COST_MULT` 静默 no-op | 实现偏差 | 待判断 |
| D5 | §3.3 行82-88 / §8.1 行149 | `MapModifierAdapter.cpp:75`、`MonsterModifierAdapter.cpp:200` | 绕过 registry，产物死数据 | 实现偏差 | 改实现/改文档 |
| D6 | §3.1 行33-49 | `ModifierSchemaV2Validation.cpp:81-86,113-114` | `constraints` 仅校验不生效 | 文档未覆盖 | 改文档 |
| D7 | §3.2 行53-75 | `gen_modifier_runtime_v2.py` main | `--check` 会覆盖产物，不做漂移比对 | 文档缺失 | 改文档+实现 |
| D8 | §8.2 行150 | `scripts/check_monster_behavior_dispatch.py:29-70` | 分发覆盖门禁未接线 | 实现偏差 | 改实现 |
| D9 | §3.1 行33-49 | 数据/产物含 `debug{name,source}` | 第 7 字段未记录 | 文档未覆盖 | 改文档 |

一致：5 个适配器类名；数据层 6 文件架构；产物/脚本名；`(priority,id)` 稳定排序与四表；装备词缀 id 链路 `ItemStats.hpp:305,192 → ItemFactory.cpp:356,467,605 → EquipmentModifierAdapter.cpp:19`；`MatchesFilters`（`ModifierEvaluator.cpp:51,88`）。

### 4.3 职业被动和技能设置（24）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | §3 行51-80 | `TalentData.hpp:18-26`、`profession_talents.json` | 三职业→六扇区（仅剑修实装） | 文档过时 | 改文档 |
| D2 | §3.1-3.3 行58/67/76 | `CombatEventDispatcher.cpp:100-124`、`AttributePipeline.cpp:73,95,704` | 职业专属回蓝未实现 | 文档过时 | 改文档 |
| D3 | §3.1-3.3 行59-80 | `assets/data/skills.json` | 示例技能（旋风斩等）全部不存在 | 文档过时 | 改文档 |
| D4 | §5.2 行156 | `ProgressionSystem.cpp:53-78`、`SkillDefs.hpp:496,509` | 无技能独立等级/使用升级 | 实现偏差 | 待判断 |
| D5 | §5.2 行156 | `assets/data/skills.json`、`mastery_skill_trees.json:8-15,863-870,1617-1624` | 节点数 25-29（文档 15-20） | 文档过时 | 改文档 |
| D6 | §4.1 行86-140 | `TalentData.hpp:14,28-33,36-40`、`AstrolabeConstants.hpp:46` | 星盘六扇区/Tier 制 | 文档过时 | 改文档 |
| D7 | §4.1.1-4.1.3 行93-140 | `profession_talents.json` | 节点名整体替换 | 文档过时 | 改文档 |
| D8 | §4.1.3 行137-140 | `TalentData.hpp:28-33` | Keystone 活动系统缺失 | 实现偏差 | 待判断 |
| D9 | §4.2 行142-145 | 全库无 `starbridge` | 星桥机制不存在 | 文档过时 | 改文档 |
| D10 | §2.1 行31-40 | `TagRegistry.hpp:12-56` | 缺 Shadow、Ice→Cold、Nature→Poison | 文档过时 | 改文档 |
| D11 | §2.2 行42-49 | `TagRegistry.hpp:12-56` | Chain/Curse/Minion/Invulnerable/BloodMagic 已删 | 文档过时 | 改文档 |
| D12 | §5.3 行176-178 | `SkillSystem.cpp:78`、`SkillSystem.hpp:51-55` | 触发深度上限 2（文档 1） | 实现偏差 | 待判断 |
| D13 | §5.2 行161-163 | `assets/data/skills.json` | 无 Physical→Void 转质节点 | 文档过时 | 改文档 |
| D14 | §8 行254-264 | `skill_spec_modifiers.json`(1)、`talent_modifiers.json`(1) | UMR 仅 2/382，主载体为 `skill_mechanics.json` | 实现偏差 | 待判断 |
| D15 | §1.2 行15-19 | `SkillDefs.hpp:29-33`、`BladeResourceService.hpp`、`blade_masteries.json` | 剑意+3专精资源未记录 | 文档未覆盖 | 改文档 |
| D16 | §1.2 行19 | `blade_masteries.json`、`BladeMasteryService.cpp:40-45,85-96,127-195` | Mastery 系统未记录 | 文档未覆盖 | 改文档 |
| D17 | §5.2/§6.1 | `skills.json:6368/6394/6419`、`mastery_skill_trees.json` | 技能10/11/12 双数据源未记录 | 文档未覆盖 | 改文档 |
| D18 | 全文 | `ElementPathSystem.hpp:1-22` | 技能8 元素路径未记录 | 文档未覆盖 | 改文档 |
| D19 | §5.2-§6 | `behaviors/*.cpp`、`behaviors/generated/*SpecState.gen.hpp`、`SpecStateTable.hpp:53-55`、`scripts/gen_skill_contracts.py` | SpecState 架构未记录 | 文档未覆盖 | 改文档 |
| D20 | §6.2 行209-223 | `contracts/CombatFormula.hpp`（仅防御） | 伤害基式落点不存在 | 文档过时 | 改文档 |
| D21 | §6.1 行204 | `TagRegistry.hpp`、`ailment_contracts.json` | Ailment/Summon 非标签 | 文档过时 | 改文档 |
| D22 | §7 行240-252 | 2026-03~09 进展 | 路线图状态滞后 | 文档过时 | 改文档 |
| D23 | §8 行260 | `SkillSpecModifierAdapter.cpp`、`SkillSpecializationBaker.cpp`(248处)、`BeamChannelDeliverySystem.cpp:180,212,230,347,459` | 硬编码残留 | 实现偏差 | 改实现 |
| D24 | §5.2 行156 | `SkillDefs.hpp:27`、`ProgressionSystem.cpp:53-78`、`SkillDefs.hpp:509` | 技能点上限数值缺规格 | 文档缺失 | 改文档 |

一致：5 槽位 `SkillDefs.hpp:26,704-715`；影子施法 `SkillSystem.cpp:1508,1603`；trigger.effectiveness；ProcBudget 四类；标签转换 `SkillDefs.hpp:59-119`；契约门禁脚本。

### 4.4 职业设计草案_剑修（20）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | §2.1 行15-20 | `AttributePipeline.cpp:681-689`、`Progression.hpp:85` | 剑心通明空壳，无读取方 | 实现偏差 | 改实现 |
| D2 | §2.2 行24 | `AttributePipeline.cpp:657-679` | 剑意每层攻速/移速缺失 | 实现偏差 | 改实现 |
| D3 | §2.2 行27 | `InfiniteBlades.cpp:256-258`、`BladeFormation.cpp:164,425-429` | 剑压非全局 | 实现偏差 | 待判断 |
| D4 | §2.2 行29 | 全库无 `mana_spent/intent.*mana` | 灵力共鸣缺失 | 实现偏差 | 改实现 |
| D5 | §2.2 行23 | `SkillSystem.cpp:620-718` | 剑意按施放而非命中 | 实现偏差 | 改文档 |
| D6 | §2.2 行28 | `SkillSystem.cpp:585-617`、`FlowingThrust.cpp:162`、`SkillSystem.cpp:1865,2145` | 满层增幅无统一 ×2 | 实现偏差 | 待判断 |
| D7 | §2.3 行33-35 | `MovementStanceSystem.cpp:23-46`、`PlayerState.hpp:75` | 御剑飞行无移速/免陷阱 | 实现偏差 | 改实现 |
| D8 | §2.3 行37-40 | `SevenStarSlashShared.hpp:139-154`、`FlowingThrust.cpp:101-104`、`BuffIds.hpp:22,67` | 御剑步归属/数值不符 | 实现偏差 | 待判断 |
| D9 | §5.2 行1072 | `skill_contracts_compact.json` skill11 节点1111 | 效力 0.1 vs 25% | 实现偏差 | 改实现 |
| D10 | §5.3 行1243 | `skill_contracts_compact.json` skill12 节点1211 | CD 3.5 vs 4s | 实现偏差 | 改实现 |
| D11 | §5.3 行1104 | `BloodSea.cpp:334-353,442-541`、`AttributePipeline.cpp:71` | 魔剑每损失生命吸血缺失 | 实现偏差 | 改实现 |
| D12 | §5.2 行937 | `HeavenlySwordDescent.cpp:94-95` | 智力→元素强度/穿透缺失 | 实现偏差 | 改实现 |
| D13 | §4 行620-660 | `profession_talents.json`(45 节点) | 星盘表整体不符 | 文档过时 | 改文档 |
| D14 | §3.0 行50 | `skills.json`/`skill_contracts_compact.json` | 节点 28-29（文档 24-26） | 文档过时 | 改文档 |
| D15 | §5.4.1 行1270-1274 | `mastery_skill_trees.json`、`skill_contracts_compact.json` | 七星斩 26 vs 25 | 文档过时 | 改文档 |
| D16 | §5.1 行728 | `BladeResourceService.cpp:145-170,297-308` | 剑流重启窗口+2层未记录 | 文档未覆盖 | 改文档 |
| D17 | §5.2 行982 | `HeavenlySwordDescent.cpp:548-551` | 灵剑阶上限可至 7 | 文档未覆盖 | 改文档 |
| D18 | §6 | `skill_spec_modifiers.json`(1条) | 节点效果未落 UMR | 文档未覆盖 | 待判断 |
| D19 | §6 | `SkillDefs.hpp:295-306` | `SkillBehaviorId` 仅登记 2 项 | 文档未覆盖 | 待判断 |
| D20 | §5.3 行1102-1103 | `BladeResourceService.cpp:216-234`、`SkillSystem.cpp:2068-2070` | 血誓额外 +1 嗜血未记录 | 文档未覆盖 | 改文档 |

一致：三专精资源枚举 `BladeMasteryData.hpp`；剑流 3%暴击/2%攻速；嗜血 5%More/3%承伤；七星斩 6%More/7 斩/0.5s 无敌；天剑 50% 元素转化。

### 4.5 怪物和AI设计 / 怪物词缀设计（20）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | 词缀 §3.1-3.4 行39-78 | `MonsterAffixRegistry.hpp:20-59` | 词缀清单增删（缺 Multi-Shot/Homing/Regenerator） | 文档过时+未覆盖 | 改文档 |
| D2 | 词缀 §3.3 行63 | `MonsterAffixRegistry.hpp:185-195`、`monster_modifiers.json:31-36` | Fast 攻速 +30 vs +50 | 实现偏差 | 待判断 |
| D3 | 词缀 §3.2 行52 | `MonsterAffixRegistry.hpp:137-140`、`MonsterAffixSystem.hpp:841-876` | Vortex 8s/300 vs 5s/400 | 实现偏差 | 待判断 |
| D4 | 词缀 §4 行97 | 枚举/JSON 无 Summoner | 引用未实现词缀 | 文档缺失 | 改文档 |
| D5 | 词缀 §3.4 行78 | 无 Regenerator | 机制缺失 | 文档缺失 | 待判断 |
| D6 | 怪物AI §1 行14 | `EnemySpawnSystem.cpp:546-575` | 固定词缀池，种族不筛选 | 实现偏差 | 待判断 |
| D7 | 词缀 §2 行25-30 | `EnemyComponent.hpp:186-199`、`EnemySpawnSystem.cpp:546-551` | 稀有度命名/数量不符 | 文档过时 | 改文档 |
| D8 | 怪物AI §2 行20-36 | `EnemyComponent.hpp:33-68`、`FactionComponent.hpp:14-20` | 种族/标签体系不符 | 文档过时 | 改文档 |
| D9 | 怪物AI §3 行40-58 | `EnemyComponent.hpp:71-113`、`AISystem.cpp:382-394` | 5 类原型+状态机（非行为树） | 文档未覆盖 | 改文档 |
| D10 | 词缀 §5.2 行116-125 | `NemesisGenerator.cpp:142-154` | 反制规则不符，无 Tank Buster | 实现偏差 | 待判断 |
| D11 | 词缀 §5.2 行122 | `AdvancedAffixComponents.hpp:167-175`、`MonsterAffixSystem.hpp:777-781` | PhaseShield 3s vs 2s，寄生于 Shielding | 实现偏差 | 待判断 |
| D12 | 词缀 §5.1 行112-114 | `NemesisGenerator.cpp:198-268,336-350` | 无 Elemental Plating | 实现偏差 | 改文档 |
| D13 | 词缀 §3.3 行67 | `MonsterAffixSystem.hpp:475-513` | Berserker 无免控 | 实现偏差 | 改实现 |
| D14 | 词缀 §3.1 行41/44 | `MonsterAffixSystem.hpp:363-408,1047-1089`、`MonsterAffixRegistry.hpp:272-281` | Molten 无死亡爆炸、Toxic 无叠毒 | 实现偏差 | 待判断 |
| D15 | 词缀 §3.1 行45 | `MonsterAffixSystem.hpp:654-672` | VoidZone 走 Shadow 伤害 | 实现偏差 | 待判断 |
| D16 | 词缀 §6.1 行149-158 | `MonsterAffixRegistry.hpp:474-508` | `timer1/2`→`timers[4]` | 文档过时 | 改文档 |
| D17 | 怪物AI §6.1 行79 | 全库无 `AilmentState/DefenseState` | 组件不存在 | 文档缺失 | 改文档 |
| D18 | 词缀 §6.0 行139/142/143 | `build.bat:268-286` | precheck 未执行两脚本 | 实现偏差 | 改实现 |
| D19 | 词缀 §6.0 行139 | `scripts/gen_map_monster_modifier_v2.py:13-14` | 源路径失效 | 实现偏差 | 改实现 |
| D20 | 怪物AI §1 行16 / 词缀 §3.3-3.4 | `MonsterAffixSystem.hpp:56-75` | 词缀行为无 ProcBudget 门禁 | 实现偏差 | 待判断 |

一致：UMR 属性链；行为 Op 分发覆盖；生成链数据结构；动态难度 `1.08^floor`；更新分级；Boss 合同。

### 4.6 装备和存储 / 符文之语 / legendary_affixes（19）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | 装备 §1.2 行13-22 | `ItemComponent.hpp:79-89`、`ItemFactory.cpp:250-269` | 稀有度枚举/掉落集不符 | 实现偏差 | 待判断 |
| D2 | 符文 §2/§4.1 行19-77,130-143 | `assets/data/runes.json`(3001-3033)、`ItemFactory.cpp:1108-1149` | 符文 id/阶位 11/11/11 不符 | 实现偏差 | 改文档 |
| D3 | 符文 §4.3 行155-158 | `ItemFactory.cpp:1108-1149` | 堆叠 99 vs 100 | 实现偏差 | 改文档 |
| D4 | 符文 §4.3 行155-158 | `ItemComponent.hpp:18-27` | 无 `ItemType::Rune` | 文档过时 | 改文档 |
| D5 | 符文 §3.1-3.4/§4.1 | `runewords.json`(4条)、`RunewordSystem.cpp:88-132` | 配方集/数值大面积不符 | 文档过时+实现偏差 | 改文档 |
| D6 | 符文 §4.1 行130-143 | `RunewordSystem.hpp:40-47` | 字段名不符，无 sockets/isLegacy | 文档过时 | 改文档 |
| D7 | 符文 §4.2 行145-153 | `RunewordSystem.cpp:153-210` | 不校验底材稀有度 | 实现偏差 | 改实现 |
| D8 | 符文 §5 行162-181 | `CraftingSystem.cpp:238-281` | 单符文基础属性未应用 | 文档未覆盖 | 改实现 |
| D9 | 符文 §5 行162-181 | `RunewordSystem.cpp:213-263` | 映射覆盖不足，词缀被丢弃 | 文档未覆盖 | 改实现 |
| D10 | 符文 §5 行162-181 | `RunewordSystem.cpp:265-297,299-316` | 无 `baseName`，名称不可逆 | 文档未覆盖 | 改实现 |
| D11 | 装备 §1 行12-13 | `RunewordSystem.cpp:265-297` | 符文语恒为 Legendary | 实现偏差 | 改文档 |
| D12 | legendary 全文 | `ItemFactory.cpp:150-152`、`ItemStats.hpp:168-171`、`CraftingSystem.cpp:398-404` | 传说词缀从未实例化 | 实现偏差 | 改实现 |
| D13 | legendary 行17-80 | `ItemStats.hpp:246-274` | `allowedTags` 部位解析失效 | 实现偏差 | 改实现 |
| D14 | 装备 §3.2 行49-50 | `CraftingSystem.cpp:319-433,347-352` | fodder 条件不符（4 词缀 vs Exalted） | 文档过时+实现偏差 | 改文档 |
| D15 | 装备 §2 行33-34 | `ItemFactory.cpp:274-343`、`affixes.json` | 属性类被标后缀 | 实现偏差 | 待判断 |
| D16 | legendary 行7-9 | `legendary_affixes.json` | 数值全为 1.0 占位，机制勾子未落地 | 实现偏差 | 改实现 |
| D17 | 装备 §6 行86-90 | `ItemPersistenceCodec.hpp:20-27`、`ItemStorageTypes.hpp:108-157` | `SerializedItem` 不存在 | 文档过时 | 改文档 |
| D18 | 装备 §7 行94-102 | `ItemStorageTypes.hpp:93-102` | 存储轨丢 UMR record ids | 实现偏差 | 改实现 |
| D19 | 装备 §5 行75-80 | `LootFilter.hpp:71`、`DropSystem.cpp:273,451` | `GPULootPass` 不存在 | 文档过时 | 改文档 |

一致：背包 40+4×56、仓库 144×10、可打造上限 T5、稀有度决定前后缀数、LP 0-4。

### 4.7 地图生物群落 / 地图和敌人刷新机制（18）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | 生物群落 §2 行12-22 | `assets/data/biomes.json:5-146`、`BiomeTypes.hpp:53-60` | 城镇 7 vs 6；名称为英文 | 实现偏差 | 待判断 |
| D2 | 生物群落 §2 行12 | `LevelManager.hpp:34-35`、`MainMenuState.cpp:82-85,103-106` | 城镇 128×128 vs 60×60 | 实现偏差 | 待判断 |
| D3 | 刷新 §1/§4 | `SceneManager.cpp:200-208` | 战斗图 500×500 未记录 | 文档未覆盖 | 改文档 |
| D4 | 生物群落 §3.1-3.3 | `biomes.json:147`、`BiomeTypes.hpp` | 额外 `cave` 群落 | 文档未覆盖 | 改文档 |
| D5 | 生物群落 §3.1-3.3 | `biomes.json:174,194,214,…` | `wallProbability` 大面积不符 | 实现偏差 | 改文档 |
| D6 | 生物群落 §3.1 | `biomes.json:175` | `smoothIterations` 统一 3 | 文档过时 | 改文档 |
| D7 | 生物群落 §3.1-3.3 | `biomes.json` | `enemyPool` 约 12/21 不符 | 实现偏差 | 待判断 |
| D8 | 生物群落 §3.3 行54-65 | `biomes.json` | features 集合不符 | 实现偏差 | 待判断 |
| D9 | 生物群落 §3.3/§4 | `BiomeRegistry.cpp:55,60,129-130`、`BiomeRegistry.hpp:29-30` | low_gravity/friction 死配置 | 实现偏差 | 改实现/改文档 |
| D10 | 生物群落 §4.3 行80-88 | `biomes.json:451`、`BiomeRegistry.cpp:55` | `usesAirWall`→`features` | 文档过时 | 改文档 |
| D11 | 刷新 §1.2 行11-25 | `MapSystem.cpp:383-413`、`BiomeMapGenerator.cpp:43-56`、`MapGeneratorConstants.hpp:10` | 生成器架构/初始墙率 0.05 | 文档过时 | 改文档 |
| D12 | 刷新 §3.1 行72-78 | `EnemySpawnSystem.cpp:270-288,290-296` | 无 2-4 族随机子集 | 实现偏差 | 待判断 |
| D13 | 刷新 §3.1 行72-78 | `EnemySpawnSystem.cpp:160-181`、`EnemyConstants.hpp:15,43-44` | 密度/规模数值未记录 | 文档未覆盖 | 改文档 |
| D14 | 刷新 §3.1 行72-78 | `EnemySpawnSystem.cpp:392-405`、`MosaicMapGenerator.cpp:266-300` | Boss 纯随机关，无遭遇预算 | 文档缺失 | 待判断 |
| D15 | 刷新 §4 行92-101 | `LevelManager.cpp:189-217`、`PortalSystem.cpp:99-125` | 传送门流程含 Mosaic/裂痕；击杀门槛未拦截 | 实现偏差 | 改文档 |
| D16 | 刷新 §2.2 行61-68 | `MapComponent.hpp:13` | 地形枚举不符 | 实现偏差 | 改文档 |
| D17 | 两文档 | `map_modifiers.json`(3条)、`MapAffixRegistry.hpp:38-99`、`MapAffixRegistry.cpp:9-11` | 地图词缀源不统一 | 文档缺失 | 待判断 |
| D18 | 任务指定 | `src/game/contracts/` | 无地图/群落/刷新契约 | 文档未覆盖 | 改文档 |

一致：安全区不刷怪；空气墙；Dormant 分层；连通性；出口放置；异步加载。

### 4.8 星系天赋 / 局外成长与终局玩法（14）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | 局外 §5 行73-79 | `NightmareFloorState.cpp:40-43` | 层间推进不可达（无 PushState） | 实现偏差 | 改实现 |
| D2 | 局外 §5 行75 | `LeaderboardSystem.hpp:141-209`、`NightmareFloorState.cpp:59-60` | 排行榜只写不读 | 实现偏差 | 改实现 |
| D3 | 局外 §5 行74-75 | `LeaderboardSystem.hpp:20-35` | 无赛季维度 | 实现偏差 | 待判断 |
| D4 | 局外 §3.1 行43-45 | `FactionAggroSystem.cpp:12-20`、`NemesisGenerator.cpp:41-60` | Nemesis 触发未接入 | 实现偏差 | 改实现 |
| D5 | 局外 §3.2 行49 | `NemesisGenerator.cpp:198-269,336-350` | 反制缺免控/反伤 | 实现偏差 | 待判断 |
| D6 | 局外 §3 行43-56 | `NemesisGenerator.cpp:81,288` | NemesisID 双生成不一致 | 实现偏差 | 改实现 |
| D7 | §4.1 行62-63 | `Progression.hpp:19-55`、`SaveManager.cpp:62,85,143-144,266,656-660` | 账号级虚空星盘缺失 | 文档缺失 | 待判断 |
| D8 | §4.2 行67 | `HeirloomVault.hpp:30-35`、`ItemPersistenceCodec.cpp:516-517`、`SaveManager.cpp:404,656-660` | 传家宝归角色档 | 实现偏差 | 待判断 |
| D9 | 星系 §2/§4 行8-14,37-38 | `scripts/gen_profession_talents.py:99-168`、`profession_talents.json` | 仅剑修一系有节点 | 实现偏差 | 改实现 |
| D10 | 技术路线 §8 行157-163 | `talent_modifiers.json:6-35` | 白名单节点 1100 不存在 | 文档过时 | 改实现 |
| D11 | 星系/技术路线 §8 | `AttributePipeline.cpp:597-615`、`StatsSystem.cpp:337-346`、`TalentModifierAdapter.cpp:14-58,86-115` | 天赋实际硬编码驱动 | 文档过时 | 改文档 |
| D12 | 星系 §3.2 行27-34 | `AstrolabeSystem.cpp:46-87`、`SkillSystem.cpp:2337-2403` | 星盘无重置/洗点 | 文档未覆盖 | 待判断 |
| D13 | 局外 §3.3 行55-56 | `EndgameModifierContract.hpp:15-21`、`.cpp:150,179` | 终局合同无生产者 | 实现偏差 | 改实现 |
| D14 | 星系 §3.1 行19 | `TalentData.hpp:149,291-318`、`AstrolabeSystem.cpp:10-44` | `prerequisites` 死字段 | 文档未覆盖 | 改实现/改文档 |

一致：六扇区布局 `AstrolabeConstants.hpp:27-43`；亲和阈值 TIER_2=10/TIER_3=25；誓约机制；无尽难度 `1.08^floor`；Anti-Meta。

### 4.9 游戏流程与状态管理 / 存档与持久化系统（21）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | 流程 §2.1 行16-20 | `src/app/Game.hpp:1`、`Game.cpp` | `Application`→`Game`，`Run`→`run` | 文档过时 | 改文档 |
| D2 | 流程 §4.1 行110 | `scene/State.hpp:23` | `OnRender` 返回 void | 实现偏差 | 改文档 |
| D3 | 流程 §2.1 行29-37 | `scene/State.hpp:26` | `IsTransparent()` 未记录 | 文档未覆盖 | 改文档 |
| D4 | 流程 §5 行124 | `scene/StateManager.hpp:11`、`scene/State.hpp:9` | 位置在 application/scene | 文档过时 | 改文档 |
| D5 | 流程 §2.1 行38-43 | `foundation/SharedContext.hpp:31-53,73` | 字段集大幅变化 | 文档过时 | 改文档 |
| D6 | 流程 §3.1 行62-64 | 无 `IntroState` | 开场状态缺失 | 文档缺失 | 待判断 |
| D7 | 流程 §3.4 行86-91 | `GameplayState.cpp:428-431` | 背包改为 UI 宿主 overlay | 文档过时 | 改文档 |
| D8 | 流程 §3.5 行93-95 | `LoadingState.cpp:22,54-80`、`MainMenuState.cpp:80,101` | LoadingState 无进度条，传送门走 SceneManager | 实现偏差 | 改文档 |
| D9 | 流程 §3.3 行82-84 | 全库无 `OnSuspend` 覆写 | 暂停不停 Taskflow | 实现偏差 | 待判断 |
| D10 | 流程 §3 行60-95 | `states/*.hpp` | 多出 5+ 状态 | 文档未覆盖 | 改文档 |
| D11 | 存档 §1.2/§2.1/§4.2/§6 | `SaveManager.cpp:530-540`、`ItemPersistenceCodec.hpp:27,120-121` | 二进制 `.nmd` 单轨 | 文档过时 | 改文档 |
| D12 | 存档 §2.2 行35-38 | `SaveManager.cpp:638-661,404`、`Settings.hpp:70,94`、`LeaderboardSystem.hpp:64,141,172`、`AchievementSystem.hpp:76,197,231` | global 内容归属全面变化 | 文档过时 | 待判断 |
| D13 | 存档 §2.3 行40-62 | `foundation/data/SaveData.hpp:16,21-31,56-75` | 角色档 schema 不符（v1→4） | 文档过时 | 改文档 |
| D14 | 存档 §3.1/§4.2/§5 | `SerializedItem` 等已删除 | DTO 方案废止 | 文档过时 | 改文档 |
| D15 | 存档 §4.1 行129-141 | `persistence/SaveManager.hpp:37,42` | 快照函数转 public，非单例 | 实现偏差 | 待判断 |
| D16 | 存档 §1.1 行9 | `PauseState.cpp:28-31`、`GameplayState.cpp:434-436,441-444` | 手动保存为 F5 热键，非城镇菜单 | 实现偏差 | 待判断 |
| D17 | 存档 §1.2 行16 | `SaveManager.cpp:530-540,597-599` | `.bak` 回退未覆盖 | 文档未覆盖 | 改文档 |
| D18 | 存档 全文 | `SaveData.hpp:16`、`ItemPersistenceCodec.hpp:27` | 版本不等即拒载，无迁移 | 文档未覆盖 | 改文档 |
| D19 | 存档 全文 | `ItemPersistenceCodec.hpp:206-213` | 无加密+CRC32 未记录 | 文档未覆盖 | 改文档 |
| D20 | 存档 §1.1 行10-12 | `SaveData.hpp:56-75`、`PortalSystem.cpp:133-139` | 地牢无状态基本一致；缺裂缝退出点记录 | 文档过时 | 改文档 |
| D21 | 存档 §5 行176-185 | SaveManager/Portal/MainMenu 已落地 | To-Do 状态交错 | 文档缺失 | 改文档 |

一致：Pending Changes `StateManager.hpp:44,49-66`；Update 阻止底层；透明度栈渲染；Taskflow 归属 `GameplayState.hpp:61`；异步快照+原子写入；回城自动保存 `PortalSystem.cpp:132-148`。

### 4.10 GPU 渲染（18）
| # | 文档 | 实现 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | QuickRef:5 / 技术路线:53 | `graph/RenderGraph.hpp:17` | 契约版本 3 vs 4 | 文档过时 | 改文档 |
| D2 | V3:93 / V5:254 | `RenderSystem.cpp:1576-1585` | Shadow 在 LightCulling 前 | 文档过时 | 改文档 |
| D3 | QuickRef:178-182 | `RenderSystem.cpp:1622,1625,1627,1634,1641-1653,1655,1662,1666,1695` | Volumetric 位置/缺 Fluid/GPULoot 已停用 | 文档过时 | 改文档 |
| D4 | 无 | `RenderSystem.cpp:1611`、`GPUParticleSystem.cpp:1005` | `VFXEmissionSnapshotPass` 未记录 | 文档缺失 | 改文档 |
| D5 | QuickRef:162,180,466-469 / V4 §4 | `RenderSystem.cpp:1641-1653` | GPULoot 停用 | 文档过时 | 改文档 |
| D6 | QuickRef:264,306 | `GPUData.hpp:962` | `GPUVisualStats` 16B→64B | 文档过时 | 改文档 |
| D7 | QuickRef:273,307,485 | `GPUData.hpp` `GPUMaterialDataV3` | 64B→128B | 文档过时 | 改文档 |
| D8 | QuickRef:269,278,524,599 | `RenderConstants.hpp:32,39,73-91` | binding8 拆分+新增 16 | 文档过时 | 改文档 |
| D9 | V5:680-689 | `GPUData.hpp` `GPUFluidParticle` | 字段顺序过时 | 文档过时 | 改文档 |
| D10 | V4:826-840,598 / V5:489 | `GPUData.hpp:237-254` | `GPULightV2`→`GPULight`，字段命名漂移 | 文档过时 | 改文档 |
| D11 | V4:377 | `MaterialManager.cpp:974,983-984`、`entity_mdi.frag:115` | roughness bias 双重应用 | 实现偏差 | 待判断 |
| D12 | QuickRef:375 | `QualityTierManagerInternal.hpp:285-310`、`QualityTierManager_Presets.cpp` | Medium 阴影关/GPU文字开 | 文档过时 | 改文档 |
| D13 | V5:461-467 | `QualityTierManagerInternal.hpp:48-52` | GI/Fluid 细分配置键未落地 | 文档过时 | 待判断 |
| D14 | QuickRef:583-584 | `RenderConstants.hpp:295-296` | 上限 200k/1024 vs 100k/10000 | 文档过时 | 改文档 |
| D15 | V4:608,610 | `volumetric_light.frag:19`、`abi_manifest.json` | 手写 GPULight，manifest 缺 9 结构体 | 实现偏差 | 改实现 |
| D16 | pbr:47 | `GPUData.hpp:12` | 指南写 ABI=4 | 文档过时 | 改文档 |
| D17 | V5:418,553 | `QualityTierManager_Presets.cpp:377-380` | 发布构建强制关 Fluid | 实现偏差 | 改文档 |
| D18 | V4:591,610 | `abi_manifest.json`、`generate_gpu_abi.py:16-20` | manifest 无版本字段、命名不一致 | 文档未覆盖 | 待判断 |

一致：ABI=5；全局 SSBO 0-15 与 `SSBO_LOOT_INSTANCE=15`；Tier 枚举与 6 步降级；`RadianceCascadeConfig` 32B；Volumetric 在 VFX 前；Low/Medium 阴影关。

### 4.11 VFX / UI（14）
| # | 文档 | 实现/资产 | 差异 | 类型 | 倾向 |
| --- | --- | --- | --- | --- | --- |
| D1 | VFX_Design §3.2 / v3 §11.2 | 无 `vfx_dissolve.frag` | 溶解 shader 未实现 | 实现偏差 | 改文档/补实现 |
| D2 | v3 §11.2 行1204-1213 | `assets/shaders/vfx/beam_instanced.*`、`aoe_array.frag` | `vfx_resist_overlay`/`vfx_afterimage` 缺失、beam/array 改名 | 实现偏差 | 改文档 |
| D3 | VFX_Design §3.2 / v2 §6 | `trail/trail.*`、`vfx/distortion.fs`、`particle.compute`、`vfx/aura.fs` | 命名体系不符；`aura.fs` 疑似死资产 | 实现偏差 | 改文档 |
| D4 | v3 §11.1/§3 行1184-1198,138-183 | `assets/textures/vfx/*`、`vfx_element_switch.glslinc:4-14` | 元素贴图无引用，改用调色板 | 实现偏差 | 改文档 |
| D5 | Portal §3.1 行15-32 | `assets/shaders/vfx/portal_vortex.fs:37,49,56`、`PortalSystem.cpp:429-432` | 漩涡相位/遮罩/亮度算法不符 | 实现偏差 | 改文档 |
| D6 | Portal §3.2 行34-38 | `PortalSystem.cpp:402-418` | 颜色映射不符 | 实现偏差 | 待判断 |
| D7 | v2 §3.1 / v3 §6.1 | `src/engine/render/SkillVfxEvent.hpp:64-76` | 缺 `effectiveTags` | 实现偏差 | 改文档 |
| D8 | v3 §2.3 行121 | `systems/vfx/GhostSystem.cpp:18-30` | 残影为精灵幽灵，非后像 | 实现偏差 | 改文档 |
| D9 | Asset_Regeneration_List 全文 | `assets/generated/*`（数字占位） | 29 资产名全部未采用 | 文档过时 | 改文档 |
| D10 | v3 §2.1 行75 | `SwordIntentVisualSystem.cpp:250`、`SwordIntentWidget.cpp:122-142` | 剑意已完整实现，被文档排除 | 文档过时 | 改文档 |
| D11 | v2/v3 §6-§8 | `GPUSkillEffectSystem.hpp:48-97`、`.cpp:36-47,393-394` | recipe/ABI 实现未记录 | 文档未覆盖 | 改文档 |
| D12 | v2 §8 行225-228 / v3 §10.1 | `src/engine/vfx/VFXBudgetEstimator.cpp:20-110` | ms 预算无运行时校验 | 文档未覆盖 | 改文档/补实现 |
| D13 | 特效和UI 目录 | `docs/reports/ui-system-rearchitecture/` | UI 架构重构未反映 | 文档未覆盖 | 改文档 |
| D14 | 特效和UI 目录 | `VFXTypes.hpp:24-36,248-263`、`VFXSequenceManager.hpp`、`assets/vfx/*.json` | VFX 序列数据模型无专文 | 文档缺失 | 改文档 |

一致：事件契约 8 事件/5 元素/5 抗性；每技能并发上限 `GPUSkillEffectSystem.cpp:36-47`；RenderGraph 资源契约；元素经 flags 低 4 位；Portal 已用 `portal_vortex` 着色器。

## 5. 建议处理顺序

1. **先修实现缺陷（§3.1、§3.2）**：UMR 门禁接线与脚本路径、剑心通明/剑意、传说词缀实例化与 allowedTags、存储轨 record ids、Nemesis 触发与 ID、排行榜 load、终局合同生产者、NightmareFloorState 可达性。这些若不修，会让「已完成」的文档承诺持续误导。
2. **再批量更新过时文档（§3.3）**：存档二进制单轨、GPU 版本/尺寸/binding、职业技能六扇区、剑修星盘、VFX 资产命名——建议按簇一次性重写，而非逐条打补丁。
3. **补齐文档未覆盖项**：SpecState/skill_mechanics、Mastery、ElementPath、VFX 序列模型、UI 架构、地图词缀——需要新增章节。
4. **收敛文档缺失（11 条）逐项定性**：IntroState、赛季、虚空星盘、Boss 遭遇预算、Regenerator/Summoner 等，明确「补实现」或「从文档移除」。

## 6. 未证实项（需后续验证）

- 事件一致率 ≥99.9%、DoT 生效率、触发越界率等指标无遥测实现，阈值无文档数值。
- UMR `map_modifiers.json`/`monster_modifiers.json` 产物是否与当前脚本体一致（脚本源路径已失效，禁跑未验证）。
- `gen_map_monster_modifier_v2.py`/`check_monster_behavior_dispatch.py` 实际运行行为（只读约束下未执行）。
- PBR roughness 双重 bias 的运行时视觉影响。
- 8 张 V3 元素/抗性贴图是否被 tools/ 或 UI 间接消费。
- `assets/data/astrolabe.json` 与 `profession_talents.json` 的关系与最终生效源。
- UI 重构后 `src/game/foundation/ui_shared/` 的实际职责边界。
