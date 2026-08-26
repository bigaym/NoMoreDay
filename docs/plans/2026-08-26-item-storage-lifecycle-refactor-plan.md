# 物品与存储数据生命周期重构实施方案（Item Storage & Persistence Refactor Plan）

- 日期：2026-08-26（v2.1 审查修订版）
- 关联设计：`docs/designs/2026-08-26-item-storage-lifecycle-refactor-design.md`（v2 综合修订版）
- 关联审查：`docs/reviews/2026-08-26-item-storage-lifecycle-refactor-plan-review.md`（首次审查结论：已针对发现项完成全量修订）
- 状态：待实施（Ready for Implementation）
- 范围：设计 §7 全部阶段 P0a–P6；本计划是实施引导，不含完整代码

---

## 0. 计划输入与前置确认

- 设计文档：范围、合同（§5 所有权表）、验收标准（§8）、未决问题（§9）已齐。
- 相邻 Track/可复用资产（规划前已查）：
  - `conductor/archive/persistence_system/PLAN.md`、`SPEC.md`：历史持久化 Track，实施 P4 前通读，复用其 temp/rename、版本迁移约定。
  - `conductor/archive/dropped_item_optimization_20260118/summary.md`：地面掉落优化历史，P2 地面物品改造前查其结论避免重复。
  - `conductor/archive/stash_system_20260121/plan.md`、`runewords_20260115/plan.md`、`salvage_system_20260117/plan.md`：P2/P0b 涉及系统的历史计划，复用其术语与测试边界。
- 代码图谱清点（P2 影响面基线）：`ItemComponent` 读取点 ≈ 60 源文件，主要读者——gameplay（InventorySystem/StashSystem/CraftingSystem/SalvageSystem/RunewordSystem/ItemFactory/DropSystem/FragmentDropSystem/LootFilter/ItemEquipValidationService/SharedStash/MapAffixCalculator/EnemySpawnSystem/SerializationSystem）、stats（`src/game/foundation/stats/AttributePipeline.cpp`、`src/game/systems/modifier/EquipmentModifierAdapter.cpp`、`src/game/contracts/impl/StatsSystem.cpp`）、render（GPUEntitySync/GPULootAdapter/GameplayRenderAdapter）、UI（GameUiSnapshotBuilder/GameUiHost/OverlayController/UIRenderer/UIInventoryController/UICraftingController/UICommon/UIPanelDragService/UISystem）。
- 全局约束（贯穿所有阶段）：
  1. 线程模型不变：主线程写，异步编码任务只读冻结快照；
  2. 回退路径与适配隔离：feature flag `settings->useItemStore`（默认 false）为启动期配置，双轨逻辑严格收口在 `IItemStorageAdapter` 内部，60 个业务系统零散落分支；P4 验收前旧路径常驻可切回；
  3. 存档可靠性机制不降级：坚持「内存增量编码 + 写入 temp + 原子 rename + `.bak` 备份」，禁止不安全的原位修改；
  4. 零分配目标只对热路径（拾取/移动/整理/净帧 UI/过图）成立，冷路径（存读档）允许受控分配但须有预算；
  5. 服务依赖严格单向注入：`ItemStorageService` 在 P2 初始阶段即组装进 `Game` 组合根并通过 `SharedContext.itemStorage` 注入，杜绝临时单例；
  6. 每阶段独立可验收、可回退，禁止跨阶段交错改动。

---

## 1. 总体实施思路

**策略：先止血、再护栏、提前注入、逐层替换、适配隔离、每阶段闭环。**

- **P0b 先于一切**：§1.7 数据丢失是玩家数据正确性致命缺陷（包含锁、符文之语、孔数与孔位索引、扩展背包、材料银行、槽位布局、套装属性哈希），不依赖任何新架构，改动面仅限 DTO 与 SaveManager 序列化层，风险最低收益最高。P0a 护栏可与 P0b 并行（不同文件、无依赖）。
- **P1–P3 是数据面替换**：模板化 → 池化 → 迁移令牌，逐步把物品数据从「ECS 实体 + 胖组件」迁到「ItemStore + 句柄」。
  - P2 在任务开始时（T-P2-0b）即完成 `SharedContext.itemStorage` 基础设施注入，并通过 `IItemStorageAdapter` 适配层保持 `InventorySystem`/`StashSystem` 等对外签名不变，收口双轨逻辑。
  - P3 场景迁移令牌显式接管并批量销毁 `GroundPending` 容器句柄，根除内存泄漏与 suspend/resume JSON。
