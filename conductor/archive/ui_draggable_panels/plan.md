# UI Draggable Panels Plan

## 1. 基础数据结构与工具 (UI System Core)
- [x] **任务 1.1**: 更新 `UIContext.hpp`
    - 添加 `enum class UIPanelID`。
    - 添加 `struct PanelState`。
    - 在 `UIState` 中添加 `panelStates` 数组和 `activeDragPanel`。
- [x] **任务 1.2**: 在 `UISystem` 中实现 `UpdatePanelDrag`
    - 在 `UISystem.hpp` 声明静态辅助函数。
    - 在 `UISystem.cpp` 实现拖动逻辑（包括边界限制）。

## 2. 面板适配 (Panel Integration)
- [x] **任务 2.1**: 适配 Character Panel (`UICharacter`)
    - 引入 `UIPanelID::Character`。
    - 在 `Draw` 开始处调用 `UpdatePanelDrag`。
    - 使用更新后的 `panelX/panelY` 进行绘制。
- [x] **任务 2.2**: 适配 Inventory Panel (`UIInventory`)
    - 引入 `UIPanelID::Inventory`。
    - 在 `Draw` 开始处调用 `UpdatePanelDrag`。
    - 注意 `invX` 等依赖于 `panelX` 的相对坐标，确保它们随动。
- [x] **任务 2.3**: 适配 Crafting Panel (`UICrafting`)
    - 引入 `UIPanelID::Crafting`。
    - **注意**: `UICrafting` 目前使用 `scaleFactor` 计算坐标，与前两者不同。需要统一或适配它的计算方式。
    - 确保 `DrawMergePanel` 也使用新的动态坐标。

## 3. 验证与打磨
- [x] **任务 3.1**: 编译并测试
    - 验证所有三个面板是否都能被拖动。
    - 验证拖动范围限制是否生效。
    - 验证关闭并重新打开面板后，位置是否保留。
- [x] **任务 3.2**: 代码审查与格式化
    - 确保符合 C++20 规范。
    - 移除调试日志。
