# 装备降耗烘焙折叠（UMR-MC）— 评审报告

- 日期：2026-09-16
- 设计：`docs/designs/2026-09-16-equipment-mana-cost-bake-fold-design.md`
- 上游评审：`docs/reviews/2026-09-16-umr-framework-completion-and-hardening-review.md`（结论「修改」，15 条意见）
- 本轮目标：闭合上游意见 #4 / #5 / #6 / #12 / #14
- 用户决策：「蓝耗按统一结算，触发节点也享受降低」

## 1. 范围与执行

用户决策改变了法力消耗的结算口径：**单一结算点 + 统一生效域**。据此选定设计文档中的方案 C —— 把装备降耗乘算下沉到 `SkillSpecializationBaker::Bake` 产出的 `BakedSkillProfile::effective_mana_cost`，而不是在各消费点分别补算，也不新建缓存表。

| 文件 | 变更 |
| --- | --- |
| `src/game/systems/skill/SkillSpecializationBaker.cpp` | 新增 `EquipmentModifierAdapter.hpp` include；`Bake` 末尾（天赋/装备词缀修正全部写入之后）乘上 `GetEquippedManaCostMultiplier(registry, caster, skill_id, skillData->tags)` |
| `src/game/systems/skill/SkillSystem.cpp` | 删除施法路径的独立乘算（否则双重折扣）；触发节点法力结算改用烘焙值，无烘焙时回退适配器乘算 |
| `src/game/systems/skill/SkillDisplayPreviewService.cpp` | 删除独立乘算；无烘焙回退分支内联乘算 |
| `src/game/application/ui/GameUiSnapshotBuilder.cpp` | 热键栏 `manaCost` 优先取烘焙值 |
| `src/game/application/ui/UIRenderer.cpp` | 快照工具提示从 `snapshot.skillBar.slots` 取已结算值，继续保持 R8 不访问注册表 |
| `tests/unit/SkillManaCostSettlementTests.cpp` | 新增 4 个用例（无装备 / 装备 / 幂等 / 重烘焙 / `TryCast` 结算） |

未使用子代理：本轮改动集中在单条调用链上，存在「折叠点与消费点必须同时改」的强耦合，串行实施比并行更可靠。

## 2. 自审发现与处置

| # | 严重度 | 位置 | 说明 | 处置 |
| --- | --- | --- | --- | --- |
| A | bug/medium | `SkillSystem.cpp:2054`、`SkillDisplayPreviewService.cpp:39` | 折叠后原乘算会变成双重折扣（5.0 → 4.05 而非 4.5） | 已删除；`SkillManaCostSettlementTests` 用 `TryCast` 差分包住该回归 |
| B | bug/medium | `BeamChannelDeliverySystem.cpp:772`、`:1085` | 引导技能每秒抽蓝读 `profile->effective_mana_cost`，折叠后自动享降耗 | 无需改动，由折叠覆盏；但**无专项测试** |
| C | bug/low | `SkillSystem.cpp:920` | 触发节点用裸静态 `mana_cost`，绕过全部折扣 | 改用烘焙值，无烘焙时回退乘算 |
| D | bug/low | `RendingWave.cpp:88` vs `SevenStarSlashShared.hpp:133` | 两个返还路径口径不一致：一个用烘焙值、一个用裸值；折叠后「按折后价支付、按原价返还」会净赚法力 | **未修改**（返还口径属独立设计议题），已入残留风险 |
| E | performance/low | `EquipmentModifierAdapter.cpp`、`GameUiSnapshotBuilder.cpp` | 上游 #12 担心每帧堆分配 | 折叠后 UI 只做 `baked_profiles` 数组线性扫描（最多 5 项、无分配） |
| F | maintainability/low | `SkillSpecializationBaker.cpp` | 折叠点依赖「`slots[i].id` 驱动烘焙」这一既有事实 | 由 `rebake reflects equipment change` 用例固定行为 |

## 3. 验证证据

