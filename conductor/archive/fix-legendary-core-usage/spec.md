# 技术规格书: 传奇核心使用逻辑修复

## 1. 问题陈述
玩家期望 "Legendary Core" (ID 10001) 此类关键物品被标记为 `Consumable` 类型，并且可以通过右键菜单的“使用”功能直接打开对应的功能面板（传奇融合），目前无效。

## 2. 解决方案

### 2.1 类型修正 (ItemFactory)
在 `ItemFactory::createMaterial` 中，针对 ID 10001 (Legendary Core) 进行特殊处理，强制将其类型覆盖为 `ItemType::Consumable`。
同时，确保其稀有度为 `Legendary`。

### 2.2 允许作为融合材料 (UICrafting)
修改 `UICrafting::DrawMergePanel` 中的拖拽判定逻辑，允许 `ItemType::Consumable` 类型的物品放入 "Catalyst" (核心) 槽位。

### 2.3 快捷入口 (UICrafting Interface)
在 `UICrafting` 类中公开 `OpenMergePanel()` 静态方法，用于从外部直接打开并切换到“融合”页签。

### 2.4 使用逻辑实现 (InventorySystem)
在 `InventorySystem::useItem` 中，增加对 ID 10001 的处理：
- 调用 `UICrafting::OpenMergePanel()`。
- **不**消耗物品（只打开界面，放入槽位由玩家操作，或自动放入？目前仅打开界面即可，自动放入可能涉及所有权转移逻辑，暂时保持简单的打开界面）。
- 返回 `false` 或 `true`？如果返回 `true`，`useItem` 通常会减少数量或销毁。
    - 针对 Core，我们不希望“使用”就消失。我们希望它打开 UI。
    - 修改 `useItem` 的返回值语义或在内部处理。
    - 如果 `useItem` 返回 `true`，调用者会减少数量。
    - **关键点**: 对于 Legendary Core，我们应执行操作（打开 UI）并返回 `false`（表示物品未被“消耗”掉），或者修改 `useItem` 的逻辑以支持不消耗的 Use Action。
    - 查看 `InventorySystem::useItem`：如果返回 `true`，它会执行 `if (itemComp->quantity > 1) ...`。
    - 所以必须返回 `false` 才能保留物品？
    - 但 `InventorySystem::useItem` 中 `effectApplied` 标志控制了是否进入消耗逻辑。如果我设置 `effectApplied = false`，则不会消耗。
    - 但如果 `effectApplied` 是 false，则此函数返回 false。
    - 如果返回 false，UI 层（Context Menu）可能会认为“无法使用”。
    - 需要检查调用者 `UIInventory` 或 `UIContext` 如何处理返回值。通常只关心是否发生了动作。
    - **策略**: 修改 `useItem` 逻辑，允许处理“非消耗性使用”。添加一个 `consumeOnUse` 标志。

## 3. 验收标准
1.  新获得的 Legendary Core 显示为 `Consumable` 类型（可能需要新档或新掉落，或重新生成）。
2.  右键点击 Core -> 使用 -> 传奇融合面板打开，且自动切换到第二页签。
3.  将 Core 拖入 Catalyst 槽位 -> 成功放入。
