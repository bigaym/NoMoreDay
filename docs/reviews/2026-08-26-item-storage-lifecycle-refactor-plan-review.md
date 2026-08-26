# 物品与存储数据生命周期重构实施计划审查报告（Plan Review）

- **审查目标**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`
- **审查基准**：
  - 设计文档：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（v2 综合修订版）
  - 工作流标准：`docs/workflows/review.md`、`docs/workflows/planning.md`
  - 规范基准：`conductor/code_standard.md` (V2.1)、`conductor/tech-stack.md`
  - 代码库现状：`ItemComponent.hpp`、`SerializedItem.hpp`、`SaveData.hpp`、`SaveManager.cpp`、`ItemFactory.cpp`、`InventoryComponent.hpp`、`MaterialBankComponent.hpp`、`SerializationSystem.hpp`、`GameUiSnapshotBuilder.cpp`、`GameUiSnapshot.hpp`、`SharedStash.cpp`、`HeirloomVault.hpp`、`PortalSystem.cpp`、`AttributePipeline.cpp`、`CraftingSystem.cpp`、`RunewordSystem.cpp`、`SaveManagerBenchmark.cpp` 等源码检视。
- **审查轮次**：首次审查（First Round）
- **审查结论**：`修改`（Request Changes / Needs Revision）

---

## 1. 总体评价与结论

### 1.1 总体结论：`修改`

本实施计划（`2026-08-26-item-storage-lifecycle-refactor-plan.md`）基于已批准的设计文档 v2，提出了将物品与存储体系从「ECS 实体 + 胖组件」迁移为「句柄池 + 定长 POD 实例 + 独立存储服务」的完整重构路径。整体战略方向（P0b 紧急止血数据丢失、数据面与生命周期解耦、持久化单轨化、UI 脏驱动增量与按需详情、单例治理）正确且具备深度。

然而，在对代码库现状进行严格的代码图谱检索与源码逐行对拍后，发现该计划存在 **2 项 Blocker 级缺陷、2 项 High 级缺陷、3 项 Medium 级缺陷**。其中最严重的问题包括：
1. **P0b 止血清单遗漏致命数据丢失点**（`socketCount` 空孔丢失、符文错位、套装属性读档失效）；
2. **阶段依赖拓扑倒置**（P2 适配层即需要 `SharedContext` 注入，但计划将其推迟到 P6）；
3. **P4 增量 CoW 块表与原子写安全性机制的概念冲突与过度设计**；
4. **P2-P4 双轨 feature flag 的桥接生命周期未具体化**。

必须对实施计划进行针对性修订并补齐上述缺口后，方可进入编码实施阶段。

---

## 2. 核心发现项（按严重度排序）

### 【Blocker 01】P0b 止血清单遗漏 `socketCount` 与孔位索引，导致空孔丢失及符文错位
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:90-116`（T-P0b-1 ~ T-P0b-6）、`src/game/systems/item/ItemFactory.cpp:276-280, 986-992`、`src/game/foundation/data/SerializedItem.hpp:31-45`
- **代码实证**：
  1. `ItemComponent` 中 `socketCount`（可用孔数，1~3）与 `sockets`（`vector<entt::entity>`，已镶嵌实体）分离。
  2. 在 `ItemFactory::serializeItem` 中，仅遍历 `sockets` 中 valid 的实体存入 `dto.socketedItems`，`socketCount` 完全未落盘。
  3. 在 `ItemFactory::restoreItem`（Line 991）中，`item.socketCount = (int)item.sockets.size();`。
  4. **后果 1（空孔归零）**：若武器生成时拥有 3 个空插槽，玩家未镶嵌符文时存档并读档，`dto.socketedItems` 为空，读档后 `socketCount` 变为 0，玩家装备的打孔/天然孔全部蒸发！
  5. **后果 2（孔位错位破坏符文之语）**：若武器有 3 孔，孔 0 为空，孔 1 镶嵌符文。序列化时只存 1 个符文，反序列化时顺序 `push_back` 到孔 0，导致符文索引从 1 错位到 0。`RunewordSystem::checkForRuneword`（`RunewordSystem.cpp:179`）对符文序列严格匹配，此错位直接破坏符文之语的激活与识别。
- **违背原则**：P0b「彻底止血玩家数据丢失」的核心目标未达成。
- **修复建议**：
  - 在 `SerializedItem::StatsSnapshot` 中补齐 `int socketCount = 0;`。
  - 在 `SerializedItem` 中将 `socketedItems` 改造为 `vector<SerializedSocketEntry{uint8_t socketIndex; SerializedItem item;}>`（或定长占位数组），保证空孔与指定孔位索引的精确复原。
  - 在 `T-P0b-1` 与 `T-P0b-6` 回归断言中增加「含空孔装备」、「部分镶嵌装备」的 round-trip 验证。