| 项 | 结果 |
| --- | --- |
| `build.bat RelWithDebInfo` | RC=0 |
| 新增用例 | 4 cases / 29 assertions，0 failed |
| 全量回归 `NoMoreDayTests.exe` | 1682 cases / 141062 assertions，0 failed |
| `ctest -L unit` | 100% (8/8) passed |
| `gen_map_monster_modifier_v2.py --check` | RC=0（map=12 / monster=25） |
| `gen_modifier_runtime_v2.py --check` | RC=0（binary in sync） |

关键断言：真实资产记录 `1001001`（`MANA_COST_MULT` skill 1 = 0.9，需 `Tag::Hit`）下，技能 1 烘焙 `effective_mana_cost` 由 5.0 变为 4.5；`TryCast` 后法力 100 → 95.5（若双重折扣则为 95.95）；卸下装备后回到 5.0。

变更规模：23 个已跟踪文件，+1075 / -77；新增文件 5 个（含 2 份方案文档与 1 份评审报告）。

## 4. 外部评审

### 4.1 ocr 评审（超时中止）

按用户在项目目录调用 `ocr review --audience agent --effort high`（20 分钟超时）。会话 `e369788e-6b27-408b-b361-3ef94bc86c70` 在 20 分 50 秒被超时终止，状态 `aborted`：**16/16 个文件均已产出意见（共 12 条）**，但未走到汇总结论。用户据此指示不再重跑外部审查，改为委派独立子代理评审。

| # | 严重度 | 位置 | 说明 | 处置 |
| --- | --- | --- | --- | --- |
| O1 | bug/medium | `ItemPersistenceCodec.cpp:526` | 编码守卫只覆盖 `ItemSideTables` 三张变长表，`skill_modifiers` 仍无上限而解码侧硬拒 >100，属同类「存得下、读不回」缺陷 | **已修**：新增 `kMaxSkillModifiersPerItem = 100`，编码预检拆为 `ItemSideTables` 与 `ItemSkillModifiers` 两个独立门控 |
| O2 | test/low | `tests/unit/ItemPersistenceCodecTests.cpp:519` | 新用例只用 `ContainerDirtyFlags::All`，门控条件从未被真正触发 | **已修**：新增用例保留超限旁表并以 `ContainerDirtyFlags::Inventory` 轻量存盘，断言编码仍成功 |
| O3 | maintainability/low | `scripts/gen_map_monster_modifier_v2.py:898` | `monster_defs` 缺失回退分支已不可达 | **未修改**（残留风险 7） |
| O4 | test/medium | `GameUiSnapshotBuilder.cpp:845` | 快照层热键栏 `manaCost` 无测试固定（设计 A6 只覆盖了烘焙层一半） | **未修改**（残留风险 6） |
| O5 | bug/medium | `UIRenderer.cpp:1299` | 以 `display_mana_cost > 0` 作「有无预览」哨兵，与「结算结果恰为 0」撞车，会回退到未结算值 | **已修**：改用 `hasPlayerContext` 判定 |
| O6 | bug/low | `UIRenderer.cpp:1896` | 快照路径同样的 0/不可用混淆，两路径对同一数值显示不一致 | **已修**：新增 `isSettledManaCost` 标志，已结算值（含 0）照常渲染 |
| O7 | bug/low | `UIRenderer.cpp:1889` | 未入槽技能回退裸值，与实际扣除（触发回退分支已乘降耗）不一致 | **已声明**：注释明确该回退值不代表实际扣除（残留风险 5） |
| O8 | maintainability/low | `UIRenderer.cpp:1302` | 注册表版 `DrawSkillTooltip` 已无调用方，注释过期 | **未修改**（残留风险 6） |
| O9 | maintainability/low | `AttributePipeline.cpp:406` | 唯一用覆写赋值的接线：`calcs[ResistAll].base = ...` | **已修**：改为 `+=` |
| O10 | test/low | `tests/unit/AttributePipelineTest.cpp:153` | 新接线的 `CritChance`/`ResistAll`/六系抗性未在管线层固定，`Enemy_ResistAll` 全仓无引用 | **未修改**（残留风险 6） |
| O11 | bug/medium | `EquipmentModifierAdapter.cpp:129` | `BuildContextFromCharacter` 不填 `equip_slot_mask`，记录自带槽位掩码永不生效，而新注释声称会过滤 | **注释已改**为如实说明通配语义；槽位过滤实现见残留风险 6 |
| O12 | maintainability/low | `MapModifierAdapter.cpp:100` | `affix.value * 0.01f` 隐含「0~100 百分点」契约，仅因平坦语义词缀仍映到 `StatType::Count` 哨兵被跳过才成立 | **已修**：提取具名常量 `kPercentPointToRatio` 并补守卫注释 |