- **P4 是持久化替换**：二进制 codec 接入 SaveManager（CPU 级内存 Section 缓存 + 全量原子写），旧 JSON 档一次性导入，SerializationSystem 全量世界轨退役（F5/F8 重定向）。至此 `useItemStore` flag 与适配器旧分支删除，双写期结束。
- **P5 是读路径优化**：UI 快照门控 + version 复用 + 按需详情（`displayedItems` 直接查池 O(1) 获取详情，Stash 搜索改基于模板名称匹配），只动 `GameUiSnapshotBuilder` 与 `GameUiHost`，Controller/Renderer 合同不动。
- **P6 收尾**：清除历史单例门面残留、PortalSystem 分层修复、文档。

**每阶段的完成定义统一格式**（下文中不再重复）：任务清单全部勾选 + 该阶段测试命令全绿 + 该阶段证据（benchmark 输出/断言清单）归档到 `docs/reviews/` 或计划附录 + 无新增 TODO/临时桩（桩必须有注释与跟进任务编号）。

---

## 2. 阶段实施细节

### P0a. 护栏：基准与观测

**原理**：任何性能改造前必须有可复现基线。现有 benchmark（`tests/performance/SaveManagerBenchmark.cpp`、`ItemFactoryBenchmark.cpp`、`StashBenchmark.cpp`、`DropSystemBenchmark.cpp`）提供局部基线；本阶段补齐改造目标侧的基准与 Tracy 区段，使 P2–P5 的每项性能声明有对照数字。

**数据流**：基准二进制 `NoMoreDayTests` → doctest `[Performance]` 用例 → 构造 registry + 物品负载 → 计时（沿用现有 `std::chrono::high_resolution_clock` + `std::cout` 模式）→ 输出均值/多次采样。Tracy 区段在游戏运行时采集（`%NMD_DEVTOOLS%` 下工具，按 performance 工作流）。

**伪代码引导**（骨架，沿用现有 benchmark 文件风格）：
```
// tests/performance/ItemStoreBenchmark.cpp
TEST_CASE("[Performance] ItemStore - move/swap/visit (10k)") {
  ItemStore store; SeedStore(store, /*count=*/10'000);
  const auto t0 = Clock::now();
  for (int i = 0; i < 1'000'000; ++i) store.move(slotA[i%N], slotB[i%N]);
  Report("move", t0);           // 目标 < 10ns/次
  // visit / split / merge / sort 同构
}
```
```
// tests/performance/UiSnapshotBenchmark.cpp —— 依赖 P5 的 version 接口，
// P0a 阶段先落"现状基线"版本：Build 全量快照 3 次采样，记录每帧成本。
```

**原子任务**：
- [ ] T-P0a-1 新增 `tests/performance/ItemStoreBenchmark.cpp` 骨架（P2 落地 ItemStore 后填充；本阶段先占位并记录「待 P2 启用」）
- [ ] T-P0a-2 新增 `tests/performance/UiSnapshotBenchmark.cpp`：现状 `GameUiSnapshotBuilder::Build` 全量构建基线（背包 40 + 装备 + 仓库 3×144 + 地面 20 负载，采样 ≥50 帧取均值/p95）
- [ ] T-P0a-3 在 `GameUiSnapshotBuilder::Build`、`SaveManager::createSnapshot/restoreFromSnapshot`、`ItemFactory::serializeItem/restoreItem` 加 Tracy 区段（`ZoneScoped`，不改变行为）
- [ ] T-P0a-4 记录基线：`ctest -C Release -L performance` 输出 + Tracy 一次采集报告 → 归档 `docs/reviews/2026-08-26-item-storage-baseline.md`（含表格：createSnapshot 1000 件实测、restore 实测、UI 快照实测、StashBenchmark 现有数字）

**测试**：仅性能层。命令：`build.bat` → `ctest --test-dir build -C Release -L performance --output-on-failure`。
**完成定义**：基线文档存在且数字齐备；Tracy 区段可采集；无行为改动（`git diff` 仅限区段与 benchmark 文件）。

---

### P0b. 数据丢失热修（最高优先级，不依赖新架构）

**原理**：问题全在序列化边界（§1.7 已验证，包含新增排查的 `socketCount`、空孔丢失、符文错位及套装哈希），运行时结构不动。修复 = DTO 补字段 + SaveManager 两端对称读写 + 槽位与孔位索引保留 + 存档版本 3→4 迁移。注意「不引入新架构」纪律：本阶段修复直接落在现有 `SerializedItem`/`CharacterSaveData` 结构上，P4 的二进制 codec 与 P2 的 ItemStore 均复用这些修复后的字段语义（避免修两遍）。

**数据流（现状→修复后）**：
```
现状：ItemComponent ──serializeItem(丢 isLocked/activeRunewordId/conversions/damage_modifiers/socketCount/空孔)──> SerializedItem
      InventoryComponent ──createSnapshot(丢 capacity/bag_slots/MaterialBank, 无槽位索引)──> CharacterSaveData
      restore 时顺序 push + pad null（SaveManager.cpp:222-223）→ 布局压缩；sockets 顺序重置 → 空孔蒸发、符文错位
修复后：全字段对称 + vector<SerializedInventoryEntry{slotIndex, item}> + bagSlots 数组 + materialBank 账户数组 +
       socketCount 显式落盘 + vector<SerializedSocketEntry{socketIndex, item}> + restoreItem 套装 setNameHash 重算
```

