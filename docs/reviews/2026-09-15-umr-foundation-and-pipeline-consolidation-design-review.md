# UMR 基础夯实与工程闭环方案 审查报告

- **文档路径**：`docs/reviews/2026-09-15-umr-foundation-and-pipeline-consolidation-design-review.md`
- **审查日期**：2026-09-15
- **审查目标**：
  - `docs/designs/2026-09-15-umr-foundation-and-pipeline-consolidation-design.md`
  - `docs/plans/2026-09-15-umr-foundation-and-pipeline-consolidation-plan.md`
- **结论**：`修改`
- **审查轮次**：第 1 轮
- **输入**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `docs/reviews/2026-09-15-design-doc-implementation-divergence-audit.md`
  - 代码与脚本实况：`scripts/gen_map_monster_modifier_v2.py`、`scripts/gen_modifier_runtime_v2.py`、`src/game/systems/modifier/*`、`src/game/systems/item/storage/*`、`assets/data/modifier_v2/*`

## 1. 范围对齐

方案的五项目标（门禁通电、`MANA_COST_MULT` 闭环、适配器去旁路、存储轨字段对齐、接入范式）与上游 UMR 基线及 09-15 偏差审计方向一致，优先级划分（P0 工具链/算子，P1 适配器/回归）总体合理。

问题集中在**"适配器去旁路"（design §3.3 / plan Phase 3）**：该阶段与生成器契约、地图词缀动态数值、怪物词缀过滤语义三处存在硬冲突，若不修订就实施会造成门禁自锁与静默数值回归。存储轨目标文件在 design 与 plan 之间也不一致。

## 2. 发现项

### 2.1 Blocker — 地图适配器"去旁路"与生成器契约自相矛盾

- `scripts/gen_map_monster_modifier_v2.py:319-342` 的 `_parse_map_adapter_enemy_templates()` 用正则**从 `MapModifierAdapter.cpp` 的 `case MapAffixType::X:` 代码块中提取 `MapAffixType -> StatType` 映射**，匹配串要求出现 `AddPercentMultRecord( records, nodeId, StatType::Y, affix.value )`；无匹配即抛 `RuntimeError`。
- design §3.3.3 / plan Task 3.1 要求"完全移除内部手拼 `ModifierRecord` 的临时逻辑"。一旦移除，Phase 1 刚修通的 `gen_map_monster_modifier_v2.py --check` 将**永久失败**，Phase 1.3 接入的 `build.bat` 门禁自锁。
- 即 Phase 1 与 Phase 3 存在顺序性依赖冲突，plan 未识别。

**修订要求**：在移除适配器 switch 之前，必须先"解耦生成器与适配器"——把战斗属性映射移到 `MapAffixRegistry.hpp` 的 `MapAffixDefinition`（新增战斗 `StatType` 字段），生成器改为解析该注册表，适配器再改为消费该字段。plan 已据此新增 Phase 3 Task 3.0。

### 2.2 Blocker — 地图适配器直连 Registry 会丢失分层滚动数值与共鸣词缀

- `assets/data/modifier_v2/map_modifiers.json` 的 `param_f32` 是 `MapAffixRegistry` 的 **valT1**（`_build_map_records` 取 `map_val_t1`，见 :590-607），而现 `MapModifierAdapter.cpp:39-77` 运行时使用的是 `affix.value`（`MapAffix::value` 按 tier 滚动，注释见 `src/game/foundation/data/MapAffix.hpp:65`）。
- 直接改为 registry 求值，会把所有地图词缀**固定为 T1 强度**，高阶地图词缀强度整体塌陷。
- 且共鸣词缀走的是 node `799999`、倍率 `state.resonance.totalEnemyDensity * 0.05f`，JSON 中**不存在**该记录，改走 registry 后会被静默丢弃。

**修订要求**：`ModifierEvaluator` 增加 `param_f32` 覆盖（override）能力，使"记录形状来自 registry、数值来自运行时滚值"成立；并为共鸣词缀在数据层补齐记录。plan 已据此改写 Task 3.2。