---

### 【Blocker 02】阶段依赖拓扑倒置：P2 适配层需要 `SharedContext` 注入，但计划将其推迟到 P6
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:183-188, 194, 297-304`（P2 伪代码、T-P2-3 适配层、T-P6-1 组合根注入）
- **问题分析**：
  - 计划在 P2 阶段要求改造约 60 个调用文件（`InventorySystem`、`StashSystem`、`CraftingSystem` 等），将实体操作改为调用 `ItemStorageService`。
  - 计划给出的伪代码明确显示：`auto& svc = ...SharedContext->itemStorage;`。
  - 然而，计划中的任务 **T-P6-1**（`ItemStorageService` 组装到 `Game` 组合根 + `SharedContext.itemStorage` 注入）却被安排在 **P6 收尾阶段**。
  - **冲突**：在 P2 到 P5 的漫长开发周期内，60 个业务系统若无法从 `SharedContext` 获取 `ItemStorageService`，开发者将被迫使用临时单例（如 `ItemStorageService::Get()`）或全局指针。这直接违背了单例治理的初衷，且会在 P6 造成二次大规模重构。
- **违背原则**：`docs/workflows/planning.md` 依赖拓扑单向无环原则；`conductor/code_standard.md` §1 架构边界。
- **修复建议**：
  - 将 `ItemStorageService` 的创建及其在 `SharedContext` 中的注入（`SharedContext.itemStorage`）前移至 **P2 初始任务（T-P2-0b）**。
  - P6 仅保留「彻底移除 `SharedStash::Get()` 和 `HeirloomVault::Get()` 等旧单例门面」，不再承担服务注入的基础设施工作。

---

### 【High 01】P4 磁盘 CoW 增量块表与原子文件写入（Temp+Rename）存在架构冲突与过度设计
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:228-256`（P4 原理、T-P4-2）
- **代码实证与分析**：
  1. `SaveManager.cpp:309-411` 的现有机制是：主线程快照 -> Taskflow 异步写临时文件 `saves/temp/slot_N.json` -> `std::filesystem::rename` 原子替换到 `saves/slot_N.json`。
  2. 计划在 P4 提出在二进制持久化格式中引入 "copy-on-write 块表"，仅重编码并写入脏容器 section。
  3. **架构矛盾**：
     - 若采用原子写入安全策略（写入 temp 然后 rename），每次必须输出完整的合法单文件。在 temp 文件中，未修改的 section 仍然需要从旧文件复制或从内存镜像写入，无法跳过磁盘 I/O。
     - 1000 件物品的二进制紧凑编码体积仅约 20KB~50KB，在 Taskflow 后台线程中一次性顺序写入耗时 < 0.5ms，根本不需要复杂的磁盘级 CoW 块管理。
     - 若强行在单文件中做原位（in-place）CoW 增量更新，一旦写入中断或掉电，将直接破坏存档文件，丧失原子写入的防损保护。
- **违背原则**：KISS 原则；`conductor/tech-stack.md` 避免不必要的复杂状态机；防数据损坏硬保证。
- **修复建议**：
  - 澄清并修正 P4 的增量概念：**「增量」仅发生在内存编码阶段（CPU 级）**——即维护内存中的序列化 section 缓存，仅对脏容器重新执行二进制打包；
  - **磁盘写入仍坚持全量原子写入**：将所有 section 顺序写入 temp 文件并原子 rename，彻底剔除磁盘级 CoW 块表的设计与任务（简化 T-P4-2 为内存 section 缓存），兼顾极致的 CPU 性能与 100% 的 I/O 掉电安全。

---