**伪代码引导**：
```cpp
// 1) SerializedItem（foundation/data/SerializedItem.hpp）新增（全部带 from_json 缺省值）：
bool   isLocked{false};
uint32 activeRunewordId{0};
int32  socketCount{0};                                // 修复空孔蒸发：保存天然/打孔总数（ItemComponent::socketCount）
struct SerializedSocketEntry { uint8_t socketIndex; SerializedItem item; };
vector<SerializedSocketEntry> socketedItems;          // 改：保留精确镶嵌孔位，防止符文错位
vector<SavedStatConversion> conversions;              // 新 DTO：{type, value}，对照 ItemComponent::conversions
vector<SavedDamageModifier> damageModifiers;          // 新 DTO：对照 damage_modifiers

// 2) CharacterSaveData 新增：
int32  inventoryCapacity;                             // InventoryComponent::capacity
vector<SerializedInventoryEntry> inventory;           // 改：原 vector<SerializedItem> 换 entry 结构
vector<SerializedBagSlot{uint8 index; SerializedItem bag}> bagSlots;  // bag_slots 实体（背包本身也是物品，递归 serializeItem）
vector<SerializedMaterialEntry{uint32 id; int32 count}> materialBank; // MaterialBankComponent 账户

// 3) serializeItem 逐字段拷贝补齐（ItemFactory.cpp:227-283）：
dto.isLocked = item.isLocked;  dto.activeRunewordId = item.activeRunewordId;
dto.socketCount = item.socketCount;
dto.socketedItems.clear();
for (size_t i = 0; i < item.sockets.size(); ++i) {
  if (registry.valid(item.sockets[i])) {
    dto.socketedItems.push_back({(uint8_t)i, serializeItem(registry, item.sockets[i])});
  }
}

// 4) restoreItem 反向补齐（ItemFactory.cpp:937-1010）：
item.socketCount = dto.socketCount;
item.sockets.assign(item.socketCount, entt::null);
for (const auto& entry : dto.socketedItems) {
  if (entry.socketIndex < item.sockets.size()) {
    item.sockets[entry.socketIndex] = restoreItem(registry, entry.item);
  }
}
// 套装 setName 与 setNameHash 重建（AttributePipeline.cpp:450 依赖）：
if (item.rarity == Rarity::Set && item.setName.empty()) {
  item.setName = GetBaseItemSetName(item.baseId);
  item.setNameHash = NoMoreDay::utils::Hash(item.setName);
}

// 5) createSnapshot/restoreFromSnapshot：inventory 按 {index,item} 存读；restore 先按 index 直接落位（扩容到
//    capacity），再补 pad null；bag_slots 按索引恢复实体；materialBank 直灌账户。
// 6) CURRENT_CHARACTER_SAVE_VERSION: 3 -> 4；新增 MigrateSaveDataV3toV4（缺省字段填默认、inventory 摊平为
//    {连续index, item}），模式仿 MigrateLegacySpecializedSlots。
```

**原子任务**：
- [ ] T-P0b-1 DTO 扩展：`SerializedItem` 补 `isLocked/activeRunewordId/conversions/damageModifiers/socketCount`，将 `socketedItems` 改造为带 `socketIndex` 的 `vector<SerializedSocketEntry>`（含 JSON 缺省兼容）
- [ ] T-P0b-2 DTO 扩展：`CharacterSaveData` 补 `inventoryCapacity/inventory(带index)/bagSlots/materialBank`
- [ ] T-P0b-3 `ItemFactory::serializeItem/restoreItem` 字段对称补齐 + 模板派生字段校验日志 + 空孔与插槽索引复原 + 套装 `setNameHash` 重算
- [ ] T-P0b-4 `SaveManager::createSnapshot/restoreFromSnapshot` 填充/读取新字段；恢复按索引落位保布局
- [ ] T-P0b-5 版本 3→4 + `MigrateSaveDataV3toV4` 迁移分支
- [ ] T-P0b-6 回归测试 `tests/unit/ItemSaveRoundTripTests.cpp`（`[Unit]*Item*Save*`）：构造「锁定物品 + 天然 3 空孔装备 + 符文之语武器（孔0空、孔1/2镶嵌）+ 扩展背包（含包内物品）+ 材料银行 + 套装装备 + 空槽散布布局」→ createSnapshot → 写 JSON → 读 → restore → 字段级断言（isLocked/activeRunewordId/socketCount/conversions/setNameHash 全等、孔位顺序严格不移位、capacity 等值、bag_slots 逐索引等值、银行账户全等、非空槽 index 序列与布局全等）
- [ ] T-P0b-7 对拍测试：v3 档迁移后字段默认值断言（不 crash、不丢已有字段、空孔安全降级）