### 2.3 Blocker — 怪物适配器直连 Registry 会改变三档评估语义并造成吸血双计

- `src/game/systems/modifier/MonsterModifierAdapter.cpp:154-202` 的三档语义：`EvaluateAffixDelta`=(stats,behavior)、`EvaluateAffixEvents`=(events)、`EvaluateBehaviorOps`=(behavior)，由 `includeStats/includeEvents/includeBehaviorOps` 在内部逐条过滤 opcode 决定，非三条独立数据。
- `Monster_Vampiric`（record 5001016）同时含 `ADD_STAT_FLAT(LifeSteal=param_u32 37)`、`MONSTER_EVENT_ON_HIT`、`MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT`；而适配器以 `IsVampiricLifeStealStat`（:148）在含行为时**排除**该属性，以免与行为系统重复计算。整条 record 求值会同时恢复受限 stats 并造成吸血双计。

**修订要求**：生成器需与运行时语义对齐（Vampiric 不产出 LifeSteal 属性 op），适配器再按"必要时按 opcode 类别过滤"消费整条 record；此改动必须在负例测试中覆盖。plan 已据此改写 Task 3.1。

### 2.4 High — 存储轨目标文件错误且与 POD 设计冲突

- design §3.4.1 指向 `src/game/foundation/components/ItemStorageTypes.hpp`——该路径**不存在**；真实文件为 `src/game/systems/item/storage/ItemStorageTypes.hpp`。
- `CompactAffix` 是 **8 字节 POD**（`static_assert(sizeof(CompactAffix) == 8)`），design 建议加入 `std::vector<uint32_t> modifier_record_ids` 会破坏布局与断言，并与"存储轨零堆分配"目标冲突。
- plan §1.1/§2.3 已正确选择 `ItemSideTableData` + `ItemPersistenceCodec.cpp`，但 design 未同步，二者矛盾。

**修订要求**：design §3.4 改为 `ItemSideTableData.modifier_record_ids`，落点 `ItemPersistenceCodec.cpp` Section 3 编解码，并明确 `ITEM_STORE_BINARY_VERSION`（`ItemPersistenceCodec.hpp:27`）的处理决定。设计已修订。

### 2.5 High — `MANA_COST_MULT` 缺消费端，plan 与 design DoD 不一致

- design §3.2.3 要求"接入技能系统调用点"，但 plan 无对应任务，DoD（plan §5）也未覆盖。
- 现状：仓库**没有**统一的技能魔耗结算点（`SkillSystem::CalculateManaCost` 不存在）；耗蓝仅散落于 `BeamChannelDeliverySystem`（`effective_mana_cost`）与个别行为的 `skill->mana_cost`。
- 在无权威结算点的情况下强行接线会引入"接在哪一处"的随意性。

**修订要求**：design §3.2.3 已明确本阶段只交付"求值 + 结果存储 + 单测"，技能消耗链接入列为后续独立任务并登记风险；plan 增列 scope-out 说明与后续待办。

### 2.6 Medium — `gen_modifier_runtime_v2.py` 默认模式语义未定义

- 现脚本 `:267-268` 使**无参数调用**也会进入写盘分支；plan Task 1.2 只规定 `--check` 只读，未定义默认模式应"编译并写盘"。若默认也变成只读，将失去生成入口。
- **修订要求**：plan 明确——默认（无参数）/`--build` 写盘；`--check` 纯内存比对、非 0 退出、绝不写盘。已在 Task 1.2 固化。

### 2.7 Blocker — 把编译产物 `--check` 当作构建硬门禁会让干净检出失败