### 【High 02】P2-P4 双轨 Feature Flag（`settings->useItemStore`）的生命周期与桥接设计缺失
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:18-24, 160, 196`
- **问题分析**：
  - 计划声明在 P2-P4 期间通过 `settings->useItemStore` 保持双轨可回退。
  - 但现状是 `InventoryComponent.items` 存储 `vector<entt::entity>`。若开启 `useItemStore`，容器槽位由 `ItemStore` 的 `ItemHandle` 管理，实体不再创建；若关闭，则需要创建 ECS 实体并挂载 `ItemComponent`。
  - 计划中未定义过渡期数据桥接层（`ItemEntityBridge` / `IStorageAdapter`）的具体机制：
    - 运行时切换 flag 还是仅启动期决定？
    - 60 个调用方是通过统一接口访问，还是在代码中充斥 `if (useItemStore)` 分支？
    - 若无严格的统一适配层，`if-else` 分支将散落到 60 个文件中，导致代码严重劣化、测试矩阵翻倍，且在 P4-7 删除 flag 时产生巨大的清理风险。
- **修复建议**：
  - 明确规定 `useItemStore` 为**编译期/启动期配置**，禁止运行时动态切换。
  - 在 P2 定义统一的 `IItemStorageAdapter` 或内部封装代理，业务系统（如 `InventorySystem`）只面向适配接口编码，所有双轨分支严格收口在适配器内部，外部 60 个业务文件零 `if (useItemStore)`。

---

### 【Medium 01】P0b 遗漏套装标识（`setName` / `setNameHash`）在旧 DTO 恢复期的处理
- **关联位置**：`src/game/foundation/components/ItemComponent.hpp:172-177`、`src/game/systems/item/ItemFactory.cpp:937-1010`、`src/game/foundation/stats/AttributePipeline.cpp:450`
- **问题分析**：
  - `AttributePipeline.cpp:450` 计算套装属性时严格检查：`if (item.rarity == Rarity::Set && item.setNameHash != 0)`。
  - 当前 `SerializedItem` 没有 `setName` / `setNameHash` 字段，且 `ItemFactory::restoreItem` 也没有根据 `baseId` 重建 `setName` 与 `setNameHash`。
  - 在 P0b 阶段（尚未构建 P1 `ItemTemplateRegistry` 之前），如果玩家持有套装物品，存档后再读档，`setNameHash` 为 0，套装属性加成完全失效。
- **修复建议**：
  - 在 P0b 的 `T-P0b-3` 中增加：`restoreItem` 在恢复时，若 `rarity == Rarity::Set`，必须通过现有 `BaseItemDef` 或临时查找补齐 `setName` 并调用 `NoMoreDay::utils::Hash(setName)` 填充 `setNameHash`。

---

### 【Medium 02】P3 场景迁移（`ItemMigration`）中地面物品句柄回收未闭环
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:204-225`（P3 原理与 T-P3-1）
- **代码实证与分析**：
  - P2 将地面掉落物改为轻量实体 `GroundItemComponent{ ItemHandle handle; }`，句柄挂在 `ItemStore` 的 `GroundPending` 容器中。
  - P3 中 `beginSceneSwitch(policy=DropGround)` 触发 `registry.clear()`。此时地面实体被销毁。
  - 但如果不显式遍历 `GroundPending` 容器并调用 `ItemStore::destroy(handle)`，这些地面物品的 `ItemInstance` 将永远残留在 `ItemStore` 内存池中，导致句柄池内存泄漏与代际耗尽。
- **修复建议**：
  - 在 `T-P3-1` 的 `ItemMigration::beginSceneSwitch` 实现中，显式声明对 `GroundPending` 容器内所有 `ItemHandle` 的批量销毁（`store.destroy(h)`）与清空操作，并在单元测试中增加池大小/活跃实例计数断言。

---

