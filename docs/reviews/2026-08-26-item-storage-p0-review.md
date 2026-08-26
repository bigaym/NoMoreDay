# 物品与存储数据生命周期重构（P0a/P0b）代码审查报告

- **审查目标**：P0a 护栏（基准与观测）与 P0b 数据丢失热修（Data Loss Hotfix）实施结果
- **结论**：`提交`
- **审查轮次**：首次审查（阶段性验收）
- **日期**：2026-08-26
- **审查员**：独立代码审查员（Code Reviewer）

---

## 1. 输入与参考

1. **设计参考**：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（§1.7 丢数据证据, §7 P0a/P0b 阶段定义, §8 验收标准）；
2. **实施计划参考**：`docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`（§2 P0a, P0b 原子任务 T-P0a-1~4 与 T-P0b-1~7）；
3. **审查标准**：`docs/workflows/review.md` 与 `conductor/code_standard.md` (V2.1)；
4. **性能基线与观测报告**：`docs/reviews/2026-08-26-item-storage-baseline.md`；
5. **验证证据与执行结果**：
   - 编译构建：`cmake --build build --config RelWithDebInfo --target NoMoreDayTests`，0 错误 0 警告；
   - 单元测试：`bin/NoMoreDayTests.exe --test-case="[Unit]*Item*Save*"` 8 组用例 147 断言 100% 通过；
   - 物品相关全量回归：`bin/NoMoreDayTests.exe --test-case="[Unit]*Item*"` 40 组用例 311 断言 100% 通过；
   - CI 门禁：`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.ci.nonperf"` 100% 通过（耗时 6.54s）。

---

## 2. 变更文件边界

### 2.1 `git status --short` 摘要

```text
 M src/game/application/persistence/CMakeLists.txt
 M src/game/application/persistence/SaveManager.cpp
 M src/game/application/persistence/SaveManager.hpp
 M src/game/application/ui/GameUiSnapshotBuilder.cpp
 M src/game/foundation/data/SaveData.hpp
 M src/game/foundation/data/SerializedItem.hpp
 M src/game/systems/item/CMakeLists.txt
 M src/game/systems/item/ItemFactory.cpp
?? docs/reviews/2026-08-26-item-storage-baseline.md
?? docs/reviews/2026-08-26-item-storage-p0-review.md
?? tests/performance/ItemStoreBenchmark.cpp
?? tests/performance/UiSnapshotBenchmark.cpp
?? tests/unit/ItemSaveRoundTripTests.cpp
```

### 2.2 变更文件清单及职责检视

| 文件路径 | 变更类型 | 检视职责 |
|---|---|---|
| `src/game/foundation/data/SerializedItem.hpp` | 修改 | DTO 字段补齐：`isLocked`, `activeRunewordId`, `socketCount`, `setName`, `bagCapacity`, `conversions`, `damageModifiers`；`socketedItems` 升级为带 `socketIndex` 的结构并提供向下兼容 JSON 反序列化 |
| `src/game/foundation/data/SaveData.hpp` | 修改 | 存档版本升至 4 (`CURRENT_CHARACTER_SAVE_VERSION = 4`)；新增 `SerializedInventoryEntry`, `SerializedBagSlot`, `SerializedMaterialEntry`；`CharacterSaveData` 补齐容量、带索引槽位、扩展背包槽与材料银行 |
| `src/game/systems/item/ItemFactory.cpp` | 修改 | `serializeItem` / `restoreItem` 对称序列化补齐；孔位与符文索引精准还原；套装 `setNameHash` 重算；增加 Tracy `ZoneScopedN` 观测区段 |
| `src/game/systems/item/CMakeLists.txt` | 修改 | 链接 `Tracy` 性能分析库 |
| `src/game/application/persistence/SaveManager.hpp` | 修改 | 声明 `MigrateSaveDataV3toV4` 静态迁移函数 |
| `src/game/application/persistence/SaveManager.cpp` | 修改 | `createSnapshot` / `restoreFromSnapshot` 填充/读取新增容器数据；实现背包稀疏槽位落位；V3→V4 迁移；增加 Tracy `ZoneScopedN` 区段 |
| `src/game/application/persistence/CMakeLists.txt` | 修改 | 链接 `Tracy` 性能分析库 |
| `src/game/application/ui/GameUiSnapshotBuilder.cpp` | 修改 | 在 `Build` 主入口接入 Tracy `ZoneScopedN` 区段 |
| `tests/unit/ItemSaveRoundTripTests.cpp` | 新增 | 8 组深度回归测试（锁定保护、空孔保留、符文精确定位、转换与增伤、套装哈希、背包稀疏布局、扩展包与材料银行、V3 迁移） |
| `tests/performance/UiSnapshotBenchmark.cpp` | 新增 | UI 快照全量构建性能基准（40 背包 + 10 装备 + 432 仓库 + 20 地面，100 次采样） |
| `tests/performance/ItemStoreBenchmark.cpp` | 新增 | `ItemStore` 性能基准骨架（记录 P2 预算目标） |
| `docs/reviews/2026-08-26-item-storage-baseline.md` | 新增 | 记录 P0a 阶段各项基线实测数据与 Tracy 观测锚点 |