- `assets/generated/modifier_runtime_v2.bin` 与 `modifier_runtime_v2.debug.json` 被 `.gitignore:73`（"Generated modifier runtime artifacts"）显式忽略，属**本地生成产物**，不随仓库分发。
- design §3.1.3 / plan Task 1.3 原方案把 `gen_modifier_runtime_v2.py --check` 作为硬失败门禁。这要求产物必须先存在且与 JSON 一致；任何干净检出（新克隆/CI 无缓存）都会因 `does not exist` 直接失败，且 `build.bat` 中没有任何步骤会生成它（已确认 `build.bat`/CMake 均未引用）。
- **修订要求**：`build.bat` 对该产物只做**确定性生成**（写盘模式），"按构造保证"与 JSON 对齐；硬门禁只保留在受版本管理的源上（`gen_map_monster_modifier_v2.py --check`、`check_monster_behavior_dispatch.py`）。`--check` 模式仍实现，供 CI / 跨机比对或产物纳入版本管理时使用。design §3.1.2/§3.1.3 与 plan Task 1.3/1.4/1.5、DoD 已按此修订。

### 2.8 Low — 星盘节点 ID 修订的验证要求

- `talent_modifiers.json` 的 `node_id_whitelist: [1100]` 确实非法（`profession_talents.json` 节点区间 1000~1044）。plan 改为 `1001` 合理，但需在实施时以 `profession_talents.json` 实际内容为准校验，避免再次硬编码臆测值。

## 3. 最佳实践建议

1. 门禁负例必须纳入 CI/本地可复现脚本（篡改 JSON / 篡改 `.bin` 各一次），否则门禁只是"看起来存在"。
2. 生成器对 C++ 源码的正则解析属于隐式契约，建议在生成脚本头部以注释显式声明其解析依赖（本次解耦后应收敛为只解析 `MapAffixRegistry.hpp`）。
3. 存储轨变长字段应评估是否需要递增 `ITEM_STORE_BINARY_VERSION`（当前注释仅约束 `ItemInstance` 布局变更）。
4. 消除死数据应优先保证"数据能被读到"，其次才是"零堆分配"；不要为性能目标牺牲数值语义。

## 4. 剩余风险

- 地图/怪物适配器改造后，`AttributePipeline` 的实际数值是否与改动前逐项等价，需要基线对比测试而非仅跑通用例。
- `MapAffixDefinition` 新增字段会改变 `gen_map_monster_modifier_v2.py` 对注册表条目的字段计数与顺序解析（:311 依赖 `fields[5]`），需同步更新解析索引。
- 技能魔耗接入延后期间，`equipment_modifiers.json` 中的 `MANA_COST_MULT` 仍是"已配置未生效"状态。

## 5. 下一步动作（第 1 轮）

1. 已按上述修订同步更新 design 与 plan（见两份文档的修订章节）。
2. 按修订后的 Phase 1 → Phase 2 → Phase 3 → Phase 4 顺序拆分并分派实施。
3. 实施后按 `docs/workflows/review.md` 出第 2 轮复审（含 `提交`/`修改` 结论）。

---

## 6. 第 2 轮复审（实施后）

- **审查轮次**：第 2 轮
- **结论**：`提交`
- **输入**：修订后的 design / plan；本批次实现改动；集中构建与全量测试结果

### 6.1 第 1 轮发现项的关闭情况

| 发现项 | 关闭方式 |
| :--- | :--- |
| §2.1 Blocker 生成器/适配器契约冲突 | 新增 Task 3.0：`MapAffixDefinition` 新增 `StatType combatStat`，生成器改解析 `MapAffixRegistry.hpp`，删除对 `MapModifierAdapter.cpp` 的正则依赖 |
| §2.2 Blocker 地图滚值丢失 | 新增 `ModifierParamOverride` 与 4 参 `Evaluate`，滚值经 override 注入；共鸣记录入数据（id `4_099_999` / node `799_999`） |
| §2.3 Blocker 怪物三档语义与吸血双计 | Adapter 分段返回保留三档语义；生成器对 Vampiric 排除 `StatType::LifeSteal`，数据与运行时对齐 |
| §2.4 High 存储轨落点错误 | 落点改为 `ItemSideTableData.modifier_record_ids` + `ItemPersistenceCodec.cpp` Section 3 |
| §2.5 High `MANA_COST_MULT` 无消费端 | 本阶段交付求值/存储/单测，技能消耗链接线登记为后续任务 |
| §2.6 Medium 默认模式语义 | 默认/`--build` 写盘，`--check` 纯内存只读 |
| §2.7 Blocker `.bin` 硬门禁自锁 | `build.bat` 对该 gitignore 产物改为生成步骤；硬门禁只针对受版本管理的源 |
| §2.8 Low 星盘节点 | 修正为 `1001` 并按 `profession_talents.json` 实体核验 |