**测试**：unit 为主（T-P0b-6/7）；性能无要求（此阶段不优化，只修正确性）。
命令：`build.bat` → `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure` → `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`。
**完成定义**：T-P0b-6 全绿；既有 `SerializationSkillSanitizeTests` 等不回归；手工冒烟（设计 §8-7 流程）截图/日志归档。

---

### P1. 模板化：ItemTemplateRegistry

**原理**：把 ItemComponent 中的静态字段（name/description/setName/setBonuses/implicits/textureId/maxStack/isTwoHanded 等由 baseId 唯一决定的属性）移入不可变模板表，运行时实例只留模板引用。此阶段**不动容器与实体结构**（仍是 ECS 实体 + 瘦身后的 ItemComponent），只消除静态数据重复，为 P2 池化铺路。

**数据流**：`ItemFactory` 启动加载基底表（BaseItemDef 内部表 + 词缀定义）→ 构建 `ItemTemplateRegistry`（`baseId → shared_ptr<const ItemTemplate>`）→ 创建物品时 ItemComponent 填 `baseId` 为主，静态字段访问改经 `GetTemplate(item.baseId)` 解析。

**伪代码引导**：
```
// game/systems/item/storage/ItemTemplateRegistry.hpp（本阶段第一个新文件，零外部依赖）
class ItemTemplateRegistry {
  const ItemTemplate* find(uint32 baseId) const noexcept;   // 未命中返回 nullptr + telemetry 计数
  void registerTemplate(ItemTemplate t);                    // 启动期批量注册，运行期只读
};
// ItemComponent 兼容策略：静态字段保留（标记 // [deprecated: resolve via template]），
// 新增 static const ItemTemplate* 查询入口；逐系统替换读点（先从 UI/序列化侧开始）。
```

**原子任务**：
- [ ] T-P1-1 定义 `ItemTemplate`（设计 D1 字段集：baseId/kind/textureId/maxStack/minLevel/baseRanges/implicits 引用/setId/nameId）
- [ ] T-P1-2 实现 `ItemTemplateRegistry` + 单元测试（注册/查找/未命中）
- [ ] T-P1-3 `ItemFactory` 基底表迁移：现有 `BaseItemDef` 数据转为模板注册；createWeapon/createArmor/... 创建路径改「模板 + 滚动动态字段」
- [ ] T-P1-4 序列化层对齐：serializeItem 的模板派生字段改为「存 baseId，读时校验」并打日志（衔接 T-P0b-3 的校验逻辑）
- [ ] T-P1-5 读点替换（第一批低风险）：`GameUiSnapshotBuilder::ToItemView`、`ItemEquipValidationService`、`LootFilter` 改模板解析静态属性
- [ ] T-P1-6 回归：`[Unit]*Item*` 全量 + `ItemFactoryBenchmark`（1000 件创建 <5ms 保持）

**测试**：unit（T-P1-2）+ 既有 item 套件回归 + 性能（ItemFactoryBenchmark 不劣化）。
命令：`bin/NoMoreDayTests.exe --test-case="[Unit]*Item*"`、`ctest --test-dir build -C Release -L performance --output-on-failure`。
**完成定义**：模板表单测绿；ItemFactory 创建路径不再逐实例拷贝静态字符串（代码审查确认）；回归全绿。

---

### P2. ItemStore：句柄池 + POD 载荷（最大阶段，分子阶段）

**前置门**：
1. Q1（词缀上限 12、插槽上限 6）与 Q9（side-table 稀疏字段：conversions/damage_modifiers）定稿常量写入 `ItemStorageTypes.hpp`（T-P2-0）；
2. **依赖基础设施前移**：`ItemStorageService` 组装到 `Game` 组合根并通过 `SharedContext.itemStorage` 注入（T-P2-0b），为后续业务系统切换提供合法依赖入口，彻底杜绝临时单例。

**原理**：`ItemStore`（`src/game/systems/item/storage/`）提供句柄池：`ItemHandle{index, gen}` 8B、`ItemInstance` 定长 POD（设计 D2 字段集，`static_assert(std::is_trivially_copyable_v<ItemInstance>)`）、空闲链表 + 代际回收。容器槽位从 `entt::entity` 换成 `ItemHandle`；地面物品保留实体挂 `GroundItemComponent{ItemHandle}`。
**适配层隔离**：定义 `IItemStorageAdapter` 统一接口，`settings->useItemStore` 作为启动期决策由适配器内部封装，`InventorySystem`/`StashSystem` 等约 60 个调用方只面向适配器编码，公共签名不变，外部代码零散落 `if (useItemStore)`。

