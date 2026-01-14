# 技术规格书：高性能材料存储系统 (Material Storage System Spec)

## 1. 系统概述 (System Overview)
本系统旨在解决《NoMoreDay》中数百种堆叠类物品（材料、符文、词缀碎片）的存储与展示问题。为了支撑“万级实体”的性能目标，本系统将材料从传统的 **ECS 实体模型** 转换为 **紧凑型数据模型**，确保材料不占用有限的背包格子，且在大规模数据下保持极低的 CPU 和内存开销。

## 2. 核心设计原则 (Design Principles)
- **数据驱动 (Data-Driven)**：所有材料属性通过 JSON 配置，程序仅处理 ID 和数量。
- **性能导向 (Performance Oriented)**：使用有序向量 (Sorted Vector) 替代节点式存储 (Map/List)，最大化 CPU 缓存命中率。
- **零实体化 (Zero-Entity in Storage)**：材料在拾取后即刻销毁 ECS 实体，仅以数值形式存在于玩家组件中。

## 3. 系统边界 (System Boundaries)

### 3.1 包含范围 (In-Scope)
- **堆叠类材料**：如矿石、布料、皮革。
- **锻造碎片 (Crafting Shards)**：词缀升级所需的碎片。
- **符文 (Runes)**：用于符文语组合的消耗性底材。
- **通货 (Currencies)**：如特定的代币或任务进度物品。

### 3.2 不包含范围 (Out-of-Scope)
- **装备 (Equipment)**：具有随机词缀的唯一实体，继续使用 `ItemComponent` 和网格存储。
- **药水/消耗品 (Consumables)**：具有使用逻辑且需要快捷键配置的物品，保留在物品栏中。
- **遗物 (Relics)**：具有独立逻辑的特殊实体。

## 4. 架构设计 (Architecture)

### 4.1 静态数据：MaterialRegistry
- **存储**：`std::unordered_map<uint32_t, MaterialDefinition>`。
- **定义**：包含名称、描述、稀有度颜色、图标纹理 ID、所属分类标签（Tag）。
- **加载**：游戏启动时从 `assets/data/materials.json` 一次性读取。

### 4.2 动态数据：MaterialBankComponent (ECS Component)
- **结构**：`std::vector<MaterialEntry>`，其中 `MaterialEntry` 是一个 POD 结构 `(uint32_t id, int32_t count)`。
- **存储约定**：向量始终保持按 `id` 升序排列。
- **算法**：使用 `std::lower_bound` 实现 $O(\log N)$ 的插入、删除和查询。

### 4.3 表现层：Virtual List UI
- **逻辑**：不为每种材料创建独立 UI Widget。
- **渲染**：根据滚动偏移量，仅计算并绘制当前视口内的行。
- **批处理**：通过纹理图集 (Atlas) 确保整个材料列表单次 Draw Call 完成。

## 5. 系统交互 (System Interactions)

### 5.1 与 InventorySystem 的交互
- **拾取拦截**：在 `pickUpItem` 函数中增加类型判定。若物品为 `Material`，则调用 `MaterialBankComponent::Add(id, count)` 并直接 `registry.destroy(entity)`。
- **逻辑隔离**：材料不再进入 `InventoryComponent::items` 向量，不触发背包容量检查。

### 5.2 与 CraftingSystem 的交互
- **消耗判定**：锻造系统不再从背包中寻找物品实体，而是直接查询玩家实体的 `MaterialBankComponent`。
- **原子操作**：确保锻造消耗材料时，数量扣减与属性更新是原子性的。

### 5.3 与 SerializationSystem 的交互
- **轻量序列化**：存档系统只需将 `MaterialBankComponent` 序列化为简单的 ID-Count 键值对数组。
- **数据清理**：读档时若 ID 在注册表中不存在，则作为废弃数据清理。

### 5.4 与 ItemFactory 的交互
- **掉落生成**：`ItemFactory` 根据掉落表生成具有 `ItemType::Material` 的临时实体，这些实体仅包含基础外观数据。

## 6. 性能目标 (Performance Targets)
- **内存**：500 种材料的存储开销应控制在 10KB 以内。
- **查询**：任意材料的数量查询耗时应小于 1ms。
- **UI**：在材料页面滚动时，UI 响应时间不应受材料总数影响（保持 60+ FPS）。

## 7. 路线图说明 (Roadmap Context)
本系统是 Phase 9 (终局装备深度) 的基石。只有实现了高效的碎片和符文存储，后续的深度锻造和符文语系统才能在不破坏 UX 的前提下得以展开。