第 2 轮实施中新发现并已修复：
- `ITEM_STORE_BINARY_VERSION` 递增为 `2`（Section 3 布局变化会让 v1 静默错位，故显式拒绝 v1 且不迁移）。
- `build.bat` 新增中文 `REM` 被 cmd 误解析 → 改 ASCII 英文。
- 既有 `ModifierCompilerDeterminismTests.cpp` 因 `--check` 语义变更而失败 → 改用 `--build`。

### 6.2 验证证据

- `cmd /c build.bat RelWithDebInfo` → 退出码 `0`；UMR 三步 precheck 全部出现并通过。
- 测试：`*Modifier*` 60/60、`*MonsterAffix*` 30/30、`*MapModifier*` 6/6、`*MonsterModifier*` 8/8、`*AttributePipeline*` 6/6、`*ItemPersistence*` 11/11、`*ItemStore*` 12/12、`*Nemesis*` 4/4、`*MapAffix*` 7/7；`ctest -L unit` → 100% / 8。
- 门禁负例：篡改受版本管理的 `map_modifiers.json` → `build.bat` 在 precheck 阶段中止，退出码 `1`；删除 `.bin` → 构建成功并自动重生成。
- 等价性核验：地图滚值 override、共鸣倍率、怪物三档入口互斥、吸血不双计、存储轨往返与 v1 拒绝，均有对应断言且已确认非弱断言。

### 6.3 剩余风险与后续任务

1. **地图词缀数值量纲（既有缺陷，非本批次回归）**：`MapAffixCalculator.cpp:100/174` 写入的 `affix.value` 为百分数点（如 `Enemy_ExtraHealth` T1=20），`MapModifierAdapter` 原样用作 `ADD_STAT_PERCENT_MULT` 参数（求值语义 `1 + value`），疑似应为 `value * 0.01`。已用 `git show HEAD:...MapModifierAdapter.cpp` 核对：旧实现同样未归一化，故本次**忠实保留**该行为，未引入回归；修正会改变玩法平衡（design §2.2 明确排除），须单独立项评估。
2. **技能魔耗结算链接线**：待确立唯一结算函数后接入 `mana_cost_mult`。
3. **仅验证 `RelWithDebInfo`**：未验证其他构建配置。
4. **Registry 单例测试隔离**：`ModifierRuntimeRegistry` 为单例且 `EnsureLoaded` 对已加载实例提前返回，测试需显式 `ReloadModifierRuntimeFromAsset()`；新增依赖 Adapter 的测试须沿用该约定。

### 6.4 结论

第 1 轮全部发现项已关闭，DoD 全部达成，构建与全量回归通过，门禁负例有效。**结论：`提交`**（上述 §6.3 第 1、2 项为独立的后续任务，不阻塞本次提交）。

---

## 7. 第 3 轮复审（代码审查发现项修复）

- **审查轮次**：第 3 轮
- **结论**：`提交`
- **输入**：针对本批次实现的代码审查（13 项发现，覆盖 25 个条目）

### 7.1 发现项与处置

**modifier 侧（11 项）**

