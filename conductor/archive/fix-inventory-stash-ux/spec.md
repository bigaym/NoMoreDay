# 技术规格书: 背包与仓库交互体验修复 (UX Repair)

## 1. 问题陈述
当前仓库与背包系统存在三项严重影响用户体验的交互问题：
1. **个人仓库初始化失败**: 个人仓库未解锁初始页签，且无法手动解锁 (Missing Component)。
2. **输入焦点泄露**: 在仓库搜索框输入时，按键事件穿透至游戏层（如 'A' 键导致角色移动）。
3. **拖拽功能缺失**: 无法将物品从仓库直接拖拽至背包的特定格子（逆向操作支持，但正向缺失）。

## 2. 技术方案

### 2.1 个人仓库初始化 (Stash Initialization)
在 `GameplayState::InitializeEntities` 中，通过 Entity Registry 为玩家实体添加缺少的 `PersonalStashComponent` 组件，并初始化默认页签。

**代码变更**:
```cpp
// GameplayState.cpp
auto& stash = registry.emplace<PersonalStashComponent>(player);
stash.unlockedTabs = 1;
stash.tabs.resize(1);
stash.tabs[0].name = "Shared 1"; // 默认名称
```

### 2.2 输入系统阻断 (Input Blocking)
利用 `UISystem` 的全局状态引入 `isTyping` 标志位。当任意 UI 输入框获取焦点时置位，`InputSystem` 在处理角色移动前检查此标志。

**UIState (UISystem.hpp)**:
```cpp
struct UIState {
    // ... existing ...
    bool isTyping = false; // 新增：全局输入占用标志
};
```

**UIStash (UIStash.cpp)**:
```cpp
// Update Loop
UISystem::State.isTyping = m_isSearchFocused;
```

**InputSystem (InputSystem.cpp)**:
```cpp
void InputSystem::update(entt::registry& registry, Camera2D& camera) {
    if (UISystem::State.isTyping) return; // 阻断所有 Gameplay 输入
    // ... existing logic ...
}
```

### 2.3 仓库至背包拖拽 (Stash -> Inventory DnD)
在 `UIInventory` 的渲染循环中，增加对 `UISystem::State.isDraggingFromStash` 状态的响应逻辑。并为此在 `StashSystem` 中实现原子化的“指定槽位取出”函数。

**StashSystem (StashSystem.hpp)**:
```cpp
/**
 * @brief 将仓库物品移动到指定的背包槽位
 * @return true 成功移动或交换
 */
static bool withdrawToSpecificSlot(entt::registry& registry, 
                                   StashType srcType, int srcTab, int srcSlot, 
                                   entt::entity playerEntity, int invInvSlot);
```

**UIInventory (UIInventory.cpp)**:
```cpp
// Inside Item Grid Loop
if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    if (UISystem::State.isDraggingFromStash) {
         if (StashSystem::withdrawToSpecificSlot(registry, 
              UISystem::State.dragSourceStashType,
              UISystem::State.dragSourceStashTab,
              UISystem::State.dragSourceStashSlot, 
              player, i)) { // i is current inventory slot
              
              UISystem::State.draggedItem = entt::null;
         }
    }
}
```

## 3. 验收标准
1. **启动验证**: 新建存档进入游戏，点击个人仓库，应直接显示 "Personal 1" 页签，无报错。
2. **输入验证**: 打开仓库，点击搜索框，按下 'A', 'S', 'D', 'W'，角色位置应保持静止。
3. **拖拽验证**: 
   - 将物品从仓库拖到背包**空**格子 -> 成功移动。
   - 将物品从仓库拖到背包**占用**格子 -> 成功交换（若不相容则弹窗或回退）。