---

## 3. 范围对齐评估

对照 `docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md` 与 `docs/plans/2026-08-26-item-storage-lifecycle-refactor-plan.md`：

### 3.1 P0a（护栏：基准与观测）对齐
- [x] **T-P0a-1**：新增 `tests/performance/ItemStoreBenchmark.cpp` 骨架并明确标注 P2 预算指标（`< 10ns/move`, `< 0.1ms visit 10k`）；
- [x] **T-P0a-2**：新增 `tests/performance/UiSnapshotBenchmark.cpp` 真实负载基线测试（100 采样实测均值 0.022ms）；
- [x] **T-P0a-3**：在 `GameUiSnapshotBuilder::Build`、`SaveManager::createSnapshot/restoreFromSnapshot`、`ItemFactory::serializeItem/restoreItem` 全部接入 `ZoneScopedN`；
- [x] **T-P0a-4**：建立并归档 `docs/reviews/2026-08-26-item-storage-baseline.md`。

### 3.2 P0b（数据丢失热修）对齐
- [x] **T-P0b-1**：`SerializedItem` 补齐 `isLocked/activeRunewordId/conversions/damageModifiers/socketCount`，且 `socketedItems` 携带精确 `socketIndex`；
- [x] **T-P0b-2**：`CharacterSaveData` 补齐 `inventoryCapacity/inventory(带index)/bagSlots/materialBank`；
- [x] **T-P0b-3**：`ItemFactory::serializeItem/restoreItem` 完成对称补齐，插槽实体按索引复原，`setNameHash` 自动重算修复套装加成；
- [x] **T-P0b-4**：`SaveManager::createSnapshot/restoreFromSnapshot` 完整读写扩展背包、材料银行与稀疏槽位布局；
- [x] **T-P0b-5**：版本号升级为 4，提供 `MigrateSaveDataV3toV4` 迁移函数，向下兼容 V3 格式 JSON；
- [x] **T-P0b-6**：回归测试覆盖全部 8 组关键用例，断言严密且真实可复现；
- [x] **T-P0b-7**：对拍/迁移测试证实 V3 旧存档加载不崩溃、默认值安全回退。

---

## 4. 质量与风险评估（对照 `conductor/code_standard.md`）

1. **DOD / ECS 内存安全（§2.2, §5.3）**：
   - `restoreItem` 与 `restoreFromSnapshot` 中实体创建与组件 emplace 遵循安全模式，未跨增删操作持有悬空引用；
   - 槽位根据 `capacity` 进行预分配（`assign`/`resize`），索引越界处具备严格防御性检查与 push_back 容错兜底。
2. **现代 C++ 规范与无裸指针（§5.1, §5.2, §6.1）**：
   - 零裸 `new`/`delete`，全部依托 RAII 与现代标准容器；
   - 无 `dynamic_cast`，无危险 C 式转换。