| 严重度 | 发现 | 处置 |
| :--- | :--- | :--- |
| bug medium | override 按 `record_id` 只取首条并 `break`；同一地图词缀出现多次时后续滚值被静默丢弃（`(1+v_first)^N`），而改造前按每词缀建 record 是正确的 → 本批次引入的回归 | 以**每次出现一条**的 `ModifierRecordRequest{record_id, override_percent_mult, param_f32, target_stat}` 替换按 id 查表模型，逐条按序施加，得 `(1+v1)*(1+v2)*…` |
| bug medium | 同一问题在 `MapModifierAdapter` 侧的另一处表现 | 同上（适配器改为每次词缀出现构造一条 request） |
| bug low | override 不加区分地作用于该记录**所有** `ADD_STAT_PERCENT_MULT`，不校验 `op.param_u32` | request 携带 `target_stat`，仅覆盖命中 `param_u32` 的乘算 op；`kAllStatTargets` 保留“全部”语义 |
| bug medium | `EnsureLoaded()` 失败与 `FindRecordById` 未命中均静默返回空；`.bin` 为 gitignore 产物，缺资产时所有地图词缀（含无其他产者的共鸣 HP 加成）静默失效 | `ModifierRuntimeRegistry::Reload()` 无条件重读并在失败时 `LOG_ERROR`；`EnsureLoaded` 委托 `Reload`；适配器对加载失败/未命中打**一次性 `LOG_WARN`**（含类型与 recordId） |
| bug medium | 依赖 gitignore 的 `.bin` 使 CMake-only 构建静默失效 | 同上一行（失败可观测） |
| performance medium | `MonsterModifierAdapter` 全量求值再分段丢弃；`EvaluateBehaviorOps` 每帧每怪物调用仍白算属性/事件容器插入 | 新增 `ModifierOpCategory`（`Stats/Events/Behavior/All`）掩码，在**容器插入之前**跳过不相关 opcode；三入口分别传 `Stats\|Behavior`、`Events`、`Behavior`，删除“先全量再清空”后处理 |
| maintainability medium | 适配器硬编码的 4 个 ID 常量与生成器重复，`--check` 不校验，漂移时地图词缀全静默失效 | 常量收敛为 `MapModifierAdapter.hpp` 的 `MapModifierIds`，并新增守护用例：遍历非 `Count` 的 `MapAffixType` 断言记录命中且 `op.param_u32 == combatStat` |
| maintainability low | `combatStat` 仅当布尔用，目标属性实际来自编译记录，运行时无法发现错配 | 由上述守护用例在测试期锁定 header↔JSON 一致性；`MapAffixRegistry.hpp` 注释澄清“生成 JSON 为运行期目标属性权威” |
| maintainability medium | `TestCommon.hpp` 新助手与 Map/Monster 测试、`ModifierCompilerDeterminismTests` 的本地实现重复 | 删除 Map/Monster 本地副本并改用共享助手；助手改走 `registry.Reload`；`ReadAllBytes` 因职责不同保留（已说明取舍） |
| maintainability low | 助手把“文件缺失”与“解析失败”折叠成同一 `false`，缺 `.bin` 机器上难以诊断 | 助手先判 `std::filesystem::exists` 再 `Reload`，用 doctest 消息区分两种失败并提示运行生成脚本 |
| maintainability low | 适配器常量与生成器基址漂移风险 | 同“常量收敛 + 守护用例” |

**item storage 侧（2 项）**

| 严重度 | 发现 | 处置 |
| :--- | :--- | :--- |
| bug medium | `modifier_record_ids` 未纳入 `ItemStorageService::mergeStack` 的相等性比较，仅该字段不同的可堆叠物品会被静默合并、丢一份 record ids | 为 `ItemSideTableData` 增加 defaulted `operator==`/`operator!=`（全四字段），`mergeStack` 改用整体相等性，未来字段自动纳入 |
| maintainability low | Section 3 解码的 `recordIdCount` 仅以整段字节为上限，与同段 `convCount`/`dmgCount` 的固定上限不一致，可被构造载荷放大 `reserve()` | 改为 `constexpr uint32_t kMaxModifierRecordIdsPerItem = 64`，失败风格与相邻边界一致 |

### 7.2 验证证据（第 3 轮）

