# Track Plan: 高性能材料存储系统 (Material Storage System)

## 1. 目标 (Goal)
实现一个高性能、可扩展且不占用普通背包格子的材料存储系统。材料拾取后将直接进入玩家的“材料银行”，并支持在 UI 中通过虚拟化列表高效展示数百种材料。

## 2. 技术规格 (Tech Specs)
- **存储结构**：`MaterialBankComponent` 使用有序向量 (`std::vector<MaterialEntry>`)，提供 $O(\log N)$ 的查询性能和极高的缓存命中率。
- **静态数据**：`MaterialRegistry` 从 `materials.json` 加载只读元数据。
- **UI 渲染**：在 `UIInventory` 中实现虚拟化列表，仅渲染可见行，支持分类过滤和搜索。
- **持久化**：集成至 `SerializationSystem`，仅保存 ID 和数量。

## 3. 任务清单 (Tasks)

### Phase 1: 核心基础 (Core Foundation)
- [x] **Task 1.1**: 设计 `assets/data/materials.json` 格式并创建初始数据。
- [x] **Task 1.2**: 实现 `MaterialRegistry` 类，负责单例管理和 JSON 数据加载。
- [x] **Task 1.3**: 定义 `MaterialBankComponent` 及其基础操作接口（Add, Remove, GetCount）。
- [x] **Task 1.4**: 为 `MaterialBankComponent` 编写单元测试，验证排序一致性和查询效率。

### Phase 2: 逻辑集成 (Logic Integration)
- [x] **Task 2.1**: 集成到 `SerializationSystem`，实现材料数据的自动存档与读档。
- [x] **Task 2.2**: 重构 `InventorySystem::pickUpItem`，实现材料自动重定向至银行并销毁实体的逻辑。
- [x] **Task 2.3**: 在 `ItemFactory` 中增加对材料掉落生成的支持。

### Phase 3: UI 表现 (UI & UX)
- [x] **Task 3.1**: 在 `UIInventory` 中增加“材料”标签页切换逻辑。
- [x] **Task 3.2**: 实现垂直滚动的虚拟化列表渲染引擎（Virtualized List）。
- [x] **Task 3.3**: 增加按分类（符文、矿石、碎片等）过滤的功能。
- [x] **Task 3.4**: 实现简单的名称搜索过滤。

## 4. 定义完成 (Definition of Done)
- [x] 材料拾取后不再出现在普通背包格中。
- [x] 存档文件中包含正确的材料 ID 和数量。
- [x] 即使拥有 100+ 种材料，UI 切换和滚动依然保持 60+ FPS。
- [x] 所有核心接口均有单元测试覆盖。