**数据流**：
```
创建：ItemFactory → registry.template + store.create(roll) → ItemHandle → 容器槽位写入
移动：InventorySystem::moveItem → store.move(SlotRef, SlotRef)（8B 拷贝 + version++）
读取：system 拿 handle → store.get(h) → const ItemInstance*（代际失配 = 陈旧句柄，计数并拒绝）
拾取：地面实体 DropSystem → store.create → GroundPending/Inventory 槽写入 → 实体销毁
```

**伪代码引导**：
```cpp
// ItemStore 核心接口
class ItemStore {
public:
  ItemHandle create(const ItemInstance& proto) noexcept;   // 从空闲表取槽, gen 复用自增
  const ItemInstance* get(ItemHandle h) const noexcept;    // gen 校验, 失败返回 nullptr
  bool mutate(ItemHandle h, Fn&& fn) noexcept;             // 写路径唯一入口, fn 后 version++
  void destroy(ItemHandle h) noexcept;                     // 槽位归还空闲表, gen++
  uint64_t version() const noexcept;                       // 全局版本(P5 用)
  void visit(SlotFilter, Visitor&&) const noexcept;        // 零分配遍历
};

// 基础设施注入与适配器模式（T-P2-0b）
class IItemStorageAdapter {
public:
  virtual bool moveItem(entt::registry& reg, const SlotRef& from, const SlotRef& to) = 0;
  // ... 统一业务接口
};
// 业务系统示例（保持签名不变，零 if-else 分支污染）
bool InventorySystem::moveItem(entt::registry& reg, int fromSlot, int toSlot) {
  return m_adapter->moveItem(reg, SlotRef{ContainerKind::Inventory, fromSlot}, SlotRef{ContainerKind::Inventory, toSlot});
}
```

**原子任务**（依赖顺序 T-P2-0 → T-P2-0b → 1 → 2 → 3/4 并行 → 5 → 6）：
- [ ] T-P2-0 设计定稿：Q1/Q9 结论固化（词缀内联上限 12、插槽上限 6、side-table 字段清单）；`ItemStorageTypes.hpp` 常量与 `static_assert` 落盘
- [ ] T-P2-0b **基础设施前移**：`ItemStorageService` 组装到 `Game` 组合根 + `SharedContext.itemStorage` 字段注入；定义 `IItemStorageAdapter` 适配接口封装双轨 flag
- [ ] T-P2-1 `ItemStore` 实现 + 单元测试（create/destroy/代际复用/get 失配/mutate 版本号/visit 遍历 10k）
- [ ] T-P2-2 `ItemInstance` POD 与 `ItemComponent` 双向转换器（含 side-table 处理）+ 往返测试
- [ ] T-P2-3 适配层（并行小组）：InventorySystem、StashSystem、SharedStash、CraftingSystem、SalvageSystem、RunewordSystem、LootFilter、ItemEquipValidationService 逐个切换到适配接口（每系统切换 = 独立原子任务，见附录任务表；签名不变）
- [ ] T-P2-4 地面物品：`GroundItemComponent` + DropSystem/FragmentDropSystem/GPULootAdapter/GPUEntitySync 切换；过图丢弃策略显式声明
- [ ] T-P2-5 容器组件瘦身：InventoryComponent/EquipmentComponent/StashTab 槽位换 handle 数组（由适配器内部隔离）
- [ ] T-P2-6 全量回归 + ItemStoreBenchmark 启用（move <10ns/次、visit 10k <0.1ms）

**测试**：unit（T-P2-1/2 新套件）+ 既有全部 item/stash/inventory 套件不回归（`[Unit]*Item*`、`[Unit]*Stash*`、InventoryDragSwapTests、UIInventoryControllerTests、UIStashControllerTests）+ 性能（ItemStoreBenchmark 新预算）。
命令：`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure`、`ctest --test-dir build -C Release -L performance --output-on-failure`。
**完成定义**：全部 item 相关 ctest 绿；`ItemFactoryBenchmark`/`StashBenchmark` 不劣化且新预算达标；`git grep` 确认 60 个调用方无散落 `useItemStore` 判断。

---

### P3. 迁移令牌：消灭 suspend/resume JSON

**原理**：P2 完成后容器物品无实体，`registry.clear()` 不再威胁物品数据。删除 `SharedStash::suspend/resume`（SharedStash.cpp:129-144）与 `SaveManager::restoreFromSnapshot` 中的 JSON 中转（SaveManager.cpp:176-307 的 suspend/clear/resume 序列）。场景切换显式接管 `GroundPending` 容器，**批量调用 `ItemStore::destroy(h)` 释放未拾取地面物品的句柄并清空容器**，断绝句柄池内存泄漏。

**数据流**：
```
现状：过图/读档 → suspend(全量 JSON 化) → registry.clear() → resume(全量反序列化重建实体)
目标：过图 → ItemMigration::beginSceneSwitch(policy=DropGround) → 遍历 GroundPending 批量 destroy 句柄 → registry.clear()（物品无损）→ endSceneSwitch
读档 → 解码直接重建 ItemStore 内容（P4 才改编码；本阶段读档路径仍走 DTO 但不再清/重建容器实体）
```