- `build.bat RelWithDebInfo` → 退出码 `0`，**一次通过、零编译修复**；`Reload` 生成 `.bin`（`compiled records: 32`）。
- `*Modifier*/*MonsterAffix*/*MapModifier*/*MonsterModifier*/*AttributePipeline*` → 100/100 用例通过（525 断言）；`*ItemPersistence*/*ItemStore*/*ItemStorage*` → 35/35（472 断言）；`*Equipment*/*Attribute*` 回归 → 19/19；`ctest -L unit` → 8/8（100%）。
- 7 条新增回归用例经定点运行确认**真实执行且具鉴别力**：
  - 重复词缀滚值 0.30/0.50 ⇒ `ApplyStat(100, MaxHealth) == 195.0`（`1.3×1.5`），可与旧的“只取首值”`1.3²=1.69` 明确区分。
  - override 仅命中 `target_stat`（Stat4 改、Stat7 不动；`target_stat=99` 全不动）。
  - 类别掩码：`Stats` 无事件、`Events` 无属性。
  - 常量守护遍历全程非真空（末尾 `CHECK(anyMapped)`）。
  - 合并拒绝用例断言两堆数量与各自 id 原样保留。
- 原调用方回归：`EquipmentModifierAdapter` 仍走保留的 `span<const uint32_t>` 重载；`ModifierEvaluator::Evaluate` 的 7 个调用文件全部编译通过；被删的 `ModifierParamOverride` 4 参重载无残留调用方（已 `rg` 确认，仅历史文档留痕）。

### 7.3 剩余风险与后续

1. **编码侧未做对称上限**：`kMaxModifierRecordIdsPerItem = 64` 目前只在解码侧校验，编码侧可写出超限数量导致该物品后续无法解码；建议补写入侧约束。
2. `ITEM_STORE_BINARY_VERSION 1→2` 显式不兼容 v1，无迁移（既定决策）。
3. 地图词缀数值量纲（`affix.value` 百分数点未 ×0.01）——既有缺陷，非本次回归（见 §6.3）。
4. `MANA_COST_MULT` 技能消耗链接线（见 §6.3）。
5. 本轮仅执行 `unit` 标签与指定过滤组，`functional/integration/performance` 套件未全量执行。
6. 当前仅 3 条地图词缀具备 `combatStat`，其余为 `Count` 哨兵不产出战斗属性；需确认是否符合玩法预期。

### 7.4 结论

13 项发现全部关闭（含 4 项真实 bug 与 1 项性能问题），构建一次通过、全量单测回归绿灯、新增回归用例经核验非真空。**结论：`提交`**（§7.3 各条为独立后续任务，不阻塞提交）。

## 8. 第 4 轮复审（独立子代理审查与修复）

- 审查目标：对第 1~3 轮全部未提交改动（33 个改动文件 + 3 份新文档）做一次独立代码审查，不依赖前几轮结论。
- 审查输入：`git diff` 全量改动、本报告、design、plan。
- 结论：**`提交`**。无 Blocker；核心链路（滚值连乘语义、`target_stat`/`kAllStatTargets` 匹配、`ModifierOpCategory`/`CategoryOfOp` 覆盖 29 个 opcode 无遗漏、`std::span` 生命周期、Section 3 编解码边界与版本拒绝、解码失败无部分写入、生成器只读性与退出码、常量双向一致）经独立核验全部正确。

### 8.1 已修复的发现