3. **字符串无关核心逻辑与哈希优化（§7.2）**：
   - `ItemFactory.cpp:1020` 对套装属性进行哈希重建（`NoMoreDay::utils::Hash(item.setName)`），保证 `AttributePipeline.cpp:450` 在计算套装词缀时可通过整数哈希 `item.setNameHash != 0` 快速匹配，避免任何运行时字符串比较。
4. **向下兼容与容错性**：
   - `from_json(SerializedItem)` 与 `from_json(CharacterSaveData)` 针对遗留旧版字段（如未携带 `socketIndex` 的扁平符文数组、未携带 `slotIndex` 的背包数组）做了分支降级处理，保障旧存档平滑过渡。
5. **重复轮子检索**：
   - 序列化复用已有的 `nlohmann::json` 规范；哈希重算复用已有的 `core/utils/HashUtils.hpp`；测试框架复用既有 `doctest` 与 `BenchmarkUtils`；未引入任何冗余或冲突的自制工具。

---

## 5. 发现项（按严重度排序）

### 5.1 Blocker / High / Medium
- **无**。所有 P0a/P0b 任务均严格按规范完成，无阻塞性缺陷。

### 5.2 Low / Best Practice
1. **[Best Practice] [SerializedItem.hpp:25](file:///D:/PRJ/NoMoreDay/src/game/foundation/data/SerializedItem.hpp#L25)**
   - **观测**：`SerializedItem` 结构体内部使用了 `using SerializedSocketEntry = NoMoreDay::SerializedSocketEntry;` 引入在其下方定义的类型别名。
   - **分析**：当前写法在 MSVC 及主流 C++20 编译器下已通过前置声明与命名空间解析正常编译，且在 P4 实施二进制序列化与 P2 POD 改造后，`SerializedItem` 将仅作为旧档 JSON 迁移通道。
   - **建议**：后续阶段进行 DTO 重组时，可将 `SerializedSocketEntry` 结构体物理定义顺序置于 `SerializedItem` 之前，以进一步提升头文件的直观可读性。

2. **[Best Practice] [SaveManager.cpp:255](file:///D:/PRJ/NoMoreDay/src/game/application/persistence/SaveManager.cpp#L255)**
   - **观测**：`restoreFromSnapshot` 中恢复背包槽位时，若 `entry.slotIndex` 超出 `inv.items.size()`，执行了 `inv.items.push_back(itemEntity)`。
   - **分析**：这是良好的防御性兜底，但会导致 `inv.items.size()` 大于 `inv.capacity`。
   - **建议**：在后续 P2 `ItemStore` 接入时，统一由统一存储容器的容量契约（`StorageError::OutOfCapacity`）严格校验。

---

## 6. 剩余风险

1. **旧存档兼容性**：当前 V3→V4 迁移已在单元测试中覆盖典型格式，但若存在早于 V3（即未包含 specialized_slots 规范）的历史极端测试存档，将按既有 `MigrateLegacySpecializedSlots` 规则兜底；
2. **P2 接入前的数据双写**：当前阶段仅修复现有实体 DTO 序列化路径，P2 落地 `ItemStore` 句柄池后需确保双轨适配器 `IItemStorageAdapter` 严格复用本次修复的全部字段语义。

---

## 7. 审查结论与下一步动作

- **最终审查结论**：`提交`（Pass）
- **依据**：
  1. 彻底根除了 `isLocked`、`activeRunewordId`、空孔与符文错位、扩展背包、材料银行、背包槽位布局压缩等 7 处重大数据丢失缺陷；
  2. 建立了完整的 UI 快照与持久化性能观测基线与 Tracy 追踪点；
  3. 全部 8 组 Round-Trip 单元测试与 40 组物品模块测试 100% 通过，CI 门禁全绿；
  4. 0 编译警告，0 架构违规。
- **下一步动作**：
  1. 允许将 P0a 与 P0b 的变更代码提交到版本控制库；
  2. 按路线图推进下一阶段 **P1（模板化：ItemTemplateRegistry）** 的设计与实施。