**原子任务**：
- [ ] T-P3-1 新增 `ItemMigration.hpp`：`beginSceneSwitch(GroundPolicy)/endSceneSwitch(token)`，显式执行 `GroundPending` 句柄全量回收（`store.destroy(h)`）与清空，单元测试增加池活跃实例计数断言
- [ ] T-P3-2 删除 `SharedStash::suspend/resume` 与 `m_suspendedData`（保留 toJson/fromJson 仅供 P4 迁移导入使用）
- [ ] T-P3-3 `SaveManager::restoreFromSnapshot` 去 suspend/clear/resume，改为直接恢复容器槽位表（依赖 P2 的恢复路径）
- [ ] T-P3-4 断言测试：过图路径上 nlohmann::json 构造计数为 0（测试内计数器/桩）；读档路径 ItemFactory::serializeItem 调用为 0
- [ ] T-P3-5 手动冒烟：城镇→副本→过图往返，仓库/背包/装备/地面物品状态正确，句柄池计数不单调递增

**测试**：unit（T-P3-4 断言）+ integration（场景切换流转，复用 GameUiHostLifecycleTests 模式）+ 手动。
**完成定义**：T-P3-4 绿；代码审查确认 SharedStash 无 suspend/resume 残留；过图后 GroundPending 句柄全量回收；冒烟证据归档。

---

### P4. 编码单轨：二进制 codec + SaveManager 接入

**原理**：`ItemPersistenceCodec`（设计 D5）：版本化 Section 二进制（magic/version/section table/CRC32 per section），容器 Section 保留槽位索引。
**持久化架构澄清**：放弃不安全的磁盘 CoW 块表，**增量仅在内存编码阶段（CPU 级）**——维护内存中的序列化 Section 缓存，仅对脏容器重新执行二进制打包；**磁盘写入坚持全量原子写入**：所有 Section 顺序写入 `saves/temp/slot_N.nmd`，校验 CRC 后原子 rename，并备份旧档为 `.bak`。SaveManager 消费 ItemStore 只读冻结快照（memcpy 级，耗时 <0.2ms）。旧 JSON 档（v3/v4）一次性导入二进制 v1；`SerializationSystem` 全量世界轨退役，F5/F8 重定向到 SaveManager 轨道。

**数据流**：
```
存档：ItemStore(冻结快照) → Codec::encode(仅重编码脏 Section，复用未脏 Section 内存块) → Taskflow 异步全量写 temp → rename(原子) → 旧档备份 .bak
读档：文件 → 版本/CRC 校验 → Codec::decode → 池直灌 + 槽位表重建（无逐实体 create）
旧档：JSON v3/v4 读取器（P0b 产物保留）→ 导入 ItemStore → 下次存写二进制
```

**伪代码引导**：
```cpp
// ItemPersistenceCodec 接口
struct EncodeInput {
  const ItemStore* store;
  ContainerDirtyMask dirtyMask;
  InMemorySectionCache* sectionCache; // 内存级 Section 缓存
};
bool encode(EncodeInput& input, std::ostream& outStream); // 组合内存 section，全量流式写出
bool decode(std::istream& inStream, ItemStore& outStore);  // 失败返回 false, 调用方回退 .bak

// SaveManager 改造点：
//   createSnapshot → ItemStorageService::freeze()（POD 内存整块 memcpy）
//   saveCharacterAsync 异步写 temp + 原子 rename
//   F5/F8：GameplayState.cpp:703 SerializationSystem::Update 调用点替换为 SaveManager 快捷全量存档
```

**原子任务**：
- [ ] T-P4-0 Q5/Q7 定稿：冻结快照方案（不可变快照值传递/双缓冲）定案；存档文件命名（`saves/slot_N.nmd`）与 temp 目录约定固化
- [ ] T-P4-1 `ItemPersistenceCodec` 二进制编码/解码 + 往返属性测试（随机 N 件含孔/套装/锁/符文之语，save→load 深比较）+ 损坏注入（截断/翻转 → 失败 + .bak 回退）
- [ ] T-P4-2 内存 Section 增量编码缓存 + 全量写 temp + 原子 rename（测试：单容器改动仅重计算该 Section 缓存，全量写盘 CRC 校验通过）
- [ ] T-P4-3 SaveManager 接入 codec；旧 JSON 导入路径（v3/v4 → ItemStore）
- [ ] T-P4-4 `SerializationSystem` 退役：删除全量世界序列化实现，F5/F8 重定向；保留 quicksave 兼容导入（Q3 定案）
- [ ] T-P4-5 对拍测试：同一存档数据旧代码 JSON 输出 vs 新二进制解码结果字段级一致
- [ ] T-P4-6 性能验收：SaveManagerBenchmark 新预算（createSnapshot 1000 <1ms、restore 1000 <2ms）达标
- [ ] T-P4-7 删除 feature flag `useItemStore` 与适配器旧路径分支（双写期结束）