| 级别 | 发现 | 处置 |
|---|---|---|
| High | `ItemSideTableData::modifier_record_ids` 在 `src/` 中**无生产端**（写入方仅 `ItemPersistenceCodec.cpp:792` 解码与 `ItemStorageService.cpp:329`/`SaveManager.cpp:320` 的 store 间复制），design §3.4.2/§5.1 与 plan DoD 声称的 `ItemComponent -> CompactItem` 桥接尚未实现（属 T-P3-3 双轨统一），往返用例属自证式、无法观测该缺口 | 不改逻辑，按事实对齐文档与注释：design §3.4.2/§4 风险表/§5.1、plan §5 DoD 收窄为“本阶段交付旁表字段 + Section 3 编解码往返，字段为**前向兼容预留**，ECS→存储桥接待 T-P3-3”；`ItemStorageTypes.hpp:166-169` 与 `ItemPersistenceCodecTests.cpp` 用例旁加注 |
| Medium | `EnsureLoaded` 首次加载后丢弃入参 `path`，单例绑定首个调用者路径 | 新增 `m_loadedPath` 比对；`LoadFromBytes` 注入的合成数据（无路径）显式视为通配来源以保住既有测试语义 |
| Medium | 怪物侧缺记录是**无告警静默 continue**，与地图侧一次性告警不对称 | 抽出 header-only `src/game/systems/modifier/ModifierRuntimeSupport.hpp` 的 `ModifierRuntimeMissingRecordWarnLimiter`（按 id 去重、上限 8、可报多个不同 id）；两侧 registry 不可用统一 `LOG_ERROR`（一次性），缺记录统一限流告警 |
| Medium | 一次性告警为单个 `std::atomic<bool>`，只能报告第一个缺失 id，多点失效会被误判为单点 | 同上 limiter 取代三处 `atomic<bool>` 哨兵 |
| Low | `ValidateCrc32` 的 `crc32 == 0` 放行是测试后门但无注释 | 补中文注释说明其用途与后果 |
| Low | `m_loaded` 为非原子 `bool`，`Reload` 写 / 热路径读 | 改 `std::atomic<bool>`，读 acquire、写 release |
| Low | `ModifierRuntimeBootstrapIntegrationTest` 因其它用例 `LoadFromBytes` 注入而**恒定通过** | 重写为 `Reload(真实 .bin)` + `FindRecordById(4001004)` 正例断言；已用 `--order-by=rand --rand-seed=1..6` 交叉验证注入用例先行时仍有效 |
| Low | 生成器按位置索引解析 `MapAffixRegistry.hpp`，字段插入在前会静默取错 | `fields[8]` 加 `re.fullmatch(r"StatType::(\w+)")` 断言，失败抛 `RuntimeError` 并提示更新 `_parse_map_registry_values` |

### 8.2 登记为后续（本轮未做）

1. `.bin` 纳入 CMake 构建图（当前仅 `build.bat` 预检生成且被 gitignore；运行期已有 `Game.cpp` 启动 `EnsureLoaded`+抛异常与一次性 `LOG_ERROR` 可观测）。
2. `MonsterAffixSystem` 中 `EvaluateBehaviorOps` 早于 `HasOnUpdate()` 调用的顺序问题（性能）。
3. 生成器“每条 record 最多一个 `ADD_STAT_PERCENT_MULT`”不变量断言。
4. §7.3 第 1、3、4、6 条（编码侧对称上限、量纲、`MANA_COST_MULT` 接线、`combatStat` 覆盖度）。

### 8.3 验证

- `build.bat RelWithDebInfo` → 退出码 `0`（零编译修复）。
- 测试：`*Modifier*` 66/66、`*MonsterAffix*` 30/30、`*MapModifier*` 10/10、`*MonsterModifier*` 8/8、`*AttributePipeline*` 6/6、`*ItemPersistence*` 11/11、`*ItemStore*` 12/12、`*ItemStorage*` 13/13、`[Integration]*` 173/173；`ctest -L unit` → 8/8（100%）。
- 生成器：`gen_map_monster_modifier_v2.py --check` 退出码 `0`（map=4/monster=25，第 8 条断言未误伤现有数据）；`gen_modifier_runtime_v2.py --build` 退出码 `0`（32 records）。
- 告警未误报：全部捕获日志中无 `missing from registry` / `registry unavailable`。
- §7.3 第 5 条（`functional/integration/performance` 未全量执行）由本轮 `[Integration]*` 全量执行部分取代，其余标签仍未全量。

### 8.4 结论

第 4 轮独立复审无阻塞项，High/Medium/Low 可修项已全部修复并重新验证。**结论：`提交`**。
