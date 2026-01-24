# 实现计划: 传奇核心交互逻辑

## Phase 1: 接口与类型 (API & Type)
**目标**: 基础能力支持。

- [ ] **Task 1.1**: 更新 `UICrafting` 接口
    - 在 `UICrafting.hpp` 添加 `OpenMergePanel()`。
    - 在 `UICrafting.cpp` 实现它（设 visible = true, currentTab = Merging）。
    - 修改 `DrawMergePanel` 允许 Consumable 入槽。
- [ ] **Task 1.2**: 修正物品类型
    - 在 `ItemFactory::createMaterial` 中，若 ID == 10001，设 type = Consumable。

## Phase 2: 逻辑接入 (Logic Hook)
**目标**: 实现点击行为。

- [ ] **Task 2.1**: 更新 `InventorySystem::useItem`
    - 引入 `UICrafting.hpp`。
    - 在 ID 判断链中增加 10001。
    - 执行 `UICrafting::OpenMergePanel()`。
    - **特别处理**: 确保不触发扣除数量逻辑（需重构 `useItem` 或添加分支）。

## 总结
工时: 0.3h
风险: 消耗逻辑处理需谨慎，避免误删传奇核心。