**测试**：unit（往返/损坏）+ integration（对拍）+ performance（新预算）。
命令：`ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure`、`ctest --test-dir build -C Release -L performance --output-on-failure`、`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`。
**完成定义**：往返/损坏/对拍全绿；性能预算达标；SerializationSystem 源文件删除且无残留引用；flag 删除后旧路径代码清零。

---

### P5. UI 增量：门控 + version 复用 + 按需详情

**原理**：保留快照架构与 `GameUiItemView` 合同不变（Controller/Renderer 零改动或极小改动）。
三层优化：
1. **面板门控**：未开启容器不构建；
2. **version 门控**：`ItemStore::version()` 未变且 options 未变 → 复用上次视图；
3. **按需详情**：列表视图只填标量 + `ItemHandle`，**废弃 3 张临时哈希表重建**；仅 `displayedItems`（hover/tooltip/拖拽/打造目标）通过 `ItemHandle` 在 O(1) 复杂度直接查 `ItemStore` 填充词缀详情；Stash 搜索高亮直接由模板名称进行即时匹配，彻底消除 UI 帧内多余的字符串拷贝。

**数据流**：
```
GameplayState::Update（GameplayState.cpp:675-677）
  → Build(registry, options, &prevSnapshot)：若 store.version==lastVersion && options 未变 → 返回缓存
  → 否则：只重建"开启中面板 + 脏容器"的视图；displayedItems 通过 ItemHandle 直接查池取详情
  → m_uiHost->Update(...) 不变
```

**原子任务**：
- [ ] T-P5-1 `GameUiSnapshotBuilder` 门控补全：stash/materials/ground 视图按面板开关构建（对齐 options 现状）
- [ ] T-P5-2 version 复用：`Build` 增加缓存命中路径；snapshot 对象由 GameUiHost 持有复用（向量 capacity 跨帧保留）
- [ ] T-P5-3 按需详情：列表视图去 affix 拷贝，移除 `inventoryById`/`equipmentById`/`groundById` 临时哈希表；`displayedItems` 直接通过 `ItemHandle` 查池填充；Stash 搜索改基于模板名称匹配
- [ ] T-P5-4 UiSnapshotBenchmark 启用新预算：净帧（version 未变）物品构建成本 = 0（断言跳过计数）
- [ ] T-P5-5 回归：GameUiSnapshotBuilderTests、UIInventoryControllerTests、UIStashControllerTests、UICraftingControllerTests、OverlayControllerTests 全绿（合同不变验证）

**测试**：unit（快照构建/复用命中）+ 既有 UI 套件 + performance（UiSnapshotBenchmark）。
命令：`ctest --test-dir build -C RelWithDebInfo -L ui --output-on-failure`、`ctest --test-dir build -C Release -L performance --output-on-failure`。
**完成定义**：净帧物品构建成本 0（benchmark 断言）；UI 套件全绿；Tracy `SnapshotBuild.Items` 在净帧为空。

---

### P6. 收尾：单例清理、分层修复、文档

**原理**：完成单例彻底清理与架构合规固化——删除 `SharedStash`/`HeirloomVault` 过渡门面；PortalSystem 不再直呼 SaveManager 单例（PortalSystem.cpp:139-143 分层破坏修复，改经 SharedContext 发存档请求）。

**原子任务**：
- [ ] T-P6-1 清理旧单例门面：彻底删除 `SharedStash::Get()`/`HeirloomVault::Get()` 历史单例实现与头文件残留
- [ ] T-P6-2 PortalSystem 分层修复 + 分层测试（world 不依赖 application/persistence 的编译级断言或脚本检查）
- [ ] T-P6-3 `SaveManager` 单例治理（跟随 service 注入后降级为常规服务）
- [ ] T-P6-4 文档收尾：术语表、`conductor/tracks.md`（若立项）、设计/计划状态更新、`docs/reviews/` 汇总报告
- [ ] T-P6-5 全量验证：`ctest -C RelWithDebInfo -L ci` + `-L performance` 全绿 + 手动冒烟全流程

**测试**：integration + 手动 + 分层检查脚本。
**完成定义**：单例删除（grep 无 Get() 残留）；全量 ctest 绿；冒烟证据归档；文档更新完成。

---

## 3. 全局测试方法与命令

