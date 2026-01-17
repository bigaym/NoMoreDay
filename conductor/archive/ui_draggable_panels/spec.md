# UI Draggable Panels Spec

## 1. 核心概念
允许玩家通过拖动标题栏（Header）来自由移动主要 UI 面板（角色、背包、打造）。面板位置在游戏会话期间保持，提升 UX 灵活性，避免面板遮挡关键游戏视野。

## 2. 用户故事
- **拖动**: 玩家按住 C 键打开角色面板，发现遮挡了左侧的敌人。玩家将鼠标悬停在“角色属性”标题栏上，按住鼠标左键并拖动，面板跟随鼠标移动。松开左键后，面板停留在新位置。
- **重置**: 每次重新启动游戏时，面板重置为默认居中位置（或者后续实现持久化保存）。
- **边界限制**: 面板不能被拖出屏幕外，保证至少有 50px 的标题栏可见。

## 3. 数据结构

### 3.1 `UIContext.hpp` 更新
在 `UIState` 结构体中添加面板状态管理。

```cpp
// UIContext.hpp

enum class UIPanelID {
    None,
    Character,
    Inventory,
    Crafting,
    Count
};

struct PanelState {
    Vector2 position = { -1.0f, -1.0f }; // -1 表示未初始化，需使用默认位置
    bool isDragging = false;
    Vector2 dragOffset = { 0.0f, 0.0f }; // 鼠标点击位置相对于面板左上角的偏移
};

// 在 UIState 中添加
struct UIState {
    // ... 现有字段 ...
    
    // 面板状态数组
    PanelState panelStates[(int)UIPanelID::Count];
    
    // 当前正在拖动的面板 (互斥)
    UIPanelID activeDragPanel = UIPanelID::None;
};
```

## 4. 系统逻辑

### 4.1 拖动逻辑 (`UISystem`)
创建一个通用辅助函数 `UpdatePanelDrag`，在每个面板的 `Draw` 函数开头调用。

```cpp
// 伪代码
void UpdatePanelDrag(UIPanelID id, float& x, float& y, float w, float h, float headerHeight) {
    auto& state = UISystem::State.panelStates[(int)id];
    Vector2 mousePos = UISystem::GetMousePositionLogic(); // 使用 Logic Mouse Position
    
    // 1. 初始化默认位置
    if (state.position.x < 0) {
        state.position = {x, y};
    }
    
    // 2. 使用存储的位置覆盖传入的 x, y (因为我们希望位置由状态控制)
    x = state.position.x;
    y = state.position.y;
    
    // 3. 处理拖动开始
    bool isMouseOverHeader = CheckCollisionPointRec(mousePos, {x, y, w, headerHeight});
    if (isMouseOverHeader && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && UISystem::State.activeDragPanel == UIPanelID::None) {
        UISystem::State.activeDragPanel = id;
        state.isDragging = true;
        state.dragOffset = { mousePos.x - x, mousePos.y - y };
    }
    
    // 4. 处理拖动中
    if (state.isDragging && UISystem::State.activeDragPanel == id) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            state.position.x = mousePos.x - state.dragOffset.x;
            state.position.y = mousePos.y - state.dragOffset.y;
            
            // 简单边界限制
            float minVis = 50.0f;
            state.position.x = std::clamp(state.position.x, -w + minVis, UI_REF_WIDTH - minVis);
            state.position.y = std::clamp(state.position.y, -h + minVis, UI_REF_HEIGHT - minVis);
            
            x = state.position.x;
            y = state.position.y;
        } else {
            // 拖动结束
            state.isDragging = false;
            UISystem::State.activeDragPanel = UIPanelID::None;
        }
    }
}
```

### 4.2 各面板适配
- **UICharacter**: 将硬编码的 `panelX/Y` 替换为调用 `UpdatePanelDrag` 后更新的坐标。
- **UIInventory**: 同上。
- **UICrafting**: 同上。

## 5. 边缘情况
- **分辨率变化**: 如果窗口大小改变，存储的 `Vector2` 可能会导致面板出界。需要在 `Update` 中检查这一情况，或者简单地在分辨率变化时重置所有位置为 `-1`。
- **重叠**: 目前 Raylib 的绘制顺序决定了 Z-order。简单实现不改变 Z-order（最后绘制的最上层）。如果拖动导致重叠，可能会出现点击穿透。
    - **解决方案**: 简单的 Z-ordering 管理（将当前拖动或点击的面板 ID 移到绘制队列末尾）可能较复杂，暂不在此次迭代实现。这可能导致“背景”面板遮挡“前景”面板。但由于这些全屏/大面板通常互斥打开（除了一些情况），风险可控。**注**: 实际上游戏中目前的面板大多是覆盖式的，很少同时打开多个大面板。

## 6. 数值与常量
- **Header Height**: 统一约定各面板可拖动区域的高度（通常是标题栏，约 40-60px）。