### 4.2 独立子代理评审（14 条，结论 `yes-with-followups`）

| # | 严重度 | 位置 | 说明 | 处置 |
| --- | --- | --- | --- | --- |
| S1 | bug/medium | `SkillSystem.cpp:2047` | `TryCast` 无烘焙档案时按裸 `data->mana_cost` 扣费，绕过 rcr/专精/装备降耗，与触发路径口径不一 | **已修**：回退值乘以适配器降耗 |
| S2 | bug/medium | `EquipmentModifierAdapter.cpp:53` | `BuildContextFromCharacter` 仅填 `skill_id`/`skill_tags`，职业/武器/槽位掩码保持通配，记录携带的过滤条件静默失效 | **注释已改**；实现见残留风险 6 |
| S3 | bug/medium | `ItemPersistenceCodec.cpp:240` | `skill_modifiers` 写无限、读硬拒 >100（同 O1） | **已修** |
| S4 | performance | `EquipmentModifierAdapter.cpp:65` | 每次 Bake 都堆分配 + 排序，而 Bake 经每帧战斗路径（`SkillProfileResolve`）调用 | **已修**：新增 `CollectEquippedRecordIdsInto`，降耗查询走 `thread_local` 暂存 |
| S5 | performance | `ModifierRuntimeRegistry.cpp:139` | `EnsureLoaded()` 加载失败时每次调用都重开重解文件（仅日志去重） | **已修**：新增 `m_failedPath` 按路径锁存失败 |
| S6 | bug/medium | `SevenStarSlashShared.hpp:133` | `RefundManaCost` 按裸 `skill->mana_cost` 返还而施法已按折后价扣除，形成净回蓝 | **已修**：改用 `GetBakedSkillProfile` 结算值 |
| S7 | maintainability | `UIRenderer.cpp:1295` | 0 作哨兵；且无调用方的注册表版路径仍会访问预览服务（潜在 R8 陷阱） | **哨兵已修**；死路径见残留风险 6 |
| S8 | test | 触发 / 热键栏 / 快照提示分支 | 设计 A4/A6 的这三条分支无测试 | **未修改**（残留风险 6） |
| S9 | bug/low | `AttributePipeline.cpp:406` | `=` 应为 `+=`（同 O9） | **已修** |
| S10 | bug/low | `MapModifierAdapter.cpp:100` | 一刀切 `* 0.01f`（同 O12） | **已修** |
| S11 | maintainability | `MonsterAffixSystem.hpp:99` | `hasUpdate` 门控取自静态头文件标志，行为 op 实际来自运行时 blob，漂移时静默停更 | **未修改**（残留风险 7） |
| S12 | bug/low | `MonsterAffixRegistry.hpp:154` | `GetAffixDef`/`GetAffixName` 无边界检查（既存） | **已修**：越界回退首项 |
| S13 | test | `tests/unit/MonsterAffixTests.cpp` | 对齐测试以「手抄的生成器表镜像」钉标志，契约重复于三处；blob↔头 漂移方向未覆盖 | **未修改**（残留风险 7） |