| 层级 | 标准化命令 | 适用阶段 |
|---|---|---|
| 构建 | `build.bat`（RelWithDebInfo；`build.bat notest` 跳过测试） | 全部 |
| 物品模块门禁 | `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.item" --output-on-failure` | P0b/P1/P2/P3/P4 |
| UI 模块门禁 | `ctest --test-dir build -C RelWithDebInfo -L ui --output-on-failure` | P5 |
| CI 全量门禁 | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | 全部 |
| 性能基准门禁 | `ctest --test-dir build -C Release -L performance --output-on-failure` | P0a/P1/P2/P4/P5 |
| Tracy 采集 | `%NMD_DEVTOOLS%` 工具，按 performance 工作流记录结果与基线 | P0a/P2/P5 |

测试命名约定沿用现有 doctest 前缀：`[Unit]`/`[Integration]`/`[Performance]`；新增用例放在对应目录（unit/integration/performance）。

---

## 4. 验收汇总（对齐设计 §8）

- 8-1 P0b 止血生效（round-trip 字段级/空孔/符文顺序/套装哈希全等断言）→ P0b 完成时
- 8-2 行为不变（既有套件不改断言全绿）→ 每阶段
- 8-3 场景切换零序列化且句柄全量回收 → P3
- 8-4 性能达标（预算表）→ P4/P5
- 8-5 存档往返/对拍 → P4
- 8-6 损坏恢复 → P4
- 8-7 手动冒烟 → P0b/P3/P6

---

## 5. 风险与回退

| 风险 | 回退/缓解 |
|---|---|
| P2 调用面大（~60 文件） | 基础设施前移（T-P2-0b）；`IItemStorageAdapter` 内部收口双轨 flag，外部业务系统零分支；逐步切换 + 回归 |
| Q1/Q9 定稿拖延阻塞 P2 | T-P2-0 设为 P1 并行任务，常量定稿与 `static_assert` 先行，不进 P2 则不阻塞 P0b/P1 |
| 二进制格式缺陷（演化/对齐） | 版本化 section + CRC + 模板指纹；拒绝 reinterpret 直读；JsonCodec 导出对照 |
| 掉电存档损坏风险 | 坚决执行内存增量编码 + 写 temp + 原子 rename + `.bak` 备份，剔除危险的原位写 |
| 性能目标未达成 | 每阶段性能测试早暴露；快照采用 POD 内存整块 memcpy（10k 实例 <0.2ms） |

---

## 6. 未决问题跟踪（与阶段绑定）

| 问题 | 绑定任务 | 结论去处 | 最终裁决建议 |
|---|---|---|---|
| Q1 词缀/孔位上限 | T-P2-0 | 设计 D2 常量固化 | 词缀内联 12，插槽 6，`static_assert` 约束 |
| Q9 side-table 稀疏字段范围 | T-P2-0 | 设计 D2 | conversions/damage_modifiers 入稀疏表 |
| Q5 冻结快照方案 | T-P4-0 | 设计 D5 | POD 内存不可变快照传递（全量 memcpy <0.2ms） |
| Q7 存档命名/目录 | T-P4-0 | 设计 D5 | `saves/slot_N.nmd` 与 `saves/temp/` |
| Q2 地面物品过图策略 | T-P2-4 / T-P3-1 | 设计 D4 | 维持丢弃策略，强化 GroundPending 句柄批量销毁 |
| Q3 F5/F8 语义 | T-P4-4 | 设计 D8 | 重定向为 SaveManager 快捷存档，世界导出转 CLI |
| Q8 组件物理形态（独立库） | P6 后另议 | 设计 Q8 | 先按 `src/game/systems/item/storage/` 落地，后续统一拆库 |
| Q4 回收站 | 不在本计划 | 设计 Q4 | v1 非目标，预留 telemetry 日志 |

---

## 附录 A. P2 适配层原子任务表（每个系统一条，可并行小组执行）

每个任务 = 适配接口切换 + 该系统既有测试回归 + 代码审查。通用模板：

- [ ] T-P2-3a InventorySystem（move/swap/equip/unequip/pickUp/drop/destroy/organize）
- [ ] T-P2-3b StashSystem（transfer/deposit/withdraw/sort/search/autoDeposit + 页解锁经济不变）
- [ ] T-P2-3c SharedStash（putItem/takeItem/toJson/fromJson——toJson 仅供 P4 导入）
- [ ] T-P2-3d CraftingSystem（目标/材料句柄化，符文镶嵌路径 CraftingSystem.cpp:258-299 保语义）
- [ ] T-P2-3e SalvageSystem（excludeLocked 依赖 isLocked 字段——P0b 已保）
- [ ] T-P2-3f RunewordSystem（checkForRuneword 读孔内句柄）
- [ ] T-P2-3g LootFilter / ItemEquipValidationService
- [ ] T-P2-3h stats 面：AttributePipeline / EquipmentModifierAdapter / StatsSystem（读 ItemInstance 或经视图，保持平坦遍历合同）
- [ ] T-P2-3i HeirloomVault（HeirloomData 改 ItemHandle + 元数据）
- [ ] T-P2-3j MaterialBank（账户模型并入统一容器，Add/Remove/GetCount 语义不变）