### 【Medium 03】P5 UI 增量与按需详情中 Stash 搜索与 Tooltip 解析遗漏
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:268-290`、`src/game/application/ui/GameUiSnapshotBuilder.cpp:525, 940-968`
- **代码实证与分析**：
  - 现状检视发现：`GameUiSnapshotBuilder.cpp` 仅构建了 `inventoryById`、`equipmentById`、`groundById`，原本就遗漏了 `stashById`，导致悬停仓库物品时 `displayedItems` 解析缺失。
  - 此外，`GameUiSnapshotBuilder.cpp:525` 在构建 Stash 视图时通过 `item->name` 做字符串匹配（`ContainsIgnoreCase`）。
  - 在 P5 实施按需详情（列表不拷贝字符串和词缀）后：
    1. `displayedItems` 不应再通过遍历三个 `unordered_map` 来查找，而应直接通过 `ItemStore::get(ItemHandle)` 在 O(1) 内获取详情；
    2. Stash 搜索高亮不应依赖 `GameUiItemView::name`，而应由 `ItemStore` 结合 `ItemTemplateRegistry` 的不可变模板名称进行即时匹配。
- **修复建议**：
  - 在 `T-P5-3` 中明确：`displayedItems` 解析彻底移除 3 张临时哈希表，改为直接通过 `domainId`（转 `ItemHandle`）从 `ItemStore` 获取；
  - 补充 Stash 搜索过滤的模板化匹配逻辑，彻底消除 UI 帧内多余的字符串拷贝。

---

### 【Low / Best Practice 01】测试命名与 CTest 过滤标签的一致性规范
- **关联位置**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md:310-323`
- **分析与建议**：
  - 计划中混合使用了 doctest 测试套名（如 `[Unit]*Item*Save*`）与 CTest 标签（`-L ci`、`-L performance`）。
  - 建议在第 3 节明确标准化测试命令：
    - 快速模块门禁：`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure`；
    - CI 全量门禁：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`；
    - 性能基准门禁：`ctest --test-dir build -C Release -L performance --output-on-failure`。

---

## 3. 各阶段逐项审查分析

| 阶段 | 评估状态 | 合理之处 | 潜在错漏与风险点 | 判定与修改要求 |
|---|---|---|---|---|
| **P0a 护栏** | 良好 | 沿用既有 doctest benchmark 模式与 Tracy 规范，基线先行。 | 需确保基线测试包含 1000 件多词缀+插槽物品的极端负载。 | **通过**（保持） |
| **P0b 止血** | **需修改** | 准确识别了锁定状态、符文之语、背包扩展、材料银行及槽位布局丢失等根本缺陷。 | **遗漏 `socketCount`、空孔索引保留及套装名称哈希恢复**（见 Blocker 01、Medium 01）。 | **必须修改**：补齐字段清单与测试。 |
| **P1 模板化** | 良好 | 模板与实例分离清晰，字段定义与 `BaseItemDef` 契合。 | 需保证模板加载在任何物品反序列化之前完成。 | **通过**（细节对齐） |
| **P2 句柄池与适配** | **需修改** | 定长 POD 内存布局合理，零堆分配，事务语义清晰。 | **`SharedContext` 注入依赖倒置**（Blocker 02）；双轨 flag 缺少适配隔离（High 02）。 | **必须修改**：注入前移至 P2，规范适配层。 |
| **P3 迁移令牌** | 良好 | 彻底根除 `suspend/resume` JSON 往返，切断场景与物品耦合。 | 需补齐 `GroundPending` 容器内未拾取句柄的批量回收（Medium 02）。 | **通过**（补齐句柄回收任务） |
| **P4 单轨持久化** | **需修改** | Section 二进制化、CRC32 校验、旧档迁移链规划完整。 | **磁盘 CoW 增量块表与 Temp+Rename 原子写冲突**（High 01）。 | **必须修改**：改为内存增量+全量原子写。 |
| **P5 UI 增量** | 良好 | Version 门控与按需详情契合 UI 性能优化诉求，Controller 合同不变。 | 需修正 `displayedItems` 直接查池与 Stash 搜索匹配（Medium 03）。 | **通过**（细化查询路径） |
| **P6 收尾与分层** | 良好 | 修复 `PortalSystem` 跨层调用，彻底清除全局单例。 | 承接前序修改后，仅负责旧单例清理与系统最终集成。 | **通过**（调整职责边界） |

---

## 4. 未决问题（Q1-Q9）绑定与阻塞风险评估

| 问题编号 | 核心议题 | 计划绑定任务 | 现状评估与阻塞风险分析 | 审查裁决与建议 |
|---|---|---|---|---|
| **Q1** | 词缀/孔位内联上限 | T-P2-0 | 实测上限（3前缀+3后缀+隐匿+传奇）明确，定长 12 槽，孔位 6 槽。**低风险**。 | 同意计划方案，T-P2-0 固化 `static_assert` 即可。 |
| **Q1b** | quantity 宽度 | 已定稿 | 实例 `uint32`，材料银行 `int32`。**无风险**。 | 维持定案。 |
| **Q2** | 地面物品过图策略 | T-P2-4 / T-P3-1 | 现行为丢弃。需确保句柄池显式回收（见 Medium 02）。**中风险**。 | 维持丢弃策略，强化句柄回收代码。 |
| **Q3** | F5/F8 快捷键语义 | T-P4-4 | 重定向为 SaveManager 快速存档，世界导出转 CLI 工具。**低风险**。 | 维持定案。 |
| **Q4** | 物品回收站机制 | v1 非目标 | 预留 telemetry 审计日志。**无风险**。 | 维持非目标定义。 |
| **Q5** | 冻结快照方案 | T-P4-0 | 10k 实例 POD 内存仅约 2MB，主线程全量拷贝耗时 < 0.2ms。**低风险**。 | 建议直接采用 **不可变快照值传递（或双缓冲）**，彻底避免复杂引用计数。 |
| **Q6** | 编解码手写 vs 库 | 已定稿 | 严格遵循零外部依赖手写。**无风险**。 | 维持手写定案。 |
| **Q7** | 存档文件命名与目录 | T-P4-0 | 约定为 `saves/slot_N.nmd` 与 `saves/temp/`。**低风险**。 | 维持定案。 |
| **Q8** | 独立静态库物理形态 | P6 后另议 | 先按 `src/game/systems/item/storage/` 落地，后续统一拆库。**低风险**。 | 维持定案。 |
| **Q9** | side-table 稀疏字段范围 | T-P2-0 | `conversions` / `damage_modifiers` 出现率低，进稀疏表。**低风险**。 | 同意方案，T-P2-0 落地统计断言。 |

---

## 5. 改进与完善建议清单（Action Items）

为使计划达到可执行、无歧义的实施标准，请在 `docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md` 中完成以下修订：

1. **修正 P0b 止血任务清单（补齐 3 处遗漏）**：
   - 更新 `T-P0b-1`：`SerializedItem` 显式增加 `int socketCount`，并将 `socketedItems` 改造为带 `socketIndex` 的结构。
   - 更新 `T-P0b-3`：`restoreItem` 恢复套装物品时强制重新计算 `setNameHash`。
   - 更新 `T-P0b-6`：测试用例增加空孔保留、符文顺序不移位的断言。
2. **调整服务注入时序（修正拓扑倒置）**：
   - 将 `T-P6-1`（`SharedContext.itemStorage` 字段增加与注入）移动至 **`T-P2-0b`**，作为 P2 的先决基础设施任务。
3. **澄清并简化 P4 持久化架构**：
   - 删除磁盘级 CoW 块表描述；将 `T-P4-2` 明确定义为「内存序列化 Section 缓存 + 全量写入 Temp + 原子 Rename」。
4. **规范 P2-P4 双轨适配层机制**：
   - 补充 `IItemStorageAdapter` 接口设计说明，明确双轨分支全部收口在适配器内部，禁止在 60 个调用文件中散落 `if (useItemStore)`。
5. **补全 P3 迁移清理与 P5 UI 查询细节**：
   - 在 `T-P3-1` 中增加 `GroundPending` 容器句柄全量回收逻辑。
   - 在 `T-P5-3` 中明确 `displayedItems` 直接通过 `ItemHandle` 查询 `ItemStore`，废弃 3 张临时哈希表。

---

## 6. 审查出口门禁核对

- [x] 变更文件边界已记录（`git status --short`）
- [x] 设计规格与实施计划对齐已逐项检查
- [x] 变更代码上下文已通过 codebase 图谱与源码直接检视
- [x] 结论精确为 `修改`
- [x] 发现项已按严重度排序并附具体代码位置、问题机理与修复建议
- [x] 审查报告已落盘至 `docs/reviews/2026-08-26-item-storage-lifecycle-refactor-plan-review.md`

---

## 7. 第二轮复查（Round 2 Follow-Up Review）

- **复查对象**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`（v2.1 修订版）
- **复查结论**：**`提交`**（Approved / Ready for Implementation）
- **发现项核对结果**：
  1. `[Blocker 01]` 已修复：`SerializedItem` 补齐 `int32 socketCount`，`socketedItems` 改造为 `vector<SerializedSocketEntry>`，`restoreItem` 恢复精确孔位并重算 `setNameHash`（T-P0b-1, T-P0b-3, T-P0b-6 已同步更新）；
  2. `[Blocker 02]` 已修复：`SharedContext.itemStorage` 组装与注入已前移至 `T-P2-0b` 作为 P2 首要基础设施任务；
  3. `[High 01]` 已修复：P4 持久化已澄清为「内存 Section 增量编码缓存 + 全量写 temp + 原子 rename」，移除了磁盘级 CoW 块表；
  4. `[High 02]` 已修复：明确了 `IItemStorageAdapter` 统一接口与启动期 flag 隔离机制；
  5. `[Medium 01-03]` 已全部修复：套装 `setNameHash` 重算、P3 `GroundPending` 句柄批量销毁、P5 `displayedItems` O(1) 直接查池与 Stash 模板名称匹配均已落实至对应原子任务；
  6. `[Low 01]` 已修复：标准化了各阶段 CTest 门禁命令。
- **剩余风险**：
  - P2 涉及约 60 个调用文件的适配，需严格遵循原子任务单系统推进与回归门禁。
- **下一步动作**：
  - 正式推进实施阶段 P0a（护栏基准）与 P0b（数据丢失热修）。