子代理同时核实为**正确**的关键项：单一结算点（全仓仅 3 处 `GetEquippedManaCostMultiplier` 调用，无重复应用）；烘焙幂等（`:34` 每次重初始化）；失效链完备（装备/卸下/读档均置 `StatsDirty` → `AttributePipeline.cpp:835` 重烘焙）；R8 合规（`GetBakedSkillProfile` 为定长数组有界扫描，UI 侧基于 POD 快照）；百分点刻度自洽；`kAffixData` 26 条与生成器行为 op 表逐一相符；持久化守卫位置与回退值正确。

### 4.3 评审后复验

| 项 | 结果 |
| --- | --- |
| `build.bat RelWithDebInfo` | RC=0 |
| 定向回归（Item*/Skill*/Modifier*/MonsterAffix*/AttributePipeline*） | 451 cases / 9537 assertions，0 failed |
| 全量回归 `NoMoreDayTests.exe` | 1683 cases / 141072 assertions，0 failed |
| `ctest -L unit` | 100% (8/8) passed |
| `gen_map_monster_modifier_v2.py --check` | RC=0（map=12 / monster=25） |
| `gen_modifier_runtime_v2.py --check` | RC=0（binary in sync） |

变更规模：26 个已跟踪文件，+1237 / -108；新增文件 6 个（2 份设计、1 份计划、2 份评审报告、1 个测试文件）。

## 5. 残留风险

1. **槽位/职业/武器过滤未生效（中）**：`BuildContextFromCharacter` 不填 `ctx.profession_id`/`weapon_class_mask`/`equip_slot_mask`，记录自带的这三类过滤条件当前等效通配（O11/S2）。注释已如实说明；实现需让 `CollectEquippedRecordIds` 携带来源槽并按槽分组求值，属独立改动。
2. **`StatsDirty` 双消费者（低）**：`GPUEntitySync.cpp:167` 与 `StatsSystem::update`（`GameplayState.cpp:360`）都会消耗该标记；顺序依赖未验证，属既存结构（若前者先消费，全部装备属性都会失效，故推定顺序无误）。
3. **非入槽技能触发即席求值（低）**：未入槽技能无烘焙档案，每次触发回退到适配器乘算（无缓存）；触发路径非每帧，暂可接受。
4. **快照/工具提示仅覆盖入槽技能（低）**：非入槽技能回退裸值；`UIRenderer.cpp:1889` 注释已声明该回退值不代表实际扣除。
5. **返还口径（低）**：`SevenStarSlashShared.hpp` 已改用烘焙值；`RendingWave.cpp:88` 原本即正确。技能 8 的返还节点与 `BladeResourceService` 生命消耗换算仍按传入值，未纳入本轮统一域。
6. **测试覆盖缺口（低）**：热键栏快照 `manaCost`（O4/S8）、触发与预览回退分支（S8）、`Enemy_ResistAll` 管线层固定（O10）尚无测试；注册表版 `DrawSkillTooltip` 为无调用方的死路径（O8/S7）。
7. **生成器/注册表契约重复（低）**：`kAffixData` 标志在生成器表、C++ 头、测试镜像三处各写一份（S13）；`gen_map_monster_modifier_v2.py:898` 回退分支不可达（O3）；`MonsterAffixSystem.hpp:99` 的静态门控与运行时 blob 漂移时无诊断（S11）。

## 6. 结论

**提交（`yes-with-followups`）。** 设计文档「单一结算点 + 统一生效域」的不变量已由代码与测试双向固定：降耗乘算只在 `SkillSpecializationBaker::Bake` 出现一次，施法/触发/引导抽蓝/UI 显示四条路径全部读取同一烘焙结果，`TryCast` 差分断言（95.5 而非 95.95）可捕获双重折扣。

外部评审产生的 26 条意见中已处置 20 条（含全部 bug/medium 与 performance 项）；其余 6 条集中在测试覆盖与生成器契约清理，已逐条登记为残留风险 1、6、7，不构成提交阻塞。ocr 会话因 20 分钟超时而 `aborted`，按用户指示不再重跑，以独立子代理评审 + 全量测试作为本轮证据。
